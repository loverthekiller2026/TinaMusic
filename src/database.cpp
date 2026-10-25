/*
 * TinaMusic database backend.
 * Copyright (C) 2026 TinaMusic Contributors
 * Licensed under the GNU Affero General Public License v3.0 or later.
 */

#include "database.hpp"
#include "logger.hpp"

#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <sys/stat.h>
#include <bson/bson.h>
#include <mongoc/mongoc.h>

namespace {
constexpr const char* kMongoDocumentId = "tinamusic_state";
constexpr const char* kDefaultMongoDatabase = "tinamusic";
constexpr const char* kMongoCollection = "bot_state";

std::string GetEnv(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}
}

Database& Database::Instance() {
    static Database instance;
    return instance;
}

void Database::Init() {
    const std::string mongo_uri = GetEnv("MONGO_DB_URI");
    if (mongo_uri.empty()) {
        Init("data/bot_database.json");
        Logger::Info("DATABASE", "MONGO_DB_URI not set; using local JSON storage.");
        return;
    }

    std::lock_guard<std::mutex> lock(db_mutex_);
    db_file_path_ = "data/bot_database.json";
    mkdir("data", 0755);

    static const bool mongo_initialized = [] {
        mongoc_init();
        return true;
    }();
    (void)mongo_initialized;

    bson_error_t error{};
    mongoc_uri_t* uri = mongoc_uri_new_with_error(mongo_uri.c_str(), &error);
    if (!uri) {
        const std::string message = std::string("Invalid MONGO_DB_URI: ") + error.message;
        Logger::Error("DATABASE", message);
        throw std::runtime_error(message);
    }

    mongoc_client_t* client = mongoc_client_new_from_uri(uri);
    const char* uri_database = mongoc_uri_get_database(uri);
    std::string database_name;
    if (uri_database && *uri_database) {
        database_name = uri_database;
    }
    if (database_name.empty()) {
        database_name = GetEnv("MONGO_DB_NAME");
    }
    if (database_name.empty()) {
        database_name = kDefaultMongoDatabase;
    }
    mongoc_uri_destroy(uri);

    if (!client) {
        Logger::Error("DATABASE", "Could not create MongoDB client.");
        throw std::runtime_error("Could not create MongoDB client.");
    }

    bson_t command;
    bson_t reply;
    bson_init(&command);
    bson_init(&reply);
    BSON_APPEND_INT32(&command, "ping", 1);
    const bool ping_ok = mongoc_client_command_simple(
        client, database_name.c_str(), &command, nullptr, &reply, &error);
    bson_destroy(&command);
    bson_destroy(&reply);
    if (!ping_ok) {
        const std::string message = std::string("MongoDB connection failed: ") + error.message;
        Logger::Error("DATABASE", message);
        mongoc_client_destroy(client);
        throw std::runtime_error(message);
    }

    mongoc_collection_t* collection =
        mongoc_client_get_collection(client, database_name.c_str(), kMongoCollection);
    if (!collection) {
        mongoc_client_destroy(client);
        Logger::Error("DATABASE", "Could not open MongoDB collection.");
        throw std::runtime_error("Could not open MongoDB collection.");
    }

    mongo_client_ = client;
    mongo_collection_ = collection;
    mongo_enabled_ = true;
    LoadFromDisk();
    Logger::Info("DATABASE", "MongoDB connected. Loaded " +
                 std::to_string(served_chats_.size()) + " active chats.");
}

void Database::Init(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    db_file_path_ = db_path;
    mongo_enabled_ = false;
    mkdir("data", 0755);
    LoadFromDisk();
    Logger::Info("DATABASE", "Local JSON database initialized. Loaded " +
                 std::to_string(served_chats_.size()) + " active chats.");
}

