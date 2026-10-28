import importlib.util
import os
import sys
import types
import unittest
from pathlib import Path
from types import SimpleNamespace


class FakeInputPeerChannel:
    def __init__(self, channel_id, access_hash):
        self.channel_id = channel_id
        self.access_hash = access_hash


class FakeInputPeerUser:
    def __init__(self, user_id, access_hash):
        self.user_id = user_id
        self.access_hash = access_hash


fake_pyrogram = types.ModuleType("pyrogram")
fake_pyrogram.Client = object
fake_pyrogram.raw = SimpleNamespace(
    types=SimpleNamespace(
        InputPeerChannel=FakeInputPeerChannel,
        InputPeerUser=FakeInputPeerUser,
    )
)
sys.modules["pyrogram"] = fake_pyrogram

script_path = Path(__file__).resolve().parents[1] / "generate_cache.py"
spec = importlib.util.spec_from_file_location("generate_cache_under_test", script_path)
generate_cache = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generate_cache)


class FakeApp:
    async def get_chat(self, chat_id):
        return SimpleNamespace(id=chat_id, title="Target group")

    async def resolve_peer(self, chat_id):
        return FakeInputPeerChannel(1836385176, 987654321)


class FailingApp:
    async def get_chat(self, chat_id):
        raise RuntimeError("assistant cannot access target chat")


class PeerCacheTests(unittest.IsolatedAsyncioTestCase):
    def test_normalizes_supergroup_chat_id(self):
        self.assertEqual(
            generate_cache.channel_id_from_chat_id(-1001836385176),
            1836385176,
        )

    def test_ignores_non_channel_ids(self):
        self.assertIsNone(generate_cache.channel_id_from_chat_id(-1836385176))
        self.assertIsNone(generate_cache.channel_id_from_chat_id(1836385176))

    async def test_refresh_replaces_stale_access_hash(self):
        cache = {
            "InputChannels": {"1836385176": 111},
            "InputUsers": {},
        }

        refreshed = await generate_cache.refresh_target_peer(
            FakeApp(), -1001836385176, cache
        )

        self.assertTrue(refreshed)
        self.assertEqual(cache["InputChannels"]["1836385176"], 987654321)

    async def test_failed_refresh_removes_stale_access_hash(self):
        cache = {
            "InputChannels": {"1836385176": 111},
            "InputUsers": {},
        }

        refreshed = await generate_cache.refresh_target_peer(
            FailingApp(), -1001836385176, cache
        )

        self.assertFalse(refreshed)
        self.assertNotIn("1836385176", cache["InputChannels"])


if __name__ == "__main__":
    unittest.main()
