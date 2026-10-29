

#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct AppConfig {
    std::string bot_token{"8881619004:AAGZLJULCjTsX1Em4QCVzGpze0XXj0ebNxs"};
    std::string api_id{"1621962"};
    std::string api_hash{"6f019552c1cf83dec1851516b1bb099c"};
    int64_t owner_id{8801719386};
    std::string mongo_uri{"mongodb+srv://kurukshethrathepride_db_user:D7fBdPjjDLnjqkdr@kurukshethra.5m7kbnw.mongodb.net/"};
    int64_t log_group_id{"-1003744652244"};
    std::string string_session{"BQAYv8oANhOvujMe85F-jJP_f9aLrNOpvaK6PK7USBYl1iRxJjJ8V4EX3vLZ697zZhBODcjwJbh3CnxWc0r-hzXwNDPBdJ6Aah4doi1Z5FM0-NWWfa1_SRyhk0Vw25n_IunY-UErdITEj5TUo6YQ8C-4zgpLQ9WQlXfesqh1NcBqi-kObYmpJdd6QS6ZgOdd_mnjW0aO0FUebMgfOhOmbu8RouChoUOMljGoryPP-TpDzMLc1de2I6fai0Spxl5UlPimr2B9qF0aYpBVY3c5GZNImD0yKOiKQkoY007sQKzpk68wTsxwBOPvhmCYyCJTi5k15rXwT854XECA65WL4HWGF8GmrQAAAAIMn5RaAA"};
    std::vector<std::string> string_sessions;
    std::string session_type{"pyrogram"};
    std::string support_chat{"https://t.me/sector_bot_support"};
    std::string support_channel{"https://t.me/HQ_Tracks"};
    std::string default_lang{"en"};
    int duration_limit{7800};
    int max_queue_size{24};
    std::string default_platform{"YouTube"};
    std::string spotify_client_id;
    std::string spotify_client_secret;
    std::string yuki_api_url{"https://music.yukiapi.site"};
    std::string yuki_api_key{"yuki_96fcd2d09a8d4f1e36896505bd6ebda8"};
    std::string music_api_url{"https://music.yukiapi.site/stream/{VIDEO_ID}?key=yuki_96fcd2d09a8d4f1e36896505bd6ebda8"};
    std::string ping_img_url{"https://telegra.ph/file/91533956c91d0fd7c9f20.jpg"};
    std::vector<std::string> start_images;
};

class ConfigLoader {
public:
    static AppConfig Load(const std::string& env_path = ".env");

private:
    static std::string GetEnvVar(const std::string& key, const std::string& default_val = "");
    static void ParseEnvFile(const std::string& path);
};
