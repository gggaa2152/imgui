#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <thread>
#include <cmath>
#include <mutex>
#include <deque>
#include <chrono>
#include <fcntl.h>
#include <sys/mman.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"
#include "HeroImages.h"

#define IMGUI_DEFINE_MATH_OPERATORS 
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include <GLES3/gl3.h>
#include <EGL/egl.h>    
#include <jni.h>
#include "dobby.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JKInternal", __VA_ARGS__)

void SaveConfig();
void LoadConfig();
void ClearGameState();
bool TryResolveSegmentCSOGame(uintptr_t* out_segment);
void UpdateMatchState();
void ParseGameMemory();
void DrawMainMenu();
void DrawCardPoolWindow();
void DrawPlayerDataWindow();
void DrawOpponentBoardWindow();
void ProcessTextureQueue();
void UpdateFontHD(bool force);
static void CaptureWindowPos(const char* name, float& x, float& y);

uintptr_t g_il2cppTrueBase = 0;
bool g_show_menu = true;

struct Offsets {
    uint32_t func_get_Instance = 0x80EF374;
    uint32_t addr2 = 0x10;
    uint32_t addr3 = 0x20;
    uint32_t addra = 0x10;
    uint32_t segmentcsogame = 0x20;
    uint32_t func_quit = 0x6ad90b4;
    uint32_t my_player_id = 0x100;          
    uint32_t segment_my_player_id = 0xEC;   
    uint32_t next_opponents_list = 0x240;
    uint32_t func_reqbuyhero = 0x46d6e94;
    uint32_t func_shop_listen = 0x722e680;
    uint32_t func_buy_hero_new = 0x7232fe4;
    uint32_t func_set_IsGameEnd = 0x6F489C8;
    uint32_t addr4 = 0x230;
    uint32_t addr5 = 0x18;
    uint32_t addr6 = 0x18;
    uint32_t addr7 = 0x10;
    uint32_t addr7_struct_size = 0x20; 
    uint32_t addr7_ptr_offset = 0x10;
    uint32_t addr9 = 0x10;
    uint32_t addr9_struct_size = 0x20;
    uint32_t addr9_ptr_offset = 0x10;
    uint32_t ph_heroId = 0x10;
    uint32_t ph_remaining = 0x1c; 
    uint32_t ph_total = 0x20;     
    uint32_t addr11 = 0x38;
    uint32_t addr12 = 0x18;
    uint32_t addr12_struct_size = 0x08;
    uint32_t addr12_ptr_offset = 0x08;
    uint32_t addr13 = 0x50;
    uint32_t pi_name = 0x18;
    uint32_t pi_id = 0x20;
    uint32_t pi_is_bot = 0x58;
    uint32_t pi_money = 0x5c;
    uint32_t pi_win_streak = 0xdc;
    uint32_t pi_lose_streak = 0xe0;
    uint32_t pi_level = 0xf0;
    uint32_t addr14 = 0x108;
    uint32_t addr15 = 0x10;
    uint32_t addr16 = 0x10;
    uint32_t shop_hero_id = 0x14; 
    uint32_t addr17 = 0x388;
    uint32_t addr18 = 0x10;
    uint32_t bench_hero_id = 0x108; 
    uint32_t addr19 = 0x390;
    uint32_t addr20 = 0x10; 
    uint32_t board_hero_id = 0x108; 
    uint32_t board_x = 0x30;
    uint32_t board_y = 0x34;
    uint32_t addr21 = 0x128;
    uint32_t addr22 = 0x18;
    uint32_t addr23 = 0x08;
    uint32_t addr23_struct_size = 0x20;
    uint32_t addr23_ptr_offset = 0x10;
    uint32_t addr26 = 0x68;              
    uint32_t pi_avatar_rank = 0x2DC;     
    uint32_t pi_avatar_player_id = 0x248; 
    uint32_t hexctrl = 0x60;
    uint32_t func_get_hex = 0x6e356d4;
};
Offsets g_off;

uintptr_t g_dbg_addr1 = 0, g_dbg_addr2 = 0, g_dbg_addr3 = 0, g_dbg_addra = 0, g_dbg_segmentcsogame = 0;
uintptr_t g_dbg_addr4 = 0, g_dbg_addr5 = 0, g_dbg_addr6 = 0, g_dbg_addr7 = 0;
uintptr_t g_dbg_addr11 = 0, g_dbg_addr12 = 0, g_dbg_addr13 = 0;
uintptr_t g_dbg_addr21 = 0, g_dbg_addr22 = 0, g_dbg_addr23 = 0;
uintptr_t g_dbg_addr26 = 0;
uintptr_t g_dbg_hexctrl = 0;

int g_my_player_id = 0;
int g_hex_qualities[3] = {0, 0, 0};
std::vector<int> g_next_opponents;

struct PoolHero { int heroId; int remaining; int total; int cost; uintptr_t addr10; };
std::vector<PoolHero> g_poolHeroes;
std::map<int, std::vector<int>> g_heroesByCost;
std::unordered_map<int, bool> g_heroAutoBuyChecked;

struct BoardHero { int heroId; int x; int y; };
struct PlayerInfo {
    std::string name; int id; bool is_bot; int money; int win_streak; int lose_streak; int level;
    int avatar_rank = 0; uintptr_t val_ptr = 0; uintptr_t addr13_ptr = 0;
    std::vector<int> shop; std::vector<int> bench; std::vector<BoardHero> board;
};
std::vector<PlayerInfo> g_players;

std::atomic<bool> g_is_in_match{false};
std::atomic<bool> g_match_enter_pending{false};
static int g_segment_valid_streak = 0;
static bool g_need_segment_gap_before_enter = false;

bool g_isImGuiInit = false; 
ImFont* g_mainFont = nullptr;
float g_autoScale = 1.0f;
float g_current_rendered_size = 0.0f;
bool g_needUpdateFontSafe = false;
float g_menuX = 100.0f, g_menuY = 100.0f;
float g_menuW = 880.0f, g_menuH = 680.0f;
bool g_menuCollapsed = false;
float g_scale = 1.0f;
int g_ui_theme = 0;
float g_ui_anim[32] = {0};
bool g_menu_orb = false;
float g_orb_x = 120.0f, g_orb_y = 120.0f;
float g_orb_r = 34.0f;

std::atomic<bool> g_engine_rendering{false};
int g_current_frame = 0;

bool g_win_cardpool = true, g_win_playerdata = true, g_win_hextech = true, g_win_hero_warn = true;
float g_alpha_cp = 1.0f, g_alpha_pd = 1.0f, g_alpha_opp = 1.0f, g_alpha_hex = 1.0f, g_alpha_hero_warn = 1.0f;
int g_cp_columns = 6, g_cp_rows = 0; 
float g_cp_box_size = 65.0f, g_cp_scale = 1.0f;
bool g_cp_show_cost[6] = { false, true, true, true, true, true };
bool g_cp_warning_enable = true; int g_cp_warning_thres = 3;
float g_pd_line_spacing = 0.0f, g_pd_vert_spacing = 0.0f, g_pd_arrow_spacing = 15.0f, g_pd_font_size = 1.0f;
bool g_pd_hero_summary_enable = true;
int g_pd_hero_count_min[6] = {0, 1, 1, 1, 1, 1};
bool g_opp_show_board = true, g_opp_show_shop = true, g_opp_show_bench = true;
float g_opp_hex_size = 25.0f, g_opp_scale = 1.0f, g_hextech_scale = 1.0f;
int g_hero_warn_thres = 3; float g_hero_warn_scale = 1.0f;

int g_cached_view_width = 0, g_cached_view_height = 0;
int g_gl_width = 0, g_gl_height = 0;

bool g_auto_clicker_enable = false;
float g_click_interval_ms = 0.0f, g_touch_duration_ms = 0.0f;    
struct ClickPos { float x = 540.0f; float y = 960.0f; };
std::vector<ClickPos> g_click_positions = { {540.0f, 960.0f} };
std::atomic<bool> g_clicker_running{false};
std::atomic<int> g_total_clicks_executed{0};
float g_realtime_cps = 0.0f, g_cps_history[100] = {0.0f}; int g_cps_hist_idx = 0;

float g_quit_x = 80.0f, g_quit_y = 520.0f; int g_quit_confirm = 0; float g_quit_timer = 0.0f;
float g_lock_x = 80.0f, g_lock_y = 460.0f; bool g_floats_locked = false;
float g_cpbtn_x = 80.0f, g_cpbtn_y = 400.0f, g_clickerbtn_x = 80.0f, g_clickerbtn_y = 340.0f;
float g_float_cp_x = -1.0f, g_float_cp_y = -1.0f, g_float_pd_x = -1.0f, g_float_pd_y = -1.0f;
float g_float_opp_x = -1.0f, g_float_opp_y = -1.0f, g_float_hex_x = -1.0f, g_float_hex_y = -1.0f, g_float_hw_x = -1.0f, g_float_hw_y = -1.0f;
static bool g_apply_saved_float_pos = false;

std::atomic<bool> g_hero_images_ready{false}; int g_hero_image_count = 0;
struct TexDecodedData { int w, h; unsigned char* pixels; };
std::mutex g_TexMutex; std::unordered_map<int, GLuint> g_heroTextureCache;
std::vector<std::pair<int, TexDecodedData>> g_HeroTexDecodedQueue;
struct DecodeRequest { int id; }; std::deque<DecodeRequest> g_DecodeRequestQueue;
std::mutex g_DecodeRequestMutex; std::atomic<bool> g_tex_worker_started{false};

struct MainThreadTasks {
    std::atomic<bool> trigger_quit{false}; std::atomic<bool> trigger_game_end{false};
    std::mutex buy_mutex; std::vector<uintptr_t> buy_slots;
} g_Tasks;

std::vector<uintptr_t> g_shop_slots; void* old_shop_listen = nullptr; std::atomic<bool> g_shop_listen_done{false};

bool SafeReadMemory(uintptr_t addr, void* buffer, size_t size) {
    if (addr < 0x10000000 || addr > 0x00007FFFFFFFFFFF) return false;
    struct iovec local[1];
    struct iovec remote[1];
    local[0].iov_base = buffer;
    local[0].iov_len = size;
    remote[0].iov_base = (void*)addr;
    remote[0].iov_len = size;
    ssize_t bytesRead = syscall(__NR_process_vm_readv, getpid(), local, 1, remote, 1, 0);
    return bytesRead == (ssize_t)size;
}

#define SAFE_READ_PTR(addr, offset) SafeReadPtr(addr, offset)
#define SAFE_READ_INT(addr, offset) SafeReadInt(addr, offset)
#define SAFE_READ_BYTE(addr, offset) SafeReadByte(addr, offset)
#define IsValidPtr(ptr) ((ptr) > 0x10000000 && (ptr) < 0x00007FFFFFFFFFFF)

