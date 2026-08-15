#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string>
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
#include <jni.h>
#include "dobby.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JKInternal", __VA_ARGS__)

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
std::vector<uintptr_t> g_dbg_list23_addrs;
struct AvatarRankProbe { uintptr_t entry = 0, addr26 = 0; int raw_rank = 0, rank = 0, pid = 0, matched_id = -1; };
std::vector<AvatarRankProbe> g_dbg_avatar_ranks;
uintptr_t g_dbg_addr26 = 0;
uintptr_t g_dbg_hexctrl = 0;

std::vector<uintptr_t> g_dbg_list7_addrs;
std::map<uintptr_t, std::vector<uintptr_t>> g_dbg_list9_map;
std::vector<uintptr_t> g_dbg_player_addrs;

int g_my_player_id = 0;
int g_hex_qualities[3] = {0, 0, 0};
std::vector<int> g_next_opponents;

struct PoolHero { int heroId; int remaining; int total; int cost; uintptr_t addr10; };
std::vector<PoolHero> g_poolHeroes;
std::map<int, std::vector<int>> g_heroesByCost;
std::unordered_map<int, bool> g_heroAutoBuyChecked;

static void UpsertPoolHero(int heroId, int remaining, int total, uintptr_t addr10) {
    if (heroId <= 0 || heroId >= 100000) return;
    int cost = (heroId / 1000) % 10;
    for (auto& ph : g_poolHeroes) {
        if (ph.heroId == heroId) {
            ph.remaining = remaining;
            ph.total = total;
            ph.addr10 = addr10;
            return;
        }
    }
    g_poolHeroes.push_back({ heroId, remaining, total, cost, addr10 });
    auto& list = g_heroesByCost[cost];
    if (std::find(list.begin(), list.end(), heroId) == list.end())
        list.push_back(heroId);
}

struct BoardHero { int heroId; int x; int y; };
struct PlayerInfo {
    std::string name;
    int id;
    bool is_bot;
    int money;
    int win_streak;
    int lose_streak;
    int level;
    int avatar_rank = 0;
    uintptr_t val_ptr = 0;
    uintptr_t addr13_ptr = 0;
    std::vector<int> shop;
    std::vector<int> bench;
    std::vector<BoardHero> board;
};
std::vector<PlayerInfo> g_players;

std::atomic<bool> g_is_in_match{false};
std::atomic<bool> g_match_enter_pending{false};
static int g_segment_valid_streak = 0;
static bool g_need_segment_gap_before_enter = false;

// UI State & Styles
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

bool g_win_cardpool = true;
bool g_win_playerdata = true;
bool g_win_hextech = true;
float g_alpha_cp = 1.0f;
float g_alpha_pd = 1.0f;
float g_alpha_opp = 1.0f;
float g_alpha_hex = 1.0f;

int g_cp_columns = 6;
int g_cp_rows = 0; 
float g_cp_box_size = 65.0f;
float g_cp_scale = 1.0f;
bool g_cp_show_cost[6] = { false, true, true, true, true, true };
bool g_cp_warning_enable = true;
int g_cp_warning_thres = 3;

float g_pd_line_spacing = 0.0f;
float g_pd_vert_spacing = 0.0f;
float g_pd_arrow_spacing = 15.0f;
float g_pd_font_size = 1.0f;
bool g_pd_hero_summary_enable = true;
int g_pd_hero_count_min[6] = {0, 1, 1, 1, 1, 1};

bool g_opp_show_board = true;
bool g_opp_show_shop = true;
bool g_opp_show_bench = true;
float g_opp_hex_size = 25.0f;
float g_opp_scale = 1.0f;

float g_hextech_scale = 1.0f;

bool g_win_hero_warn = true;
int g_hero_warn_thres = 3;
float g_hero_warn_scale = 1.0f;
float g_alpha_hero_warn = 1.0f;
float g_float_hw_x = -1.0f, g_float_hw_y = -1.0f;

int g_cached_view_width = 0;
int g_cached_view_height = 0;
int g_gl_width = 0, g_gl_height = 0;

bool g_auto_clicker_enable = false;
float g_click_interval_ms = 0.0f;    
float g_touch_duration_ms = 0.0f;    
struct ClickPos { float x = 540.0f; float y = 960.0f; };
std::vector<ClickPos> g_click_positions = { {540.0f, 960.0f} };
int g_click_pos_captured = -1;       
std::atomic<bool> g_clicker_running{false};

std::atomic<int> g_total_clicks_executed{0};
float g_realtime_cps = 0.0f;
float g_cps_history[100] = {0.0f};
int g_cps_hist_idx = 0;

float g_quit_x = 80.0f, g_quit_y = 520.0f;
int g_quit_confirm = 0;
float g_quit_timer = 0.0f;

float g_lock_x = 80.0f, g_lock_y = 460.0f;
bool g_floats_locked = false;

float g_cpbtn_x = 80.0f, g_cpbtn_y = 400.0f;
float g_clickerbtn_x = 80.0f, g_clickerbtn_y = 340.0f;

float g_float_cp_x = -1.0f, g_float_cp_y = -1.0f;
float g_float_pd_x = -1.0f, g_float_pd_y = -1.0f;
float g_float_opp_x = -1.0f, g_float_opp_y = -1.0f;
float g_float_hex_x = -1.0f, g_float_hex_y = -1.0f;
static bool g_apply_saved_float_pos = false;

std::atomic<bool> g_hero_images_ready{false};
int g_hero_image_count = 0;

struct TexDecodedData { int w, h; unsigned char* pixels; };
std::mutex g_TexMutex;
std::unordered_map<int, GLuint> g_heroTextureCache;
std::vector<std::pair<int, TexDecodedData>> g_HeroTexDecodedQueue;
struct DecodeRequest { int id; };
std::deque<DecodeRequest> g_DecodeRequestQueue;
std::mutex g_DecodeRequestMutex;
std::atomic<bool> g_tex_worker_started{false};

struct MainThreadTasks {
    std::atomic<bool> trigger_quit{false};
    std::atomic<bool> trigger_game_end{false};
    std::mutex buy_mutex;
    std::vector<uintptr_t> buy_slots;
} g_Tasks;

std::vector<uintptr_t> g_shop_slots;
void* old_shop_listen = nullptr;
std::atomic<bool> g_shop_listen_done{false};

uintptr_t hook_shop_listen(uintptr_t x0, uintptr_t x1, uintptr_t x2, uintptr_t x3, uintptr_t x4, uintptr_t x5, uintptr_t x6, uintptr_t x7) {
    if (g_is_in_match.load(std::memory_order_relaxed) && !g_shop_listen_done.load() && x0 != 0) {
        if (std::find(g_shop_slots.begin(), g_shop_slots.end(), x0) == g_shop_slots.end()) {
            g_shop_slots.push_back(x0);
            if (g_shop_slots.size() >= 5) {
                g_shop_listen_done.store(true); 
            }
        }
    }
    if (old_shop_listen) {
        return ((uintptr_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))old_shop_listen)(x0, x1, x2, x3, x4, x5, x6, x7);
    }
    return 0;
}

bool SafeReadMemory(uintptr_t addr, void* buffer, size_t size) {
    if (addr < 0x10000 || addr > 0x00007FFFFFFFFFFF) return false;
    static int safe_fd = -1;
    if (safe_fd == -1) safe_fd = open("/dev/random", O_WRONLY);
    if (safe_fd >= 0) if (write(safe_fd, (void*)addr, size) < 0) return false;
    memcpy(buffer, (void*)addr, size);
    return true;
}

uintptr_t SafeReadPtr(uintptr_t addr, uint32_t offset) {
    uintptr_t val = 0;
    if (SafeReadMemory(addr + offset, &val, sizeof(val))) return val;
    return 0;
}

int SafeReadInt(uintptr_t addr, uint32_t offset) {
    int val = 0;
    if (SafeReadMemory(addr + offset, &val, sizeof(val))) return val;
    return 0;
}

uint8_t SafeReadByte(uintptr_t addr, uint32_t offset) {
    uint8_t val = 0;
    if (SafeReadMemory(addr + offset, &val, sizeof(val))) return val;
    return 0;
}

#define SAFE_READ_PTR(addr, offset) SafeReadPtr(addr, offset)
#define SAFE_READ_INT(addr, offset) SafeReadInt(addr, offset)
#define SAFE_READ_BYTE(addr, offset) SafeReadByte(addr, offset)
#define IsValidPtr(ptr) ((ptr) > 0x10000 && (ptr) < 0x00007FFFFFFFFFFF)

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
    std::vector<uintptr_t> res;
    if (!IsValidPtr(arrayAddr)) return res;
    int count = SAFE_READ_INT(arrayAddr, 0x18);
    if (count <= 0 || count > maxCount * 10) return res;
    int readLimit = (maxCount <= 0) ? count : std::min(count, maxCount);
    for (int i = 0; i < readLimit; i++) {
        uintptr_t ptr = SAFE_READ_PTR(arrayAddr, 0x20 + i * structSize + ptrOffset);
        if (IsValidPtr(ptr)) res.push_back(ptr);
    }
    return res;
}

std::vector<uintptr_t> GetPointersInArray(uintptr_t arrayAddr, int maxCount) {
    std::vector<uintptr_t> res;
    if (!IsValidPtr(arrayAddr)) return res;
    int count = SAFE_READ_INT(arrayAddr, 0x18);
    if (count <= 0 || count > maxCount * 10) return res; 
    int readLimit = std::min(count, maxCount);
    for (int i = 0; i < readLimit; i++) {
        uintptr_t ptr = SAFE_READ_PTR(arrayAddr, 0x20 + i * 8);
        if (IsValidPtr(ptr)) res.push_back(ptr);
    }
    return res;
}

std::vector<int> GetIntsInArray(uintptr_t arrayAddr, int maxCount) {
    std::vector<int> res;
    if (!IsValidPtr(arrayAddr)) return res;
    int count = SAFE_READ_INT(arrayAddr, 0x18);
    if (count <= 0 || count > maxCount * 10) return res; 
    int readLimit = std::min(count, maxCount);
    for (int i = 0; i < readLimit; i++) {
        int val = SAFE_READ_INT(arrayAddr, 0x20 + i * 4); 
        res.push_back(val);
    }
    return res;
}

std::string GetConfigPath() {
    if (access("/sdcard/Download/", W_OK) == 0) return "/sdcard/Download/jkt_offsets.txt";
    return "/data/local/tmp/jkt_offsets.txt";
}

static void CaptureWindowPos(const char* name, float& x, float& y) {
    ImGuiWindow* w = ImGui::FindWindowByName(name);
    if (w) { x = w->Pos.x; y = w->Pos.y; }
}

void SaveConfig() {
    if (g_isImGuiInit) {
        CaptureWindowPos("##CardPoolFloat", g_float_cp_x, g_float_cp_y);
        CaptureWindowPos("##PlayerDataFloat", g_float_pd_x, g_float_pd_y);
        CaptureWindowPos("##OpponentFloat", g_float_opp_x, g_float_opp_y);
        CaptureWindowPos("##HextechFloat", g_float_hex_x, g_float_hex_y);
    }

    std::ofstream out(GetConfigPath());
    if (out.is_open()) {
        out << "# [完美UI版] 在此处或菜单内修改十六进制偏移并保存，会自动生效！\n";
        
        #define WRITE_OFF_32(name) out << #name << "=0x" << std::hex << g_off.name << "\n"
        WRITE_OFF_32(func_get_Instance); WRITE_OFF_32(addr2); WRITE_OFF_32(addr3); WRITE_OFF_32(addra); WRITE_OFF_32(segmentcsogame);
        WRITE_OFF_32(func_quit); WRITE_OFF_32(my_player_id); WRITE_OFF_32(segment_my_player_id); WRITE_OFF_32(next_opponents_list);
        WRITE_OFF_32(func_reqbuyhero); WRITE_OFF_32(func_shop_listen); WRITE_OFF_32(func_buy_hero_new);
        WRITE_OFF_32(func_set_IsGameEnd);
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
        out << "ui_theme=" << g_ui_theme << "\n";
        out << "win_cardpool=" << (g_win_cardpool ? 1 : 0) << "\n";
        out << "win_playerdata=" << (g_win_playerdata ? 1 : 0) << "\n";
        out << "win_hextech=" << (g_win_hextech ? 1 : 0) << "\n";
        out << "alpha_cp=" << g_alpha_cp << "\n";
        out << "alpha_pd=" << g_alpha_pd << "\n";
        out << "alpha_opp=" << g_alpha_opp << "\n";
        out << "alpha_hex=" << g_alpha_hex << "\n";
        out << "floats_locked=" << (g_floats_locked ? 1 : 0) << "\n";
        out << "cp_columns=" << g_cp_columns << "\n";
        out << "cp_rows=" << g_cp_rows << "\n";
        out << "cp_scale=" << g_cp_scale << "\n";
        for (int i = 1; i <= 5; i++) out << "cp_show_cost" << i << "=" << (g_cp_show_cost[i] ? 1 : 0) << "\n";
        out << "cp_warning_enable=" << (g_cp_warning_enable ? 1 : 0) << "\n";
        out << "cp_warning_thres=" << g_cp_warning_thres << "\n";
        out << "pd_line_spacing=" << g_pd_line_spacing << "\n";
        out << "pd_vert_spacing=" << g_pd_vert_spacing << "\n";
        out << "pd_arrow_spacing=" << g_pd_arrow_spacing << "\n";
        out << "pd_font_size=" << g_pd_font_size << "\n";
        out << "pd_hero_summary_enable=" << (g_pd_hero_summary_enable ? 1 : 0) << "\n";
        for (int i = 1; i <= 5; i++) out << "pd_hero_count_min" << i << "=" << g_pd_hero_count_min[i] << "\n";
        out << "opp_scale=" << g_opp_scale << "\n";
        out << "opp_show_board=" << (g_opp_show_board ? 1 : 0) << "\n";
        out << "opp_show_shop=" << (g_opp_show_shop ? 1 : 0) << "\n";
        out << "opp_show_bench=" << (g_opp_show_bench ? 1 : 0) << "\n";
        out << "opp_hex_size=" << g_opp_hex_size << "\n";
        out << "hextech_scale=" << g_hextech_scale << "\n";
        out << "win_hero_warn=" << (g_win_hero_warn ? 1 : 0) << "\n";
        out << "hero_warn_thres=" << g_hero_warn_thres << "\n";
        out << "hero_warn_scale=" << g_hero_warn_scale << "\n";
        out << "alpha_hero_warn=" << g_alpha_hero_warn << "\n";
        out << "float_hw_x=" << g_float_hw_x << "\n";
        out << "float_hw_y=" << g_float_hw_y << "\n";
        out << "menu_x=" << g_menuX << "\n";
        out << "menu_y=" << g_menuY << "\n";
        out << "menu_w=" << g_menuW << "\n";
        out << "menu_h=" << g_menuH << "\n";
        out << "menu_scale=" << g_scale << "\n";
        out << "menu_collapsed=" << (g_menu_orb ? 1 : 0) << "\n";
        out << "orb_x=" << g_orb_x << "\n";
        out << "orb_y=" << g_orb_y << "\n";
        out << "quit_x=" << g_quit_x << "\n";
        out << "quit_y=" << g_quit_y << "\n";
        out << "lock_x=" << g_lock_x << "\n";
        out << "lock_y=" << g_lock_y << "\n";
        out << "cpbtn_x=" << g_cpbtn_x << "\n";
        out << "cpbtn_y=" << g_cpbtn_y << "\n";
        out << "clickerbtn_x=" << g_clickerbtn_x << "\n";
        out << "clickerbtn_y=" << g_clickerbtn_y << "\n";
        out << "float_cp_x=" << g_float_cp_x << "\n";
        out << "float_cp_y=" << g_float_cp_y << "\n";
        out << "float_pd_x=" << g_float_pd_x << "\n";
        out << "float_pd_y=" << g_float_pd_y << "\n";
        out << "float_opp_x=" << g_float_opp_x << "\n";
        out << "float_opp_y=" << g_float_opp_y << "\n";
        out << "float_hex_x=" << g_float_hex_x << "\n";
        out << "float_hex_y=" << g_float_hex_y << "\n";
        out << "auto_clicker_enable=" << (g_auto_clicker_enable ? 1 : 0) << "\n";
        out << "click_interval_ms=" << g_click_interval_ms << "\n";
        out << "touch_duration_ms=" << g_touch_duration_ms << "\n";
        out << "click_positions=";
        for (size_t i = 0; i < g_click_positions.size(); i++) {
            if (i > 0) out << ";";
            out << g_click_positions[i].x << "," << g_click_positions[i].y;
        }
        out << "\n";
        out << "auto_buy_ids=";
        bool first = true;
        for (const auto& pair : g_heroAutoBuyChecked) {
            if (pair.second) {
                if (!first) out << ",";
                out << pair.first;
                first = false;
            }
        }
        out << "\n";
        out.close();
    }
}

