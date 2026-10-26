
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

TARGET_CHAT_IDS = [
    -1003744652244,
]

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

        is_new = channel_id not in cache["InputChannels"]

        cache["InputChannels"][channel_id] = access_hash

        title = getattr(chat, "title", None) or "Unknown"

        print(
            f"[CHANNEL_CACHED] id={channel_id} "
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

async def main():
    api_id = int(os.environ["API_ID"])
    api_hash = os.environ["API_HASH"]
    session_string = os.environ["STRING_SESSIONS"]

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

        # Explicitly check target groups missing from the dialog scan.
        for target_chat_id in TARGET_CHAT_IDS:
            channel_id = str(abs(target_chat_id) - 1000000000000)

            if channel_id in cache["InputChannels"]:
                print(
                    f"[TARGET_LOOKUP] chat_id={target_chat_id} "
                    "already exists in cache"
                )
                continue

            print(f"[TARGET_LOOKUP] Attempting chat_id={target_chat_id}")

            
            try:
                chat = await app.get_chat(target_chat_id)
                result = await cache_peer(app, chat, cache)

                if result == "channel":
                    print(
                        f"[TARGET_LOOKUP] Successfully cached "
                        f"chat_id={target_chat_id}"
                    )
                    resolved += 1
                else:
                    print(
                        f"[TARGET_LOOKUP] Unsupported peer type "
                        f"for chat_id={target_chat_id}"
                    )

            except Exception as exc:
                failed += 1
                print(
                    f"[TARGET_LOOKUP_ERROR] chat_id={target_chat_id} "
                    f"type={type(exc).__name__} error={exc}"
                )


        # Save discovered entries while retaining existing entries.
        save_cache_atomic(cache)

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
            "[CACHE] Newly discovered accessible channels will "
            "be included without manually entering their IDs."
        )

    finally:
        await app.stop()


if __name__ == "__main__":
    asyncio.run(main())