uintptr_t SafeReadPtr(uintptr_t addr, uint32_t offset) { uintptr_t val = 0; if (SafeReadMemory(addr + offset, &val, sizeof(val))) return val; return 0; }
int SafeReadInt(uintptr_t addr, uint32_t offset) { int val = 0; if (SafeReadMemory(addr + offset, &val, sizeof(val))) return val; return 0; }
uint8_t SafeReadByte(uintptr_t addr, uint32_t offset) { uint8_t val = 0; if (SafeReadMemory(addr + offset, &val, sizeof(val))) return val; return 0; }

std::string utf16_to_utf8(const std::wstring& wstr) {
    std::string res;
    for (wchar_t wc : wstr) {
        if (wc < 0x80) res += (char)wc;
        else if (wc < 0x800) { res += (char)(0xC0 | ((wc >> 6) & 0x1F)); res += (char)(0x80 | (wc & 0x3F)); }
        else { res += (char)(0xE0 | ((wc >> 12) & 0x0F)); res += (char)(0x80 | ((wc >> 6) & 0x3F)); res += (char)(0x80 | (wc & 0x3F)); }
    }
    return res;
}
std::string ReadIl2CppString(uintptr_t strAddr) {
    if (!IsValidPtr(strAddr)) return "";
    int len = SAFE_READ_INT(strAddr, 0x10);
    if (len <= 0 || len > 100) return "";
    std::wstring wstr;
    for (int i = 0; i < len; i++) { wstr += (wchar_t)(SAFE_READ_INT(strAddr, 0x14 + i * 2) & 0xFFFF); }
    return utf16_to_utf8(wstr);
}

std::vector<uintptr_t> GetStructArrayPointers(uintptr_t arrayAddr, int maxCount, int structSize, int ptrOffset) {
    std::vector<uintptr_t> res; if (!IsValidPtr(arrayAddr)) return res;
    int count = SAFE_READ_INT(arrayAddr, 0x18); if (count <= 0 || count > maxCount * 10) return res;
    int readLimit = (maxCount <= 0) ? count : std::min(count, maxCount);
    for (int i = 0; i < readLimit; i++) {
        uintptr_t ptr = SAFE_READ_PTR(arrayAddr, 0x20 + i * structSize + ptrOffset);
        if (IsValidPtr(ptr)) res.push_back(ptr);
    }
    return res;
}
std::vector<uintptr_t> GetPointersInArray(uintptr_t arrayAddr, int maxCount) {
    std::vector<uintptr_t> res; if (!IsValidPtr(arrayAddr)) return res;
    int count = SAFE_READ_INT(arrayAddr, 0x18); if (count <= 0 || count > maxCount * 10) return res; 
    int readLimit = std::min(count, maxCount);
    for (int i = 0; i < readLimit; i++) {
        uintptr_t ptr = SAFE_READ_PTR(arrayAddr, 0x20 + i * 8);
        if (IsValidPtr(ptr)) res.push_back(ptr);
    }
    return res;
}
std::vector<int> GetIntsInArray(uintptr_t arrayAddr, int maxCount) {
    std::vector<int> res; if (!IsValidPtr(arrayAddr)) return res;
    int count = SAFE_READ_INT(arrayAddr, 0x18); if (count <= 0 || count > maxCount * 10) return res; 
    int readLimit = std::min(count, maxCount);
    for (int i = 0; i < readLimit; i++) { res.push_back(SAFE_READ_INT(arrayAddr, 0x20 + i * 4)); }
    return res;
}

std::string GetConfigPath() {
    if (access("/sdcard/Download/", W_OK) == 0) return "/sdcard/Download/jkt_offsets.txt";
    return "/data/local/tmp/jkt_offsets.txt";
}

static void CaptureWindowPos(const char* name, float& x, float& y) {
    if (!g_isImGuiInit) return;
    ImGuiWindow* w = ImGui::FindWindowByName(name);
    if (w) { x = w->Pos.x; y = w->Pos.y; }
}

void SaveConfig() {
    CaptureWindowPos("##CardPoolFloat", g_float_cp_x, g_float_cp_y);
    CaptureWindowPos("##PlayerDataFloat", g_float_pd_x, g_float_pd_y);
    CaptureWindowPos("##OpponentFloat", g_float_opp_x, g_float_opp_y);
    CaptureWindowPos("##HextechFloat", g_float_hex_x, g_float_hex_y);
    std::ofstream out(GetConfigPath());
    if (out.is_open()) {
        out << "# [金铲铲助手配置]\n";
        #define WRITE_OFF_32(name) out << #name << "=0x" << std::hex << g_off.name << "\n"
        WRITE_OFF_32(func_get_Instance); WRITE_OFF_32(addr2); WRITE_OFF_32(addr3); WRITE_OFF_32(addra); WRITE_OFF_32(segmentcsogame);
        WRITE_OFF_32(func_quit); WRITE_OFF_32(my_player_id); WRITE_OFF_32(segment_my_player_id); WRITE_OFF_32(next_opponents_list);
        WRITE_OFF_32(func_reqbuyhero); WRITE_OFF_32(func_shop_listen); WRITE_OFF_32(func_buy_hero_new); WRITE_OFF_32(func_set_IsGameEnd);
        WRITE_OFF_32(addr4); WRITE_OFF_32(addr5); WRITE_OFF_32(addr6); WRITE_OFF_32(addr7);
        WRITE_OFF_32(addr7_struct_size); WRITE_OFF_32(addr7_ptr_offset);
        WRITE_OFF_32(addr9); WRITE_OFF_32(addr9_struct_size); WRITE_OFF_32(addr9_ptr_offset);
        WRITE_OFF_32(ph_heroId); WRITE_OFF_32(ph_remaining); WRITE_OFF_32(ph_total);
        WRITE_OFF_32(addr11); WRITE_OFF_32(addr12); WRITE_OFF_32(addr12_struct_size); WRITE_OFF_32(addr12_ptr_offset);
        WRITE_OFF_32(addr13); WRITE_OFF_32(pi_name); WRITE_OFF_32(pi_id); WRITE_OFF_32(pi_is_bot); WRITE_OFF_32(pi_money); 
        WRITE_OFF_32(pi_win_streak); WRITE_OFF_32(pi_lose_streak); WRITE_OFF_32(pi_level);
        WRITE_OFF_32(addr14); WRITE_OFF_32(addr15); WRITE_OFF_32(addr16); WRITE_OFF_32(shop_hero_id);
        WRITE_OFF_32(addr17); WRITE_OFF_32(addr18); WRITE_OFF_32(bench_hero_id);
        WRITE_OFF_32(addr19); WRITE_OFF_32(addr20); WRITE_OFF_32(board_hero_id); WRITE_OFF_32(board_x); WRITE_OFF_32(board_y);
        WRITE_OFF_32(addr21); WRITE_OFF_32(addr22); WRITE_OFF_32(addr23); WRITE_OFF_32(addr23_struct_size); WRITE_OFF_32(addr23_ptr_offset);
        WRITE_OFF_32(addr26); WRITE_OFF_32(pi_avatar_rank); WRITE_OFF_32(pi_avatar_player_id); WRITE_OFF_32(hexctrl); WRITE_OFF_32(func_get_hex);
        out << std::dec;
        out << "ui_theme=" << g_ui_theme << "\nwin_cardpool=" << (g_win_cardpool ? 1 : 0) << "\nwin_playerdata=" << (g_win_playerdata ? 1 : 0) << "\nwin_hextech=" << (g_win_hextech ? 1 : 0) << "\n";
        out << "alpha_cp=" << g_alpha_cp << "\nalpha_pd=" << g_alpha_pd << "\nalpha_opp=" << g_alpha_opp << "\nalpha_hex=" << g_alpha_hex << "\nfloats_locked=" << (g_floats_locked ? 1 : 0) << "\n";
        out << "cp_columns=" << g_cp_columns << "\ncp_rows=" << g_cp_rows << "\ncp_scale=" << g_cp_scale << "\n";
        for (int i = 1; i <= 5; i++) out << "cp_show_cost" << i << "=" << (g_cp_show_cost[i] ? 1 : 0) << "\n";
        out << "cp_warning_enable=" << (g_cp_warning_enable ? 1 : 0) << "\ncp_warning_thres=" << g_cp_warning_thres << "\n";
        out << "pd_line_spacing=" << g_pd_line_spacing << "\npd_vert_spacing=" << g_pd_vert_spacing << "\npd_arrow_spacing=" << g_pd_arrow_spacing << "\npd_font_size=" << g_pd_font_size << "\n";
        out << "pd_hero_summary_enable=" << (g_pd_hero_summary_enable ? 1 : 0) << "\n";
        for (int i = 1; i <= 5; i++) out << "pd_hero_count_min" << i << "=" << g_pd_hero_count_min[i] << "\n";
        out << "opp_scale=" << g_opp_scale << "\nopp_show_board=" << (g_opp_show_board ? 1 : 0) << "\nopp_show_shop=" << (g_opp_show_shop ? 1 : 0) << "\nopp_show_bench=" << (g_opp_show_bench ? 1 : 0) << "\nopp_hex_size=" << g_opp_hex_size << "\n";
        out << "hextech_scale=" << g_hextech_scale << "\nwin_hero_warn=" << (g_win_hero_warn ? 1 : 0) << "\nhero_warn_thres=" << g_hero_warn_thres << "\nhero_warn_scale=" << g_hero_warn_scale << "\nalpha_hero_warn=" << g_alpha_hero_warn << "\n";
        out << "float_hw_x=" << g_float_hw_x << "\nfloat_hw_y=" << g_float_hw_y << "\nmenu_x=" << g_menuX << "\nmenu_y=" << g_menuY << "\nmenu_w=" << g_menuW << "\nmenu_h=" << g_menuH << "\nmenu_scale=" << g_scale << "\n";
        out << "menu_collapsed=" << (g_menu_orb ? 1 : 0) << "\norb_x=" << g_orb_x << "\norb_y=" << g_orb_y << "\nquit_x=" << g_quit_x << "\nquit_y=" << g_quit_y << "\nlock_x=" << g_lock_x << "\nlock_y=" << g_lock_y << "\ncpbtn_x=" << g_cpbtn_x << "\ncpbtn_y=" << g_cpbtn_y << "\nclickerbtn_x=" << g_clickerbtn_x << "\nclickerbtn_y=" << g_clickerbtn_y << "\n";
        out << "float_cp_x=" << g_float_cp_x << "\nfloat_cp_y=" << g_float_cp_y << "\nfloat_pd_x=" << g_float_pd_x << "\nfloat_pd_y=" << g_float_pd_y << "\nfloat_opp_x=" << g_float_opp_x << "\nfloat_opp_y=" << g_float_opp_y << "\nfloat_hex_x=" << g_float_hex_x << "\nfloat_hex_y=" << g_float_hex_y << "\n";
        out << "auto_clicker_enable=" << (g_auto_clicker_enable ? 1 : 0) << "\nclick_interval_ms=" << g_click_interval_ms << "\ntouch_duration_ms=" << g_touch_duration_ms << "\nclick_positions=";
        for (size_t i = 0; i < g_click_positions.size(); i++) { if (i > 0) out << ";"; out << g_click_positions[i].x << "," << g_click_positions[i].y; }
        out << "\n"; out.close();
    }
}

