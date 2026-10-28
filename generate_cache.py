
import os
import json
import asyncio
import tempfile

from pyrogram import Client, raw


# --------------------------------------------------
# PATHS
# --------------------------------------------------

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, "data")
CACHE_PATH = os.path.join(DATA_DIR, "peer_cache.json")

def configured_target_chat_ids():
    """Read optional chat IDs that should be resolved even if absent from dialogs.

    Normal automatic discovery still scans every dialog available to the
    logged-in Telegram session. Set TARGET_CHAT_IDS to a comma-separated list
    only for chats that should be explicitly checked as an additional fallback.
    """
    raw_ids = os.environ.get("TARGET_CHAT_IDS", "").strip()
    if not raw_ids:
        return []

    target_ids = []
    for item in raw_ids.split(","):
        item = item.strip()
        if not item:
            continue
        try:
            target_ids.append(int(item))
        except ValueError as exc:
            raise ValueError(
                f"Invalid TARGET_CHAT_IDS entry {item!r}; use comma-separated integer chat IDs"
            ) from exc
    return list(dict.fromkeys(target_ids))


def channel_id_from_chat_id(chat_id):
    """Convert a Telegram supergroup/channel dialog ID to its raw channel ID."""
    if chat_id >= 0:
        return None
    absolute_id = abs(chat_id)
    if absolute_id <= 1000000000000:
        # Legacy basic groups are not InputPeerChannel peers.
        return None
    return absolute_id - 1000000000000


TARGET_CHAT_IDS = configured_target_chat_ids()

# --------------------------------------------------
# CACHE MANAGEMENT
# --------------------------------------------------

def load_existing_cache():
    """Load and preserve previously cached peers."""

    empty_cache = {
        "InputChannels": {},
        "InputUsers": {},
    }

    if not os.path.exists(CACHE_PATH):
        return empty_cache

    try:
        with open(CACHE_PATH, "r", encoding="utf-8") as file:
            data = json.load(file)

        if not isinstance(data, dict):
            raise ValueError("Cache root must be a JSON object")

        channels = data.get("InputChannels", {})
        users = data.get("InputUsers", {})

        if not isinstance(channels, dict):
            raise ValueError("InputChannels must be a JSON object")

        if not isinstance(users, dict):
            raise ValueError("InputUsers must be a JSON object")

        return {
            "InputChannels": channels,
            "InputUsers": users,
        }

    except Exception as exc:
        raise RuntimeError(
            f"Cannot safely load existing cache: {exc}"
        ) from exc


def save_cache_atomic(cache):
    """Save the complete cache without partially overwriting it."""

    os.makedirs(DATA_DIR, exist_ok=True)

    fd, temp_path = tempfile.mkstemp(
        prefix="peer_cache_",
        suffix=".tmp",
        dir=DATA_DIR,
    )

    try:
        with os.fdopen(fd, "w", encoding="utf-8") as file:
            json.dump(cache, file, indent=2)
            file.write("\n")
            file.flush()
            os.fsync(file.fileno())

        os.replace(temp_path, CACHE_PATH)

    finally:
        if os.path.exists(temp_path):
            os.unlink(temp_path)


# --------------------------------------------------
# PEER DISCOVERY
# --------------------------------------------------

async def cache_peer(app, chat, cache):
    """
    Resolve a Telegram dialog's peer and cache its access hash
    when it is a supported channel or user peer.
    """

    peer = await app.resolve_peer(chat.id)

    if isinstance(peer, raw.types.InputPeerChannel):
        channel_id = str(peer.channel_id)
        access_hash = int(peer.access_hash)
        if int(channel_id) <= 0:
            raise ValueError(f"Invalid Telegram channel ID returned by resolve_peer: {channel_id}")

        is_new = channel_id not in cache["InputChannels"]

        cache["InputChannels"][channel_id] = access_hash

        title = getattr(chat, "title", None) or "Unknown"

        print(
            f"[CHANNEL_CACHED] chat_id={chat.id} channel_id={channel_id} "
            f"new={is_new} title={title!r}"
        )

        return "channel"

    if isinstance(peer, raw.types.InputPeerUser):
        user_id = str(peer.user_id)
        access_hash = int(peer.access_hash)

        is_new = user_id not in cache["InputUsers"]

        cache["InputUsers"][user_id] = access_hash

        print(
            f"[USER_CACHED] id={user_id} new={is_new}"
        )

        return "user"

    # Basic groups can use InputPeerChat, which does not have
    # a channel access hash. The current C++ cache format does
    # not store this peer type.
    print(
        f"[PEER_UNSUPPORTED] chat_id={chat.id} "
        f"type={type(peer).__name__}"
    )

    return None


# --------------------------------------------------
# MAIN
# --------------------------------------------------