void LoadConfig() {
    std::ifstream in(GetConfigPath());
    if (!in.is_open()) { SaveConfig(); return; }
    
    std::string line;
    bool has_full = false;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        if (line.find("pi_win_streak") != std::string::npos) has_full = true;
        
        auto delim = line.find('=');
        if (delim != std::string::npos) {
            std::string key = line.substr(0, delim);
            std::string valStr = line.substr(delim + 1);
            try {
                uint32_t val = std::stoul(valStr, nullptr, 16);
                #define PARSE_OFF_32(name) if (key == #name) g_off.name = val;
                
                PARSE_OFF_32(func_get_Instance) PARSE_OFF_32(addr2) PARSE_OFF_32(addr3) PARSE_OFF_32(addra) PARSE_OFF_32(segmentcsogame)
                PARSE_OFF_32(func_quit) PARSE_OFF_32(my_player_id) PARSE_OFF_32(segment_my_player_id) PARSE_OFF_32(next_opponents_list) PARSE_OFF_32(func_reqbuyhero)
                PARSE_OFF_32(func_shop_listen) PARSE_OFF_32(func_buy_hero_new) PARSE_OFF_32(func_set_IsGameEnd)
                PARSE_OFF_32(addr4) PARSE_OFF_32(addr5) PARSE_OFF_32(addr6) PARSE_OFF_32(addr7)
                PARSE_OFF_32(addr7_struct_size) PARSE_OFF_32(addr7_ptr_offset)
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
                if (key == "addr24") g_off.addr26 = val;
            } catch (...) {}
            try {
                if (key == "ui_theme") g_ui_theme = std::stoi(valStr);
                else if (key == "win_cardpool") g_win_cardpool = (std::stoi(valStr) != 0);
                else if (key == "win_playerdata") g_win_playerdata = (std::stoi(valStr) != 0);
                else if (key == "win_hextech") g_win_hextech = (std::stoi(valStr) != 0);
                else if (key == "alpha_cp") g_alpha_cp = std::clamp(std::stof(valStr), 0.1f, 1.0f);
                else if (key == "alpha_pd") g_alpha_pd = std::clamp(std::stof(valStr), 0.1f, 1.0f);
                else if (key == "alpha_opp") g_alpha_opp = std::clamp(std::stof(valStr), 0.1f, 1.0f);
                else if (key == "alpha_hex") g_alpha_hex = std::clamp(std::stof(valStr), 0.1f, 1.0f);
                else if (key == "floats_locked") g_floats_locked = (std::stoi(valStr) != 0);
                else if (key == "cp_columns") g_cp_columns = std::stoi(valStr);
                else if (key == "cp_rows") g_cp_rows = std::stoi(valStr);
                else if (key == "cp_scale") g_cp_scale = std::stof(valStr);
                else if (key.rfind("cp_show_cost", 0) == 0) { int i = key.back() - '0'; if (i >= 1 && i <= 5) g_cp_show_cost[i] = (std::stoi(valStr) != 0); }
                else if (key == "cp_warning_enable") g_cp_warning_enable = (std::stoi(valStr) != 0);
                else if (key == "cp_warning_thres") g_cp_warning_thres = std::stoi(valStr);
                else if (key == "pd_line_spacing") g_pd_line_spacing = std::stof(valStr);
                else if (key == "pd_vert_spacing") g_pd_vert_spacing = std::stof(valStr);
                else if (key == "pd_arrow_spacing") g_pd_arrow_spacing = std::stof(valStr);
                else if (key == "pd_font_size") g_pd_font_size = std::stof(valStr);
                else if (key == "pd_hero_summary_enable") g_pd_hero_summary_enable = (std::stoi(valStr) != 0);
                else if (key.rfind("pd_hero_count_min", 0) == 0) { int i = key.back() - '0'; if (i >= 1 && i <= 5) g_pd_hero_count_min[i] = std::stoi(valStr); }
                else if (key == "opp_scale") g_opp_scale = std::stof(valStr);
                else if (key == "opp_show_board") g_opp_show_board = (std::stoi(valStr) != 0);
                else if (key == "opp_show_shop") g_opp_show_shop = (std::stoi(valStr) != 0);
                else if (key == "opp_show_bench") g_opp_show_bench = (std::stoi(valStr) != 0);
                else if (key == "opp_hex_size") g_opp_hex_size = std::stof(valStr);
                else if (key == "hextech_scale") g_hextech_scale = std::stof(valStr);
                else if (key == "win_hero_warn") g_win_hero_warn = (std::stoi(valStr) != 0);
                else if (key == "hero_warn_thres") g_hero_warn_thres = std::stoi(valStr);
                else if (key == "hero_warn_scale") g_hero_warn_scale = std::stof(valStr);
                else if (key == "alpha_hero_warn") g_alpha_hero_warn = std::stof(valStr);
                else if (key == "float_hw_x") g_float_hw_x = std::stof(valStr);
                else if (key == "float_hw_y") g_float_hw_y = std::stof(valStr);
                else if (key == "menu_x") g_menuX = std::stof(valStr);
                else if (key == "menu_y") g_menuY = std::stof(valStr);
                else if (key == "menu_w") g_menuW = std::stof(valStr);
                else if (key == "menu_h") g_menuH = std::stof(valStr);
                else if (key == "menu_scale") g_scale = std::stof(valStr);
                else if (key == "menu_collapsed") g_menu_orb = (std::stoi(valStr) != 0);
                else if (key == "orb_x") g_orb_x = std::stof(valStr);
                else if (key == "orb_y") g_orb_y = std::stof(valStr);
                else if (key == "quit_x") g_quit_x = std::stof(valStr);
                else if (key == "quit_y") g_quit_y = std::stof(valStr);
                else if (key == "lock_x") g_lock_x = std::stof(valStr);
                else if (key == "lock_y") g_lock_y = std::stof(valStr);
                else if (key == "cpbtn_x") g_cpbtn_x = std::stof(valStr);
                else if (key == "cpbtn_y") g_cpbtn_y = std::stof(valStr);
                else if (key == "clickerbtn_x") g_clickerbtn_x = std::stof(valStr);
                else if (key == "clickerbtn_y") g_clickerbtn_y = std::stof(valStr);
                else if (key == "float_cp_x") g_float_cp_x = std::stof(valStr);
                else if (key == "float_cp_y") g_float_cp_y = std::stof(valStr);
                else if (key == "float_pd_x") g_float_pd_x = std::stof(valStr);
                else if (key == "float_pd_y") g_float_pd_y = std::stof(valStr);
                else if (key == "float_opp_x") g_float_opp_x = std::stof(valStr);
                else if (key == "float_opp_y") g_float_opp_y = std::stof(valStr);
                else if (key == "float_hex_x") g_float_hex_x = std::stof(valStr);
                else if (key == "float_hex_y") g_float_hex_y = std::stof(valStr);
                else if (key == "auto_clicker_enable") g_auto_clicker_enable = (std::stoi(valStr) != 0);
                else if (key == "click_interval_ms") g_click_interval_ms = std::stof(valStr);
                else if (key == "touch_duration_ms") g_touch_duration_ms = std::stof(valStr);
                else if (key == "click_positions") {
                    g_click_positions.clear();
                    std::stringstream ss(valStr);
                    std::string pair;
                    while (std::getline(ss, pair, ';')) {
                        if (pair.empty()) continue;
                        std::stringstream ss2(pair);
                        std::string xs, ys;
                        if (std::getline(ss2, xs, ',') && std::getline(ss2, ys, ',')) {
                            try { g_click_positions.push_back({std::stof(xs), std::stof(ys)}); } catch (...) {}
                        }
                    }
                    if (g_click_positions.empty()) g_click_positions.push_back({540.0f, 960.0f});
                }
                else if (key == "auto_buy_ids") {
                    g_heroAutoBuyChecked.clear();
                    std::stringstream ss(valStr);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        if (!item.empty()) {
                            try { g_heroAutoBuyChecked[std::stoi(item)] = true; } catch (...) {}
                        }
                    }
                }
            } catch (...) {}
        }
    }
    in.close();
    g_apply_saved_float_pos = true;
    if (!has_full) SaveConfig();
}

void ClearGameState() {
    g_poolHeroes.clear();
    g_heroesByCost.clear();
    g_players.clear();
    g_next_opponents.clear();
    g_my_player_id = 0;
    g_hex_qualities[0] = g_hex_qualities[1] = g_hex_qualities[2] = 0;

    g_dbg_addr1 = 0; g_dbg_addr2 = 0; g_dbg_addr3 = 0; g_dbg_addra = 0; g_dbg_segmentcsogame = 0;
    g_dbg_addr4 = 0; g_dbg_addr5 = 0; g_dbg_addr6 = 0; g_dbg_addr7 = 0;
    g_dbg_addr11 = 0; g_dbg_addr12 = 0; g_dbg_addr13 = 0;
    g_dbg_addr21 = 0; g_dbg_addr22 = 0; g_dbg_addr23 = 0;
    g_dbg_addr26 = 0; g_dbg_hexctrl = 0;
    g_dbg_list7_addrs.clear();
    g_dbg_list9_map.clear();
    g_dbg_player_addrs.clear();
    g_dbg_list23_addrs.clear();
    g_dbg_avatar_ranks.clear();

    g_shop_slots.clear();
    g_shop_listen_done.store(false);
    g_match_enter_pending.store(false);
    g_segment_valid_streak = 0;
    g_need_segment_gap_before_enter = true;
    {
        std::lock_guard<std::mutex> lock(g_Tasks.buy_mutex);
        g_Tasks.buy_slots.clear();
    }
}

bool TryResolveSegmentCSOGame(uintptr_t* out_segment = nullptr) {
    if (g_il2cppTrueBase == 0) return false;
    typedef void* (*func_get_Instance_t)(void* method_info);
    func_get_Instance_t get_Instance = (func_get_Instance_t)(g_il2cppTrueBase + (uintptr_t)g_off.func_get_Instance);
    if (!get_Instance) return false;

    uintptr_t addr1 = 0;
    try { addr1 = (uintptr_t)get_Instance(nullptr); } catch (...) { return false; }
    if (!IsValidPtr(addr1)) return false;

    uintptr_t addr2 = SAFE_READ_PTR(addr1, g_off.addr2);
    if (!IsValidPtr(addr2)) return false;
    uintptr_t addr3 = SAFE_READ_PTR(addr2, g_off.addr3);
    if (!IsValidPtr(addr3)) return false;
    uintptr_t addra = SAFE_READ_PTR(addr3, g_off.addra);
    if (!IsValidPtr(addra)) return false;
    uintptr_t segment = SAFE_READ_PTR(addra, g_off.segmentcsogame);
    if (!IsValidPtr(segment)) return false;

    if (out_segment) *out_segment = segment;
    return true;
}

void UpdateMatchState() {
    uintptr_t segment = 0;
    bool segmentValid = TryResolveSegmentCSOGame(&segment);
    bool inMatch = g_is_in_match.load(std::memory_order_acquire);

    if (inMatch) {
        if (!segmentValid) {
            g_is_in_match.store(false, std::memory_order_release);
            g_match_enter_pending.store(false, std::memory_order_release);
            g_segment_valid_streak = 0;
            g_need_segment_gap_before_enter = true;
            g_Tasks.trigger_game_end.store(true, std::memory_order_release);
            return;
        }
        if (g_dbg_segmentcsogame != 0 && g_dbg_segmentcsogame != segment) {
            g_dbg_segmentcsogame = segment;
            g_Tasks.trigger_game_end.store(true, std::memory_order_release);
        }
        return;
    }

    if (!segmentValid) {
        g_segment_valid_streak = 0;
        g_need_segment_gap_before_enter = false;
        return;
    }

    if (g_match_enter_pending.load(std::memory_order_acquire)) {
        g_dbg_segmentcsogame = segment;
        g_is_in_match.store(true, std::memory_order_release);
        g_match_enter_pending.store(false, std::memory_order_release);
        g_segment_valid_streak = 0;
        g_need_segment_gap_before_enter = false;
        return;
    }

    if (g_need_segment_gap_before_enter) {
        g_segment_valid_streak = 0;
        return;
    }

    if (++g_segment_valid_streak >= 3) {
        g_dbg_segmentcsogame = segment;
        g_is_in_match.store(true, std::memory_order_release);
        g_segment_valid_streak = 0;
    }
}

static int NormalizeAvatarRank(int raw) {
    if (raw >= 1 && raw <= 8) return raw;
    if (raw >= 0 && raw <= 7) return raw + 1;
    return -1;
}

static int MatchPlayerIndexByAvatarPid(int pid) {
    if (pid < 0) return -1;
    for (size_t i = 0; i < g_players.size(); i++)
        if (g_players[i].id == pid) return (int)i;
    return -1;
}

static void ApplyAvatarRanksFromList23() {
    g_dbg_avatar_ranks.clear();
    for (auto& pi : g_players) pi.avatar_rank = 0;

    for (uintptr_t entry : g_dbg_list23_addrs) {
        if (!IsValidPtr(entry)) continue;
        uintptr_t addr26 = SAFE_READ_PTR(entry, g_off.addr26);
        if (!IsValidPtr(addr26)) continue;

        int raw_rank = SAFE_READ_INT(addr26, g_off.pi_avatar_rank);
        int rank = NormalizeAvatarRank(raw_rank);
        if (rank < 0) continue;

        int pid = SAFE_READ_INT(addr26, g_off.pi_avatar_player_id);
        int pidx = MatchPlayerIndexByAvatarPid(pid);

        AvatarRankProbe probe;
        probe.entry = entry;
        probe.addr26 = addr26;
        probe.raw_rank = raw_rank;
        probe.rank = rank;
        probe.pid = pid;
        probe.matched_id = (pidx >= 0) ? g_players[pidx].id : -1;
        g_dbg_avatar_ranks.push_back(probe);

        if (pidx >= 0) g_players[pidx].avatar_rank = rank;
    }
}