void LoadConfig() {
    std::ifstream in(GetConfigPath());
    if (!in.is_open()) { SaveConfig(); return; }
    std::string line; bool has_full = false;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        if (line.find("pi_win_streak") != std::string::npos) has_full = true;
        auto delim = line.find('=');
        if (delim != std::string::npos) {
            std::string key = line.substr(0, delim); std::string valStr = line.substr(delim + 1);
            try {
                uint32_t val = std::stoul(valStr, nullptr, 16);
                #define PARSE_OFF_32(name) if (key == #name) g_off.name = val;
                PARSE_OFF_32(func_get_Instance) PARSE_OFF_32(addr2) PARSE_OFF_32(addr3) PARSE_OFF_32(addra) PARSE_OFF_32(segmentcsogame)
                PARSE_OFF_32(func_quit) PARSE_OFF_32(my_player_id) PARSE_OFF_32(segment_my_player_id) PARSE_OFF_32(next_opponents_list) PARSE_OFF_32(func_reqbuyhero)
                PARSE_OFF_32(func_shop_listen) PARSE_OFF_32(func_buy_hero_new) PARSE_OFF_32(func_set_IsGameEnd)
                PARSE_OFF_32(addr4) PARSE_OFF_32(addr5) PARSE_OFF_32(addr6) PARSE_OFF_32(addr7) PARSE_OFF_32(addr7_struct_size) PARSE_OFF_32(addr7_ptr_offset)
                PARSE_OFF_32(addr9) PARSE_OFF_32(addr9_struct_size) PARSE_OFF_32(addr9_ptr_offset)
                PARSE_OFF_32(ph_heroId) PARSE_OFF_32(ph_remaining) PARSE_OFF_32(ph_total)
                PARSE_OFF_32(addr11) PARSE_OFF_32(addr12) PARSE_OFF_32(addr12_struct_size) PARSE_OFF_32(addr12_ptr_offset)
                PARSE_OFF_32(addr13) PARSE_OFF_32(pi_name) PARSE_OFF_32(pi_id) PARSE_OFF_32(pi_is_bot) PARSE_OFF_32(pi_money) 
                PARSE_OFF_32(pi_win_streak) PARSE_OFF_32(pi_lose_streak) PARSE_OFF_32(pi_level)
                PARSE_OFF_32(addr14) PARSE_OFF_32(addr15) PARSE_OFF_32(addr16) PARSE_OFF_32(shop_hero_id)
                PARSE_OFF_32(addr17) PARSE_OFF_32(addr18) PARSE_OFF_32(bench_hero_id)
                PARSE_OFF_32(addr19) PARSE_OFF_32(addr20) PARSE_OFF_32(board_hero_id) PARSE_OFF_32(board_x) PARSE_OFF_32(board_y)
                PARSE_OFF_32(addr21) PARSE_OFF_32(addr22) PARSE_OFF_32(addr23) PARSE_OFF_32(addr23_struct_size) PARSE_OFF_32(addr23_ptr_offset)
                PARSE_OFF_32(addr26) PARSE_OFF_32(pi_avatar_rank) PARSE_OFF_32(pi_avatar_player_id) PARSE_OFF_32(hexctrl) PARSE_OFF_32(func_get_hex)
            } catch (...) {}
            try {
                if (key == "ui_theme") g_ui_theme = std::stoi(valStr);
                else if (key == "win_cardpool") g_win_cardpool = (std::stoi(valStr) != 0); else if (key == "win_playerdata") g_win_playerdata = (std::stoi(valStr) != 0); else if (key == "win_hextech") g_win_hextech = (std::stoi(valStr) != 0);
                else if (key == "alpha_cp") g_alpha_cp = std::clamp(std::stof(valStr), 0.1f, 1.0f); else if (key == "alpha_pd") g_alpha_pd = std::clamp(std::stof(valStr), 0.1f, 1.0f); else if (key == "alpha_opp") g_alpha_opp = std::clamp(std::stof(valStr), 0.1f, 1.0f); else if (key == "alpha_hex") g_alpha_hex = std::clamp(std::stof(valStr), 0.1f, 1.0f);
                else if (key == "floats_locked") g_floats_locked = (std::stoi(valStr) != 0);
                else if (key == "cp_columns") g_cp_columns = std::stoi(valStr); else if (key == "cp_rows") g_cp_rows = std::stoi(valStr); else if (key == "cp_scale") g_cp_scale = std::stof(valStr);
                else if (key.rfind("cp_show_cost", 0) == 0) { int i = key.back() - '0'; if (i >= 1 && i <= 5) g_cp_show_cost[i] = (std::stoi(valStr) != 0); }
                else if (key == "cp_warning_enable") g_cp_warning_enable = (std::stoi(valStr) != 0); else if (key == "cp_warning_thres") g_cp_warning_thres = std::stoi(valStr);
                else if (key == "pd_line_spacing") g_pd_line_spacing = std::stof(valStr); else if (key == "pd_vert_spacing") g_pd_vert_spacing = std::stof(valStr); else if (key == "pd_arrow_spacing") g_pd_arrow_spacing = std::stof(valStr); else if (key == "pd_font_size") g_pd_font_size = std::stof(valStr);
                else if (key == "pd_hero_summary_enable") g_pd_hero_summary_enable = (std::stoi(valStr) != 0);
                else if (key.rfind("pd_hero_count_min", 0) == 0) { int i = key.back() - '0'; if (i >= 1 && i <= 5) g_pd_hero_count_min[i] = std::stoi(valStr); }
                else if (key == "opp_scale") g_opp_scale = std::stof(valStr); else if (key == "opp_show_board") g_opp_show_board = (std::stoi(valStr) != 0); else if (key == "opp_show_shop") g_opp_show_shop = (std::stoi(valStr) != 0); else if (key == "opp_show_bench") g_opp_show_bench = (std::stoi(valStr) != 0); else if (key == "opp_hex_size") g_opp_hex_size = std::stof(valStr);
                else if (key == "hextech_scale") g_hextech_scale = std::stof(valStr); else if (key == "win_hero_warn") g_win_hero_warn = (std::stoi(valStr) != 0); else if (key == "hero_warn_thres") g_hero_warn_thres = std::stoi(valStr); else if (key == "hero_warn_scale") g_hero_warn_scale = std::stof(valStr); else if (key == "alpha_hero_warn") g_alpha_hero_warn = std::stof(valStr);
                else if (key == "float_hw_x") g_float_hw_x = std::stof(valStr); else if (key == "float_hw_y") g_float_hw_y = std::stof(valStr);
                else if (key == "menu_x") g_menuX = std::stof(valStr); else if (key == "menu_y") g_menuY = std::stof(valStr); else if (key == "menu_w") g_menuW = std::stof(valStr); else if (key == "menu_h") g_menuH = std::stof(valStr); else if (key == "menu_scale") g_scale = std::stof(valStr);
                else if (key == "menu_collapsed") g_menu_orb = (std::stoi(valStr) != 0); else if (key == "orb_x") g_orb_x = std::stof(valStr); else if (key == "orb_y") g_orb_y = std::stof(valStr);
                else if (key == "quit_x") g_quit_x = std::stof(valStr); else if (key == "quit_y") g_quit_y = std::stof(valStr); else if (key == "lock_x") g_lock_x = std::stof(valStr); else if (key == "lock_y") g_lock_y = std::stof(valStr);
                else if (key == "cpbtn_x") g_cpbtn_x = std::stof(valStr); else if (key == "cpbtn_y") g_cpbtn_y = std::stof(valStr); else if (key == "clickerbtn_x") g_clickerbtn_x = std::stof(valStr); else if (key == "clickerbtn_y") g_clickerbtn_y = std::stof(valStr);
                else if (key == "float_cp_x") g_float_cp_x = std::stof(valStr); else if (key == "float_cp_y") g_float_cp_y = std::stof(valStr); else if (key == "float_pd_x") g_float_pd_x = std::stof(valStr); else if (key == "float_pd_y") g_float_pd_y = std::stof(valStr); else if (key == "float_opp_x") g_float_opp_x = std::stof(valStr); else if (key == "float_opp_y") g_float_opp_y = std::stof(valStr); else if (key == "float_hex_x") g_float_hex_x = std::stof(valStr); else if (key == "float_hex_y") g_float_hex_y = std::stof(valStr);
                else if (key == "auto_clicker_enable") g_auto_clicker_enable = (std::stoi(valStr) != 0); else if (key == "click_interval_ms") g_click_interval_ms = std::stof(valStr); else if (key == "touch_duration_ms") g_touch_duration_ms = std::stof(valStr);
                else if (key == "click_positions") {
                    g_click_positions.clear(); std::stringstream ss(valStr); std::string pair;
                    while (std::getline(ss, pair, ';')) {
                        if (pair.empty()) continue; std::stringstream ss2(pair); std::string xs, ys;
                        if (std::getline(ss2, xs, ',') && std::getline(ss2, ys, ',')) { try { g_click_positions.push_back({std::stof(xs), std::stof(ys)}); } catch (...) {} }
                    }
                    if (g_click_positions.empty()) g_click_positions.push_back({540.0f, 960.0f});
                }
                else if (key == "auto_buy_ids") {
                    g_heroAutoBuyChecked.clear(); std::stringstream ss(valStr); std::string item;
                    while (std::getline(ss, item, ',')) { if (!item.empty()) { try { g_heroAutoBuyChecked[std::stoi(item)] = true; } catch (...) {} } }
                }
            } catch (...) {}
        }
    }
    in.close(); g_apply_saved_float_pos = true;
    if (!has_full) SaveConfig();
}

void ClearGameState() {
    g_poolHeroes.clear(); g_heroesByCost.clear(); g_players.clear(); g_next_opponents.clear();
    g_my_player_id = 0; g_hex_qualities[0] = g_hex_qualities[1] = g_hex_qualities[2] = 0;
    g_dbg_addr1 = 0; g_dbg_addr2 = 0; g_dbg_addr3 = 0; g_dbg_addra = 0; g_dbg_segmentcsogame = 0;
    g_shop_slots.clear(); g_shop_listen_done.store(false); g_match_enter_pending.store(false);
    g_segment_valid_streak = 0; g_need_segment_gap_before_enter = true;
    std::lock_guard<std::mutex> lock(g_Tasks.buy_mutex); g_Tasks.buy_slots.clear();
}