void Database::SaveToDisk() {
    nlohmann::json root;
    root["total_plays"] = total_plays_;
    root["sudo_users"] = sudo_users_;
    root["blacklisted"] = blacklisted_;
    root["maintenance_mode"] = maintenance_mode_;
    root["maintenance_reason"] = maintenance_reason_;
    root["logger_enabled"] = logger_enabled_;
    root["autoleave_enabled"] = autoleave_enabled_;

    nlohmann::json chatsJson = nlohmann::json::object();
    for (const auto& [cid, cfg] : chat_configs_) {
        nlohmann::json c;
        c["volume"] = cfg.volume;
        c["play_mode"] = cfg.play_mode;
        c["clean_mode"] = cfg.clean_mode;
        c["language"] = cfg.language;
        c["autoplay"] = cfg.autoplay;
        c["show_thumbnails"] = cfg.show_thumbnails;
        c["command_delete"] = cfg.command_delete;
        c["admin_mode"] = cfg.admin_mode;
        c["clean_mode_duration"] = cfg.clean_mode_duration;
        c["channel_play_id"] = cfg.channel_play_id;
        c["rtmp_url"] = cfg.rtmp_url;
        c["rtmp_key"] = cfg.rtmp_key;
        c["auth_users"] = cfg.auth_users;
        chatsJson[std::to_string(cid)] = c;
    }
    root["chat_configs"] = chatsJson;
    root["served_chats"] = served_chats_;

    if (mongo_enabled_ && mongo_collection_) {
        const std::string serialized = root.dump();
        bson_error_t error{};
        bson_t* replacement = bson_new_from_json(
            reinterpret_cast<const uint8_t*>(serialized.c_str()),
            static_cast<ssize_t>(serialized.size()), &error);
        if (!replacement) {
            Logger::Error("DATABASE", std::string("Could not encode MongoDB state: ") + error.message);
            return;
        }
        BSON_APPEND_UTF8(replacement, "_id", kMongoDocumentId);

        bson_t filter;
        bson_init(&filter);
        BSON_APPEND_UTF8(&filter, "_id", kMongoDocumentId);
        bson_t reply;
        bson_init(&reply);
        const bool ok = mongoc_collection_replace_one(
            static_cast<mongoc_collection_t*>(mongo_collection_), &filter,
            replacement, nullptr, &reply, &error);
        bson_destroy(&filter);
        bson_destroy(replacement);
        bson_destroy(&reply);
        if (!ok) {
            Logger::Error("DATABASE", std::string("MongoDB save failed: ") + error.message);
        }
        return;
    }

    const std::string tmpPath = db_file_path_ + ".tmp";
    std::ofstream ofs(tmpPath);
    if (ofs.is_open()) {
        ofs << root.dump(2);
        ofs.close();
        if (std::rename(tmpPath.c_str(), db_file_path_.c_str()) != 0) {
            Logger::Error("DATABASE", "Could not replace local database file.");
        }
    } else {
        Logger::Error("DATABASE", "Could not open local database file for writing.");
    }
}

void Database::LoadFromDisk() {
    nlohmann::json root;
    bool have_state = false;
    bool migrate_local_state = false;
    bool mongo_read_failed = false;

    if (mongo_enabled_ && mongo_collection_) {
        bson_t filter;
        bson_init(&filter);
        BSON_APPEND_UTF8(&filter, "_id", kMongoDocumentId);
        mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(
            static_cast<mongoc_collection_t*>(mongo_collection_), &filter, nullptr, nullptr);
        const bson_t* document = nullptr;
        if (cursor && mongoc_cursor_next(cursor, &document) && document) {
            size_t json_length = 0;
            char* json_text = bson_as_relaxed_extended_json(document, &json_length);
            if (json_text) {
                try {
                    root = nlohmann::json::parse(json_text, json_text + json_length);
                    root.erase("_id");
                    have_state = true;
                } catch (const std::exception& e) {
                    Logger::Error("DATABASE", std::string("MongoDB state parse failed: ") + e.what());
                    mongo_read_failed = true;
                }
                bson_free(json_text);
            }
        }
        if (cursor) {
            bson_error_t cursor_error{};
            if (mongoc_cursor_error(cursor, &cursor_error)) {
                Logger::Error("DATABASE", std::string("MongoDB read failed: ") + cursor_error.message);
                mongo_read_failed = true;
            }
            mongoc_cursor_destroy(cursor);
        }
        bson_destroy(&filter);

        if (mongo_read_failed) {
            throw std::runtime_error("Could not read TinaMusic state from MongoDB.");
        }

        if (!have_state) {
            std::ifstream ifs(db_file_path_);
            if (ifs.is_open()) {
                try {
                    ifs >> root;
                    have_state = true;
                    migrate_local_state = true;
                    Logger::Info("DATABASE", "Importing existing local JSON data into MongoDB.");
                } catch (const std::exception& e) {
                    Logger::Error("DATABASE", std::string("Local JSON migration read failed: ") + e.what());
                }
            }
        }
    } else {
        std::ifstream ifs(db_file_path_);
        if (!ifs.is_open()) return;
        try {
            ifs >> root;
            have_state = true;
        } catch (const std::exception& e) {
            Logger::Error("DATABASE", std::string("Failed to parse disk database: ") + e.what());
        }
    }

    if (!have_state) return;

    try {
        total_plays_ = root.value("total_plays", 0LL);
        maintenance_mode_ = root.value("maintenance_mode", false);
        maintenance_reason_ = root.value("maintenance_reason", "");
        logger_enabled_ = root.value("logger_enabled", false);
        autoleave_enabled_ = root.value("autoleave_enabled", false);

        if (root.contains("sudo_users")) {
            sudo_users_ = root["sudo_users"].get<std::unordered_set<int64_t>>();
        }
        if (root.contains("blacklisted")) {
            blacklisted_ = root["blacklisted"].get<std::unordered_map<int64_t, std::string>>();
        }
        if (root.contains("served_chats")) {
            served_chats_ = root["served_chats"].get<std::unordered_set<int64_t>>();
        }
        if (root.contains("chat_configs")) {
            for (auto& [key, val] : root["chat_configs"].items()) {
                const int64_t cid = std::stoll(key);
                ChatConfig cfg;
                cfg.volume = val.value("volume", 100);
                cfg.play_mode = val.value("play_mode", "Everyone");
                cfg.clean_mode = val.value("clean_mode", true);
                cfg.language = val.value("language", "en");
                cfg.autoplay = val.value("autoplay", false);
                cfg.show_thumbnails = val.value("show_thumbnails", true);
                cfg.command_delete = val.value("command_delete", false);
                cfg.admin_mode = val.value("admin_mode", "adminauth");
                cfg.clean_mode_duration = val.value("clean_mode_duration", 15);
                cfg.channel_play_id = val.value("channel_play_id", 0LL);
                cfg.rtmp_url = val.value("rtmp_url", "");
                cfg.rtmp_key = val.value("rtmp_key", "");
                if (val.contains("auth_users")) {
                    cfg.auth_users = val["auth_users"].get<std::unordered_set<int64_t>>();
                }
                chat_configs_[cid] = cfg;
            }
        }
    } catch (const std::exception& e) {
        Logger::Error("DATABASE", std::string("Failed to load database state: ") + e.what());
        throw;
    }

    if (migrate_local_state) {
        SaveToDisk();
    }
}