void ParseGameMemory() {
    if (g_il2cppTrueBase == 0) return;
    if (!g_is_in_match.load(std::memory_order_acquire)) return;

    typedef void* (*func_get_Instance_t)(void* method_info);
    func_get_Instance_t get_Instance = (func_get_Instance_t)(g_il2cppTrueBase + (uintptr_t)g_off.func_get_Instance);
    if (!get_Instance) return;
    
    try { g_dbg_addr1 = (uintptr_t)get_Instance(nullptr); } catch(...) { g_dbg_addr1 = 0; }
    g_dbg_addr2 = SAFE_READ_PTR(g_dbg_addr1, g_off.addr2);
    g_dbg_addr3 = SAFE_READ_PTR(g_dbg_addr2, g_off.addr3); 
    g_dbg_addra = SAFE_READ_PTR(g_dbg_addr3, g_off.addra); 
    g_dbg_segmentcsogame = SAFE_READ_PTR(g_dbg_addra, g_off.segmentcsogame); 

    if (IsValidPtr(g_dbg_segmentcsogame))
        g_my_player_id = SAFE_READ_INT(g_dbg_segmentcsogame, g_off.segment_my_player_id);
    else
        g_my_player_id = 0;
    uintptr_t next_opp_addr = SAFE_READ_PTR(g_dbg_addr2, g_off.next_opponents_list);
    uintptr_t next_opp_list = SAFE_READ_PTR(next_opp_addr, 0x10);
    int next_opp_count = SAFE_READ_INT(next_opp_addr, 0x18);
    g_next_opponents = GetIntsInArray(next_opp_list, next_opp_count > 0 && next_opp_count < 16 ? next_opp_count : 8);

    g_dbg_addr4 = SAFE_READ_PTR(g_dbg_segmentcsogame, g_off.addr4);
    g_dbg_addr5 = SAFE_READ_PTR(g_dbg_addr4, g_off.addr5);
    g_dbg_addr6 = SAFE_READ_PTR(g_dbg_addr5, g_off.addr6);
    g_dbg_addr7 = SAFE_READ_PTR(g_dbg_addr6, g_off.addr7);
    
    auto list7 = GetStructArrayPointers(g_dbg_addr7, 2000, g_off.addr7_struct_size, g_off.addr7_ptr_offset);
    g_dbg_list7_addrs = list7;
    g_dbg_list9_map.clear();

    for (auto addr8 : list7) {
        uintptr_t addr9 = SAFE_READ_PTR(addr8, g_off.addr9);
        auto list9 = GetStructArrayPointers(addr9, 2000, g_off.addr9_struct_size, g_off.addr9_ptr_offset);
        g_dbg_list9_map[addr8] = list9;

        for (auto addr10 : list9) {
            if (IsValidPtr(addr10)) {
                int heroId = SAFE_READ_INT(addr10, g_off.ph_heroId);
                int remaining = SAFE_READ_INT(addr10, g_off.ph_remaining);
                int total = SAFE_READ_INT(addr10, g_off.ph_total);
                UpsertPoolHero(heroId, remaining, total, addr10);
            }
        }
    }

    g_dbg_addr11 = SAFE_READ_PTR(g_dbg_addr2, g_off.addr11);
    g_dbg_addr12 = SAFE_READ_PTR(g_dbg_addr11, g_off.addr12);
    
    g_players.clear();
    g_dbg_player_addrs.clear();
    
    if (IsValidPtr(g_dbg_addr12)) {
        int capacity = SAFE_READ_INT(g_dbg_addr12, 0x18);
        if (capacity > 0 && capacity < 200) {
            for (int offset = 0x20; offset < 0x20 + capacity * 0x20; offset += 8) {
                uintptr_t p_val = SAFE_READ_PTR(g_dbg_addr12, offset);
                if (IsValidPtr(p_val)) {
                    uintptr_t addr13 = SAFE_READ_PTR(p_val, g_off.addr13);
                    if (IsValidPtr(addr13)) {
                        uintptr_t nameStr = SAFE_READ_PTR(addr13, g_off.pi_name);
                        if (IsValidPtr(nameStr)) {
                            int len = SAFE_READ_INT(nameStr, 0x10);
                            if (len > 0 && len < 100) { 
                                if (std::find(g_dbg_player_addrs.begin(), g_dbg_player_addrs.end(), p_val) == g_dbg_player_addrs.end()) {
                                    g_dbg_player_addrs.push_back(p_val);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto val : g_dbg_player_addrs) {
        PlayerInfo pi;
        pi.val_ptr = val;
        uintptr_t addr13 = SAFE_READ_PTR(val, g_off.addr13);
        pi.addr13_ptr = addr13;
        if (g_dbg_addr13 == 0) g_dbg_addr13 = addr13;
        
        pi.name = ReadIl2CppString(SAFE_READ_PTR(addr13, g_off.pi_name));
        pi.id = SAFE_READ_INT(addr13, g_off.pi_id);
        pi.is_bot = SAFE_READ_BYTE(val, g_off.pi_is_bot) != 0;
        pi.money = SAFE_READ_INT(val, g_off.pi_money);
        pi.win_streak = SAFE_READ_INT(val, g_off.pi_win_streak);
        pi.lose_streak = SAFE_READ_INT(val, g_off.pi_lose_streak);
        pi.level = SAFE_READ_INT(val, g_off.pi_level);
        
        uintptr_t addr14 = SAFE_READ_PTR(val, g_off.addr14);
        uintptr_t addr15 = SAFE_READ_PTR(addr14, g_off.addr15);
        auto shopItems = GetPointersInArray(addr15, 5);
        for (size_t i = 0; i < shopItems.size(); i++) {
            uintptr_t addr16 = SAFE_READ_PTR(shopItems[i], g_off.addr16);
            int shop_hero_id = SAFE_READ_INT(addr16, g_off.shop_hero_id);
            pi.shop.push_back(shop_hero_id);
            
            if (shop_hero_id > 0 && pi.id == g_my_player_id && g_heroAutoBuyChecked[shop_hero_id]) {
                uintptr_t slot_addr = 0;
                if (g_shop_listen_done.load() && i < g_shop_slots.size())
                    slot_addr = g_shop_slots[i];
                else if (IsValidPtr(shopItems[i]))
                    slot_addr = shopItems[i];
                if (IsValidPtr(slot_addr)) {
                    extern int g_current_frame;
                    static std::map<uintptr_t, int> last_buy_frame;
                    if (g_current_frame - last_buy_frame[slot_addr] > 10) { 
                        last_buy_frame[slot_addr] = g_current_frame;
                        std::lock_guard<std::mutex> lock(g_Tasks.buy_mutex);
                        g_Tasks.buy_slots.push_back(slot_addr);
                    }
                }
            }
        }
        
        uintptr_t addr17 = SAFE_READ_PTR(val, g_off.addr17);
        uintptr_t addr18 = SAFE_READ_PTR(addr17, g_off.addr18);
        auto benchItems = GetPointersInArray(addr18, 10);
        for (auto b_item : benchItems) pi.bench.push_back(SAFE_READ_INT(b_item, g_off.bench_hero_id));
        
        uintptr_t addr19 = SAFE_READ_PTR(val, g_off.addr19);
        uintptr_t addr20 = SAFE_READ_PTR(addr19, g_off.addr20);
        auto boardItems = GetPointersInArray(addr20, 30);
        for (auto bd_item : boardItems) {
            BoardHero bh;
            bh.heroId = SAFE_READ_INT(bd_item, g_off.board_hero_id);
            bh.x = SAFE_READ_INT(bd_item, g_off.board_x);
            bh.y = SAFE_READ_INT(bd_item, g_off.board_y);
            pi.board.push_back(bh);
        }
        g_players.push_back(pi);
    }

    g_dbg_addr21 = SAFE_READ_PTR(g_dbg_segmentcsogame, g_off.addr21);
    g_dbg_addr22 = SAFE_READ_PTR(g_dbg_addr21, g_off.addr22);
    g_dbg_addr23 = SAFE_READ_PTR(g_dbg_addr22, g_off.addr23);
    g_dbg_list23_addrs = GetStructArrayPointers(g_dbg_addr23, 100, g_off.addr23_struct_size, g_off.addr23_ptr_offset);
    ApplyAvatarRanksFromList23();

    g_dbg_addr26 = 0;
    g_dbg_hexctrl = 0;
    if (!g_dbg_list23_addrs.empty()) {
        g_dbg_addr26 = SAFE_READ_PTR(g_dbg_list23_addrs[0], g_off.addr26);
        if (IsValidPtr(g_dbg_addr26))
            g_dbg_hexctrl = SAFE_READ_PTR(g_dbg_addr26, g_off.hexctrl);
        
        if (g_dbg_hexctrl != 0 && g_off.func_get_hex != 0) {
            static uintptr_t last_hexctrl = 0;
            static std::atomic<bool> hex_confirmed(false);
            static std::atomic<int> match_counter(0);
            static std::atomic<int> prev_q0(0), prev_q1(0), prev_q2(0);
            
            if (last_hexctrl != g_dbg_hexctrl) {
                last_hexctrl = g_dbg_hexctrl;
                hex_confirmed.store(false);
                match_counter.store(0);
                prev_q0.store(0); prev_q1.store(0); prev_q2.store(0);
            }

            if (!hex_confirmed.load()) {
                static int frame_counter = 0;
                frame_counter++;
                if (frame_counter > 120) { 
                    frame_counter = 0;
                    std::thread([=]() {
                        typedef void* (*il2cpp_domain_get_t)();
                        typedef void* (*il2cpp_thread_attach_t)(void*);
                        auto domain_get = (il2cpp_domain_get_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_domain_get");
                        auto thread_attach = (il2cpp_thread_attach_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_thread_attach");
                        if (domain_get && thread_attach) {
                            void* domain = domain_get();
                            if (domain) thread_attach(domain);
                        }

                        typedef int (*func_get_hex_t)(uintptr_t, int);
                        func_get_hex_t get_hex = (func_get_hex_t)(g_il2cppTrueBase + g_off.func_get_hex);
                        if (get_hex) {
                            try {
                                int q0 = get_hex(g_dbg_hexctrl, 0);
                                int q1 = get_hex(g_dbg_hexctrl, 1);
                                int q2 = get_hex(g_dbg_hexctrl, 2);
                                
                                if (q0 > 0 || q1 > 0 || q2 > 0) {
                                    if (q0 == prev_q0.load() && q1 == prev_q1.load() && q2 == prev_q2.load()) {
                                        match_counter++;
                                        if (match_counter.load() >= 1) { 
                                            hex_confirmed.store(true);
                                        }
                                    } else {
                                        prev_q0.store(q0); prev_q1.store(q1); prev_q2.store(q2);
                                        match_counter.store(0);
                                    }
                                }
                                g_hex_qualities[0] = q0;
                                g_hex_qualities[1] = q1;
                                g_hex_qualities[2] = q2;
                            } catch (...) {}
                        }
                    }).detach();
                }
            }
        }
    } else {
        g_dbg_addr26 = 0; g_dbg_hexctrl = 0;
        g_hex_qualities[0] = 0; g_hex_qualities[1] = 0; g_hex_qualities[2] = 0;
    }
}

struct FrostTheme {
    ImVec4 primary, primaryHover, accentGlow, orb1, orb2;
    const char* name;
};

static FrostTheme g_themes[4] = {
    { ImVec4(0.39f, 0.40f, 0.95f, 1.f), ImVec4(0.31f, 0.27f, 0.90f, 1.f), ImVec4(0.39f, 0.40f, 0.95f, 0.55f), ImVec4(0.39f, 0.40f, 0.95f, 0.22f), ImVec4(0.96f, 0.25f, 0.37f, 0.18f), (const char*)u8"冰晶" },
    { ImVec4(0.06f, 0.73f, 0.51f, 1.f), ImVec4(0.02f, 0.59f, 0.41f, 1.f), ImVec4(0.06f, 0.73f, 0.51f, 0.55f), ImVec4(0.06f, 0.73f, 0.51f, 0.22f), ImVec4(0.22f, 0.74f, 0.97f, 0.18f), (const char*)u8"翡翠" },
    { ImVec4(0.66f, 0.33f, 0.97f, 1.f), ImVec4(0.58f, 0.20f, 0.92f, 1.f), ImVec4(0.66f, 0.33f, 0.97f, 0.55f), ImVec4(0.66f, 0.33f, 0.97f, 0.22f), ImVec4(0.96f, 0.25f, 0.37f, 0.18f), (const char*)u8"幻紫" },
    { ImVec4(0.22f, 0.74f, 0.97f, 1.f), ImVec4(0.01f, 0.52f, 0.78f, 1.f), ImVec4(0.22f, 0.74f, 0.97f, 0.55f), ImVec4(0.22f, 0.74f, 0.97f, 0.22f), ImVec4(0.39f, 0.40f, 0.95f, 0.18f), (const char*)u8"霜天" },
};

static FrostTheme& UITheme() { return g_themes[g_ui_theme]; }

void ApplyFrostedTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    FrostTheme& t = UITheme();
    float sc = g_autoScale;
    s.WindowRounding = 20.0f * sc;
    s.ChildRounding = 14.0f * sc;
    s.FrameRounding = 12.0f * sc;
    s.PopupRounding = 14.0f * sc;
    s.ScrollbarRounding = 16.0f * sc;
    s.GrabRounding = 10.0f * sc;
    s.TabRounding = 10.0f * sc;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.ItemSpacing = ImVec2(12 * sc, 11 * sc);
    s.ItemInnerSpacing = ImVec2(9 * sc, 7 * sc);
    s.WindowPadding = ImVec2(20 * sc, 18 * sc);
    s.ScrollbarSize = 30.0f * sc;
    s.GrabMinSize = 24.0f * sc;
    s.Colors[ImGuiCol_Text] = ImVec4(0.96f, 0.97f, 0.99f, 1.0f);
    s.Colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.58f, 0.68f, 1.0f);
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.03f, 0.04f, 0.08f, 0.88f);
    s.Colors[ImGuiCol_ChildBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.035f);
    s.Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.06f, 0.10f, 0.96f);
    s.Colors[ImGuiCol_Border] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.28f);
    s.Colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
    s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.09f);
    s.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.13f);
    s.Colors[ImGuiCol_TitleBg] = ImVec4(0.03f, 0.04f, 0.08f, 0.92f);
    s.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.06f, 0.11f, 0.96f);
    s.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.0f, 1.0f, 1.0f, 0.16f);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.26f);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = t.primary;
    s.Colors[ImGuiCol_CheckMark] = t.primary;
    s.Colors[ImGuiCol_SliderGrab] = t.primary;
    s.Colors[ImGuiCol_SliderGrabActive] = t.primaryHover;
    s.Colors[ImGuiCol_Button] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.42f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.62f);
    s.Colors[ImGuiCol_ButtonActive] = ImVec4(t.primaryHover.x, t.primaryHover.y, t.primaryHover.z, 0.82f);
    s.Colors[ImGuiCol_Header] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.32f);
    s.Colors[ImGuiCol_HeaderHovered] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.46f);
    s.Colors[ImGuiCol_HeaderActive] = ImVec4(t.primary.x, t.primary.y, t.primary.z, 0.62f);
    s.Colors[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
}

static float SliderLabelReserveWidth(const char* label) {
    return ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemInnerSpacing.x + 6.0f * g_autoScale;
}

static bool SliderFloatFine(const char* label, float* v, float vmin, float vmax, const char* fmt = "%.1f") {
    float prev = *v;
    ImGui::PushItemWidth(-SliderLabelReserveWidth(label));
    ImGui::SliderFloat(label, v, vmin, vmax, fmt, ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemActive()) {
        float cap = (vmax - vmin) * 0.015f;
        float d = *v - prev;
        if (d > cap) *v = prev + cap;
        else if (d < -cap) *v = prev - cap;
    }
    ImGui::PopItemWidth();
    return *v != prev;
}

static bool SliderIntFine(const char* label, int* v, int vmin, int vmax) {
    int prev = *v;
    ImGui::PushItemWidth(-SliderLabelReserveWidth(label));
    ImGui::SliderInt(label, v, vmin, vmax, "%d", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemActive()) {
        int d = *v - prev;
        int cap = std::max(1, (vmax - vmin) / 40);
        if (d > cap) *v = prev + cap;
        else if (d < -cap) *v = prev - cap;
    }
    ImGui::PopItemWidth();
    return *v != prev;
}

void DrawAmbientOrbs() { }

void DrawFrostedPanel(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float alpha, float rounding) {
    dl->AddRectFilledMultiColor(mn, mx,
        IM_COL32(255, 255, 255, (int)(18 * alpha)), IM_COL32(255, 255, 255, (int)(10 * alpha)),
        IM_COL32(255, 255, 255, (int)(4 * alpha)), IM_COL32(255, 255, 255, (int)(12 * alpha)));
    dl->AddRectFilled(mn, mx, IM_COL32(8, 12, 22, (int)(255 * 0.72f * alpha)), rounding);
    FrostTheme& th = UITheme();
    dl->AddRect(mn, mx, ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.35f * alpha)), rounding, 0, 1.4f);
    dl->AddLine(ImVec2(mn.x + rounding * 0.5f, mn.y + 1.0f), ImVec2(mx.x - rounding * 0.5f, mn.y + 1.0f), IM_COL32(255, 255, 255, 48), 1.0f);
}

static void DrawSidebarIcon(ImDrawList* dl, ImVec2 c, float s, int id, ImU32 col) {
    switch (id) {
    case 0:
        dl->AddRect(ImVec2(c.x - s, c.y - s * 0.75f), ImVec2(c.x + s, c.y + s * 0.75f), col, 2.5f, 0, 1.5f);
        dl->AddLine(ImVec2(c.x - s * 0.5f, c.y - s * 0.75f), ImVec2(c.x - s * 0.5f, c.y + s * 0.75f), col, 1.2f);
        break;
    case 1:
        dl->AddRect(ImVec2(c.x - s * 0.65f, c.y - s), ImVec2(c.x + s * 0.65f, c.y + s), col, 2.0f, 0, 1.5f);
        dl->AddLine(ImVec2(c.x - s * 0.2f, c.y - s * 0.55f), ImVec2(c.x + s * 0.35f, c.y - s * 0.55f), col, 1.2f);
        break;
    case 2:
        dl->AddCircle(c, s * 0.55f, col, 16, 1.5f);
        dl->AddLine(ImVec2(c.x + s * 0.35f, c.y + s * 0.35f), ImVec2(c.x + s * 0.85f, c.y + s * 0.85f), col, 1.5f);
        break;
    default:
        dl->AddCircle(c, s * 0.55f, col, 16, 1.5f);
        for (int i = 0; i < 6; i++) {
            float a = (float)(M_PI * 2.0 * i / 6.0);
            ImVec2 p1(c.x + cosf(a) * s * 0.55f, c.y + sinf(a) * s * 0.55f);
            ImVec2 p2(c.x + cosf(a) * s * 0.85f, c.y + sinf(a) * s * 0.85f);
            dl->AddLine(p1, p2, col, 1.2f);
        }
        break;
    }
}

void DrawGlassSeparator() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    dl->AddLine(p, ImVec2(p.x + w, p.y + 1), IM_COL32(255, 255, 255, 42), 1.0f);
    ImGui::Dummy(ImVec2(w, 14 * g_autoScale));
}

void DrawSectionTitle(const char* title) {
    ImGui::TextColored(ImVec4(UITheme().primary.x, UITheme().primary.y, UITheme().primary.z, 0.95f), "%s", title);
    DrawGlassSeparator();
}

bool ThemePill(const char* name, bool active, int idx) {
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImGuiID id = win->GetID(name);
    ImVec2 pos = win->DC.CursorPos;
    ImVec2 pad(12.0f * g_autoScale, 6.0f * g_autoScale);
    ImVec2 tSz = ImGui::CalcTextSize(name);
    ImRect bb(pos, pos + tSz + pad * 2.0f);
    ImGui::ItemSize(bb);
    bool pressed = false;
    if (ImGui::ItemAdd(bb, id)) {
        bool hovered, held;
        if ((pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held))) g_ui_theme = idx;
        float& anim = g_ui_anim[20 + idx];
        anim = ImLerp(anim, (active || hovered) ? 1.0f : 0.0f, 1.0f - expf(-20.0f * ImGui::GetIO().DeltaTime));
        ImU32 bg = active ? ImGui::GetColorU32(UITheme().primary) : ImGui::GetColorU32(ImLerp(ImVec4(1,1,1,0.06f), ImVec4(1,1,1,0.12f), anim));
        win->DrawList->AddRectFilled(bb.Min, bb.Max, bg, 8.0f * g_autoScale);
        if (active) win->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(UITheme().accentGlow), 8.0f * g_autoScale, 0, 1.5f);
        win->DrawList->AddText(ImVec2(bb.Min.x + pad.x, bb.Min.y + pad.y), active ? IM_COL32(255,255,255,255) : IM_COL32(180,190,205,255), name);
    }
    return pressed;
}

void DrawThemeBar() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mn = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 42.0f * g_autoScale * g_scale;
    ImVec2 mx(mn.x + w, mn.y + h);
    DrawFrostedPanel(dl, mn, mx, 0.92f, 10.0f * g_autoScale);
    ImGui::SetCursorScreenPos(mn);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * g_autoScale, (h - ImGui::GetFontSize() - 12.0f * g_autoScale) * 0.5f));
    ImGui::BeginChild("ThemeBarInner", ImVec2(w, h), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    ImGui::TextColored(ImVec4(0.78f, 0.84f, 0.92f, 1.f), (const char*)u8"磨砂主题");
    ImGui::SameLine();
    float pillTotal = 0.0f;
    for (int i = 0; i < 4; i++) pillTotal += ImGui::CalcTextSize(g_themes[i].name).x + 24.0f * g_autoScale + 6.0f * g_autoScale;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), w - pillTotal));
    for (int i = 0; i < 4; i++) {
        if (i > 0) ImGui::SameLine(0, 6.0f * g_autoScale);
        ThemePill(g_themes[i].name, g_ui_theme == i, i);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(ImVec2(mn.x, mx.y + 8.0f * g_autoScale));
}

void DrawStatusHeader() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    FrostTheme& th = UITheme();
    ImVec2 mn = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 52.0f * g_autoScale * g_scale;
    ImVec2 mx(mn.x + w, mn.y + h);
    DrawFrostedPanel(dl, mn, mx, 1.0f, 12.0f * g_autoScale);
    float sc = g_autoScale * g_scale;
    dl->AddCircleFilled(ImVec2(mn.x + 22.0f * sc, mn.y + h * 0.5f), 5.0f * sc, ImGui::GetColorU32(th.primary));
    dl->AddCircleFilled(ImVec2(mn.x + 22.0f * sc, mn.y + h * 0.5f), 9.0f * sc, ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.35f)));
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.05f, ImVec2(mn.x + 36.0f * sc, mn.y + (h - ImGui::GetFontSize() * 1.05f) * 0.5f),
        IM_COL32(255, 255, 255, 255), (const char*)u8"金铲铲助手 Frosted Studio");
    bool online = g_is_in_match.load(std::memory_order_relaxed);
    const char* stTxt = online ? (const char*)u8"对局中" : (const char*)u8"等待连接";
    ImVec2 stSz = ImGui::CalcTextSize(stTxt);
    float badgeW = stSz.x + 28.0f * sc;
    ImVec2 bMin(mx.x - badgeW - 14.0f * sc, mn.y + (h - 26.0f * sc) * 0.5f);
    ImVec2 bMax(bMin.x + badgeW, bMin.y + 26.0f * sc);
    dl->AddRectFilled(bMin, bMax, online ? IM_COL32(52, 211, 153, 30) : IM_COL32(248, 113, 113, 30), 13.0f * sc);
    dl->AddRect(bMin, bMax, online ? IM_COL32(52, 211, 153, 90) : IM_COL32(248, 113, 113, 90), 13.0f * sc);
    dl->AddCircleFilled(ImVec2(bMin.x + 12.0f * sc, (bMin.y + bMax.y) * 0.5f), 3.5f * sc, online ? IM_COL32(52, 211, 153, 255) : IM_COL32(248, 113, 113, 255));
    dl->AddText(ImVec2(bMin.x + 20.0f * sc, bMin.y + (bMax.y - bMin.y - stSz.y) * 0.5f), online ? IM_COL32(110, 231, 183, 255) : IM_COL32(252, 165, 165, 255), stTxt);
    ImGui::Dummy(ImVec2(w, h + 10.0f * g_autoScale));
}