bool TryResolveSegmentCSOGame(uintptr_t* out_segment) {
    if (g_il2cppTrueBase == 0) return false;
    typedef void* (*func_get_Instance_t)(void* method_info);
    func_get_Instance_t get_Instance = (func_get_Instance_t)(g_il2cppTrueBase + (uintptr_t)g_off.func_get_Instance);
    if (!get_Instance) return false;
    uintptr_t addr1 = 0; try { addr1 = (uintptr_t)get_Instance(nullptr); } catch (...) { return false; }
    if (!IsValidPtr(addr1)) return false;
    uintptr_t addr2 = SAFE_READ_PTR(addr1, g_off.addr2); if (!IsValidPtr(addr2)) return false;
    uintptr_t addr3 = SAFE_READ_PTR(addr2, g_off.addr3); if (!IsValidPtr(addr3)) return false;
    uintptr_t addra = SAFE_READ_PTR(addr3, g_off.addra); if (!IsValidPtr(addra)) return false;
    uintptr_t segment = SAFE_READ_PTR(addra, g_off.segmentcsogame); if (!IsValidPtr(segment)) return false;
    if (out_segment) *out_segment = segment; return true;
}

void UpdateMatchState() {
    uintptr_t segment = 0; bool segmentValid = TryResolveSegmentCSOGame(&segment);
    bool inMatch = g_is_in_match.load(std::memory_order_acquire);
    if (inMatch) {
        if (!segmentValid) {
            g_is_in_match.store(false, std::memory_order_release); g_match_enter_pending.store(false, std::memory_order_release);
            g_segment_valid_streak = 0; g_need_segment_gap_before_enter = true; g_Tasks.trigger_game_end.store(true, std::memory_order_release);
            return;
        }
        if (g_dbg_segmentcsogame != 0 && g_dbg_segmentcsogame != segment) { g_dbg_segmentcsogame = segment; g_Tasks.trigger_game_end.store(true, std::memory_order_release); }
        return;
    }
    if (!segmentValid) { g_segment_valid_streak = 0; g_need_segment_gap_before_enter = false; return; }
    if (g_match_enter_pending.load(std::memory_order_acquire)) {
        g_dbg_segmentcsogame = segment; g_is_in_match.store(true, std::memory_order_release);
        g_match_enter_pending.store(false, std::memory_order_release); g_segment_valid_streak = 0; g_need_segment_gap_before_enter = false; return;
    }
    if (g_need_segment_gap_before_enter) { g_segment_valid_streak = 0; return; }
    if (++g_segment_valid_streak >= 3) {
        g_dbg_segmentcsogame = segment; g_is_in_match.store(true, std::memory_order_release); g_segment_valid_streak = 0;
    }
}

static void UpsertPoolHero(int heroId, int remaining, int total, uintptr_t addr10) {
    if (heroId <= 0 || heroId >= 100000) return; int cost = (heroId / 1000) % 10;
    for (auto& ph : g_poolHeroes) { if (ph.heroId == heroId) { ph.remaining = remaining; ph.total = total; ph.addr10 = addr10; return; } }
    g_poolHeroes.push_back({ heroId, remaining, total, cost, addr10 });
    auto& list = g_heroesByCost[cost]; if (std::find(list.begin(), list.end(), heroId) == list.end()) list.push_back(heroId);
}

void ParseGameMemory() {
    if (g_il2cppTrueBase == 0 || !g_is_in_match.load(std::memory_order_acquire)) return;
    typedef void* (*func_get_Instance_t)(void* method_info);
    func_get_Instance_t get_Instance = (func_get_Instance_t)(g_il2cppTrueBase + (uintptr_t)g_off.func_get_Instance);
    if (!get_Instance) return;
    
    try { g_dbg_addr1 = (uintptr_t)get_Instance(nullptr); } catch(...) { g_dbg_addr1 = 0; }
    g_dbg_addr2 = SAFE_READ_PTR(g_dbg_addr1, g_off.addr2); g_dbg_addr3 = SAFE_READ_PTR(g_dbg_addr2, g_off.addr3); 
    g_dbg_addra = SAFE_READ_PTR(g_dbg_addr3, g_off.addra); g_dbg_segmentcsogame = SAFE_READ_PTR(g_dbg_addra, g_off.segmentcsogame); 
    g_my_player_id = IsValidPtr(g_dbg_segmentcsogame) ? SAFE_READ_INT(g_dbg_segmentcsogame, g_off.segment_my_player_id) : 0;
    
    uintptr_t next_opp_addr = SAFE_READ_PTR(g_dbg_addr2, g_off.next_opponents_list);
    g_next_opponents = GetIntsInArray(SAFE_READ_PTR(next_opp_addr, 0x10), SAFE_READ_INT(next_opp_addr, 0x18));

    g_dbg_addr4 = SAFE_READ_PTR(g_dbg_segmentcsogame, g_off.addr4); g_dbg_addr5 = SAFE_READ_PTR(g_dbg_addr4, g_off.addr5);
    g_dbg_addr6 = SAFE_READ_PTR(g_dbg_addr5, g_off.addr6); g_dbg_addr7 = SAFE_READ_PTR(g_dbg_addr6, g_off.addr7);
    
    auto list7 = GetStructArrayPointers(g_dbg_addr7, 2000, g_off.addr7_struct_size, g_off.addr7_ptr_offset);
    for (auto addr8 : list7) {
        auto list9 = GetStructArrayPointers(SAFE_READ_PTR(addr8, g_off.addr9), 2000, g_off.addr9_struct_size, g_off.addr9_ptr_offset);
        for (auto addr10 : list9) if (IsValidPtr(addr10)) UpsertPoolHero(SAFE_READ_INT(addr10, g_off.ph_heroId), SAFE_READ_INT(addr10, g_off.ph_remaining), SAFE_READ_INT(addr10, g_off.ph_total), addr10);
    }

    g_dbg_addr11 = SAFE_READ_PTR(g_dbg_addr2, g_off.addr11); g_dbg_addr12 = SAFE_READ_PTR(g_dbg_addr11, g_off.addr12);
    g_players.clear(); std::vector<uintptr_t> player_vals;
    if (IsValidPtr(g_dbg_addr12)) {
        int cap = SAFE_READ_INT(g_dbg_addr12, 0x18);
        if (cap > 0 && cap < 200) {
            for (int off = 0x20; off < 0x20 + cap * 0x20; off += 8) {
                uintptr_t p = SAFE_READ_PTR(g_dbg_addr12, off);
                if (IsValidPtr(p) && IsValidPtr(SAFE_READ_PTR(p, g_off.addr13))) player_vals.push_back(p);
            }
        }
    }

    for (auto val : player_vals) {
        PlayerInfo pi; pi.val_ptr = val; pi.addr13_ptr = SAFE_READ_PTR(val, g_off.addr13);
        pi.name = ReadIl2CppString(SAFE_READ_PTR(pi.addr13_ptr, g_off.pi_name));
        pi.id = SAFE_READ_INT(pi.addr13_ptr, g_off.pi_id); pi.is_bot = SAFE_READ_BYTE(val, g_off.pi_is_bot) != 0;
        pi.money = SAFE_READ_INT(val, g_off.pi_money); pi.win_streak = SAFE_READ_INT(val, g_off.pi_win_streak);
        pi.lose_streak = SAFE_READ_INT(val, g_off.pi_lose_streak); pi.level = SAFE_READ_INT(val, g_off.pi_level);
        
        auto shopItems = GetPointersInArray(SAFE_READ_PTR(SAFE_READ_PTR(val, g_off.addr14), g_off.addr15), 5);
        for (size_t i = 0; i < shopItems.size(); i++) {
            int hid = SAFE_READ_INT(SAFE_READ_PTR(shopItems[i], g_off.addr16), g_off.shop_hero_id); pi.shop.push_back(hid);
            if (hid > 0 && pi.id == g_my_player_id && g_heroAutoBuyChecked[hid]) {
                uintptr_t s_addr = (g_shop_listen_done.load() && i < g_shop_slots.size()) ? g_shop_slots[i] : shopItems[i];
                if (IsValidPtr(s_addr)) {
                    static std::map<uintptr_t, int> last_buy;
                    if (g_current_frame - last_buy[s_addr] > 10) { last_buy[s_addr] = g_current_frame; std::lock_guard<std::mutex> lock(g_Tasks.buy_mutex); g_Tasks.buy_slots.push_back(s_addr); }
                }
            }
        }
        for (auto b : GetPointersInArray(SAFE_READ_PTR(SAFE_READ_PTR(val, g_off.addr17), g_off.addr18), 10)) pi.bench.push_back(SAFE_READ_INT(b, g_off.bench_hero_id));
        for (auto bd : GetPointersInArray(SAFE_READ_PTR(SAFE_READ_PTR(val, g_off.addr19), g_off.addr20), 30)) { pi.board.push_back({SAFE_READ_INT(bd, g_off.board_hero_id), SAFE_READ_INT(bd, g_off.board_x), SAFE_READ_INT(bd, g_off.board_y)}); }
        g_players.push_back(pi);
    }
}

std::string FindChineseFontPath() {
    const char* known_paths[] = {
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/NotoSansTC-Regular.otf",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/SysSans-Hans-Regular.ttf",
        "/system/fonts/Miui-Regular.ttf",
        "/system/fonts/SourceHanSansCN-Regular.otf",
        "/system/fonts/FZLanTingHei-R-GBK.ttf",
        "/system/fonts/HarmonyOS_Sans_SC.ttf",
        "/system/fonts/OplusSC-Regular.ttf",
        "/system/fonts/VivoSansSC-Regular.ttf"
    };
    for (const char* path : known_paths) {
        if (access(path, R_OK) == 0) return path;
    }
    DIR* dir = opendir("/system/fonts");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name.find(".ttf") != std::string::npos || name.find(".otf") != std::string::npos || name.find(".ttc") != std::string::npos) {
                if (name.find("SC") != std::string::npos || name.find("CJK") != std::string::npos ||
                    name.find("Hans") != std::string::npos || name.find("Fallback") != std::string::npos ||
                    name.find("Chinese") != std::string::npos) {
                    std::string full_path = "/system/fonts/" + name;
                    if (access(full_path.c_str(), R_OK) == 0) {
                        closedir(dir);
                        return full_path;
                    }
                }
            }
        }
        closedir(dir);
    }
    return "";
}

