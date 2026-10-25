import os
import json
import asyncio
from pyrogram import Client, raw

API_ID = int(os.environ["API_ID"])
API_HASH = os.environ["API_HASH"]
SESSION_STRING = os.environ["STRING_SESSIONS"]

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

    try:
        # Iterate through every dialog, not just the first 100. Resolve each
        # dialog through Pyrogram's peer storage to retrieve its MTProto hash.
        async for dialog in app.get_dialogs(limit=0):
            chat = dialog.chat
            try:
                peer = await app.resolve_peer(chat.id)
                print(
                f"[PEER] chat_id={chat.id}, "
                f"type={type(peer).__name__}, "
                f"peer_id={getattr(peer, 'channel_id', getattr(peer, 'user_id', None))}"
    )
            except Exception as exc:
                print(f"Skipping peer {chat.id}: {exc}")
                continue

            if isinstance(peer, raw.types.InputPeerChannel):
                channels[str(peer.channel_id)] = peer.access_hash
            elif isinstance(peer, raw.types.InputPeerUser):
                users[str(peer.user_id)] = peer.access_hash

        os.makedirs("data", exist_ok=True)

        with open("data/peer_cache.json", "w") as f:
            json.dump({
                "InputChannels": channels,
                "InputUsers": users,
            }, f, indent=2)

        print(f"Saved {len(channels)} channels and {len(users)} users.")

    finally:
        await app.stop()

asyncio.run(main())