void DrawMenuOrb() {
    ImGuiIO& io = ImGui::GetIO();
    float r = g_orb_r * g_autoScale;
    ImVec2 center(g_orb_x, g_orb_y);
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    FrostTheme& th = UITheme();
    fg->AddCircleFilled(center, r + 3.0f, ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.25f)), 48);
    fg->AddCircleFilled(center, r, ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.88f)), 48);
    fg->AddCircle(center, r, IM_COL32(255, 255, 255, 90), 48, 2.0f);
    fg->AddCircleFilled(center, r * 0.32f, IM_COL32(255, 255, 255, 140), 24);
    ImGui::SetNextWindowPos(ImVec2(center.x - r, center.y - r), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(r * 2.0f, r * 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##JKMenuOrb", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
    static bool s_orb_dragged = false;
    if (ImGui::InvisibleButton("##orb_btn", ImVec2(r * 2.0f, r * 2.0f))) {
        if (!s_orb_dragged) g_menu_orb = false;
        s_orb_dragged = false;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        s_orb_dragged = true;
        g_orb_x += io.MouseDelta.x;
        g_orb_y += io.MouseDelta.y;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

static void RaiseCapsuleWindow(const char* name) {
    if (ImGuiWindow* w = ImGui::FindWindowByName(name))
        ImGui::BringWindowToDisplayFront(w);
}

void DrawQuitCapsule() {
    ImGuiIO& io = ImGui::GetIO();
    float sc = g_autoScale;
    const char* label = (g_quit_confirm == 0) ? (const char*)u8"退出本局" : (const char*)u8"再次点击确认";
    ImVec2 txtSz = ImGui::CalcTextSize(label);
    float padX = 22.0f * sc, padY = 12.0f * sc;
    float capW = txtSz.x + padX * 2.0f;
    float capH = txtSz.y + padY * 2.0f;
    ImGui::SetNextWindowPos(ImVec2(g_quit_x, g_quit_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(capW, capH));
    ImGuiWindowFlags capFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (g_floats_locked) capFlags |= ImGuiWindowFlags_NoMove;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##QuitCapsule", nullptr, capFlags);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mn = ImGui::GetWindowPos();
    ImVec2 mx(mn.x + capW, mn.y + capH);
    float rounding = capH * 0.5f;
    ImU32 bg = (g_quit_confirm == 0) ? IM_COL32(180, 40, 48, 210) : IM_COL32(220, 30, 38, 235);
    dl->AddRectFilled(mn, mx, bg, rounding);
    dl->AddRect(mn, mx, IM_COL32(255, 120, 120, 160), rounding, 0, 1.5f);
    dl->AddText(ImVec2(mn.x + padX, mn.y + padY), IM_COL32(255, 255, 255, 245), label);
    if (ImGui::InvisibleButton("##quit_cap", ImVec2(capW, capH))) {
        if (g_quit_confirm == 0) {
            g_quit_confirm = 1;
            g_quit_timer = 3.0f;
        } else {
            g_Tasks.trigger_quit.store(true);
            g_quit_confirm = 0;
        }
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !g_floats_locked) {
        g_quit_x += io.MouseDelta.x;
        g_quit_y += io.MouseDelta.y;
    }
    if (g_quit_confirm > 0) {
        g_quit_timer -= io.DeltaTime;
        if (g_quit_timer <= 0.0f) g_quit_confirm = 0;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    RaiseCapsuleWindow("##QuitCapsule");
}

void DrawLockCapsule() {
    ImGuiIO& io = ImGui::GetIO();
    float sc = g_autoScale;
    const char* label = g_floats_locked ? (const char*)u8"浮窗已锁定" : (const char*)u8"锁定浮窗";
    ImVec2 txtSz = ImGui::CalcTextSize(label);
    float padX = 22.0f * sc, padY = 12.0f * sc;
    float capW = txtSz.x + padX * 2.0f;
    float capH = txtSz.y + padY * 2.0f;
    ImGui::SetNextWindowPos(ImVec2(g_lock_x, g_lock_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(capW, capH));
    ImGuiWindowFlags capFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (g_floats_locked) capFlags |= ImGuiWindowFlags_NoMove;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##LockCapsule", nullptr, capFlags);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mn = ImGui::GetWindowPos();
    ImVec2 mx(mn.x + capW, mn.y + capH);
    float rounding = capH * 0.5f;
    ImU32 bg = g_floats_locked ? IM_COL32(52, 120, 200, 220) : IM_COL32(60, 70, 90, 210);
    ImU32 border = g_floats_locked ? IM_COL32(120, 180, 255, 200) : IM_COL32(160, 170, 190, 160);
    dl->AddRectFilled(mn, mx, bg, rounding);
    dl->AddRect(mn, mx, border, rounding, 0, 1.5f);
    dl->AddText(ImVec2(mn.x + padX, mn.y + padY), IM_COL32(255, 255, 255, 245), label);
    if (ImGui::InvisibleButton("##lock_cap", ImVec2(capW, capH))) {
        g_floats_locked = !g_floats_locked;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !g_floats_locked) {
        g_lock_x += io.MouseDelta.x;
        g_lock_y += io.MouseDelta.y;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    RaiseCapsuleWindow("##LockCapsule");
}

void DrawCardPoolCapsule() {
    ImGuiIO& io = ImGui::GetIO();
    float sc = g_autoScale;
    const char* label = g_win_cardpool ? (const char*)u8"\u9690\u85cf\u724c\u5e93" : (const char*)u8"\u663e\u793a\u724c\u5e93";
    ImVec2 txtSz = ImGui::CalcTextSize(label);
    float padX = 22.0f * sc, padY = 12.0f * sc;
    float capW = txtSz.x + padX * 2.0f;
    float capH = txtSz.y + padY * 2.0f;
    ImGui::SetNextWindowPos(ImVec2(g_cpbtn_x, g_cpbtn_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(capW, capH));
    ImGuiWindowFlags capFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoNav;
    if (g_floats_locked) capFlags |= ImGuiWindowFlags_NoMove;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.01f));
    ImGui::Begin("##CardPoolCapsule", nullptr, capFlags);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mn = ImGui::GetWindowPos();
    ImVec2 mx(mn.x + capW, mn.y + capH);
    float rounding = capH * 0.5f;
    ImU32 bg = g_win_cardpool ? IM_COL32(38, 155, 80, 215) : IM_COL32(70, 75, 85, 210);
    ImU32 border = g_win_cardpool ? IM_COL32(100, 220, 140, 180) : IM_COL32(140, 145, 155, 160);
    dl->AddRectFilled(mn, mx, bg, rounding);
    dl->AddRect(mn, mx, border, rounding, 0, 1.5f);
    dl->AddText(ImVec2(mn.x + padX, mn.y + padY), IM_COL32(255, 255, 255, 245), label);
    if (ImGui::InvisibleButton("##cpbtn_cap", ImVec2(capW, capH))) {
        g_win_cardpool = !g_win_cardpool;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !g_floats_locked) {
        g_cpbtn_x += io.MouseDelta.x;
        g_cpbtn_y += io.MouseDelta.y;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    RaiseCapsuleWindow("##CardPoolCapsule");
}

void DrawClickerCapsule() {
    ImGuiIO& io = ImGui::GetIO();
    float sc = g_autoScale;
    const char* label = g_auto_clicker_enable ? (const char*)u8"\u8fde\u70b9: \u5f00" : (const char*)u8"\u8fde\u70b9: \u5173";
    ImVec2 txtSz = ImGui::CalcTextSize(label);
    float padX = 22.0f * sc, padY = 12.0f * sc;
    float capW = txtSz.x + padX * 2.0f;
    float capH = txtSz.y + padY * 2.0f;
    ImGui::SetNextWindowPos(ImVec2(g_clickerbtn_x, g_clickerbtn_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(capW, capH));
    ImGuiWindowFlags capFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoNav;
    if (g_floats_locked) capFlags |= ImGuiWindowFlags_NoMove;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.01f));
    ImGui::Begin("##ClickerCapsule", nullptr, capFlags);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mn = ImGui::GetWindowPos();
    ImVec2 mx(mn.x + capW, mn.y + capH);
    float rounding = capH * 0.5f;
    ImU32 bg = g_auto_clicker_enable ? IM_COL32(200, 150, 40, 215) : IM_COL32(70, 75, 85, 210);
    ImU32 border = g_auto_clicker_enable ? IM_COL32(255, 200, 60, 180) : IM_COL32(140, 145, 155, 160);
    dl->AddRectFilled(mn, mx, bg, rounding);
    dl->AddRect(mn, mx, border, rounding, 0, 1.5f);
    dl->AddText(ImVec2(mn.x + padX, mn.y + padY), IM_COL32(255, 255, 255, 245), label);
    if (ImGui::InvisibleButton("##clickerbtn_cap", ImVec2(capW, capH))) {
        g_auto_clicker_enable = !g_auto_clicker_enable;
        g_clicker_running.store(g_auto_clicker_enable);
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !g_floats_locked) {
        g_clickerbtn_x += io.MouseDelta.x;
        g_clickerbtn_y += io.MouseDelta.y;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    RaiseCapsuleWindow("##ClickerCapsule");
}

void DrawClickerFeedback() {
    if (!g_auto_clicker_enable) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float time = (float)ImGui::GetTime();
    
    float sc = g_autoScale;
    float scale_x = 1.0f, scale_y = 1.0f;
    if (g_cached_view_width > 0 && g_gl_width > 0) scale_x = (float)g_gl_width / g_cached_view_width;
    if (g_cached_view_height > 0 && g_gl_height > 0) scale_y = (float)g_gl_height / g_cached_view_height;
    
    for (size_t pi = 0; pi < g_click_positions.size(); pi++) {
        ImVec2 center(g_click_positions[pi].x * scale_x, g_click_positions[pi].y * scale_y);
        float cl = 12.0f * sc;
        dl->AddLine(ImVec2(center.x - cl, center.y), ImVec2(center.x + cl, center.y), IM_COL32(255, 200, 50, 200), 2.0f * sc);
        dl->AddLine(ImVec2(center.x, center.y - cl), ImVec2(center.x, center.y + cl), IM_COL32(255, 200, 50, 200), 2.0f * sc);
        
        float freq = g_click_interval_ms > 0 ? (1000.0f / g_click_interval_ms) : 30.0f;
        freq = std::clamp(freq, 2.0f, 20.0f); 
        
        float pulse = fmodf(time * freq, 1.0f); 
        float radius = (8.0f + 30.0f * pulse) * sc;
        float alpha = 1.0f - pulse;
        
        dl->AddCircle(center, radius, IM_COL32(255, 200, 50, (int)(alpha * 255.0f)), 0, 2.5f * sc);
    }
}

static bool BeginContentFloatWindow(const char* id, bool* open, float* pos_x = nullptr, float* pos_y = nullptr, float alpha = 1.0f) {
    if (open && !*open) return false;
    if (pos_x && pos_y && *pos_x >= 0.0f && *pos_y >= 0.0f) {
        ImGuiCond cond = g_apply_saved_float_pos ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
        ImGui::SetNextWindowPos(ImVec2(*pos_x, *pos_y), cond);
    }
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * g_autoScale, 10.0f * g_autoScale));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(alpha, 0.1f, 1.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration;
    if (g_floats_locked)
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMouseInputs;
    bool vis = ImGui::Begin(id, open, flags);
    if (!vis) {
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
        return false;
    }
    return true;
}

static void DrawFloatScaleGrip(const char* grip_id, float* scale, float min_s, float max_s) {
    if (!scale || g_floats_locked) return;
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushID(grip_id);
    ImGuiID gid = ImGui::GetID("##scale_grip");
    ImGuiStorage* st = &win->StateStorage;

    float ball_r = 9.0f * g_autoScale;
    float pick_r = ball_r * 2.4f;
    float pick_r2 = pick_r * pick_r;
    ImRect wr = win->Rect();
    ImVec2 anchor(wr.Max.x - 3.0f * g_autoScale, wr.Max.y - 3.0f * g_autoScale);

    bool dragging = st->GetInt(gid, 0) != 0;
    if (ImGui::IsMouseClicked(0)) {
        if (ImLengthSqr(io.MousePos - anchor) <= pick_r2) {
            st->SetInt(gid, 1);
            st->SetFloat(gid + 1, *scale);
            st->SetFloat(gid + 2, io.MousePos.x);
            st->SetFloat(gid + 3, io.MousePos.y);
            dragging = true;
        }
    }

    if (dragging) {
        if (ImGui::IsMouseDown(0)) {
            float start_scale = st->GetFloat(gid + 1, *scale);
            float start_mx = st->GetFloat(gid + 2, io.MousePos.x);
            float start_my = st->GetFloat(gid + 3, io.MousePos.y);
            ImVec2 delta(io.MousePos.x - start_mx, io.MousePos.y - start_my);
            float target = std::clamp(start_scale + (delta.x + delta.y) / (280.0f * g_autoScale), min_s, max_s);
            *scale = ImLerp(*scale, target, 1.0f - expf(-22.0f * io.DeltaTime));
        } else {
            st->SetInt(gid, 0);
            dragging = false;
        }
    }

    ImVec2 center = dragging ? io.MousePos : anchor;
    bool hot = dragging || ImLengthSqr(io.MousePos - anchor) <= pick_r2;

    ImGui::SetCursorScreenPos(wr.Max - ImVec2(ball_r * 2.2f, ball_r * 2.2f));
    ImGui::Dummy(ImVec2(1.0f, 1.0f));

    ImDrawList* dl = dragging ? ImGui::GetForegroundDrawList() : win->DrawList;
    FrostTheme& th = UITheme();
    if (dragging) {
        dl->AddCircleFilled(center, ball_r + 5.0f, ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.28f)), 28);
    }
    dl->AddCircleFilled(center, ball_r + 2.0f, IM_COL32(0, 0, 0, 90), 24);
    dl->AddCircleFilled(center, ball_r, hot ? ImGui::GetColorU32(th.primary) : IM_COL32(255, 255, 255, 220), 24);
    dl->AddCircle(center, ball_r, IM_COL32(255, 255, 255, 160), 24, 1.5f);
    ImGui::PopID();
}

static void EndContentFloatWindowSimple() {
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
    ImGui::End();
}

static void EndContentFloatWindow(const char* grip_id, float* scale, float min_s = 0.5f, float max_s = 2.5f) {
    DrawFloatScaleGrip(grip_id, scale, min_s, max_s);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
    ImGui::End();
}

bool FrostSidebarBtn(const char* label, bool selected, int id) {
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImGuiID btnId = win->GetID(label);
    ImVec2 pos = win->DC.CursorPos;
    float sc = g_autoScale * g_scale;
    ImVec2 size(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 1.55f);
    ImRect bb(pos, pos + size);
    ImGui::ItemSize(bb);
    bool pressed = false;
    if (ImGui::ItemAdd(bb, btnId)) {
        bool hovered, held;
        if ((pressed = ImGui::ButtonBehavior(bb, btnId, &hovered, &held))) { }
        float& anim = g_ui_anim[id];
        anim = ImLerp(anim, (selected || hovered) ? 1.0f : 0.0f, 1.0f - expf(-16.0f * ImGui::GetIO().DeltaTime));
        FrostTheme& th = UITheme();
        ImU32 bg = selected ? ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.55f))
                            : ImGui::GetColorU32(ImLerp(ImVec4(1,1,1,0.03f), ImVec4(1,1,1,0.09f), anim));
        win->DrawList->AddRectFilled(bb.Min, bb.Max, bg, 10.0f * sc);
        if (selected) win->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(th.accentGlow), 10.0f * sc, 0, 1.5f);
        float pad = 10.0f * sc;
        ImU32 iconCol = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 190, 205, 255);
        DrawSidebarIcon(win->DrawList, ImVec2(bb.Min.x + pad + 8.0f * sc, bb.Min.y + size.y * 0.5f), 5.5f * sc, id, iconCol);
        win->DrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(bb.Min.x + pad + 22.0f * sc, bb.Min.y + (size.y - ImGui::GetFontSize()) * 0.5f),
            selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 190, 205, 255), label);
    }
    return pressed;
}

void SetupImGuiStyle() { ApplyFrostedTheme(); }

void UpdateFontHD(bool force = false) {
    ImGuiIO& io = ImGui::GetIO();
    float screenH = (io.DisplaySize.y > 100.0f) ? io.DisplaySize.y : 2400.0f; 
    g_autoScale = screenH / 1080.0f;
    float targetSize = std::clamp(20.0f * g_autoScale, 16.0f, 45.0f); 
    if (!force && std::abs(targetSize - g_current_rendered_size) < 2.0f) return;
    
    ImGui_ImplOpenGL3_DestroyDeviceObjects(); 
    io.Fonts->Clear(); 
    g_mainFont = nullptr; 
    
    ImFontConfig configMain; configMain.OversampleH = 2; configMain.OversampleV = 2; configMain.PixelSnapH = false; 
    
    float mainResFactor = 1.5f; 
    const char* fonts[] = { "/system/fonts/Miui-Regular.ttf", "/system/fonts/SysSans-Hans-Regular.ttf", "/system/fonts/NotoSansCJK-Regular.ttc", "/system/fonts/DroidSansFallback.ttf" };
    
    bool loaded = false;
    for(const char* path : fonts) {
        if (access(path, R_OK) == 0) { 
            g_mainFont = io.Fonts->AddFontFromFileTTF(path, targetSize * mainResFactor, &configMain, io.Fonts->GetGlyphRangesChineseSimplifiedCommon()); 
            if (g_mainFont) g_mainFont->Scale = 1.0f / mainResFactor;
            loaded = true; 
            break; 
        }
    }
    if(!loaded || !g_mainFont) { 
        g_mainFont = io.Fonts->AddFontDefault(); 
        if (g_mainFont) g_mainFont->Scale = 1.0f / mainResFactor; 
    }
    io.Fonts->Build(); 
    ImGui_ImplOpenGL3_CreateDeviceObjects(); 
    g_current_rendered_size = targetSize;
}

void DrawOffsetAdjuster(const char* label, uint32_t* value) {
    ImGui::PushID(label);
    ImGui::Text("%-35s", label); 
    float controls_width = (36 * 4 + 75 + 4 * 4) * g_autoScale * g_scale; 
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - controls_width);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * g_autoScale * g_scale, 0));
    if (ImGui::Button("-10", ImVec2(36 * g_autoScale * g_scale, 0))) *value -= 0x10;
    ImGui::SameLine();
    if (ImGui::Button("-1", ImVec2(36 * g_autoScale * g_scale, 0))) *value -= 0x1;
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(75 * g_autoScale * g_scale);
    int temp_val = *value;
    if (ImGui::InputInt("##val", &temp_val, 0, 0, ImGuiInputTextFlags_CharsHexadecimal)) *value = temp_val;
    
    ImGui::SameLine();
    if (ImGui::Button("+1", ImVec2(36 * g_autoScale * g_scale, 0))) *value += 0x1;
    ImGui::SameLine();
    if (ImGui::Button("+10", ImVec2(36 * g_autoScale * g_scale, 0))) *value += 0x10;
    ImGui::PopStyleVar();
    ImGui::PopID();
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
    int star = rawHeroId;
    while (star >= 10) star /= 10;
    return std::clamp(star, 1, 3);
}

static void DrawStarGlyph(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 tip[5];
    for (int i = 0; i < 5; i++) {
        float a = - (float)M_PI * 0.5f + i * (2.0f * (float)M_PI / 5.0f);
        tip[i] = ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r);
    }
    for (int i = 0; i < 5; i++) {
        ImVec2 p1 = tip[i];
        ImVec2 p2 = tip[(i + 2) % 5];
        dl->AddTriangleFilled(c, p1, p2, col);
    }
    dl->AddCircleFilled(c, r * 0.28f, col, 12);
}

static void DrawHeroStars(ImDrawList* dl, ImVec2 center, int stars, float star_r) {
    if (stars <= 0 || !dl) return;
    stars = std::clamp(stars, 1, 3);
    float gap = star_r * 2.35f;
    float x0 = center.x - (stars - 1) * gap * 0.5f;
    ImU32 glow    = ImGui::GetColorU32(IM_COL32(255, 200, 0, 90));
    ImU32 outline = ImGui::GetColorU32(IM_COL32(20, 12, 0, 240));
    ImU32 fill    = ImGui::GetColorU32(IM_COL32(255, 230, 50, 255));
    for (int i = 0; i < stars; i++) {
        ImVec2 c(x0 + i * gap, center.y);
        DrawStarGlyph(dl, c, star_r + 3.0f, glow);
        DrawStarGlyph(dl, c, star_r + 1.8f, outline);
        DrawStarGlyph(dl, c, star_r, fill);
    }
}

void BuildHeroImageIndex() {
    std::thread([]() {
        int found = 0;
        for (int i = 1; i <= 99999; i++) {
            int len = 0;
            const unsigned char* ptr = GetHeroImageBytes(i, &len);
            if (ptr != nullptr && len > 0) found++;
        }
        g_hero_image_count = found;
        g_hero_images_ready.store(true);
        LOGI("HeroImages: indexed %d hero images", found);
    }).detach();
}

