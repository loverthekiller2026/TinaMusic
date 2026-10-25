import os
import json
import asyncio
from pyrogram import Client, raw

API_ID = int(os.environ["API_ID"])
API_HASH = os.environ["API_HASH"]
SESSION_STRING = os.environ["STRING_SESSIONS"]

# These are the two groups that currently fail in TinaMusic.
TARGET_CHAT_IDS = {-1003744652244, -1004378431588}


async def main():
    app = Client(
        "cache_generator",
        api_id=API_ID,
        api_hash=API_HASH,
        session_string=SESSION_STRING,
        in_memory=True,
    )

    await app.start()

    channels = {}
    users = {}
    seen_chat_ids = set()

    try:
        print("[CACHE] Scanning dialogs visible to STRING_SESSIONS...")

        async for dialog in app.get_dialogs(limit=0):
            chat = dialog.chat
            seen_chat_ids.add(chat.id)
            title = getattr(chat, "title", None) or getattr(chat, "first_name", None) or "unknown"

            print(f"[DIALOG] chat_id={chat.id}, title={title!r}, type={getattr(chat, 'type', 'unknown')}")

            try:
                peer = await app.resolve_peer(chat.id)
            except Exception as exc:
                print(f"[PEER_ERROR] chat_id={chat.id}, title={title!r}, error={exc}")
                continue

            if isinstance(peer, raw.types.InputPeerChannel):
                channels[str(peer.channel_id)] = peer.access_hash
                print(f"[PEER] chat_id={chat.id}, type=InputPeerChannel, peer_id={peer.channel_id}")
            elif isinstance(peer, raw.types.InputPeerUser):
                users[str(peer.user_id)] = peer.access_hash
                print(f"[PEER] chat_id={chat.id}, type=InputPeerUser, peer_id={peer.user_id}")

        for target_id in sorted(TARGET_CHAT_IDS):
            if target_id in seen_chat_ids:
                print(f"[TARGET_FOUND] chat_id={target_id} appears in session dialogs.")
            else:
                print(f"[TARGET_MISSING] chat_id={target_id} does NOT appear in session dialogs.")

        os.makedirs("data", exist_ok=True)
        cache_path = os.path.join("data", "peer_cache.json")

        with open(cache_path, "w", encoding="utf-8") as f:
            json.dump(
                {"InputChannels": channels, "InputUsers": users},
                f,
                indent=2,
            )

        print(f"[CACHE] Saved {len(channels)} channels and {len(users)} users to {cache_path}.")

        missing_channel_ids = {
            str(abs(chat_id) - 1000000000000)
            for chat_id in TARGET_CHAT_IDS
            if chat_id not in seen_chat_ids
        }
        for channel_id in sorted(missing_channel_ids):
            if channel_id not in channels:
                print(f"[TARGET_CACHE_MISSING] channel_id={channel_id} is not in the generated channel cache.")

    finally:
        await app.stop()


asyncio.run(main())
