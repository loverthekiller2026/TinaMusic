/*
 * ============================================================================
 *   Project: TinaMusic - Telegram's First Native C++20 Music Bot Engine
 *   Architecture: Native C++20 Core + Pure C++20 TinaProto Voice Engine
 *   Copyright (C) 2026 SUDEEPBOTS / TinaMusic Contributors
 *   Support: https://t.me/Z0iiw
 *   License: GNU Affero General Public License v3.0
 * ============================================================================
 */

#include "tinaproto/client.hpp"
#include "logger.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tinaproto {

namespace {
std::mutex g_pool_mutex;
std::vector<TinaProtoClient*> g_clients;
std::vector<std::unique_ptr<TinaProtoClient>> g_extra_clients;
std::unordered_map<int64_t, size_t> g_chat_session;
std::vector<bool> g_session_busy;
std::function<void(int64_t)> g_stream_end_callback;
}


TinaProtoClient& TinaProtoClient::GetInstance() {
    static TinaProtoClient instance;
    return instance;
}

TinaProtoClient::TinaProtoClient() = default;

TinaProtoClient::~TinaProtoClient() = default;

bool TinaProtoClient::InitializePool(const std::vector<std::string>& sessions, int32_t api_id,
                                     const std::string& api_hash) {
    std::vector<std::string> usable;
    std::unordered_set<std::string> unique_sessions;
    for (const auto& session : sessions) {
        if (session.empty()) continue;
        if (!unique_sessions.insert(session).second) {
            Logger::Error("TINAPROTO", "Duplicate STRING_SESSIONS entry detected; each concurrent group needs a different Telegram account");
            return false;
        }
        usable.push_back(session);
    }
    if (usable.empty()) {
        Logger::Error("TINAPROTO", "No STRING_SESSIONS configured");
        return false;
    }

    std::lock_guard<std::mutex> pool_lock(g_pool_mutex);
    if (!g_clients.empty()) return g_clients.size() == usable.size();

    TinaProtoClient& primary = GetInstance();
    if (!primary.Initialize(usable.front(), api_id, api_hash, "data/peer_cache.json")) return false;
    g_clients.push_back(&primary);
    g_session_busy.push_back(false);

    for (size_t i = 1; i < usable.size(); ++i) {
        auto client = std::make_unique<TinaProtoClient>();
        const std::string cache_path = "data/peer_cache_" + std::to_string(i + 1) + ".json";
        if (!client->Initialize(usable[i], api_id, api_hash, cache_path)) {
            Logger::Error("TINAPROTO", "Failed to initialize assistant session " + std::to_string(i + 1));
            return false;
        }
        client->SetOnStreamEnd(g_stream_end_callback);
        g_clients.push_back(client.get());
        g_extra_clients.push_back(std::move(client));
        g_session_busy.push_back(false);
    }
    Logger::Info("TINAPROTO", "Initialized " + std::to_string(g_clients.size()) +
                 " independent assistant session(s) for concurrent group calls");
    return true;
}

TinaProtoClient* TinaProtoClient::ForChat(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(g_pool_mutex);
    auto existing = g_chat_session.find(chat_id);
    if (existing != g_chat_session.end() && existing->second < g_clients.size()) return g_clients[existing->second];
    for (size_t i = 0; i < g_clients.size(); ++i) {
        if (!g_session_busy[i]) {
            g_session_busy[i] = true;
            g_chat_session[chat_id] = i;
            Logger::Info("TINAPROTO", "Assigned assistant session " + std::to_string(i + 1) +
                         " to chat " + std::to_string(chat_id));
            return g_clients[i];
        }
    }
    Logger::Warn("TINAPROTO", "No free assistant session for chat " + std::to_string(chat_id) +
                 "; configure one STRING_SESSIONS entry per concurrent voice chat");
    return nullptr;
}

TinaProtoClient* TinaProtoClient::FindForChat(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(g_pool_mutex);
    auto existing = g_chat_session.find(chat_id);
    if (existing == g_chat_session.end() || existing->second >= g_clients.size()) return nullptr;
    return g_clients[existing->second];
}

void TinaProtoClient::ReleaseChat(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(g_pool_mutex);
    auto existing = g_chat_session.find(chat_id);
    if (existing == g_chat_session.end()) return;
    if (existing->second < g_session_busy.size()) g_session_busy[existing->second] = false;
    g_chat_session.erase(existing);
}

void TinaProtoClient::SetOnStreamEndForAll(std::function<void(int64_t)> cb) {
    std::lock_guard<std::mutex> lock(g_pool_mutex);
    g_stream_end_callback = std::move(cb);
    for (auto* client : g_clients) if (client) client->SetOnStreamEnd(g_stream_end_callback);
}

size_t TinaProtoClient::SessionCount() {
    std::lock_guard<std::mutex> lock(g_pool_mutex);
    return g_clients.size();
}

std::string TinaProtoClient::BuildFfmpegCommand(const std::string& url) {
    std::string base = "ffmpeg ";
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        base += "-reconnect 1 -reconnect_at_eof 1 -reconnect_streamed 1 -reconnect_delay_max 2 ";
    }
    base += "-v quiet -i \"" + url + "\" -f s16le -ac 2 -ar 48000 pipe:1";
    return base;
}