void TextureDecodingWorkerThread() {
    while (true) {
        DecodeRequest req;
        bool hasReq = false;
        {
            std::lock_guard<std::mutex> lock(g_DecodeRequestMutex);
            if (!g_DecodeRequestQueue.empty()) {
                req = g_DecodeRequestQueue.front();
                g_DecodeRequestQueue.pop_front();
                hasReq = true;
            }
        }
        if (hasReq) {
            int imgLen = 0;
            const unsigned char* imgData = GetHeroImageBytes(req.id, &imgLen);
            if (imgData != nullptr && imgLen > 0) {
                int w, h, channels;
                unsigned char* data = stbi_load_from_memory(imgData, imgLen, &w, &h, &channels, 4);
                if (data && w > 0 && h > 0 && w <= 2048 && h <= 2048) {
                    std::lock_guard<std::mutex> lock(g_TexMutex);
                    g_HeroTexDecodedQueue.push_back({req.id, {w, h, data}});
                } else if (data) {
                    stbi_image_free(data);
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

void EnsureTextureWorkerStarted() {
    if (g_tex_worker_started.exchange(true)) return;
    std::thread(TextureDecodingWorkerThread).detach();
    BuildHeroImageIndex();
}

GLuint GetHeroTexture(int heroId) {
    int baseId = GetBaseHeroImageId(heroId);
    auto it = g_heroTextureCache.find(baseId);
    if (it != g_heroTextureCache.end()) return it->second;
    g_heroTextureCache[baseId] = 0;
    EnsureTextureWorkerStarted();
    std::lock_guard<std::mutex> lock(g_DecodeRequestMutex);
    g_DecodeRequestQueue.push_back({baseId});
    return 0;
}

void ProcessTextureQueue() {
    std::lock_guard<std::mutex> lock(g_TexMutex);
    if (g_HeroTexDecodedQueue.empty()) return;
    GLint last_unpack = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &last_unpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (auto& item : g_HeroTexDecodedQueue) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (tex != 0) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, item.second.w, item.second.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, item.second.pixels);
        }
        stbi_image_free(item.second.pixels);
        g_heroTextureCache[item.first] = tex;
    }
    g_HeroTexDecodedQueue.clear();
    glPixelStorei(GL_UNPACK_ALIGNMENT, last_unpack);
}

static void DrawHeroIcon(ImDrawList* dl, int heroId, ImVec2 pMin, ImVec2 pMax, float rounding, ImU32 fallbackColor) {
    int baseId = GetBaseHeroImageId(heroId);
    GLuint tex = GetHeroTexture(baseId);
    if (tex != 0) {
        dl->AddImageRounded((ImTextureID)(intptr_t)tex, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1),
            ImGui::GetColorU32(IM_COL32(255, 255, 255, 255)), rounding);
        return;
    }
    char idBuf[16];
    snprintf(idBuf, sizeof(idBuf), "%d", baseId);
    ImVec2 tSz = ImGui::CalcTextSize(idBuf);
    dl->AddText(ImVec2(pMin.x + (pMax.x - pMin.x - tSz.x) * 0.5f, pMin.y + (pMax.y - pMin.y - tSz.y) * 0.5f),
        ImGui::GetColorU32(fallbackColor), idBuf);
}

static ImVec4 CostColor(int cost) {
    if (cost == 1) return ImVec4(0.82f, 0.82f, 0.85f, 1.0f);
    if (cost == 2) return ImVec4(0.35f, 0.95f, 0.50f, 1.0f);
    if (cost == 3) return ImVec4(0.40f, 0.65f, 1.0f, 1.0f);
    if (cost == 4) return ImVec4(0.90f, 0.40f, 0.95f, 1.0f);
    return ImVec4(1.0f, 0.85f, 0.25f, 1.0f);
}

static int CalcGridRows(int count, int cols) {
    if (count <= 0) return 0;
    return (count + std::max(1, cols) - 1) / std::max(1, cols);
}

static std::map<int, int> BuildHeroCounts(const PlayerInfo& pi) {
    std::map<int, int> counts;
    auto add = [&](int id) {
        if (id <= 0) return;
        int star = GetHeroStarLevel(id);
        int cards = (star == 3) ? 9 : (star == 2) ? 3 : 1;
        counts[GetBaseHeroImageId(id)] += cards;
    };
    for (int id : pi.shop) add(id);
    for (int id : pi.bench) add(id);
    for (auto& bh : pi.board) add(bh.heroId);
    return counts;
}

static void DrawHeroCountStrip(ImDrawList* dl, const std::map<int, int>& counts, float icon_sz, float spacing) {
    ImVec2 cur = ImGui::GetCursorScreenPos();
    int n = 0;
    float num_h = ImGui::GetFontSize() + 2.0f;
    for (auto& kv : counts) {
        int hero_cost = (kv.first / 1000) % 10;
        int min_thresh = (hero_cost >= 1 && hero_cost <= 5) ? g_pd_hero_count_min[hero_cost] : 1;
        if (kv.second < min_thresh) continue;
        ImVec2 pMin(cur.x + n * (icon_sz + spacing), cur.y);
        ImVec2 pMax(pMin.x + icon_sz, pMin.y + icon_sz);
        dl->AddRectFilled(pMin, pMax, ImGui::GetColorU32(IM_COL32(255, 255, 255, 12)), 4.0f);
        DrawHeroIcon(dl, kv.first, pMin, pMax, 4.0f, IM_COL32(255, 255, 255, 240));
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", kv.second);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(pMin.x + (icon_sz - ts.x) * 0.5f, pMax.y + 1.0f), ImGui::GetColorU32(IM_COL32(220, 230, 240, 255)), buf);
        n++;
    }
    if (n > 0) ImGui::Dummy(ImVec2(n * (icon_sz + spacing) - spacing, icon_sz + num_h));
}

void DrawCardPoolWindow() {
    if (!g_win_cardpool) return;
    if (!g_is_in_match.load(std::memory_order_relaxed)) return;
    if (!BeginContentFloatWindow("##CardPoolFloat", &g_win_cardpool, &g_float_cp_x, &g_float_cp_y, g_alpha_cp)) return;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    float sc = g_autoScale * g_cp_scale;
    float box_size = g_cp_box_size * sc;
    float spacing = 0.0f;
    int cols = std::max(1, g_cp_columns);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    float font_size = ImGui::GetFontSize() * sc;
    float flash = 0.5f + 0.5f * sinf(ImGui::GetTime() * 6.0f);
    ImVec2 base = ImGui::GetCursorScreenPos();
    float y_off = 0.0f;
    float max_w = 0.0f;
    for (int cost = 1; cost <= 5; cost++) {
        if (!g_cp_show_cost[cost]) continue;
        std::vector<PoolHero> filtered;
        for (auto& ph : g_poolHeroes) if (ph.cost == cost) filtered.push_back(ph);
        if (filtered.empty()) continue;
        ImVec4 cost_color = CostColor(cost);
        int rows = CalcGridRows((int)filtered.size(), cols);
        float grid_w = cols * box_size + (cols > 1 ? (cols - 1) * spacing : 0.0f);
        float grid_h = rows * box_size + (rows > 1 ? (rows - 1) * spacing : 0.0f);
        max_w = std::max(max_w, grid_w);
        ImVec2 origin(base.x, base.y + y_off);
        dl->PushClipRect(
            ImVec2(origin.x - 2.0f, origin.y - 2.0f),
            ImVec2(origin.x + grid_w + 2.0f, origin.y + grid_h + 2.0f),
            false);
        for (size_t i = 0; i < filtered.size(); i++) {
            auto& ph = filtered[i];
            int r = (int)(i / cols), c = (int)(i % cols);
            ImVec2 pMin(origin.x + c * (box_size + spacing), origin.y + r * (box_size + spacing));
            ImVec2 pMax(pMin.x + box_size, pMin.y + box_size);
            dl->AddRectFilled(pMin, pMax, ImGui::GetColorU32(IM_COL32(255, 255, 255, 14)), 6.0f);
            dl->AddRect(pMin, pMax, ImGui::GetColorU32(cost_color), 6.0f, 0, 2.0f);
            DrawHeroIcon(dl, ph.heroId, pMin, pMax, 6.0f * sc, IM_COL32(255, 255, 255, 240));
            char count_text[16];
            snprintf(count_text, 16, "%d/%d", ph.remaining, ph.total);
            ImVec2 t_size2 = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, count_text);
            dl->AddText(font, font_size,
                ImVec2(pMin.x + (box_size - t_size2.x) * 0.5f, pMin.y + box_size - t_size2.y - 4.0f * sc),
                ImGui::GetColorU32(cost_color), count_text);
            if (g_cp_warning_enable && ph.remaining <= g_cp_warning_thres) {
                float t = ImGui::GetTime() * 5.0f;
                ImU32 warnCol = ImGui::GetColorU32(IM_COL32(
                    (int)(127.0f + 127.0f * sinf(t)),
                    (int)(127.0f + 127.0f * sinf(t + 2.094f)),
                    (int)(127.0f + 127.0f * sinf(t + 4.189f)),
                    (int)(180.0f + 75.0f * flash)));
                dl->AddRect(pMin, pMax, warnCol, 6.0f, 0, 5.0f + 2.0f * flash);
                dl->AddRect(
                    ImVec2(pMin.x - 2.0f, pMin.y - 2.0f),
                    ImVec2(pMax.x + 2.0f, pMax.y + 2.0f),
                    warnCol, 8.0f, 0, 2.5f + 1.5f * flash);
            }
        }
        dl->PopClipRect();
        y_off += grid_h;
    }
    if (max_w > 0.0f) ImGui::Dummy(ImVec2(max_w, y_off));
    ImGui::PopStyleVar(2);
    EndContentFloatWindow("cp_grip", &g_cp_scale);
}

void DrawPlayerDataWindow() {
    if (!g_win_playerdata) return;
    if (!g_is_in_match.load(std::memory_order_relaxed)) return;
    if (!BeginContentFloatWindow("##PlayerDataFloat", &g_win_playerdata, &g_float_pd_x, &g_float_pd_y, g_alpha_pd)) return;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f * g_autoScale, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * g_autoScale, 2.0f * g_autoScale));
    ImGui::SetWindowFontScale(g_autoScale * g_pd_font_size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float icon_sz = 30.0f * g_autoScale * g_pd_font_size;
    float icon_sp = 1.0f * g_autoScale;
    
    int main_next_opp_id = -1;
    for (size_t i = 0; i + 1 < g_next_opponents.size(); i += 2) {
        if (g_next_opponents[i] == g_my_player_id) { main_next_opp_id = g_next_opponents[i + 1]; break; }
        else if (g_next_opponents[i + 1] == g_my_player_id) { main_next_opp_id = g_next_opponents[i]; break; }
    }

    std::vector<PlayerInfo*> sorted;
    sorted.reserve(g_players.size());
    for (auto& pi : g_players) sorted.push_back(&pi);
    std::stable_sort(sorted.begin(), sorted.end(), [](const PlayerInfo* a, const PlayerInfo* b) {
        int ra = (a->avatar_rank >= 1 && a->avatar_rank <= 8) ? a->avatar_rank : 999;
        int rb = (b->avatar_rank >= 1 && b->avatar_rank <= 8) ? b->avatar_rank : 999;
        if (ra != rb) return ra < rb;
        return a->id < b->id;
    });
    for (PlayerInfo* pi_ptr : sorted) {
        PlayerInfo& pi = *pi_ptr;
        ImGui::PushID(pi.id);
        ImVec4 streak_col(0.75f, 0.78f, 0.82f, 1.f);
        char streak_buf[16];
        if (pi.win_streak > 0) { streak_col = ImVec4(1.f, 0.45f, 0.45f, 1.f); snprintf(streak_buf, sizeof(streak_buf), "+%d", pi.win_streak); }
        else if (pi.lose_streak > 0) { streak_col = ImVec4(0.45f, 0.78f, 1.f, 1.f); snprintf(streak_buf, sizeof(streak_buf), "-%d", pi.lose_streak); }
        else snprintf(streak_buf, sizeof(streak_buf), "0");
        std::string disp_name_storage;
        if (pi.name.empty()) disp_name_storage = "Player " + std::to_string(pi.id);
        else disp_name_storage = pi.name;
        if (pi.is_bot && pi.id != g_my_player_id) disp_name_storage += (const char*)u8" [机]";
        ImGui::TextColored(ImVec4(1.f, 0.82f, 0.28f, 1.f), "Lv.%d", pi.level); ImGui::SameLine(0, g_pd_line_spacing);
        ImGui::TextColored(ImVec4(1.f, 0.95f, 0.35f, 1.f), "$%d", pi.money); ImGui::SameLine(0, g_pd_line_spacing);
        ImGui::TextColored(streak_col, "%s", streak_buf); ImGui::SameLine(0, g_pd_line_spacing);
        {
            bool is_next_opp = (main_next_opp_id != -1 && pi.id == main_next_opp_id);
            ImVec4 name_col = (pi.id == g_my_player_id)
                ? ImVec4(0.25f, 1.f, 0.45f, 1.f)
                : (pi.is_bot ? ImVec4(0.85f, 0.88f, 0.92f, 1.f) : ImVec4(0.35f, 0.95f, 1.f, 1.f));
            ImU32 rainbow_col = 0;
            if (is_next_opp) {
                float t = ImGui::GetTime() * 4.0f;
                name_col = ImVec4(0.5f + 0.5f * sinf(t), 0.5f + 0.5f * sinf(t + 2.094f), 0.5f + 0.5f * sinf(t + 4.189f), 1.0f);
                rainbow_col = ImGui::GetColorU32(name_col);
            }
            ImVec2 np = ImGui::GetCursorScreenPos();
            ImU32 nc = ImGui::GetColorU32(name_col);
            ImU32 outline = ImGui::GetColorU32(IM_COL32(0, 0, 0, 200));
            const float o = 1.2f * g_autoScale;
            dl->AddText(ImVec2(np.x - o, np.y), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x + o, np.y), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x, np.y - o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x, np.y + o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x - o, np.y - o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x + o, np.y + o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x - o, np.y + o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x + o, np.y - o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x + 0.5f, np.y), nc, disp_name_storage.c_str());
            dl->AddText(np, nc, disp_name_storage.c_str());
            
            ImVec2 ts = ImGui::CalcTextSize(disp_name_storage.c_str());
            
            if (is_next_opp) {
                float arr_s = ImGui::GetFontSize() * 0.7f;
                ImVec2 arrow_center = np + ImVec2(ts.x + g_pd_arrow_spacing * g_autoScale + arr_s, ts.y * 0.5f);
                ImVec2 p1 = arrow_center + ImVec2(-arr_s, 0);
                ImVec2 p2 = arrow_center + ImVec2(arr_s, -arr_s * 0.8f);
                ImVec2 p3 = arrow_center + ImVec2(arr_s, arr_s * 0.8f);
                dl->AddTriangleFilled(p1, p2, p3, rainbow_col);
            }
            ImGui::Dummy(ImVec2(ts.x + g_pd_arrow_spacing * g_autoScale + ImGui::GetFontSize() * 1.4f, ts.y));
        }
        if (g_pd_hero_summary_enable) {
            auto counts = BuildHeroCounts(pi);
            if (!counts.empty()) DrawHeroCountStrip(dl, counts, icon_sz, icon_sp);
        }
        ImGui::PopID();
        if (g_pd_vert_spacing > 0.0f)
            ImGui::Dummy(ImVec2(0.0f, g_pd_vert_spacing * g_autoScale));
    }
    ImGui::PopStyleVar(2);
    EndContentFloatWindow("pd_grip", &g_pd_font_size);
}

void DrawOpponentBoardWindow() {
    if (!g_is_in_match.load(std::memory_order_relaxed)) return;
    if (!BeginContentFloatWindow("##OpponentFloat", nullptr, &g_float_opp_x, &g_float_opp_y, g_alpha_opp)) return;
    float sc = g_autoScale * g_opp_scale;
    ImGui::SetWindowFontScale(sc);
    int opp_id = -1;
    for (size_t i = 0; i + 1 < g_next_opponents.size(); i += 2) {
        if (g_next_opponents[i] == g_my_player_id) { opp_id = g_next_opponents[i + 1]; break; }
        else if (g_next_opponents[i + 1] == g_my_player_id) { opp_id = g_next_opponents[i]; break; }
    }
    PlayerInfo* opp = nullptr;
    for (auto& p : g_players) { if (p.id == opp_id) { opp = &p; break; } }
    if (!opp) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), (const char*)u8"未匹配到下场对手");
    } else {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), (const char*)u8"对战: %s", opp->name.empty() ? std::to_string(opp->id).c_str() : opp->name.c_str());
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (g_opp_show_board) {
            float R = g_opp_hex_size * sc;
            float W = sqrtf(3.0f) * R;
            float Y_SPACING = 1.5f * R;
            float row_shift_base = 1.5f * W;
            float left_pad = row_shift_base + R;
            float content_w = 7.0f * W + W * 0.5f;
            float grid_w = content_w + left_pad + R;
            float grid_h = R + 3.0f * Y_SPACING + R;
            ImVec2 cur = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(grid_w, grid_h));
            float start_x = cur.x + left_pad + content_w - W * 0.5f;
            for (int gy = 0; gy < 4; gy++) {
                for (int gx = 0; gx < 7; gx++) {
                    float cx = start_x - gx * W - row_shift_base + ((gy % 2 == 1) ? W * 0.5f : 0.0f);
                    float cy = cur.y + R + gy * Y_SPACING;
                    ImVec2 points[6];
                    for (int i = 0; i < 6; i++) {
                        float angle = (float)(M_PI / 180.0 * (60.0 * i - 30.0));
                        points[i] = ImVec2(cx + R * cosf(angle), cy + R * sinf(angle));
                    }
                    dl->AddPolyline(points, 6, ImGui::GetColorU32(ImVec4(0.45f, 0.82f, 1.f, 1.f)), ImDrawFlags_Closed, 3.0f * sc);
                    for (auto& bh : opp->board) {
                        if (bh.x == gx && bh.y == gy) {
                            dl->AddConvexPolyFilled(points, 6, ImGui::GetColorU32(ImVec4(0.2f, 0.6f, 0.8f, 0.45f)));
                            float pad = R * 0.18f;
                            DrawHeroIcon(dl, bh.heroId, ImVec2(cx - R + pad, cy - R + pad), ImVec2(cx + R - pad, cy + R - pad), R * 0.35f, IM_COL32(255, 255, 255, 240));
                            DrawHeroStars(dl, ImVec2(cx, cy + R * 0.62f), GetHeroStarLevel(bh.heroId), R * 0.28f);
                            break;
                        }
                    }
                }
            }
        }
        float sq_sz = 40.0f * sc;
        auto draw_sq_array = [&](const std::vector<int>& arr, int max_c, const char* label) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", label);
            ImVec2 cur = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(max_c * (sq_sz + 4), sq_sz + 4));
            for (size_t i = 0; i < arr.size() && i < (size_t)max_c; i++) {
                ImVec2 p_min(cur.x + i * (sq_sz + 4), cur.y);
                ImVec2 p_max(p_min.x + sq_sz, p_min.y + sq_sz);
                dl->AddRectFilled(p_min, p_max, ImGui::GetColorU32(IM_COL32(255, 255, 255, 24)), 4.0f);
                dl->AddRect(p_min, p_max, ImGui::GetColorU32(IM_COL32(220, 235, 255, 200)), 4.0f, 0, 2.5f * sc);
                if (arr[i] > 0) {
                    DrawHeroIcon(dl, arr[i], p_min, p_max, 4.0f, IM_COL32(255, 255, 255, 240));
                    DrawHeroStars(dl, ImVec2((p_min.x + p_max.x) * 0.5f, p_max.y - 5.0f * sc), GetHeroStarLevel(arr[i]), 6.5f * sc);
                }
            }
        };
        if (g_opp_show_shop) draw_sq_array(opp->shop, 5, (const char*)u8"敌方商店");
        if (g_opp_show_bench) draw_sq_array(opp->bench, 10, (const char*)u8"敌方备战区");
    }
    EndContentFloatWindow("opp_grip", &g_opp_scale);
}