void UpdateFontHD(bool force) {
    ImGuiIO& io = ImGui::GetIO();
    float screenH = (io.DisplaySize.y > 100.0f) ? io.DisplaySize.y : 2400.0f; 
    g_autoScale = screenH / 1080.0f;
    float targetSize = std::clamp(20.0f * g_autoScale, 16.0f, 45.0f); 
    if (!force && std::abs(targetSize - g_current_rendered_size) < 2.0f) return;
    
    ImGui_ImplOpenGL3_DestroyDeviceObjects(); io.Fonts->Clear(); g_mainFont = nullptr; 
    ImFontConfig configMain; configMain.OversampleH = 2; configMain.OversampleV = 2; configMain.PixelSnapH = false; 
    
    std::string fontPath = FindChineseFontPath();
    if (!fontPath.empty()) {
        g_mainFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), targetSize * 1.5f, &configMain, io.Fonts->GetGlyphRangesChineseSimplifiedCommon()); 
        if (g_mainFont) { 
            g_mainFont->Scale = 1.0f / 1.5f; 
            LOGI("[+] Successfully loaded Chinese font: %s", fontPath.c_str());
        }
    }
    
    if (!g_mainFont) { 
        LOGI("[-] System Chinese font not found, falling back to default font");
        g_mainFont = io.Fonts->AddFontDefault(); 
        if (g_mainFont) g_mainFont->Scale = 1.0f / 1.5f; 
    }
    
    io.Fonts->Build(); 
    ImGui_ImplOpenGL3_CreateDeviceObjects(); 
    g_current_rendered_size = targetSize;
}

inline int GetBaseHeroImageId(int rawHeroId) {
    if (rawHeroId < 10) return rawHeroId;
    if (rawHeroId >= 10000) return rawHeroId - (rawHeroId / 10000) * 10000 + 10000;
    if (rawHeroId >= 1000) return rawHeroId - (rawHeroId / 1000) * 1000 + 1000;
    if (rawHeroId >= 100) return rawHeroId - (rawHeroId / 100) * 100 + 100;
    return rawHeroId - (rawHeroId / 10) * 10 + 10;
}
inline int GetHeroStarLevel(int rawHeroId) {
    if (rawHeroId <= 0) return 0;
    int star = rawHeroId; while (star >= 10) star /= 10; return std::clamp(star, 1, 3);
}
void BuildHeroImageIndex() {
    std::thread([]() {
        int found = 0;
        for (int i = 1; i <= 99999; i++) { int len = 0; if (GetHeroImageBytes(i, &len) != nullptr && len > 0) found++; }
        g_hero_image_count = found; g_hero_images_ready.store(true);
    }).detach();
}
void TextureDecodingWorkerThread() {
    while (true) {
        DecodeRequest req; bool hasReq = false;
        { std::lock_guard<std::mutex> lock(g_DecodeRequestMutex); if (!g_DecodeRequestQueue.empty()) { req = g_DecodeRequestQueue.front(); g_DecodeRequestQueue.pop_front(); hasReq = true; } }
        if (hasReq) {
            int imgLen = 0; const unsigned char* imgData = GetHeroImageBytes(req.id, &imgLen);
            if (imgData != nullptr && imgLen > 0) {
                int w, h, channels; unsigned char* data = stbi_load_from_memory(imgData, imgLen, &w, &h, &channels, 4);
                if (data && w > 0 && h > 0) { std::lock_guard<std::mutex> lock(g_TexMutex); g_HeroTexDecodedQueue.push_back({req.id, {w, h, data}}); } else if (data) stbi_image_free(data);
            }
        } else std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
void EnsureTextureWorkerStarted() {
    if (g_tex_worker_started.exchange(true)) return;
    std::thread(TextureDecodingWorkerThread).detach(); BuildHeroImageIndex();
}
GLuint GetHeroTexture(int heroId) {
    int baseId = GetBaseHeroImageId(heroId);
    auto it = g_heroTextureCache.find(baseId); if (it != g_heroTextureCache.end()) return it->second;
    g_heroTextureCache[baseId] = 0; EnsureTextureWorkerStarted();
    std::lock_guard<std::mutex> lock(g_DecodeRequestMutex); g_DecodeRequestQueue.push_back({baseId});
    return 0;
}
void ProcessTextureQueue() {
    std::lock_guard<std::mutex> lock(g_TexMutex); if (g_HeroTexDecodedQueue.empty()) return;
    GLint last_unpack; glGetIntegerv(GL_UNPACK_ALIGNMENT, &last_unpack); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (auto& item : g_HeroTexDecodedQueue) {
        GLuint tex = 0; glGenTextures(1, &tex);
        if (tex != 0) {
            glBindTexture(GL_TEXTURE_2D, tex); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, item.second.w, item.second.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, item.second.pixels);
        }
        stbi_image_free(item.second.pixels); g_heroTextureCache[item.first] = tex;
    }
    g_HeroTexDecodedQueue.clear(); glPixelStorei(GL_UNPACK_ALIGNMENT, last_unpack);
}
static void DrawHeroIcon(ImDrawList* dl, int heroId, ImVec2 pMin, ImVec2 pMax, float rounding, ImU32 fallbackColor) {
    int baseId = GetBaseHeroImageId(heroId); GLuint tex = GetHeroTexture(baseId);
    if (tex != 0) { dl->AddImageRounded((ImTextureID)(intptr_t)tex, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(IM_COL32(255, 255, 255, 255)), rounding); return; }
    char idBuf[16]; snprintf(idBuf, sizeof(idBuf), "%d", baseId); ImVec2 tSz = ImGui::CalcTextSize(idBuf);
    dl->AddText(ImVec2(pMin.x + (pMax.x - pMin.x - tSz.x) * 0.5f, pMin.y + (pMax.y - pMin.y - tSz.y) * 0.5f), ImGui::GetColorU32(fallbackColor), idBuf);
}
static void DrawStarGlyph(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 tip[5];
    for (int i = 0; i < 5; i++) { float a = - (float)M_PI * 0.5f + i * (2.0f * (float)M_PI / 5.0f); tip[i] = ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r); }
    for (int i = 0; i < 5; i++) { dl->AddTriangleFilled(c, tip[i], tip[(i + 2) % 5], col); }
    dl->AddCircleFilled(c, r * 0.28f, col, 12);
}
static void DrawHeroStars(ImDrawList* dl, ImVec2 center, int stars, float star_r) {
    if (stars <= 0 || !dl) return; stars = std::clamp(stars, 1, 3); float gap = star_r * 2.35f; float x0 = center.x - (stars - 1) * gap * 0.5f;
    ImU32 glow = ImGui::GetColorU32(IM_COL32(255, 200, 0, 90)), outline = ImGui::GetColorU32(IM_COL32(20, 12, 0, 240)), fill = ImGui::GetColorU32(IM_COL32(255, 230, 50, 255));
    for (int i = 0; i < stars; i++) { ImVec2 c(x0 + i * gap, center.y); DrawStarGlyph(dl, c, star_r + 3.0f, glow); DrawStarGlyph(dl, c, star_r + 1.8f, outline); DrawStarGlyph(dl, c, star_r, fill); }
}

struct FrostTheme { ImVec4 primary, primaryHover, accentGlow, orb1, orb2; const char* name; };
static FrostTheme g_themes[4] = {
    { ImVec4(0.39f, 0.40f, 0.95f, 1.f), ImVec4(0.31f, 0.27f, 0.90f, 1.f), ImVec4(0.39f, 0.40f, 0.95f, 0.55f), ImVec4(0.39f, 0.40f, 0.95f, 0.22f), ImVec4(0.96f, 0.25f, 0.37f, 0.18f), (const char*)u8"冰晶" },
    { ImVec4(0.06f, 0.73f, 0.51f, 1.f), ImVec4(0.02f, 0.59f, 0.41f, 1.f), ImVec4(0.06f, 0.73f, 0.51f, 0.55f), ImVec4(0.06f, 0.73f, 0.51f, 0.22f), ImVec4(0.22f, 0.74f, 0.97f, 0.18f), (const char*)u8"翡翠" },
    { ImVec4(0.66f, 0.33f, 0.97f, 1.f), ImVec4(0.58f, 0.20f, 0.92f, 1.f), ImVec4(0.66f, 0.33f, 0.97f, 0.55f), ImVec4(0.66f, 0.33f, 0.97f, 0.22f), ImVec4(0.96f, 0.25f, 0.37f, 0.18f), (const char*)u8"幻紫" },
    { ImVec4(0.22f, 0.74f, 0.97f, 1.f), ImVec4(0.01f, 0.52f, 0.78f, 1.f), ImVec4(0.22f, 0.74f, 0.97f, 0.55f), ImVec4(0.22f, 0.74f, 0.97f, 0.22f), ImVec4(0.39f, 0.40f, 0.95f, 0.18f), (const char*)u8"霜天" },
};
static FrostTheme& UITheme() { return g_themes[g_ui_theme]; }
void ApplyFrostedTheme() {
    ImGuiStyle& s = ImGui::GetStyle(); FrostTheme& t = UITheme(); float sc = g_autoScale;
    s.WindowRounding = 20.0f * sc; s.ChildRounding = 14.0f * sc; s.FrameRounding = 12.0f * sc; s.PopupRounding = 14.0f * sc;
    s.WindowBorderSize = 1.0f; s.Colors[ImGuiCol_Text] = ImVec4(0.96f, 0.97f, 0.99f, 1.0f);
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.03f, 0.04f, 0.08f, 0.88f); s.Colors[ImGuiCol_Border] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.28f);
    s.Colors[ImGuiCol_Button] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.42f); s.Colors[ImGuiCol_ButtonHovered] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.62f); s.Colors[ImGuiCol_ButtonActive] = ImVec4(t.primaryHover.x, t.primaryHover.y, t.primaryHover.z, 0.82f);
}
static ImVec4 CostColor(int cost) {
    if (cost == 1) return ImVec4(0.82f, 0.82f, 0.85f, 1.0f); if (cost == 2) return ImVec4(0.35f, 0.95f, 0.50f, 1.0f);
    if (cost == 3) return ImVec4(0.40f, 0.65f, 1.0f, 1.0f); if (cost == 4) return ImVec4(0.90f, 0.40f, 0.95f, 1.0f); return ImVec4(1.0f, 0.85f, 0.25f, 1.0f);
}

