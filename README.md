<p align="center">
  <img src="assets/banner.png" alt="TinaMusic Banner" width="100%">
</p>

<h1 align="center">🌸《ᴤᴛᴲᴎᴣᴌᴇ᚛〄᚜ᴹᴀᴙᴉᴀ》⚡ Telegram's First Native 100% Pure C++20 Music Engine</h1>

<p align="center">
  <b>The pioneer, high-performance Telegram music bot crafted 100% entirely in modern compiled C++20 with the in-process TinaProto native MTProto & WebRTC voice engine. Zero Go, Zero Python, Zero auxiliary processes.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/First_Ever-Telegram_Pure_C%2B%2B20_Music_Bot-FF69B4?style=for-the-badge&logo=cplusplus" alt="Telegram 1st C++ Music Bot">
  <img src="https://img.shields.io/badge/Language-100%25_Pure_C%2B%2B20-00599C?style=for-the-badge&logo=cplusplus" alt="100% C++20">
  <img src="https://img.shields.io/badge/Engine-TinaProto_Native-FF1493?style=for-the-badge&logo=cplusplus" alt="TinaProto Native">
  <img src="https://img.shields.io/badge/RAM-Under_20MB-success?style=for-the-badge" alt="Under 20MB RAM">
  <img src="https://img.shields.io/badge/License-GNU_AGPLv3-blue?style=for-the-badge" alt="AGPLv3">
---

<p align="center">
  ⭐ <b>Enjoying TinaMusic? Please Star this repository!</b> ⭐<br>
  <i>Your stars motivate continued high-performance open-source development and help other developers discover Telegram's first native C++ audio engine.</i>
</p>

---

## ⚡ Why TinaMusic?

Traditional Telegram music bots written in Python (Pyrogram & PyTgCalls) suffer from heavy runtime bloat:
- **Massive RAM Consumption**: Python bots consume 300MB – 500MB of RAM, causing constant crashes on budget VPS instances and cloud free tiers.
- **Python Global Interpreter Lock (GIL)**: High CPU contention and stuttering audio during group voice chat playback.
- **Slow Startup**: Heavy dependency graphs take 20–45 seconds to spin up.

### 🚀 The TinaMusic Difference
**TinaMusic** is built from the ground up to redefine Telegram audio streaming:
- **Telegram's First Ever C++20 Music Bot**: 100% compiled native binary execution.
- **Ultra-Low Memory Footprint**: Runs rock-solid in **~30 MB RAM** — less than 1/10th the memory of Python bots!
- **Zero Python Runtime**: No Python, no Pyrogram, no PyTgCalls, no pip dependencies.
- **Instant Voice Chat Connection**: Joins and streams within **1–2 seconds**.
- **Mature & Clean Console Logs**: No emojis or cluttered outputs in console logs. Strictly structured `[TIMESTAMP] [LEVEL] [SCOPE] Message`.
- **Bulletproof Cloud Deployment**: Runs buttery-smooth on **Heroku**, **Render**, and **Railway** without crashing or hitting memory quotas!

---

### 💜 1. Deploy on Heroku

[![Deploy](https://www.herokucdn.com/deploy/button.svg)](https://heroku.com/deploy)

1. Fork or clone this repository to your GitHub.
2. Click the **Deploy to Heroku** button above or link your GitHub repo in the Heroku Dashboard.
3. Heroku automatically detects `heroku.yml` and builds the optimized multi-stage Docker container.
4. Fill in your environment variables (`API_ID`, `API_HASH`, `BOT_TOKEN`, `STRING_SESSION`, `OWNER_ID`).
5. Scale your worker dyno in the **Resources** tab:
   ```bash
   heroku ps:scale worker=1
   ```

### 💙 2. Deploy on Render

1. Create a free account on [Render](https://render.com/).
2. Create a **New Blueprint Instance** or a **New Background Worker**.
3. Select **Docker** runtime. Render will automatically read `render.yaml` or `Dockerfile`.
4. Add your `.env` variables under the Environment tab.
5. Click **Deploy**. Your bot will build and start streaming within minutes on Render's free tier!


---

## ⚙️ Configuration Reference (`.env`)

| Variable | Required | Description | Example |
| :--- | :---: | :--- | :--- |
| `API_ID` | **Yes** | Telegram App API ID from [my.telegram.org](https://my.telegram.org) | `1234567` |
| `API_HASH` | **Yes** | Telegram App API Hash from [my.telegram.org](https://my.telegram.org) | `abcdef0123456789...` |
| `BOT_TOKEN` | **Yes** | Telegram Bot Token from [@BotFather](https://t.me/BotFather) | `123456:ABC-DEF...` |
| `STRING_SESSION` | **Yes** | One Pyrogram assistant session string. This single account can play in one group voice chat at a time; it must be a member of the target group and have access to its voice chat. `STRING_SESSIONS` is accepted as a backward-compatible alias for a single value. | `YOUR_SESSION_STRING` |
| `TARGET_CHAT_IDS` | No | Comma-separated supergroup/channel IDs to force-refresh from Telegram during startup. Use this when a peer-cache entry may be stale; each assistant session must have access to the group. | `-1001836385176` |
| `SESSION_TYPE` | No | Session string format (`pyrogram` or `telethon`, default: `pyrogram`) | `pyrogram` |
| `OWNER_ID` | **Yes** | Telegram User ID of the primary owner | `620476130` |
| `MONGO_DB_URI` | No | MongoDB URI for persistent settings & auth lists | `mongodb://...` |
| `LOGGER_ID` | No | Telegram Chat ID for administrative log channel | `-1001234567890` |
| `MUSIC_API_URL` | No | Yuki CDN stream URL template | `https://music.yukiapi.site/stream/{VIDEO_ID}?key=...` |
| `DURATION_LIMIT` | No | Maximum song duration in seconds (default: `4200` = 70 min) | `4200` |
| `QUEUE_LIMIT` | No | Maximum tracks permitted in a queue | `25` |
| `DEFAULT_LANG` | No | Bot interface language (`en`, `hi`, `ar`, `tr`) | `en` |

---

## 🎮 Command Reference

### User Commands
| Command | Arguments | Description |
| :--- | :--- | :--- |
| `/play` | `<track title / link>` | Stream high-quality audio in active group voice chat |
| `/vplay` | `<track title / link>` | Stream video/audio in active voice chat |
| `/queue` | - | Display current chat queue and upcoming songs |
| `/ping` | - | Measure bot latency and system host performance |
| `/help` | - | View interactive help and usage guide |
| `/settings` | - | Configure chat language and audio preferences |

### Admin Commands
| Command | Arguments | Description |
| :--- | :--- | :--- |
| `/pause` | - | Temporarily pause current audio stream |
| `/resume` | - | Resume paused audio stream |
| `/skip` | - | Skip current song and immediately play next in queue |
| `/end` / `/stop` | - | Clear playback queue and disconnect assistant from VC |
| `/shuffle` | - | Randomize the order of tracks in the queue |
| `/loop` | `[1-10 / disable]` | Enable repeat mode for single track or queue |
| `/volume` | `[1-200]` | Set voice chat output volume |
| `/auth` | `[reply / username]` | Grant bot administrative privileges to a user |
| `/unauth` | `[reply / username]` | Revoke administrative privileges from a user |

---