void DrawMyHeroWarningWindow() {
    if (!g_win_hero_warn) return;
    if (!g_is_in_match.load(std::memory_order_relaxed)) return;

    PlayerInfo* me = nullptr;
    for (auto& p : g_players) { if (p.id == g_my_player_id) { me = &p; break; } }
    if (!me) return;

    std::map<int, int> myHeroIds; 
    for (auto& bh : me->board) {
        if (bh.heroId > 0) {
            int base = GetBaseHeroImageId(bh.heroId);
            int star = GetHeroStarLevel(bh.heroId);
            if (myHeroIds.find(base) == myHeroIds.end() || star > myHeroIds[base])
                myHeroIds[base] = star;
        }
    }
    for (int id : me->bench) {
        if (id > 0) {
            int base = GetBaseHeroImageId(id);
            int star = GetHeroStarLevel(id);
            if (myHeroIds.find(base) == myHeroIds.end() || star > myHeroIds[base])
                myHeroIds[base] = star;
        }
    }

    struct WarnHero { int baseId; int remaining; int total; int star; int cost; };
    std::vector<WarnHero> warnings;
    for (auto& kv : myHeroIds) {
        for (auto& ph : g_poolHeroes) {
            if (GetBaseHeroImageId(ph.heroId) == kv.first && ph.remaining <= g_hero_warn_thres) {
                warnings.push_back({ kv.first, ph.remaining, ph.total, kv.second, ph.cost });
                break;
            }
        }
    }
    if (warnings.empty()) return;

    if (!BeginContentFloatWindow("##HeroWarnFloat", &g_win_hero_warn, &g_float_hw_x, &g_float_hw_y, g_alpha_hero_warn)) return;
    float sc = g_autoScale * g_hero_warn_scale;
    ImGui::SetWindowFontScale(sc);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float flash = 0.5f + 0.5f * sinf(ImGui::GetTime() * 6.0f);
    float t = ImGui::GetTime() * 5.0f;

    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), (const char*)u8"\u4f59\u91cf\u9884\u8b66"); 

    float box_sz = 55.0f * sc;
    float spacing = 6.0f * sc;
    int cols = std::max(1, (int)warnings.size());
    if (cols > 5) cols = 5;
    float total_w = cols * box_sz + (cols > 1 ? (cols - 1) * spacing : 0.0f);
    int rows = ((int)warnings.size() + cols - 1) / cols;
    float total_h = rows * (box_sz + 22.0f * sc) + (rows > 1 ? (rows - 1) * spacing : 0.0f);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(total_w, total_h));

    ImFont* font = ImGui::GetFont();
    float font_size = ImGui::GetFontSize() * 0.85f;

    for (size_t i = 0; i < warnings.size(); i++) {
        auto& w = warnings[i];
        int r = (int)(i / cols), c = (int)(i % cols);
        ImVec2 pMin(origin.x + c * (box_sz + spacing), origin.y + r * (box_sz + 22.0f * sc + spacing));
        ImVec2 pMax(pMin.x + box_sz, pMin.y + box_sz);

        dl->AddRectFilled(pMin, pMax, ImGui::GetColorU32(IM_COL32(255, 255, 255, 14)), 8.0f);
        DrawHeroIcon(dl, w.baseId, pMin, pMax, 8.0f * sc, IM_COL32(255, 255, 255, 240));
        DrawHeroStars(dl, ImVec2((pMin.x + pMax.x) * 0.5f, pMax.y - 6.0f * sc), w.star, 5.5f * sc);

        ImU32 warnCol = ImGui::GetColorU32(IM_COL32(
            (int)(127.0f + 127.0f * sinf(t)),
            (int)(127.0f + 127.0f * sinf(t + 2.094f)),
            (int)(127.0f + 127.0f * sinf(t + 4.189f)),
            (int)(180.0f + 75.0f * flash)));
        dl->AddRect(pMin, pMax, warnCol, 8.0f, 0, 3.0f + 2.0f * flash);
        dl->AddRect(
            ImVec2(pMin.x - 2.0f, pMin.y - 2.0f),
            ImVec2(pMax.x + 2.0f, pMax.y + 2.0f),
            warnCol, 10.0f, 0, 1.5f + 1.0f * flash);

        char buf[16];
        snprintf(buf, sizeof(buf), "%d/%d", w.remaining, w.total);
        ImVec2 tSz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, buf);
        ImVec4 cost_col = CostColor(w.cost);
        dl->AddText(font, font_size,
            ImVec2(pMin.x + (box_sz - tSz.x) * 0.5f, pMax.y + 2.0f * sc),
            ImGui::GetColorU32(cost_col), buf);
    }

    EndContentFloatWindow("hw_grip", &g_hero_warn_scale);
}

void DrawHextechCapsule() {
    if (!g_win_hextech) return;
    if (!g_is_in_match.load(std::memory_order_relaxed)) return;
    if (!BeginContentFloatWindow("##HextechFloat", &g_win_hextech, &g_float_hex_x, &g_float_hex_y, g_alpha_hex)) return;
    ImGui::SetWindowFontScale(g_autoScale * g_hextech_scale);
    std::string txt = (const char*)u8"海克斯预测: ";
    const char* qn[] = { (const char*)u8"无", (const char*)u8"银", (const char*)u8"金", (const char*)u8"彩" };
    for (int i = 0; i < 3; i++) {
        int q = g_hex_qualities[i];
        txt += (q >= 0 && q <= 3) ? qn[q] : "?";
        if (i < 2) txt += " | ";
    }
    ImGui::TextColored(ImVec4(0.92f, 0.96f, 1.f, 1.f), "%s", txt.c_str());
    EndContentFloatWindow("hex_grip", &g_hextech_scale);
}

float g_anim[30] = {0.0f};

bool ModernToggle(const char* label, bool* v, int idx) {
    ImGuiWindow* win = ImGui::GetCurrentWindow(); const ImGuiStyle& style = ImGui::GetStyle(); ImGuiID id = win->GetID(label);
    float h = ImGui::GetFrameHeight(); float w = h * 2.2f; const ImRect bb(win->DC.CursorPos, win->DC.CursorPos + ImVec2(w + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label).x, h));
    ImGui::ItemSize(bb, style.FramePadding.y); bool pressed = false;
    if (ImGui::ItemAdd(bb, id)) { bool hovered, held; if ((pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held))) *v = !(*v); }
    g_anim[idx] = ImLerp(g_anim[idx], *v ? 1.0f : 0.0f, 1.0f - expf(-20.0f * ImGui::GetIO().DeltaTime));
    FrostTheme& th = UITheme();
    ImVec4 offCol(0.18f, 0.20f, 0.26f, 0.95f);
    ImVec4 onCol(th.primary.x, th.primary.y, th.primary.z, 0.95f);
    win->DrawList->AddRectFilled(bb.Min, bb.Min + ImVec2(w, h), ImGui::GetColorU32(ImLerp(offCol, onCol, g_anim[idx])), h*0.5f);
    win->DrawList->AddRect(bb.Min, bb.Min + ImVec2(w, h), IM_COL32(255, 255, 255, 35), h*0.5f, 0, 1.0f);
    ImVec2 hc = bb.Min + ImVec2(h*0.5f + g_anim[idx]*(w-h), h*0.5f);
    win->DrawList->AddCircleFilled(hc + ImVec2(0, 1.5f), h*0.5f - 2.5f, IM_COL32(0, 0, 0, 90)); win->DrawList->AddCircleFilled(hc, h*0.5f - 2.5f, IM_COL32_WHITE);
    ImGui::RenderText(ImVec2(bb.Min.x + w + style.ItemInnerSpacing.x, bb.Min.y + (h - ImGui::GetFontSize()) * 0.5f), label);
    return pressed;
}

void ModernTierSelector(const char* label, bool* tierArray) {
    ImGuiWindow* win = ImGui::GetCurrentWindow(); const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFrameHeight() * 2.2f + style.ItemInnerSpacing.x); ImGui::Text("%s", label); ImGui::SameLine();
    for (int i = 1; i <= 5; i++) {
        if (i > 1) ImGui::SameLine(0, 5.0f * g_autoScale);
        ImVec2 pos = win->DC.CursorPos; ImRect bb(pos, pos + ImVec2(ImGui::GetFrameHeight()*1.2f, ImGui::GetFrameHeight())); ImGui::ItemSize(bb);
        if (ImGui::ItemAdd(bb, win->GetID(tierArray + i))) { bool h, he; if (ImGui::ButtonBehavior(bb, win->GetID(tierArray+i), &h, &he)) tierArray[i] = !tierArray[i]; }
        static std::unordered_map<void*, float> anims_map; float& a = (anims_map[tierArray + i] = ImLerp(anims_map[tierArray + i], tierArray[i] ? 1.0f : 0.0f, 1.0f - expf(-20.0f * ImGui::GetIO().DeltaTime)));
        win->DrawList->AddRectFilled(bb.Min, bb.Max, IM_COL32((int)(30+70*a), (int)(35+100*a), (int)(45+50*a), 255), 4.0f * g_autoScale);
        if (a > 0.01f) win->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImVec4(UITheme().primary.x, UITheme().primary.y, UITheme().primary.z, 0.85f * a)), 4.0f*g_autoScale, 0, 1.5f*g_autoScale);
        char buf[4]; snprintf(buf, sizeof(buf), "%d", i); ImVec2 t_sz = ImGui::CalcTextSize(buf);
        win->DrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), pos + ImVec2((bb.GetWidth() - t_sz.x)*0.5f, (bb.GetHeight() - t_sz.y)*0.5f), tierArray[i] ? IM_COL32(0,0,0,255) : IM_COL32_WHITE, buf);
    }
}