static bool BeginContentFloatWindow(const char* id, bool* open, float* pos_x, float* pos_y, float alpha) {
    if (open && !*open) return false;
    if (pos_x && pos_y && *pos_x >= 0.0f && *pos_y >= 0.0f) { ImGui::SetNextWindowPos(ImVec2(*pos_x, *pos_y), g_apply_saved_float_pos ? ImGuiCond_Always : ImGuiCond_FirstUseEver); }
    ImGui::SetNextWindowBgAlpha(0.0f); ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0)); ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * g_autoScale, 10.0f * g_autoScale)); ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(alpha, 0.1f, 1.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration;
    if (g_floats_locked) flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMouseInputs;
    bool vis = ImGui::Begin(id, open, flags);
    if (!vis) { ImGui::End(); ImGui::PopStyleVar(3); ImGui::PopStyleColor(); return false; }
    return true;
}
static void EndContentFloatWindow(const char* grip_id, float* scale, float min_s = 0.5f, float max_s = 2.5f) {
    ImGui::PopStyleVar(3); ImGui::PopStyleColor(); ImGui::End();
}

void DrawMenuOrb() {
    ImGuiIO& io = ImGui::GetIO(); float r = g_orb_r * g_autoScale; ImVec2 center(g_orb_x, g_orb_y); ImDrawList* fg = ImGui::GetForegroundDrawList(); FrostTheme& th = UITheme();
    fg->AddCircleFilled(center, r + 3.0f, ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.25f)), 48); fg->AddCircleFilled(center, r, ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.88f)), 48);
    ImGui::SetNextWindowPos(ImVec2(center.x - r, center.y - r), ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(r * 2.0f, r * 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::Begin("##JKMenuOrb", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
    static bool s_orb_dragged = false; if (ImGui::InvisibleButton("##orb_btn", ImVec2(r * 2.0f, r * 2.0f))) { if (!s_orb_dragged) g_menu_orb = false; s_orb_dragged = false; }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) { s_orb_dragged = true; g_orb_x += io.MouseDelta.x; g_orb_y += io.MouseDelta.y; }
    ImGui::End(); ImGui::PopStyleVar();
}

void DrawMainMenu() {
    ApplyFrostedTheme();
    if (g_menu_orb) { DrawMenuOrb(); return; }
    static bool firstMenuOpen = true;
    if (firstMenuOpen) { ImGui::SetNextWindowPos(ImVec2(g_menuX, g_menuY), ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(g_menuW, g_menuH), ImGuiCond_Always); ImGui::SetNextWindowCollapsed(g_menuCollapsed, ImGuiCond_Always); firstMenuOpen = false; }
    bool menu_visible = true;
    if (ImGui::Begin((const char*)u8"金铲铲助手 Frosted Studio 完美版", &menu_visible, ImGuiWindowFlags_NoSavedSettings)) {
        g_menuX = ImGui::GetWindowPos().x; g_menuY = ImGui::GetWindowPos().y; g_menuCollapsed = ImGui::IsWindowCollapsed();
        if (!menu_visible || g_menuCollapsed) { g_orb_x = g_menuX + 28.0f * g_autoScale; g_orb_y = g_menuY + 28.0f * g_autoScale; g_menu_orb = true; }
        if (!g_menuCollapsed) {
            float curW = ImGui::GetWindowSize().x, curH = ImGui::GetWindowSize().y;
            if (std::abs(curW - g_menuW) > 5.0f || std::abs(curH - g_menuH) > 5.0f) { g_menuW = curW; g_menuH = curH; g_scale = std::clamp(curW / (560.0f * g_autoScale), 0.5f, 2.5f); }
            ImGui::SetWindowFontScale(g_scale);
            
            if(ImGui::Button((const char*)u8"保存配置")) SaveConfig();
            ImGui::SameLine();
            if(ImGui::Button((const char*)u8"收起为悬浮球")) g_menu_orb = true;
            ImGui::Separator();
            
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "当前游戏状态: %s", g_is_in_match.load() ? "对局中" : "未在对局");
            ImGui::Text("Player ID: %d | Frame: %d", g_my_player_id, g_current_frame);

            ImGui::Checkbox((const char*)u8"显示牌库", &g_win_cardpool); ImGui::SameLine();
            ImGui::Checkbox((const char*)u8"显示对局信息", &g_win_playerdata); ImGui::SameLine();
            ImGui::Checkbox((const char*)u8"显示敌方棋盘", &g_opp_show_board);
            
            ImGui::Checkbox((const char*)u8"开启连点器", &g_auto_clicker_enable);
            if (g_auto_clicker_enable) { g_clicker_running.store(true); } else { g_clicker_running.store(false); }
            
            ImGui::Separator();
            ImGui::Text((const char*)u8"自动拿牌设置:");
            for (int cost = 1; cost <= 5; cost++) {
                ImGui::TextColored(CostColor(cost), "%d费: ", cost); ImGui::SameLine();
                for (auto& ph : g_poolHeroes) {
                    if (ph.cost == cost) {
                        char buf[32]; snprintf(buf, sizeof(buf), "%d##%d", ph.heroId, ph.heroId);
                        bool is_checked = g_heroAutoBuyChecked[ph.heroId];
                        if (ImGui::Checkbox(buf, &is_checked)) g_heroAutoBuyChecked[ph.heroId] = is_checked;
                        ImGui::SameLine();
                    }
                }
                ImGui::NewLine();
            }
        }
    }
    ImGui::End();
}

void DrawCardPoolWindow() {
    if (!g_win_cardpool || !g_is_in_match.load(std::memory_order_relaxed)) return;
    if (!BeginContentFloatWindow("##CardPoolFloat", &g_win_cardpool, &g_float_cp_x, &g_float_cp_y, g_alpha_cp)) return;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f)); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    float sc = g_autoScale * g_cp_scale; float box_size = g_cp_box_size * sc; int cols = std::max(1, g_cp_columns);
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImFont* font = ImGui::GetFont(); float font_size = ImGui::GetFontSize() * sc;
    ImVec2 base = ImGui::GetCursorScreenPos(); float y_off = 0.0f; float max_w = 0.0f;
    for (int cost = 1; cost <= 5; cost++) {
        if (!g_cp_show_cost[cost]) continue;
        std::vector<PoolHero> filtered; for (auto& ph : g_poolHeroes) if (ph.cost == cost) filtered.push_back(ph);
        if (filtered.empty()) continue;
        int rows = (filtered.size() + cols - 1) / cols; float grid_w = cols * box_size; float grid_h = rows * box_size;
        max_w = std::max(max_w, grid_w); ImVec2 origin(base.x, base.y + y_off);
        for (size_t i = 0; i < filtered.size(); i++) {
            auto& ph = filtered[i]; int r = (int)(i / cols), c = (int)(i % cols);
            ImVec2 pMin(origin.x + c * box_size, origin.y + r * box_size); ImVec2 pMax(pMin.x + box_size, pMin.y + box_size);
            dl->AddRectFilled(pMin, pMax, ImGui::GetColorU32(IM_COL32(255, 255, 255, 14)), 6.0f); dl->AddRect(pMin, pMax, ImGui::GetColorU32(CostColor(cost)), 6.0f, 0, 2.0f);
            DrawHeroIcon(dl, ph.heroId, pMin, pMax, 6.0f * sc, IM_COL32(255, 255, 255, 240));
            char cnt[16]; snprintf(cnt, 16, "%d/%d", ph.remaining, ph.total); ImVec2 t_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, cnt);
            dl->AddText(font, font_size, ImVec2(pMin.x + (box_size - t_sz.x) * 0.5f, pMin.y + box_size - t_sz.y - 4.0f * sc), ImGui::GetColorU32(CostColor(cost)), cnt);
        }
        y_off += grid_h;
    }
    if (max_w > 0.0f) ImGui::Dummy(ImVec2(max_w, y_off)); ImGui::PopStyleVar(2);
    EndContentFloatWindow("cp_grip", &g_cp_scale);
}

void DrawPlayerDataWindow() {
    if (!g_win_playerdata || !g_is_in_match.load(std::memory_order_relaxed)) return;
    if (!BeginContentFloatWindow("##PlayerDataFloat", &g_win_playerdata, &g_float_pd_x, &g_float_pd_y, g_alpha_pd)) return;
    ImGui::SetWindowFontScale(g_autoScale * g_pd_font_size);
    for (auto& pi : g_players) {
        ImVec4 streak_col(0.75f, 0.78f, 0.82f, 1.f); char streak_buf[16];
        if (pi.win_streak > 0) { streak_col = ImVec4(1.f, 0.45f, 0.45f, 1.f); snprintf(streak_buf, 16, "+%d", pi.win_streak); }
        else if (pi.lose_streak > 0) { streak_col = ImVec4(0.45f, 0.78f, 1.f, 1.f); snprintf(streak_buf, 16, "-%d", pi.lose_streak); } else snprintf(streak_buf, 16, "0");
        std::string dname = pi.name.empty() ? "Player " + std::to_string(pi.id) : pi.name; if (pi.is_bot && pi.id != g_my_player_id) dname += " [机]";
        ImGui::TextColored(ImVec4(1.f, 0.82f, 0.28f, 1.f), "Lv.%d", pi.level); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.f, 0.95f, 0.35f, 1.f), "$%d", pi.money); ImGui::SameLine();
        ImGui::TextColored(streak_col, "%s", streak_buf); ImGui::SameLine(); ImGui::TextColored((pi.id == g_my_player_id) ? ImVec4(0.25f, 1.f, 0.45f, 1.f) : ImVec4(0.35f, 0.95f, 1.f, 1.f), "%s", dname.c_str());
    }
    EndContentFloatWindow("pd_grip", &g_pd_font_size);
}