bool TinaProtoClient::Initialize(const std::string& session_str, int32_t api_id, const std::string& api_hash,
                                 const std::string& peer_cache_path) {
    (void)api_hash;
    std::lock_guard<std::mutex> lock(mtx_);
    if (initialized_) {
        return true;
    }

    auto sess_opt = SessionDecoder::DecodePyrogram(session_str);
    if (!sess_opt) {
        Logger::Error("TINAPROTO", "Failed to decode Pyrogram session string");
        return false;
    }

    if (!mtproto_.Connect(*sess_opt)) {
        Logger::Error("TINAPROTO", "Failed to connect MTProto transport");
        return false;
    }

    if (!mtproto_.InitConnection(static_cast<uint32_t>(api_id))) {
        Logger::Error("TINAPROTO", "Failed to initialize MTProto connection layer");
        return false;
    }

    if (!mtproto_.LoadPeerCache(peer_cache_path)) {
        Logger::Warn("TINAPROTO", "Peer cache could not be loaded from " + peer_cache_path + "; playback may fail for channels absent from cache");
    }

    if (!voice_binding_.Init()) {
        Logger::Error("TINAPROTO", "Failed to initialize native NTgCalls engine");
        return false;
    }

    voice_binding_.SetOnStreamEnd([this](int64_t chat_id) {
        Logger::Info("TINAPROTO", "Voice stream ended for chat " + std::to_string(chat_id));
        if (on_stream_end_cb_) {
            on_stream_end_cb_(chat_id);
        }
    });

    initialized_ = true;
    Logger::Info("TINAPROTO", "Pure C++20 TinaProto voice engine initialized successfully");
    return true;
}

StreamResult TinaProtoClient::Play(int64_t chat_id, const std::string& stream_url) {
    std::lock_guard<std::mutex> lock(mtx_);
    StreamResult result;
    if (!initialized_) {
        result.ok = false;
        result.error = "NOT_INITIALIZED";
        result.detail = "TinaProto client has not been initialized";
        return result;
    }

    std::string cmd = BuildFfmpegCommand(stream_url);

    if (voice_binding_.HasActiveCall(chat_id)) {
        // Replacing an FFmpeg source can produce a late stream-end callback
        // for the old source. Do not let it advance the new track's queue.
        voice_binding_.SuppressStreamEndFor(chat_id);
        if (voice_binding_.SetStreamSources(chat_id, cmd)) {
            result.ok = true;
            Logger::Info("TINAPROTO", "Updated active stream sources for chat " + std::to_string(chat_id));
            return result;
        } else {
            Logger::Warn("TINAPROTO", "SetStreamSources failed on active call, leaving the old Telegram call before recreating it for chat " + std::to_string(chat_id));
            voice_binding_.Stop(chat_id);
            // Stopping NTgCalls alone can leave a stale server-side participant.
            mtproto_.LeaveGroupCall(chat_id);
        }
    }

    std::string local_sdp = voice_binding_.CreateCall(chat_id);
    if (local_sdp.empty()) {
        result.ok = false;
        result.error = "CALL_CREATE_FAILED";
        result.detail = "libntgcalls failed to generate local WebRTC SDP offer";
        return result;
    }

    if (!voice_binding_.SetStreamSources(chat_id, cmd)) {
        voice_binding_.Stop(chat_id);
        result.ok = false;
        result.error = "SET_SOURCES_FAILED";
        result.detail = "libntgcalls failed to attach audio stream command";
        return result;
    }

    std::string remote_sdp = mtproto_.JoinGroupCall(chat_id, local_sdp);
    if (remote_sdp.empty() || remote_sdp == "{\"transport\": null}") {
        voice_binding_.Stop(chat_id);
        // Telegram may have accepted joinGroupCall even if its transport response
        // could not be decoded; best-effort leave before allowing another retry.
        mtproto_.LeaveGroupCall(chat_id);
        result.ok = false;
        result.error = "JOIN_GROUP_CALL_FAILED";
        result.detail = "Telegram MTProto rejected joinGroupCall or returned empty transport";
        return result;
    }

    if (!voice_binding_.Connect(chat_id, remote_sdp)) {
        voice_binding_.Stop(chat_id);
        mtproto_.LeaveGroupCall(chat_id);
        result.ok = false;
        result.error = "WEBRTC_CONNECT_FAILED";
        result.detail = "libntgcalls failed to establish WebRTC connection with Telegram server";
        return result;
    }

    result.ok = true;
    Logger::Info("TINAPROTO", "WebRTC voice call connected and streaming for chat " + std::to_string(chat_id));
    return result;
}

void TinaProtoClient::Pause(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    voice_binding_.Pause(chat_id);
}

void TinaProtoClient::Resume(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    voice_binding_.Resume(chat_id);
}

void TinaProtoClient::Stop(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    voice_binding_.SuppressStreamEndFor(chat_id);
    voice_binding_.Stop(chat_id);
    mtproto_.LeaveGroupCall(chat_id);
}

int64_t TinaProtoClient::Time(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    return voice_binding_.Time(chat_id);
}

void TinaProtoClient::SetOnStreamEnd(std::function<void(int64_t)> cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    on_stream_end_cb_ = std::move(cb);
}

std::optional<InputGroupCall> TinaProtoClient::GetGroupCall(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    return mtproto_.GetGroupCall(chat_id);
}

bool TinaProtoClient::HasActiveCall(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    return voice_binding_.HasActiveCall(chat_id);
}

}