void DrawMainMenu() {
    ApplyFrostedTheme();
    if (g_menu_orb) {
        DrawMenuOrb();
        return;
    }
    static int current_tab = 0;

    static bool firstMenuOpen = true;
    if (firstMenuOpen) {
        ImGui::SetNextWindowPos(ImVec2(g_menuX, g_menuY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(g_menuW, g_menuH), ImGuiCond_Always);
        ImGui::SetNextWindowCollapsed(g_menuCollapsed, ImGuiCond_Always);
        firstMenuOpen = false;
    }

    bool menu_visible = true;
    if (ImGui::Begin((const char*)u8"金铲铲助手 Frosted Studio", &menu_visible, ImGuiWindowFlags_NoSavedSettings)) {
        g_menuX = ImGui::GetWindowPos().x;
        g_menuY = ImGui::GetWindowPos().y;
        g_menuCollapsed = ImGui::IsWindowCollapsed();
        if (!menu_visible || g_menuCollapsed) {
            g_orb_x = g_menuX + 28.0f * g_autoScale;
            g_orb_y = g_menuY + 28.0f * g_autoScale;
            g_menu_orb = true;
        }
        if (!g_menuCollapsed) {
            float curW = ImGui::GetWindowSize().x, curH = ImGui::GetWindowSize().y;
            if (std::abs(curW - g_menuW) > 5.0f || std::abs(curH - g_menuH) > 5.0f) {
                g_menuW = curW; g_menuH = curH;
                g_scale = std::clamp(curW / (560.0f * g_autoScale), 0.5f, 2.5f);
            }
        }

        if (!g_menuCollapsed) {
            ImGui::SetWindowFontScale(g_scale);
            DrawStatusHeader();
            DrawGlassSeparator();

            float sidebarW = 132.0f * g_autoScale * g_scale;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::BeginChild("FrostSidebar", ImVec2(sidebarW, 0), true, ImGuiWindowFlags_NoScrollbar);
            const char* tabLabels[] = { u8"浮窗", u8"拿牌", u8"监视", u8"调试" };
            for (int i = 0; i < 4; i++) {
                if (FrostSidebarBtn(tabLabels[i], current_tab == i, i)) current_tab = i;
                ImGui::Dummy(ImVec2(0, 4.0f * g_autoScale));
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::BeginChild("FrostContent", ImVec2(0, 0), true);

            switch (current_tab) {
            case 0:
                DrawSectionTitle((const char*)u8"浮窗控制");
                if (ImGui::Button((const char*)u8"保存全部配置", ImVec2(-1, 34 * g_autoScale * g_scale))) SaveConfig();
                ModernToggle((const char*)u8"英雄牌库悬浮窗", &g_win_cardpool, 0);
                ModernToggle((const char*)u8"玩家数据悬浮窗", &g_win_playerdata, 1);
                ModernToggle((const char*)u8"海克斯预测悬浮", &g_win_hextech, 3);
                ModernToggle((const char*)u8"余量预警悬浮窗", &g_win_hero_warn, 10);
                ModernToggle((const char*)u8"锁定全部浮窗", &g_floats_locked, 9);
                DrawGlassSeparator();
                if (ImGui::TreeNode((const char*)u8"悬浮透明度")) {
                    float pct_cp = g_alpha_cp * 100.0f, pct_pd = g_alpha_pd * 100.0f;
                    float pct_opp = g_alpha_opp * 100.0f, pct_hex = g_alpha_hex * 100.0f;
                    SliderFloatFine((const char*)u8"牌库透明度", &pct_cp, 10.0f, 100.0f, "%.0f%%");
                    SliderFloatFine((const char*)u8"玩家数据透明度", &pct_pd, 10.0f, 100.0f, "%.0f%%");
                    SliderFloatFine((const char*)u8"对手透视透明度", &pct_opp, 10.0f, 100.0f, "%.0f%%");
                    SliderFloatFine((const char*)u8"海克斯透明度", &pct_hex, 10.0f, 100.0f, "%.0f%%");
                    float pct_hw = g_alpha_hero_warn * 100.0f;
                    SliderFloatFine((const char*)u8"余量预警透明度", &pct_hw, 10.0f, 100.0f, "%.0f%%");
                    g_alpha_cp = std::clamp(pct_cp / 100.0f, 0.1f, 1.0f);
                    g_alpha_pd = std::clamp(pct_pd / 100.0f, 0.1f, 1.0f);
                    g_alpha_opp = std::clamp(pct_opp / 100.0f, 0.1f, 1.0f);
                    g_alpha_hex = std::clamp(pct_hex / 100.0f, 0.1f, 1.0f);
                    g_alpha_hero_warn = std::clamp(pct_hw / 100.0f, 0.1f, 1.0f);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode((const char*)u8"牌库详细设置")) {
                    SliderIntFine((const char*)u8"排布列数", &g_cp_columns, 3, 100);
                    {
                        int total = 0;
                        for (auto& ph : g_poolHeroes) if (ph.cost >= 1 && ph.cost <= 5 && g_cp_show_cost[ph.cost]) total++;
                        int show_rows = CalcGridRows(total, g_cp_columns);
                        ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.f, 1.f), (const char*)u8"当前布局: %d列 × %d行 (共 %d 英雄)", g_cp_columns, show_rows, total);
                    }
                    ImGui::Text((const char*)u8"显示费用: "); ImGui::SameLine();
                    ModernTierSelector((const char*)u8"显示几费卡", g_cp_show_cost);
                    ImGui::NewLine();
                    ModernToggle((const char*)u8"开启余量预警", &g_cp_warning_enable, 4);
                    if (g_cp_warning_enable) SliderIntFine((const char*)u8"预警阈值", &g_cp_warning_thres, 1, 10);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode((const char*)u8"玩家数据设置")) {
                    SliderFloatFine((const char*)u8"行距", &g_pd_line_spacing, 0.0f, 40.0f);
                    SliderFloatFine((const char*)u8"竖距", &g_pd_vert_spacing, 0.0f, 40.0f);
                    SliderFloatFine((const char*)u8"\u7bad\u5934\u95f4\u8ddd", &g_pd_arrow_spacing, 0.0f, 500.0f);
                    ModernToggle((const char*)u8"显示英雄汇总", &g_pd_hero_summary_enable, 8);
                    if (g_pd_hero_summary_enable) {
                        SliderIntFine((const char*)u8"1费卡至少", &g_pd_hero_count_min[1], 1, 20);
                        SliderIntFine((const char*)u8"2费卡至少", &g_pd_hero_count_min[2], 1, 20);
                        SliderIntFine((const char*)u8"3费卡至少", &g_pd_hero_count_min[3], 1, 20);
                        SliderIntFine((const char*)u8"4费卡至少", &g_pd_hero_count_min[4], 1, 20);
                        SliderIntFine((const char*)u8"5费卡至少", &g_pd_hero_count_min[5], 1, 20);
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode((const char*)u8"对手透视设置")) {
                    ModernToggle((const char*)u8"显示敌方棋盘", &g_opp_show_board, 5);
                    ModernToggle((const char*)u8"显示敌方商店", &g_opp_show_shop, 6);
                    ModernToggle((const char*)u8"显示敌方备战席", &g_opp_show_bench, 7);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode((const char*)u8"余量预警设置")) {
                    SliderIntFine((const char*)u8"预警阈值(张)", &g_hero_warn_thres, 1, 15);
                    ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.f, 1.f), (const char*)u8"当自己英雄在牌库中剩余 \u2264 %d 张时显示预警", g_hero_warn_thres);
                    ImGui::TreePop();
                }
                break;
            case 1:
                DrawSectionTitle((const char*)u8"自动拿牌");
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), (const char*)u8"商店格子捕获进度: %zu / 5", g_shop_slots.size());
                if (g_shop_listen_done.load()) ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), (const char*)u8"已成功获取5个格子地址，就绪！");
                else ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), (const char*)u8"请在游戏内刷新一次商店以捕获...");
                DrawGlassSeparator();
                if (g_hero_images_ready.load()) {
                    if (g_hero_image_count > 0)
                        ImGui::TextColored(ImVec4(0.45f, 1.0f, 0.55f, 1.0f), (const char*)u8"已加载 %d 张英雄头像 (HeroImages.h)", g_hero_image_count);
                    else
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), (const char*)u8"HeroImages.h 中未找到有效图片，请用 generate_hero_images.ps1 重新生成");
                } else {
                    ImGui::TextColored(ImVec4(0.65f, 0.72f, 0.82f, 1.f), (const char*)u8"正在扫描 HeroImages.h ...");
                }
                ImGui::Text((const char*)u8"点击图块勾选/取消 (按费用分组):");
                {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    float box_sz = 56.0f * g_autoScale * g_scale;
                    float spacing = 8.0f * g_autoScale * g_scale;
                    int cols = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (box_sz + spacing)));
                    float time = ImGui::GetTime();
                    float flash = 0.5f + 0.5f * sinf(time * 6.0f);
                    ImU32 rgbBorder = ImGui::GetColorU32(ImVec4(sinf(time*5)*0.5f+0.5f, sinf(time*5+2)*0.5f+0.5f, sinf(time*5+4)*0.5f+0.5f, 1.0f));
                    float selBorder = 5.0f + 3.0f * flash;
                    for (int cost = 1; cost <= 5; cost++) {
                        std::vector<PoolHero> filtered;
                        for (auto& ph : g_poolHeroes) if (ph.cost == cost) filtered.push_back(ph);
                        if (filtered.empty()) continue;
                        ImGui::TextColored(CostColor(cost), (const char*)u8"%d费", cost);
                        ImVec2 cur = ImGui::GetCursorScreenPos();
                        int rows = (int)std::ceil((float)filtered.size() / cols);
                        ImGui::Dummy(ImVec2(cols * (box_sz + spacing), rows * (box_sz + spacing)));
                        for (size_t i = 0; i < filtered.size(); i++) {
                            int hid = filtered[i].heroId;
                            int r = (int)(i / cols), c = (int)(i % cols);
                            ImVec2 pMin(cur.x + c * (box_sz + spacing), cur.y + r * (box_sz + spacing));
                            ImVec2 pMax(pMin.x + box_sz, pMin.y + box_sz);
                            ImGui::SetCursorScreenPos(pMin);
                            ImGui::PushID(hid);
                            if (ImGui::InvisibleButton("##btn", ImVec2(box_sz, box_sz))) g_heroAutoBuyChecked[hid] = !g_heroAutoBuyChecked[hid];
                            ImGui::PopID();
                            dl->AddRectFilled(pMin, pMax, IM_COL32(255, 255, 255, 12), 8.0f);
                            dl->AddRectFilled(pMin, pMax, IM_COL32(12, 16, 28, 210), 8.0f);
                            if (g_heroAutoBuyChecked[hid]) {
                                dl->AddRect(pMin, pMax, rgbBorder, 8.0f, 0, selBorder);
                                dl->AddRect(
                                    ImVec2(pMin.x - 3.0f, pMin.y - 3.0f),
                                    ImVec2(pMax.x + 3.0f, pMax.y + 3.0f),
                                    rgbBorder, 10.0f, 0, 2.5f + 1.5f * flash);
                            }
                            else dl->AddRect(pMin, pMax, ImGui::GetColorU32(CostColor(cost)), 8.0f, 0, 1.5f);
                            DrawHeroIcon(dl, hid, pMin, pMax, 8.0f, IM_COL32(255, 255, 255, 240));
                        }
                        ImGui::Dummy(ImVec2(0, 6.0f * g_autoScale));
                    }
                    DrawGlassSeparator();
                    ImGui::Text((const char*)u8"已选英雄:");
                    float sel_sz = 44.0f * g_autoScale * g_scale;
                    float sel_sp = 6.0f * g_autoScale * g_scale;
                    for (int cost = 1; cost <= 5; cost++) {
                        std::vector<int> selected;
                        for (auto& ph : g_poolHeroes) {
                            if (ph.cost == cost && g_heroAutoBuyChecked[ph.heroId])
                                selected.push_back(ph.heroId);
                        }
                        if (selected.empty()) continue;
                        ImGui::TextColored(CostColor(cost), (const char*)u8"%d费", cost); ImGui::SameLine();
                        ImVec2 cur = ImGui::GetCursorScreenPos();
                        ImGui::Dummy(ImVec2(selected.size() * (sel_sz + sel_sp), sel_sz));
                        for (size_t i = 0; i < selected.size(); i++) {
                            ImVec2 pMin(cur.x + i * (sel_sz + sel_sp), cur.y);
                            ImVec2 pMax(pMin.x + sel_sz, pMin.y + sel_sz);
                            dl->AddRectFilled(pMin, pMax, IM_COL32(255, 255, 255, 14), 6.0f);
                            dl->AddRect(pMin, pMax, ImGui::GetColorU32(CostColor(cost)), 6.0f, 0, 1.5f);
                            DrawHeroIcon(dl, selected[i], pMin, pMax, 6.0f, IM_COL32(255, 255, 255, 240));
                        }
                        ImGui::NewLine();
                    }
                }
                DrawGlassSeparator();
                DrawSectionTitle((const char*)u8"\u8fde\u70b9\u5668");
                {
                    float now_time = (float)ImGui::GetTime();
                    static float last_cps_calc_time = 0.0f;
                    if (now_time - last_cps_calc_time >= 0.1f) {
                        static int last_click_count = 0;
                        int curr_click_count = g_total_clicks_executed.load(std::memory_order_relaxed);
                        float dt = now_time - last_cps_calc_time;
                        if (dt > 0.001f) {
                            g_realtime_cps = (curr_click_count - last_click_count) / dt;
                        }
                        last_click_count = curr_click_count;
                        last_cps_calc_time = now_time;

                        g_cps_history[g_cps_hist_idx] = g_realtime_cps;
                        g_cps_hist_idx = (g_cps_hist_idx + 1) % 100;
                    }

                    bool was_on = g_auto_clicker_enable;
                    ModernToggle((const char*)u8"\u5f00\u542f\u8fde\u70b9\u5668", &g_auto_clicker_enable, 11);
                    if (g_auto_clicker_enable && !was_on) g_clicker_running.store(true);
                    if (!g_auto_clicker_enable) g_clicker_running.store(false);

                    SliderFloatFine((const char*)u8"点击间隔(ms)", &g_click_interval_ms, 0.0f, 200.0f, "%.1f ms");
                    SliderFloatFine((const char*)u8"按下持续(ms)", &g_touch_duration_ms, 0.0f, 50.0f, "%.1f ms");
                    
                    if (g_click_interval_ms <= 0.0f && g_touch_duration_ms <= 0.0f) {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), (const char*)u8"模式: 零延迟极速爆破");
                    } else {
                        ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.f, 1.f), (const char*)u8"设定理论极限: %.0f 次/秒", 1000.0f / std::max(g_click_interval_ms + g_touch_duration_ms, 0.1f));
                    }

                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), (const char*)u8"实时实际速率: %.1f 次/秒 (CPS)", g_realtime_cps);
                    {
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        ImVec2 graphPos = ImGui::GetCursorScreenPos();
                        float graphW = ImGui::GetContentRegionAvail().x;
                        float graphH = 60.0f * g_autoScale * g_scale;

                        ImGui::Dummy(ImVec2(graphW, graphH));

                        dl->AddRectFilled(graphPos, ImVec2(graphPos.x + graphW, graphPos.y + graphH), IM_COL32(10, 16, 28, 220), 6.0f);
                        dl->AddRect(graphPos, ImVec2(graphPos.x + graphW, graphPos.y + graphH), IM_COL32(50, 120, 200, 100), 6.0f, 0, 1.0f);

                        for (int i = 1; i < 3; i++) {
                            float y = graphPos.y + graphH * (i / 3.0f);
                            dl->AddLine(ImVec2(graphPos.x, y), ImVec2(graphPos.x + graphW, y), IM_COL32(255, 255, 255, 12), 1.0f);
                        }

                        float max_cps = 50.0f;
                        for (int i = 0; i < 100; i++) {
                            if (g_cps_history[i] > max_cps) max_cps = g_cps_history[i];
                        }

                        for (int i = 0; i < 99; i++) {
                            int idx1 = (g_cps_hist_idx + i) % 100;
                            int idx2 = (g_cps_hist_idx + i + 1) % 100;

                            float val1 = std::clamp(g_cps_history[idx1] / max_cps, 0.0f, 1.0f);
                            float val2 = std::clamp(g_cps_history[idx2] / max_cps, 0.0f, 1.0f);

                            ImVec2 p1(graphPos.x + (i / 99.0f) * graphW, graphPos.y + graphH * (1.0f - val1 * 0.85f) - 4.0f);
                            ImVec2 p2(graphPos.x + ((i + 1) / 99.0f) * graphW, graphPos.y + graphH * (1.0f - val2 * 0.85f) - 4.0f);

                            ImVec2 poly[4] = { p1, p2, ImVec2(p2.x, graphPos.y + graphH - 2), ImVec2(p1.x, graphPos.y + graphH - 2) };
                            dl->AddConvexPolyFilled(poly, 4, IM_COL32(0, 180, 255, 25));
                            dl->AddLine(p1, p2, IM_COL32(0, 220, 255, 255), 2.0f);
                        }

                        char limit_str[32];
                        snprintf(limit_str, sizeof(limit_str), "峰值: %.0f CPS", max_cps);
                        dl->AddText(ImVec2(graphPos.x + 8, graphPos.y + 4), IM_COL32(255, 255, 255, 120), limit_str);
                    }

                    ImGui::TextColored(ImVec4(1.f, 0.85f, 0.3f, 1.f), (const char*)u8"点击位置数量: %d", (int)g_click_positions.size());
                    if (ImGui::Button((const char*)u8"+ 添加点击位置")) { g_click_positions.push_back({g_click_positions.back().x, g_click_positions.back().y}); }
                    ImGui::SameLine();
                    if (ImGui::Button((const char*)u8"- 删除最后一个")) { if (g_click_positions.size() > 1) g_click_positions.pop_back(); }
                    for (size_t i = 0; i < g_click_positions.size(); i++) {
                        char label[32]; snprintf(label, sizeof(label), "位置 %zu", i+1);
                        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.f, 1.f), "%s", label);
                        SliderFloatFine((const char*)u8"X", &g_click_positions[i].x, 0.0f, (float)g_cached_view_width > 0 ? (float)g_cached_view_width : 1440.0f);
                        SliderFloatFine((const char*)u8"Y", &g_click_positions[i].y, 0.0f, (float)g_cached_view_height > 0 ? (float)g_cached_view_height : 3200.0f);
                        if (ImGui::Button("删除", ImVec2(60 * g_autoScale, 0))) {
                            if (g_click_positions.size() > 1) g_click_positions.erase(g_click_positions.begin() + (int)i--);
                        }
                    }
                }
                break;
            case 2:
                DrawSectionTitle((const char*)u8"全量监视");
                ImGui::TextColored(UITheme().primary, (const char*)u8"【主线核心基址】");
                ImGui::Text("il2cppTrueBase: 0x%lx", g_il2cppTrueBase);
                ImGui::Text("addr1: 0x%lx | segment: 0x%lx | my_id: %d | %s", g_dbg_addr1, g_dbg_segmentcsogame,
                    g_my_player_id,
                    g_is_in_match.load() ? (const char*)u8"对局中" : (const char*)u8"未在对局");
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【牌库链全量遍历】");
                ImGui::Indent();
                for (size_t i = 0; i < g_dbg_list7_addrs.size(); i++) {
                    uintptr_t a8 = g_dbg_list7_addrs[i];
                    ImGui::Text("addr7[%zu] -> addr8: 0x%lx", i, a8);
                    if (g_dbg_list9_map.count(a8)) {
                        ImGui::Indent();
                        for (size_t j = 0; j < g_dbg_list9_map[a8].size(); j++) {
                            uintptr_t a10 = g_dbg_list9_map[a8][j];
                            if (IsValidPtr(a10)) {
                                int hId = SAFE_READ_INT(a10, g_off.ph_heroId);
                                if (hId > 0 && hId < 100000) ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "  -> ID=%d, 余=%d, 总=%d", hId, SAFE_READ_INT(a10, g_off.ph_remaining), SAFE_READ_INT(a10, g_off.ph_total));
                                else ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  -> 无效 ID:%d", hId);
                            }
                        }
                        ImGui::Unindent();
                    }
                }
                ImGui::Unindent();
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【商店格子地址 (5格)】");
                ImGui::Text((const char*)u8"捕获进度: %zu / 5", g_shop_slots.size());
                ImGui::Indent();
                for (size_t i = 0; i < g_shop_slots.size(); i++)
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.85f, 1.0f), "ShopSlot[%zu]: 0x%lx", i, g_shop_slots[i]);
                for (size_t i = g_shop_slots.size(); i < 5; i++)
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "ShopSlot[%zu]: (未捕获)", i);
                ImGui::Unindent();
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【玩家遍历 addr12】");
                ImGui::Text("发现合法玩家指针 %zu 条", g_dbg_player_addrs.size());
                ImGui::Indent();
                for (size_t i = 0; i < g_dbg_player_addrs.size(); i++) ImGui::Text("Player[%zu]: 0x%lx", i, g_dbg_player_addrs[i]);
                ImGui::Unindent();
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【addr23 头像排位】");
                ImGui::Text((const char*)u8"addr23=0x%lx | 列表条目 %zu | 读到排位 %zu | 已匹配玩家 %d",
                    g_dbg_addr23, g_dbg_list23_addrs.size(), g_dbg_avatar_ranks.size(),
                    (int)std::count_if(g_players.begin(), g_players.end(), [](const PlayerInfo& p) { return p.avatar_rank > 0; }));
                ImGui::Indent();
                for (size_t i = 0; i < g_dbg_avatar_ranks.size(); i++) {
                    const auto& ar = g_dbg_avatar_ranks[i];
                    ImGui::TextColored(ar.matched_id >= 0 ? ImVec4(0.5f, 1.0f, 0.85f, 1.0f) : ImVec4(1.0f, 0.55f, 0.45f, 1.0f),
                        "[%zu] raw=%d rank=%d pid=%d matched_id=%d", i, ar.raw_rank, ar.rank, ar.pid, ar.matched_id);
                    ImGui::Text("      entry=0x%lx addr26=0x%lx", ar.entry, ar.addr26);
                }
                if (g_dbg_list23_addrs.empty())
                    ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), (const char*)u8"  ! list23 为空：检查 addr23 / struct_size / ptr_offset");
                else if (g_dbg_avatar_ranks.empty())
                    ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), (const char*)u8"  ! 有条目但读不到 rank：检查 addr26(0x68) / pi_avatar_rank(0x2DC)");
                else if (std::none_of(g_players.begin(), g_players.end(), [](const PlayerInfo& p) { return p.avatar_rank > 0; }))
                    ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), (const char*)u8"  ! rank 已读到但未匹配玩家：检查 addr26+0x248 玩家 id 是否与列表 id 一致");
                ImGui::Unindent();
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【玩家 avatar_rank】");
                ImGui::Indent();
                for (auto& pi : g_players)
                    ImGui::Text("id=%d rank=%d name=%s val=0x%lx addr13=0x%lx", pi.id, pi.avatar_rank, pi.name.c_str(), pi.val_ptr, pi.addr13_ptr);
                ImGui::Unindent();
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【海克斯链】");
                ImGui::Text("addr26: 0x%lx | hexctrl: 0x%lx", g_dbg_addr26, g_dbg_hexctrl);
                break;
            case 3:
                DrawSectionTitle((const char*)u8"偏移调试");
                if (ImGui::Button((const char*)u8"保存全部配置", ImVec2(-1, 34 * g_autoScale * g_scale))) SaveConfig();
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【主线基础寻址与全局功能】");
                DrawOffsetAdjuster("func_get_Instance", &g_off.func_get_Instance);
                DrawOffsetAdjuster("addr2", &g_off.addr2);
                DrawOffsetAdjuster("addr3", &g_off.addr3);
                DrawOffsetAdjuster("addra", &g_off.addra);
                DrawOffsetAdjuster("segmentcsogame", &g_off.segmentcsogame);
                DrawOffsetAdjuster("segment_my_player_id", &g_off.segment_my_player_id);
                DrawOffsetAdjuster("func_quit (极速退游)", &g_off.func_quit);
                DrawOffsetAdjuster("func_set_IsGameEnd (对局状态)", &g_off.func_set_IsGameEnd);
                DrawOffsetAdjuster("my_player_id (addr2视角)", &g_off.my_player_id);
                DrawOffsetAdjuster("next_opponents_list", &g_off.next_opponents_list);
                DrawOffsetAdjuster("func_reqbuyhero (自动拿牌)", &g_off.func_reqbuyhero);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【玩家字典链 (addr11~12)】");
                DrawOffsetAdjuster("addr11", &g_off.addr11);
                DrawOffsetAdjuster("addr12 (dict)", &g_off.addr12);
                DrawOffsetAdjuster(" -> dict struct_size", &g_off.addr12_struct_size);
                DrawOffsetAdjuster(" -> dict ptr_offset", &g_off.addr12_ptr_offset);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【玩家基本属性 (addr13)】");
                DrawOffsetAdjuster("addr13 (从val偏移)", &g_off.addr13);
                DrawOffsetAdjuster("pi_name", &g_off.pi_name);
                DrawOffsetAdjuster("pi_id", &g_off.pi_id);
                DrawOffsetAdjuster("pi_is_bot", &g_off.pi_is_bot);
                DrawOffsetAdjuster("pi_money", &g_off.pi_money);
                DrawOffsetAdjuster("pi_level", &g_off.pi_level);
                DrawOffsetAdjuster("pi_win_streak", &g_off.pi_win_streak);
                DrawOffsetAdjuster("pi_lose_streak", &g_off.pi_lose_streak);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【玩家商店、备战区与场上】");
                DrawOffsetAdjuster("addr14 (商店基址)", &g_off.addr14);
                DrawOffsetAdjuster("addr15", &g_off.addr15);
                DrawOffsetAdjuster("addr16", &g_off.addr16);
                DrawOffsetAdjuster("shop_hero_id", &g_off.shop_hero_id);
                DrawOffsetAdjuster("addr17 (备战区基址)", &g_off.addr17);
                DrawOffsetAdjuster("addr18", &g_off.addr18);
                DrawOffsetAdjuster("bench_hero_id", &g_off.bench_hero_id);
                DrawOffsetAdjuster("addr19 (场上基址)", &g_off.addr19);
                DrawOffsetAdjuster("addr20", &g_off.addr20);
                DrawOffsetAdjuster("board_hero_id", &g_off.board_hero_id);
                DrawOffsetAdjuster("board_x", &g_off.board_x);
                DrawOffsetAdjuster("board_y", &g_off.board_y);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【牌库字典链 (addr4~9)】");
                DrawOffsetAdjuster("addr4", &g_off.addr4);
                DrawOffsetAdjuster("addr5", &g_off.addr5);
                DrawOffsetAdjuster("addr6", &g_off.addr6);
                DrawOffsetAdjuster("addr7 (dict 外层)", &g_off.addr7);
                DrawOffsetAdjuster(" -> addr7 struct_size", &g_off.addr7_struct_size);
                DrawOffsetAdjuster(" -> addr7 ptr_offset", &g_off.addr7_ptr_offset);
                DrawOffsetAdjuster("addr9 (dict 内层)", &g_off.addr9);
                DrawOffsetAdjuster(" -> addr9 struct_size", &g_off.addr9_struct_size);
                DrawOffsetAdjuster(" -> addr9 ptr_offset", &g_off.addr9_ptr_offset);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【牌库底层数据 (addr10)】");
                DrawOffsetAdjuster("ph_heroId (英雄ID)", &g_off.ph_heroId);
                DrawOffsetAdjuster("ph_remaining (剩余数)", &g_off.ph_remaining);
                DrawOffsetAdjuster("ph_total (总数)", &g_off.ph_total);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【海克斯链 (addr21~26)】");
                DrawOffsetAdjuster("addr21", &g_off.addr21);
                DrawOffsetAdjuster("addr22", &g_off.addr22);
                DrawOffsetAdjuster("addr23", &g_off.addr23);
                DrawOffsetAdjuster(" -> addr23 struct_size", &g_off.addr23_struct_size);
                DrawOffsetAdjuster(" -> addr23 ptr_offset", &g_off.addr23_ptr_offset);
                DrawOffsetAdjuster("addr26 (每条+偏移)", &g_off.addr26);
                DrawOffsetAdjuster("pi_avatar_rank (addr26内)", &g_off.pi_avatar_rank);
                DrawOffsetAdjuster("pi_avatar_player_id (addr26+0x248)", &g_off.pi_avatar_player_id);
                DrawOffsetAdjuster("hexctrl (addr26内)", &g_off.hexctrl);
                DrawOffsetAdjuster("func_get_hex (函数)", &g_off.func_get_hex);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【自动拿牌 (新版)】");
                DrawOffsetAdjuster("func_shop_listen (监听)", &g_off.func_shop_listen);
                DrawOffsetAdjuster("func_buy_hero_new (单参数购买)", &g_off.func_buy_hero_new);
                break;
            }

            ImGui::EndChild();
        }
    }
    ImGui::End();
}