void DrawOpponentBoardWindow() {
    if (!g_is_in_match.load(std::memory_order_relaxed)) return;
    if (!BeginContentFloatWindow("##OpponentFloat", nullptr, &g_float_opp_x, &g_float_opp_y, g_alpha_opp)) return;
    float sc = g_autoScale * g_opp_scale; ImGui::SetWindowFontScale(sc);
    int opp_id = -1; for (size_t i = 0; i + 1 < g_next_opponents.size(); i += 2) { if (g_next_opponents[i] == g_my_player_id) { opp_id = g_next_opponents[i + 1]; break; } else if (g_next_opponents[i + 1] == g_my_player_id) { opp_id = g_next_opponents[i]; break; } }
    PlayerInfo* opp = nullptr; for (auto& p : g_players) { if (p.id == opp_id) { opp = &p; break; } }
    if (!opp) ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), (const char*)u8"未匹配对手");
    else {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), (const char*)u8"对战: %s", opp->name.empty() ? std::to_string(opp->id).c_str() : opp->name.c_str());
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (g_opp_show_board) {
            float R = g_opp_hex_size * sc; float W = sqrtf(3.0f) * R; float Y_SPACING = 1.5f * R; float row_shift_base = 1.5f * W; float left_pad = row_shift_base + R; float content_w = 7.0f * W + W * 0.5f;
            ImVec2 cur = ImGui::GetCursorScreenPos(); ImGui::Dummy(ImVec2(content_w + left_pad + R, R + 3.0f * Y_SPACING + R)); float start_x = cur.x + left_pad + content_w - W * 0.5f;
            for (int gy = 0; gy < 4; gy++) {
                for (int gx = 0; gx < 7; gx++) {
                    float cx = start_x - gx * W - row_shift_base + ((gy % 2 == 1) ? W * 0.5f : 0.0f); float cy = cur.y + R + gy * Y_SPACING; ImVec2 pts[6];
                    for (int i = 0; i < 6; i++) { float a = (float)(M_PI / 180.0 * (60.0 * i - 30.0)); pts[i] = ImVec2(cx + R * cosf(a), cy + R * sinf(a)); }
                    dl->AddPolyline(pts, 6, ImGui::GetColorU32(ImVec4(0.45f, 0.82f, 1.f, 1.f)), ImDrawFlags_Closed, 3.0f * sc);
                    for (auto& bh : opp->board) {
                        if (bh.x == gx && bh.y == gy) {
                            dl->AddConvexPolyFilled(pts, 6, ImGui::GetColorU32(ImVec4(0.2f, 0.6f, 0.8f, 0.45f))); float pad = R * 0.18f;
                            DrawHeroIcon(dl, bh.heroId, ImVec2(cx - R + pad, cy - R + pad), ImVec2(cx + R - pad, cy + R - pad), R * 0.35f, IM_COL32(255, 255, 255, 240));
                            DrawHeroStars(dl, ImVec2(cx, cy + R * 0.62f), GetHeroStarLevel(bh.heroId), R * 0.28f); break;
                        }
                    }
                }
            }
        }
    }
    EndContentFloatWindow("opp_grip", &g_opp_scale);
}

unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
unsigned int (*old_eglSwapWithDamage)(EGLDisplay, EGLSurface, EGLint*, EGLint) = nullptr;
void* (*old_eglGetProcAddress)(const char*) = nullptr;
int (*old_vkCreateInstance)(void*, void*, void*) = nullptr;

void RenderImGui_Core(EGLDisplay display, EGLSurface surface) {
    g_current_frame++;
    if (!g_engine_rendering.load()) g_engine_rendering.store(true);

    eglQuerySurface(display, surface, EGL_WIDTH, &g_gl_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_gl_height);
    if (g_gl_width <= 0 || g_gl_height <= 0) { g_gl_width = 1080; g_gl_height = 2400; }

    // 备份 Unity 原始 OpenGL 状态
    GLint last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
    GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    GLint last_fbo; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_fbo);

    if (!g_isImGuiInit) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr;
        const char* gl_ver = (const char*)glGetString(GL_VERSION);
        const char* glsl_ver = "#version 300 es"; if (gl_ver && strstr(gl_ver, "OpenGL ES 2.")) glsl_ver = "#version 100";
        ImGui_ImplOpenGL3_Init(glsl_ver);
        io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
        UpdateFontHD(true); 
        g_isImGuiInit = true;
        LoadConfig(); 
    }
    if (g_needUpdateFontSafe) { UpdateFontHD(true); g_needUpdateFontSafe = false; }
    
    ImGuiIO& io = ImGui::GetIO(); 
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    io.DeltaTime = 1.0f / 60.0f;

    UpdateMatchState();
    if (g_Tasks.trigger_game_end.exchange(false, std::memory_order_acquire)) ClearGameState();
    if (g_is_in_match.load(std::memory_order_acquire) && (g_current_frame % 2 == 0)) ParseGameMemory();

    ProcessTextureQueue();
    ImGui_ImplOpenGL3_NewFrame(); ImGui::NewFrame();
    
    DrawMainMenu(); 
    DrawCardPoolWindow(); 
    DrawPlayerDataWindow(); 
    DrawOpponentBoardWindow();

    ImGui::Render();
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_gl_width, g_gl_height);

    // 强力重置与洗版（保证中途注入菜单必定在最上层显示）
    glDisable(GL_DEPTH_TEST);   
    glDisable(GL_CULL_FACE);    
    glDisable(GL_SCISSOR_TEST); 
    glEnable(GL_BLEND);         
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // 恢复 Unity 原始状态
    glUseProgram(last_program); glBindTexture(GL_TEXTURE_2D, last_texture); glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array); glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo); glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);
    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

unsigned int hook_eglSwap(EGLDisplay display, EGLSurface surface) {
    RenderImGui_Core(display, surface);
    if (old_eglSwap) return old_eglSwap(display, surface);
    return 1;
}

unsigned int hook_eglSwapWithDamage(EGLDisplay display, EGLSurface surface, EGLint* rects, EGLint n_rects) {
    RenderImGui_Core(display, surface);
    if (old_eglSwapWithDamage) return old_eglSwapWithDamage(display, surface, rects, n_rects);
    return 1;
}

void* hook_eglGetProcAddress(const char* procname) {
    void* real_addr = old_eglGetProcAddress ? old_eglGetProcAddress(procname) : nullptr;
    if (procname && strcmp(procname, "eglSwapBuffers") == 0) {
        if (!old_eglSwap && real_addr) old_eglSwap = (unsigned int (*)(EGLDisplay, EGLSurface))real_addr;
        return (void*)hook_eglSwap;
    }
    if (procname && strcmp(procname, "eglSwapBuffersWithDamageKHR") == 0) {
        if (!old_eglSwapWithDamage && real_addr) old_eglSwapWithDamage = (unsigned int (*)(EGLDisplay, EGLSurface, EGLint*, EGLint))real_addr;
        return (void*)hook_eglSwapWithDamage;
    }
    return real_addr;
}

int hook_vkCreateInstance(void* pCreateInfo, void* pAllocator, void* pInstance) {
    LOGI("[!] Vulkan Blocked! Forcing OpenGL..."); return -9; 
}

uintptr_t hook_shop_listen(uintptr_t x0, uintptr_t x1, uintptr_t x2, uintptr_t x3, uintptr_t x4, uintptr_t x5, uintptr_t x6, uintptr_t x7) {
    if (g_is_in_match.load(std::memory_order_relaxed) && !g_shop_listen_done.load() && x0 != 0) {
        if (std::find(g_shop_slots.begin(), g_shop_slots.end(), x0) == g_shop_slots.end()) {
            g_shop_slots.push_back(x0); if (g_shop_slots.size() >= 5) g_shop_listen_done.store(true); 
        }
    }
    if (old_shop_listen) return ((uintptr_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))old_shop_listen)(x0, x1, x2, x3, x4, x5, x6, x7);
    return 0;
}

typedef void (*func_set_IsGameEnd_t)(void* thisObj, uint8_t isEnd);
func_set_IsGameEnd_t orig_set_IsGameEnd = nullptr;
void hook_set_IsGameEnd(void* thisObj, uint8_t isEnd) {
    if (orig_set_IsGameEnd) orig_set_IsGameEnd(thisObj, isEnd);
    if (!thisObj || !IsValidPtr((uintptr_t)thisObj)) return;
    if (isEnd == 0) g_match_enter_pending.store(true, std::memory_order_release);
    else if (g_is_in_match.load(std::memory_order_acquire)) {
        g_is_in_match.store(false, std::memory_order_release); g_match_enter_pending.store(false, std::memory_order_release);
        g_need_segment_gap_before_enter = true; g_Tasks.trigger_game_end.store(true, std::memory_order_release);
    }
}

typedef void* (*func_SendWillRenderCanvases_t)();
func_SendWillRenderCanvases_t orig_SendWillRenderCanvases = nullptr;
void* hook_SendWillRenderCanvases() {
    {
        std::lock_guard<std::mutex> lock(g_Tasks.buy_mutex);
        if (!g_Tasks.buy_slots.empty()) {
            typedef void (*func_buy_new_t)(void*);
            func_buy_new_t buy_hero = (func_buy_new_t)(g_il2cppTrueBase + g_off.func_buy_hero_new);
            if (buy_hero) { for (uintptr_t s_addr : g_Tasks.buy_slots) { try { buy_hero((void*)s_addr); } catch(...) {} } }
            g_Tasks.buy_slots.clear();
        }
    }
    if (g_Tasks.trigger_quit.load()) {
        g_Tasks.trigger_quit.store(false);
        typedef void (*func_quit_t)(uintptr_t, int, int); func_quit_t quit_func = (func_quit_t)(g_il2cppTrueBase + g_off.func_quit);
        if (quit_func && IsValidPtr(g_dbg_segmentcsogame)) { try { quit_func(g_dbg_segmentcsogame, g_my_player_id, 1); } catch(...) {} }
        g_is_in_match.store(false, std::memory_order_release); g_need_segment_gap_before_enter = true; g_Tasks.trigger_game_end.store(true, std::memory_order_release);
    }
    if (orig_SendWillRenderCanvases) return orig_SendWillRenderCanvases();
    return nullptr;
}

JavaVM* g_jvm = nullptr; jobject g_view_obj = nullptr; jobject g_context = nullptr;
void (*old_nativeInjectEvent)(JNIEnv*, jobject, jobject) = nullptr;

void InjectTouchClick(JNIEnv* env, jobject view, float x, float y) {
    if (!env || !view || !old_nativeInjectEvent) return;
    static jclass SystemClock = nullptr; static jmethodID uptimeMillis = nullptr; static jclass MotionEvent = nullptr; static jmethodID obtainBasic = nullptr; static jmethodID setSource = nullptr;
    if (!SystemClock) {
        jclass sc = env->FindClass("android/os/SystemClock"); if (sc) { SystemClock = (jclass)env->NewGlobalRef(sc); uptimeMillis = env->GetStaticMethodID(SystemClock, "uptimeMillis", "()J"); env->DeleteLocalRef(sc); }
        jclass me = env->FindClass("android/view/MotionEvent"); if (me) { MotionEvent = (jclass)env->NewGlobalRef(me); obtainBasic = env->GetStaticMethodID(MotionEvent, "obtain", "(JJIFFI)Landroid/view/MotionEvent;"); setSource = env->GetMethodID(MotionEvent, "setSource", "(I)V"); env->DeleteLocalRef(me); }
    }
    if (!uptimeMillis || !obtainBasic) return;
    long time = env->CallStaticLongMethod(SystemClock, uptimeMillis);
    jobject eventDown = env->CallStaticObjectMethod(MotionEvent, obtainBasic, time, time, 0, (jfloat)x, (jfloat)y, 0);
    if (eventDown) { if (setSource) env->CallVoidMethod(eventDown, setSource, 4098); old_nativeInjectEvent(env, view, eventDown); env->DeleteLocalRef(eventDown); }
    if (g_touch_duration_ms > 0.0f) std::this_thread::sleep_for(std::chrono::microseconds((long long)(g_touch_duration_ms * 1000.0f)));
    long timeUp = env->CallStaticLongMethod(SystemClock, uptimeMillis);
    jobject eventUp = env->CallStaticObjectMethod(MotionEvent, obtainBasic, time, timeUp, 1, (jfloat)x, (jfloat)y, 0);
    if (eventUp) { if (setSource) env->CallVoidMethod(eventUp, setSource, 4098); old_nativeInjectEvent(env, view, eventUp); env->DeleteLocalRef(eventUp); }
    g_total_clicks_executed.fetch_add(1, std::memory_order_relaxed);
}