ChatConfig Database::GetChatConfig(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    auto it = chat_configs_.find(chat_id);
    if (it != chat_configs_.end()) return it->second;
    ChatConfig def;
    chat_configs_[chat_id] = def;
    return def;
}

void Database::SetChatConfig(int64_t chat_id, const ChatConfig& cfg) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    chat_configs_[chat_id] = cfg;
    SaveToDisk();
}

bool Database::IsAutoplayEnabled(int64_t chat_id) {
    return GetChatConfig(chat_id).autoplay;
}

void Database::SetAutoplay(int64_t chat_id, bool enabled) {
    ChatConfig cfg = GetChatConfig(chat_id);
    cfg.autoplay = enabled;
    SetChatConfig(chat_id, cfg);
}

bool Database::IsThumbnailsEnabled(int64_t chat_id) {
    return GetChatConfig(chat_id).show_thumbnails;
}

void Database::SetThumbnails(int64_t chat_id, bool enabled) {
    ChatConfig cfg = GetChatConfig(chat_id);
    cfg.show_thumbnails = enabled;
    SetChatConfig(chat_id, cfg);
}

bool Database::IsMaintenanceEnabled() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return maintenance_mode_;
}

std::string Database::GetMaintenanceReason() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return maintenance_reason_;
}

void Database::SetMaintenance(bool enabled, const std::string& reason) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    maintenance_mode_ = enabled;
    maintenance_reason_ = reason;
    SaveToDisk();
}

bool Database::IsLoggerEnabled() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return logger_enabled_;
}

void Database::SetLoggerEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    logger_enabled_ = enabled;
    SaveToDisk();
}

bool Database::IsAutoLeaveEnabled() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return autoleave_enabled_;
}

void Database::SetAutoLeaveEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    autoleave_enabled_ = enabled;
    SaveToDisk();
}

void Database::AddSudo(int64_t user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    sudo_users_.insert(user_id);
    SaveToDisk();
}

void Database::RemoveSudo(int64_t user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    sudo_users_.erase(user_id);
    SaveToDisk();
}

bool Database::IsSudo(int64_t user_id, int64_t owner_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (user_id == owner_id) return true;
    return sudo_users_.find(user_id) != sudo_users_.end();
}

std::unordered_set<int64_t> Database::GetSudoUsers() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return sudo_users_;
}

void Database::Blacklist(int64_t id, const std::string& reason) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    blacklisted_[id] = reason;
    SaveToDisk();
}

void Database::Unblacklist(int64_t id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    blacklisted_.erase(id);
    SaveToDisk();
}

bool Database::IsBlacklisted(int64_t id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return blacklisted_.find(id) != blacklisted_.end();
}

void Database::IncrementPlays() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    total_plays_++;
    SaveToDisk();
}

void Database::RegisterChat(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    served_chats_.insert(chat_id);
    SaveToDisk();
}

int64_t Database::GetTotalPlays() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return total_plays_;
}

size_t Database::GetTotalChats() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return served_chats_.size();
}

std::unordered_set<int64_t> Database::GetServedChats() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    return served_chats_;
}