async def build_cache(session_string, cache_path):
    global CACHE_PATH
    CACHE_PATH = cache_path
    api_id = int(os.environ["API_ID"])
    api_hash = os.environ["API_HASH"]

    cache = load_existing_cache()

    previous_channels = len(cache["InputChannels"])
    previous_users = len(cache["InputUsers"])

    app = Client(
        "cache_generator",
        api_id=api_id,
        api_hash=api_hash,
        session_string=session_string,
        in_memory=True,
    )

    await app.start()

    scanned = 0
    resolved = 0
    failed = 0

    try:
        print("[CACHE] Starting automatic Telegram peer discovery...")
        print("[CACHE] Scanning all available Telegram dialogs.")

        async for dialog in app.get_dialogs():
            chat = dialog.chat
            scanned += 1

            title = (
                getattr(chat, "title", None)
                or getattr(chat, "first_name", None)
                or "Unknown"
            )

            print(
                f"[DIALOG] id={chat.id} "
                f"type={getattr(chat, 'type', 'unknown')} "
                f"title={title!r}"
            )

            try:
                result = await cache_peer(app, chat, cache)

                if result is not None:
                    resolved += 1

            except Exception as exc:
                failed += 1
                print(
                    f"[PEER_ERROR] chat_id={chat.id} "
                    f"title={title!r} error={exc}"
                )

        # Optional fallback for configured IDs that did not appear in dialogs.
        # This list is not required for normal discovery.
        for target_chat_id in TARGET_CHAT_IDS:
            raw_channel_id = channel_id_from_chat_id(target_chat_id)
            if raw_channel_id is None:
                print(
                    f"[TARGET_LOOKUP] Skipping chat_id={target_chat_id}: "
                    "not a supergroup/channel ID"
                )
                continue

            channel_id = str(raw_channel_id)
            previous_hash = cache["InputChannels"].pop(channel_id, None)
            print(
                f"[TARGET_LOOKUP] Refreshing chat_id={target_chat_id} "
                f"channel_id={channel_id} "
                f"previous_entry={'present' if previous_hash is not None else 'missing'}"
            )

            try:
                chat = await app.get_chat(target_chat_id)
                result = await cache_peer(app, chat, cache)

                if result == "channel":
                    print(
                        f"[TARGET_LOOKUP] Refreshed peer cache for "
                        f"chat_id={target_chat_id}"
                    )
                    resolved += 1
                else:
                    failed += 1
                    print(
                        f"[TARGET_LOOKUP] Unsupported peer type "
                        f"for chat_id={target_chat_id}; stale entry was removed"
                    )

            except Exception as exc:
                failed += 1
                print(
                    f"[TARGET_LOOKUP_ERROR] chat_id={target_chat_id} "
                    f"type={type(exc).__name__} error={exc}; stale entry was removed"
                )


        # Save discovered entries while retaining existing entries.
        save_cache_atomic(cache)

        # Verify the final on-disk cache before reporting success.
        with open(CACHE_PATH, "r", encoding="utf-8") as file:
            saved_cache = json.load(file)
        if not isinstance(saved_cache.get("InputChannels"), dict):
            raise RuntimeError("Saved peer cache is missing InputChannels")
        if not isinstance(saved_cache.get("InputUsers"), dict):
            raise RuntimeError("Saved peer cache is missing InputUsers")
        cache = saved_cache

        print("")
        print("[CACHE] Discovery finished.")
        print(f"[CACHE] Dialogs scanned: {scanned}")
        print(f"[CACHE] Peers resolved: {resolved}")
        print(f"[CACHE] Resolution failures: {failed}")
        print(
            f"[CACHE] Channels: {previous_channels} -> "
            f"{len(cache['InputChannels'])}"
        )
        print(
            f"[CACHE] Users: {previous_users} -> "
            f"{len(cache['InputUsers'])}"
        )
        print(f"[CACHE] Saved to: {CACHE_PATH}")

        print(
            "[CACHE] Automatic discovery scans all dialogs visible to this "
            "Telegram session; accessible supergroups and channels are cached "
            "without manually entering their IDs."
        )
        if not TARGET_CHAT_IDS:
            print(
                "[CACHE] TARGET_CHAT_IDS is unset; no hardcoded target list "
                "is required for automatic dialog discovery."
            )

    finally:
        await app.stop()


async def main():
    # TinaMusic is configured to use one assistant account/session.
    session_string = os.environ.get("STRING_SESSION", os.environ.get("STRING_SESSIONS", "")).strip()
    if not session_string:
        raise RuntimeError("STRING_SESSION is required (one Pyrogram session string)")

    cache_path = os.path.join(DATA_DIR, "peer_cache.json")
    print("\n[CACHE] Building peer cache for the single assistant session")
    await build_cache(session_string, cache_path)


if __name__ == "__main__":
    asyncio.run(main())