void AutoClickerThread() {
    JNIEnv* env = nullptr; bool attached = false;
    while (true) {
        if (!g_clicker_running.load(std::memory_order_relaxed) || !g_jvm || !g_view_obj) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        if (!env) { if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) { g_jvm->AttachCurrentThread(&env, nullptr); attached = true; } }
        if (env && g_view_obj) { for (const auto& cp : g_click_positions) { InjectTouchClick(env, g_view_obj, cp.x, cp.y); } }
        float interval = g_click_interval_ms; if (interval > 0) std::this_thread::sleep_for(std::chrono::microseconds((long long)(interval * 1000.0f))); else std::this_thread::yield();
    }
    if (attached && g_jvm) g_jvm->DetachCurrentThread();
}

extern "C" void hook_nativeInjectEvent(JNIEnv* env, jobject obj, jobject event) {
    if (!g_jvm) env->GetJavaVM(&g_jvm);
    if (obj && (!g_view_obj || !env->IsSameObject(g_view_obj, obj))) {
        if (g_view_obj) env->DeleteGlobalRef(g_view_obj);
        g_view_obj = env->NewGlobalRef(obj);
    }
    if (event) {
        static jclass motionEventClassGlobal = nullptr;
        if (!motionEventClassGlobal) { jclass meClass = env->FindClass("android/view/MotionEvent"); if (meClass) { motionEventClassGlobal = (jclass)env->NewGlobalRef(meClass); env->DeleteLocalRef(meClass); } }
        if (motionEventClassGlobal && env->IsInstanceOf(event, motionEventClassGlobal)) {
            static jmethodID getWidthMid = nullptr, getHeightMid = nullptr, getActionMid = nullptr, getXMid = nullptr, getYMid = nullptr;
            if (getWidthMid == nullptr) {
                jclass viewClass = env->GetObjectClass(obj); getWidthMid = env->GetMethodID(viewClass, "getWidth", "()I"); getHeightMid = env->GetMethodID(viewClass, "getHeight", "()I"); env->DeleteLocalRef(viewClass);
                getActionMid = env->GetMethodID(motionEventClassGlobal, "getAction", "()I"); getXMid = env->GetMethodID(motionEventClassGlobal, "getX", "()F"); getYMid = env->GetMethodID(motionEventClassGlobal, "getY", "()F"); 
            }
            if (getActionMid && getXMid && getYMid) {
                int action = env->CallIntMethod(event, getActionMid) & 255;
                if ((action == 0 && g_cached_view_width <= 0) || g_cached_view_width <= 0) { g_cached_view_width = env->CallIntMethod(obj, getWidthMid); g_cached_view_height = env->CallIntMethod(obj, getHeightMid); }
                float raw_x = env->CallFloatMethod(event, getXMid), raw_y = env->CallFloatMethod(event, getYMid);
                float scale_x = 1.0f, scale_y = 1.0f;
                if (g_cached_view_width > 0 && g_gl_width > 0) scale_x = (float)g_gl_width / g_cached_view_width;
                if (g_cached_view_height > 0 && g_gl_height > 0) scale_y = (float)g_gl_height / g_cached_view_height;
                ImGuiIO& io = ImGui::GetIO(); io.AddMousePosEvent(raw_x * scale_x, raw_y * scale_y);
                if (action == 0) io.AddMouseButtonEvent(0, true); else if (action == 1) io.AddMouseButtonEvent(0, false);
                if (io.WantCaptureMouse) return; // 拦截触摸，防止点穿到游戏
            }
        }
    }
    if (old_nativeInjectEvent) old_nativeInjectEvent(env, obj, event);
}

void FindAndHookHiddenJNI() {
    FILE* fp = fopen("/proc/self/maps", "r"); if (!fp) return;
    char line[1024]; struct MemRegion { uintptr_t start, end; bool is_rw; }; std::vector<MemRegion> regions;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "libunity.so")) {
            bool is_r = strstr(line, "r-") != nullptr; bool is_rw = strstr(line, "rw") != nullptr;
            if (is_r || is_rw) { uintptr_t start, end; sscanf(line, "%lx-%lx", &start, &end); regions.push_back({start, end, is_rw}); }
        }
    }
    fclose(fp);
    const char* target_string = "nativeInjectEvent"; std::vector<uintptr_t> string_addrs;
    for (const auto& reg : regions) {
        if (!reg.is_rw) {
            for (uintptr_t p = reg.start; p < reg.end - strlen(target_string); p++) {
                if (memcmp((void*)p, target_string, strlen(target_string)) == 0) string_addrs.push_back(p);
            }
        }
    }
    if (string_addrs.empty()) return;
    bool found_func = false;
    for (const auto& reg : regions) {
        uintptr_t align_start = (reg.start + 7) & ~7;
        for (uintptr_t p = align_start; p < reg.end - sizeof(void*)*3; p += sizeof(void*)) {
            uintptr_t ptr_val = *(uintptr_t*)p;
            for (uintptr_t str_addr : string_addrs) {
                if (ptr_val == str_addr) {
                    void** fnPtr_addr = (void**)(p + 16); void* real_function_addr = *fnPtr_addr;
                    if (real_function_addr != nullptr && (uintptr_t)real_function_addr > 0x100000) {
                        DobbyHook(real_function_addr, (void*)hook_nativeInjectEvent, (void**)&old_nativeInjectEvent);
                        found_func = true; break;
                    }
                }
            }
            if (found_func) break;
        }
        if (found_func) break;
    }
    std::thread(AutoClickerThread).detach();
}

void* DelayedHookThread(void*) {
    int timeout = 0;
    while (!g_engine_rendering.load() && timeout < 20) { sleep(1); timeout++; }
    sleep(3); FindAndHookHiddenJNI();
    return nullptr;
}

void* SetupThread(void*) {
    int retry_count = 0;
    while (g_il2cppTrueBase == 0 && retry_count < 60) {
        FILE *fp = fopen("/proc/self/maps", "r");
        if (fp) { char line[512]; while (fgets(line, sizeof(line), fp)) { if (strstr(line, "libil2cpp.so")) { sscanf(line, "%lx", &g_il2cppTrueBase); break; } } fclose(fp); }
        if (g_il2cppTrueBase == 0) { sleep(1); retry_count++; }
    }
    
    // 1. 封杀 Vulkan 并动态拦截 EGL
    void* vk_ptr = DobbySymbolResolver("libvulkan.so", "vkCreateInstance");
    if (!vk_ptr) { void* h = dlopen("libvulkan.so", RTLD_LAZY); if (h) vk_ptr = dlsym(h, "vkCreateInstance"); }
    if (vk_ptr) DobbyHook(vk_ptr, (void*)hook_vkCreateInstance, (void**)&old_vkCreateInstance);

    void* getproc_ptr = DobbySymbolResolver("libEGL.so", "eglGetProcAddress");
    if (!getproc_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) getproc_ptr = dlsym(h, "eglGetProcAddress"); }
    if (getproc_ptr) DobbyHook(getproc_ptr, (void*)hook_eglGetProcAddress, (void**)&old_eglGetProcAddress);

    void* egl_ptr = (void*)eglGetProcAddress("eglSwapBuffers");
    if (!egl_ptr) egl_ptr = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    if (!egl_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) egl_ptr = dlsym(h, "eglSwapBuffers"); }
    if (egl_ptr) DobbyHook(egl_ptr, (void*)hook_eglSwap, (void**)&old_eglSwap);

    void* egl_damage_ptr = (void*)eglGetProcAddress("eglSwapBuffersWithDamageKHR");
    if (!egl_damage_ptr) egl_damage_ptr = DobbySymbolResolver("libEGL.so", "eglSwapBuffersWithDamageKHR");
    if (!egl_damage_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) egl_damage_ptr = dlsym(h, "eglSwapBuffersWithDamageKHR"); }
    if (egl_damage_ptr) DobbyHook(egl_damage_ptr, (void*)hook_eglSwapWithDamage, (void**)&old_eglSwapWithDamage);

    // 2. Hook 游戏逻辑
    if (g_off.func_shop_listen != 0) DobbyHook((void*)(g_il2cppTrueBase + g_off.func_shop_listen), (void*)hook_shop_listen, (void**)&old_shop_listen);
    if (g_off.func_set_IsGameEnd != 0) DobbyHook((void*)(g_il2cppTrueBase + g_off.func_set_IsGameEnd), (void*)hook_set_IsGameEnd, (void**)&orig_set_IsGameEnd);

    typedef void* (*il2cpp_domain_get_t)(); typedef void* (*il2cpp_domain_assembly_open_t)(void*, const char*); typedef void* (*il2cpp_assembly_get_image_t)(void*); typedef void* (*il2cpp_class_from_name_t)(void*, const char*, const char*); typedef void* (*il2cpp_class_get_method_from_name_t)(void*, const char*, int);
    auto domain_get = (il2cpp_domain_get_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_domain_get");
    auto assembly_open = (il2cpp_domain_assembly_open_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_domain_assembly_open");
    auto assembly_get_image = (il2cpp_assembly_get_image_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_assembly_get_image");
    auto class_from_name = (il2cpp_class_from_name_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_class_from_name");
    auto class_get_method_from_name = (il2cpp_class_get_method_from_name_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_class_get_method_from_name");
    if (domain_get && assembly_open && assembly_get_image && class_from_name && class_get_method_from_name) {
        void* domain = domain_get(); if (domain) { void* assembly = assembly_open(domain, "UnityEngine.UIModule.dll"); if (assembly) { void* image = assembly_get_image(assembly); if (image) { void* klass = class_from_name(image, "UnityEngine", "Canvas"); if (klass) { void* method = class_get_method_from_name(klass, "SendWillRenderCanvases", 0); if (method) { void* method_ptr = *(void**)method; if (method_ptr) DobbyHook(method_ptr, (void*)hook_SendWillRenderCanvases, (void**)&orig_SendWillRenderCanvases); } } } } }
    }

    pthread_t t2; pthread_create(&t2, 0, DelayedHookThread, 0); pthread_detach(t2);
    return nullptr;
}

__attribute__((constructor)) void Init() { 
    pthread_t t; pthread_create(&t, 0, SetupThread, 0); pthread_detach(t); 
}