std::atomic<bool> g_engine_rendering{false};
int g_current_frame = 0;

JavaVM* g_jvm = nullptr;
jobject g_view_obj = nullptr;
jobject g_context = nullptr;
void (*old_nativeInjectEvent)(JNIEnv*, jobject, jobject) = nullptr;

// ==============================================================
// ★ 核心变更：独立的渲染核心，提取出来供 Unity 引擎内部函数调用
// ==============================================================
void RenderImGui_Core() {
    g_current_frame++;
    if (!g_engine_rendering.load()) g_engine_rendering.store(true);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    g_gl_width = viewport[2];
    g_gl_height = viewport[3];

    if (g_gl_width <= 0 || g_gl_height <= 0) {
        g_gl_width = 1080; g_gl_height = 2400; 
    }
    
    if (!g_isImGuiInit) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        
        const char* gl_ver = (const char*)glGetString(GL_VERSION);
        const char* glsl_ver = "#version 300 es";
        if (gl_ver && strstr(gl_ver, "OpenGL ES 2.")) {
            glsl_ver = "#version 100";
        }
        ImGui_ImplOpenGL3_Init(glsl_ver);
        
        io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
        SetupImGuiStyle();
        UpdateFontHD(true);
        g_isImGuiInit = true;
        
        LOGI("[+] ImGui Initialized with %s, Res: %dx%d", glsl_ver, g_gl_width, g_gl_height);
    }
    
    if (g_needUpdateFontSafe) { UpdateFontHD(true); g_needUpdateFontSafe = false; }
    
    ImGuiIO& io = ImGui::GetIO(); 
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    io.DeltaTime = 1.0f / 60.0f; 
    
    UpdateMatchState();

    if (g_Tasks.trigger_game_end.exchange(false, std::memory_order_acquire))
        ClearGameState();

    if (g_is_in_match.load(std::memory_order_acquire) && (g_current_frame % 2 == 0))
        ParseGameMemory();

    ProcessTextureQueue();
    ImGui_ImplOpenGL3_NewFrame(); 
    ImGui::NewFrame();

    DrawMainMenu();
    DrawCardPoolWindow();
    DrawPlayerDataWindow();
    DrawOpponentBoardWindow();
    DrawMyHeroWarningWindow();
    DrawHextechCapsule();
    DrawQuitCapsule();
    DrawLockCapsule();
    DrawCardPoolCapsule();
    DrawClickerCapsule();
    DrawClickerFeedback();
    
    if (g_apply_saved_float_pos) g_apply_saved_float_pos = false;
    
    ImGui::Render();
    
    GLint last_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_fbo);
    
    // 强制绑定默认画板 0，确保画面直接输出到屏幕
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_gl_width, g_gl_height);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // 恢复游戏原本的 FBO
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo); 

    if (g_current_frame % 300 == 0) {
        LOGI("[*] Internal Render Heartbeat: Frame %d | FBO was %d | Viewport %dx%d", g_current_frame, last_fbo, g_gl_width, g_gl_height);
    }
}

// 触摸事件注入逻辑保持不变
void InjectTouchClick(JNIEnv* env, jobject view, float x, float y) {
    if (!env || !view || !old_nativeInjectEvent) return;
    static jclass SystemClock = nullptr;
    static jmethodID uptimeMillis = nullptr;
    static jclass MotionEvent = nullptr;
    static jmethodID obtainFull = nullptr;
    static jmethodID obtainBasic = nullptr;
    static jmethodID setSource = nullptr;

    if (!SystemClock) {
        jclass sc = env->FindClass("android/os/SystemClock");
        if (sc) {
            SystemClock = (jclass)env->NewGlobalRef(sc);
            uptimeMillis = env->GetStaticMethodID(SystemClock, "uptimeMillis", "()J");
            env->DeleteLocalRef(sc);
        }

        jclass me = env->FindClass("android/view/MotionEvent");
        if (me) {
            MotionEvent = (jclass)env->NewGlobalRef(me);
            obtainFull = env->GetStaticMethodID(MotionEvent, "obtain", "(JJIFFFFIFFII)Landroid/view/MotionEvent;");
            if (!obtainFull) {
                obtainBasic = env->GetStaticMethodID(MotionEvent, "obtain", "(JJIFFI)Landroid/view/MotionEvent;");
            }
            setSource = env->GetMethodID(MotionEvent, "setSource", "(I)V");
            env->DeleteLocalRef(me);
        }
    }

    if (!uptimeMillis || (!obtainFull && !obtainBasic)) return;

    long time = env->CallStaticLongMethod(SystemClock, uptimeMillis);
    
    jobject eventDown = nullptr;
    if (obtainFull) {
        eventDown = env->CallStaticObjectMethod(MotionEvent, obtainFull, time, time, 0, (jfloat)x, (jfloat)y, 1.0f, 1.0f, 0, 1.0f, 1.0f, 0, 0);
    } else {
        eventDown = env->CallStaticObjectMethod(MotionEvent, obtainBasic, time, time, 0, (jfloat)x, (jfloat)y, 0);
    }

    if (eventDown) {
        if (setSource) env->CallVoidMethod(eventDown, setSource, 4098 | 8194); 
        old_nativeInjectEvent(env, view, eventDown);
        env->DeleteLocalRef(eventDown);
    }

    if (g_touch_duration_ms > 0.0f) {
        std::this_thread::sleep_for(std::chrono::microseconds((long long)(g_touch_duration_ms * 1000.0f)));
    }
    long timeUp = env->CallStaticLongMethod(SystemClock, uptimeMillis);

    jobject eventUp = nullptr;
    if (obtainFull) {
        eventUp = env->CallStaticObjectMethod(MotionEvent, obtainFull, time, timeUp, 1, (jfloat)x, (jfloat)y, 0.0f, 1.0f, 0, 1.0f, 1.0f, 0, 0);
    } else {
        eventUp = env->CallStaticObjectMethod(MotionEvent, obtainBasic, time, timeUp, 1, (jfloat)x, (jfloat)y, 0);
    }

    if (eventUp) {
        if (setSource) env->CallVoidMethod(eventUp, setSource, 4098 | 8194);
        old_nativeInjectEvent(env, view, eventUp);
        env->DeleteLocalRef(eventUp);
    }

    g_total_clicks_executed.fetch_add(1, std::memory_order_relaxed);
}

void AutoClickerThread() {
    JNIEnv* env = nullptr;
    bool attached = false;
    while (true) {
        if (!g_clicker_running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (!g_jvm || !g_view_obj) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (!env) {
            if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
                g_jvm->AttachCurrentThread(&env, nullptr);
                attached = true;
            }
        }

        if (env && g_view_obj) {
            for (const auto& cp : g_click_positions) { InjectTouchClick(env, g_view_obj, cp.x, cp.y); }
        }

        float interval = g_click_interval_ms;
        if (interval > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds((long long)(interval * 1000.0f)));
        } else {
            std::this_thread::yield();
        }
    }
    if (attached && g_jvm) g_jvm->DetachCurrentThread();
}

extern "C" void hook_nativeInjectEvent(JNIEnv* env, jobject obj, jobject event) {
    if (!g_jvm) env->GetJavaVM(&g_jvm);
    if (obj) {
        if (!g_view_obj || !env->IsSameObject(g_view_obj, obj)) {
            if (g_view_obj) env->DeleteGlobalRef(g_view_obj);
            g_view_obj = env->NewGlobalRef(obj);
        }
        if (!g_context) {
            jclass viewClass = env->GetObjectClass(obj);
            jmethodID getContextMid = env->GetMethodID(viewClass, "getContext", "()Landroid/content/Context;");
            if (getContextMid) {
                jobject ctx = env->CallObjectMethod(obj, getContextMid);
                if (ctx) g_context = env->NewGlobalRef(ctx);
            }
            env->DeleteLocalRef(viewClass);
        }
    }
    if (event) {
        static jclass motionEventClassGlobal = nullptr;
        if (!motionEventClassGlobal) {
            jclass meClass = env->FindClass("android/view/MotionEvent");
            if (meClass) { motionEventClassGlobal = (jclass)env->NewGlobalRef(meClass); env->DeleteLocalRef(meClass); }
        }
        if (motionEventClassGlobal && env->IsInstanceOf(event, motionEventClassGlobal)) {
            static jmethodID getWidthMid = nullptr, getHeightMid = nullptr;
            static jmethodID getActionMid = nullptr, getXMid = nullptr, getYMid = nullptr;
            
            if (getWidthMid == nullptr) {
                jclass viewClass = env->GetObjectClass(obj);
                getWidthMid = env->GetMethodID(viewClass, "getWidth", "()I"); getHeightMid = env->GetMethodID(viewClass, "getHeight", "()I"); env->DeleteLocalRef(viewClass);
                getActionMid = env->GetMethodID(motionEventClassGlobal, "getAction", "()I"); getXMid = env->GetMethodID(motionEventClassGlobal, "getX", "()F"); getYMid = env->GetMethodID(motionEventClassGlobal, "getY", "()F"); 
            }
            if (getActionMid && getXMid && getYMid) {
                int action = env->CallIntMethod(event, getActionMid) & 255;
                if ((action == 0 && g_cached_view_width <= 0) || g_cached_view_width <= 0) { 
                    g_cached_view_width = env->CallIntMethod(obj, getWidthMid); 
                    g_cached_view_height = env->CallIntMethod(obj, getHeightMid); 
                }
                float raw_x = env->CallFloatMethod(event, getXMid), raw_y = env->CallFloatMethod(event, getYMid);
                float scale_x = 1.0f, scale_y = 1.0f;
                if (g_cached_view_width > 0 && g_gl_width > 0) scale_x = (float)g_gl_width / g_cached_view_width;
                if (g_cached_view_height > 0 && g_gl_height > 0) scale_y = (float)g_gl_height / g_cached_view_height;
                
                ImGuiIO& io = ImGui::GetIO(); io.AddMousePosEvent(raw_x * scale_x, raw_y * scale_y);
                if (action == 0) io.AddMouseButtonEvent(0, true); else if (action == 1) io.AddMouseButtonEvent(0, false);
                if (action == 0 && !io.WantCaptureMouse) {
                    g_click_positions.push_back({raw_x, raw_y});
                }
                if (io.WantCaptureMouse) return;
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

typedef void (*func_set_IsGameEnd_t)(void* thisObj, uint8_t isEnd);
func_set_IsGameEnd_t orig_set_IsGameEnd = nullptr;

void hook_set_IsGameEnd(void* thisObj, uint8_t isEnd) {
    if (orig_set_IsGameEnd) orig_set_IsGameEnd(thisObj, isEnd);
    if (!thisObj || !IsValidPtr((uintptr_t)thisObj)) return;
    if (isEnd == 0) {
        g_match_enter_pending.store(true, std::memory_order_release);
    } else if (g_is_in_match.load(std::memory_order_acquire)) {
        g_is_in_match.store(false, std::memory_order_release);
        g_match_enter_pending.store(false, std::memory_order_release);
        g_need_segment_gap_before_enter = true;
        g_Tasks.trigger_game_end.store(true, std::memory_order_release);
    }
}

// ==============================================================
// ★ 降维打击：在业务主循环 SendWillRenderCanvases 内直接画图
// ==============================================================
typedef void* (*func_SendWillRenderCanvases_t)();
func_SendWillRenderCanvases_t orig_SendWillRenderCanvases = nullptr;
void* hook_SendWillRenderCanvases() {
    
    // 1. 处理原本在它这儿执行的【自动拿牌逻辑】
    {
        std::lock_guard<std::mutex> lock(g_Tasks.buy_mutex);
        if (!g_Tasks.buy_slots.empty()) {
            typedef void (*func_buy_new_t)(void*);
            func_buy_new_t buy_hero = (func_buy_new_t)(g_il2cppTrueBase + g_off.func_buy_hero_new);
            if (buy_hero) {
                for (uintptr_t slot_addr : g_Tasks.buy_slots) {
                    try { buy_hero((void*)slot_addr); } catch(...) {}
                }
            }
            g_Tasks.buy_slots.clear();
        }
    }
    
    // 2. 处理【极速退游逻辑】
    if (g_Tasks.trigger_quit.load()) {
        g_Tasks.trigger_quit.store(false);
        typedef void (*func_quit_t)(uintptr_t, int, int);
        func_quit_t quit_func = (func_quit_t)(g_il2cppTrueBase + g_off.func_quit);
        if (quit_func && IsValidPtr(g_dbg_segmentcsogame)) {
            try { quit_func(g_dbg_segmentcsogame, g_my_player_id, 1); } catch(...) {}
        }
        g_is_in_match.store(false, std::memory_order_release);
        g_need_segment_gap_before_enter = true;
        g_Tasks.trigger_game_end.store(true, std::memory_order_release);
    }

    // 3. ★ 核心魔法：将原本属于 EGL 的作画行为，直接挂载到 Unity 画布渲染前夕执行！
    // 此时主线程不仅持有业务权限，还持有合法的 OpenGL Context。完美规避 Houdini EGL 隔离。
    RenderImGui_Core();

    // 4. 调用原函数，把游戏画面铺到底层去
    if (orig_SendWillRenderCanvases) return orig_SendWillRenderCanvases();
    return nullptr;
}
// ==============================================================


void* DelayedHookThread(void*) {
    int timeout = 0;
    LOGI("[+] DelayedHookThread: Waiting for ImGui Init or timeout...");
    // 放宽等待，即使渲染没起来，也尝试去 Hook 输入层
    while (!g_engine_rendering.load() && timeout < 20) { 
        sleep(1); 
        timeout++;
    }
    
    LOGI("[+] Attempting hidden JNI hook for touches...");
    FindAndHookHiddenJNI();
    LOGI("[+] DelayedHookThread: Hidden JNI hooked.");
    return nullptr;
}

void* SetupThread(void*) {
    LOGI("[+] SetupThread Started! Waiting for libil2cpp.so...");
    int retry_count = 0;
    while (g_il2cppTrueBase == 0 && retry_count < 60) {
        FILE *fp = fopen("/proc/self/maps", "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "libil2cpp.so")) {
                    sscanf(line, "%lx", &g_il2cppTrueBase); 
                    break;
                }
            }
            fclose(fp);
        }
        if (g_il2cppTrueBase == 0) {
            sleep(1);
            retry_count++;
        }
    }
    
    if (g_il2cppTrueBase == 0) {
        LOGI("[-] SetupThread abort: libil2cpp.so not found after 60s.");
        return nullptr; 
    }
    
    LOGI("[+] Found libil2cpp.so Base: 0x%lx", g_il2cppTrueBase);
    
    LoadConfig();
    EnsureTextureWorkerStarted();
    
    LOGI("[+] Hooking Game Logic (shop_listen & IsGameEnd)...");
    if (g_off.func_shop_listen != 0) {
        DobbyHook((void*)(g_il2cppTrueBase + g_off.func_shop_listen), (void*)hook_shop_listen, (void**)&old_shop_listen);
    }
    if (g_off.func_set_IsGameEnd != 0) {
        DobbyHook((void*)(g_il2cppTrueBase + g_off.func_set_IsGameEnd), (void*)hook_set_IsGameEnd, (void**)&orig_set_IsGameEnd);
    }
    
    // 【修改点】：直接通过 IL2CPP 反射寻找 Unity 的内部渲染核心 SendWillRenderCanvases
    // 因为这属于纯业务层，绝不会受到跨架构 Houdini 的干扰！
    LOGI("[+] Starting hook for Unity Canvases (Internal Render)...");
    
    typedef void* (*il2cpp_domain_get_t)();
    typedef void* (*il2cpp_domain_assembly_open_t)(void*, const char*);
    typedef void* (*il2cpp_assembly_get_image_t)(void*);
    typedef void* (*il2cpp_class_from_name_t)(void*, const char*, const char*);
    typedef void* (*il2cpp_class_get_method_from_name_t)(void*, const char*, int);

    auto domain_get = (il2cpp_domain_get_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_domain_get");
    auto assembly_open = (il2cpp_domain_assembly_open_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_domain_assembly_open");
    auto assembly_get_image = (il2cpp_assembly_get_image_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_assembly_get_image");
    auto class_from_name = (il2cpp_class_from_name_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_class_from_name");
    auto class_get_method_from_name = (il2cpp_class_get_method_from_name_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_class_get_method_from_name");

    if (domain_get && assembly_open && assembly_get_image && class_from_name && class_get_method_from_name) {
        void* domain = domain_get();
        if (domain) {
            void* assembly = assembly_open(domain, "UnityEngine.UIModule.dll");
            if (assembly) {
                void* image = assembly_get_image(assembly);
                if (image) {
                    void* klass = class_from_name(image, "UnityEngine", "Canvas");
                    if (klass) {
                        void* method = class_get_method_from_name(klass, "SendWillRenderCanvases", 0);
                        if (method) {
                            void* method_ptr = *(void**)method;
                            if (method_ptr) {
                                DobbyHook(method_ptr, (void*)hook_SendWillRenderCanvases, (void**)&orig_SendWillRenderCanvases);
                                LOGI("[+] SUCCESS! SendWillRenderCanvases hooked as Internal Render Engine!");
                            }
                        }
                    }
                }
            }
        }
    } else {
        LOGI("[-] Missing il2cpp exports for Canvases hook.");
    }

    pthread_t t;
    pthread_create(&t, 0, DelayedHookThread, 0);
    pthread_detach(t);
    LOGI("[+] SetupThread Finished. EGL hooking completely bypassed.");
    return nullptr;
}

__attribute__((constructor)) void Init() { 
    LOGI("[+] libMyMenu.so Loaded! Init Constructor Called.");
    pthread_t t; 
    pthread_create(&t, 0, SetupThread, 0); 
    pthread_detach(t); 
}
