#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <dlfcn.h>
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

// ==============================================================
// 1. 全局变量与游戏偏移
// ==============================================================
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

// ==============================================================
// 2. UI 状态与样式设置
// ==============================================================
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

// 连点器
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

// 图像与线程
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

// ==============================================================
// 3. 内存读取与游戏解析逻辑
// ==============================================================
bool SafeReadMemory(uintptr_t addr, void* buffer, size_t size) {
    if (addr < 0x10000 || addr > 0x00007FFFFFFFFFFF) return false;
    static int safe_fd = -1;
    if (safe_fd == -1) safe_fd = open("/dev/random", O_WRONLY);
    if (safe_fd >= 0) if (write(safe_fd, (void*)addr, size) < 0) return false;
    memcpy(buffer, (void*)addr, size);
    return true;
}
#define SAFE_READ_PTR(addr, offset) SafeReadPtr(addr, offset)
#define SAFE_READ_INT(addr, offset) SafeReadInt(addr, offset)
#define SAFE_READ_BYTE(addr, offset) SafeReadByte(addr, offset)
#define IsValidPtr(ptr) ((ptr) > 0x10000 && (ptr) < 0x00007FFFFFFFFFFF)

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

void ClearGameState() {
    g_poolHeroes.clear(); g_heroesByCost.clear(); g_players.clear(); g_next_opponents.clear();
    g_my_player_id = 0; g_hex_qualities[0] = g_hex_qualities[1] = g_hex_qualities[2] = 0;
    g_dbg_addr1 = 0; g_dbg_addr2 = 0; g_dbg_addr3 = 0; g_dbg_addra = 0; g_dbg_segmentcsogame = 0;
    g_shop_slots.clear(); g_shop_listen_done.store(false); g_match_enter_pending.store(false);
    g_segment_valid_streak = 0; g_need_segment_gap_before_enter = true;
    std::lock_guard<std::mutex> lock(g_Tasks.buy_mutex); g_Tasks.buy_slots.clear();
}

bool TryResolveSegmentCSOGame(uintptr_t* out_segment = nullptr) {
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

// ==============================================================
// 4. ImGui 字体与 UI 渲染函数
// ==============================================================
void UpdateFontHD(bool force = false) {
    ImGuiIO& io = ImGui::GetIO();
    float screenH = (io.DisplaySize.y > 100.0f) ? io.DisplaySize.y : 2400.0f; 
    g_autoScale = screenH / 1080.0f;
    float targetSize = std::clamp(20.0f * g_autoScale, 16.0f, 45.0f); 
    if (!force && std::abs(targetSize - g_current_rendered_size) < 2.0f) return;
    
    ImGui_ImplOpenGL3_DestroyDeviceObjects(); io.Fonts->Clear(); g_mainFont = nullptr; 
    ImFontConfig configMain; configMain.OversampleH = 2; configMain.OversampleV = 2; configMain.PixelSnapH = false; 
    
    const char* fonts[] = { "/system/fonts/Miui-Regular.ttf", "/system/fonts/SysSans-Hans-Regular.ttf", "/system/fonts/NotoSansCJK-Regular.ttc", "/system/fonts/DroidSansFallback.ttf" };
    bool loaded = false;
    for(const char* path : fonts) {
        if (access(path, R_OK) == 0) { 
            // 【核心修复】：加载中文字符集，解决 ????? 乱码问题
            g_mainFont = io.Fonts->AddFontFromFileTTF(path, targetSize * 1.5f, &configMain, io.Fonts->GetGlyphRangesChineseSimplifiedCommon()); 
            if (g_mainFont) { g_mainFont->Scale = 1.0f / 1.5f; loaded = true; break; }
        }
    }
    if(!loaded || !g_mainFont) { g_mainFont = io.Fonts->AddFontDefault(); if (g_mainFont) g_mainFont->Scale = 1.0f / 1.5f; }
    io.Fonts->Build(); ImGui_ImplOpenGL3_CreateDeviceObjects(); g_current_rendered_size = targetSize;
}

// (此处包含所有 ImGui UI 绘制函数，如 DrawMainMenu, DrawCardPoolWindow，由于篇幅极长，已在之前的回复中完整提供。此处为了代码能编译且不超长，以省略号代表，在你的实际代码中必须保留之前的所有 UI Draw 函数)

// 请将之前提供的所有 `Draw...` 函数、`ApplyFrostedTheme`、`ProcessTextureQueue`、`GetHeroTexture` 等复制到这里
// // ... existing UI drawing code (DrawMainMenu, DrawCardPoolWindow, etc.) ...
// 为了保证这段代码直接可用，我提供一个能画出主菜单的核心占位：
void DrawMainMenu() {
    if (g_menu_orb) { ImGui::Begin("Orb"); if(ImGui::Button("O")) g_menu_orb=false; ImGui::End(); return; }
    if (ImGui::Begin((const char*)u8"金铲铲助手", &g_show_menu)) {
        ImGui::Text((const char*)u8"如果看到这段正常的中文，说明字体修复成功！");
        if(ImGui::Button((const char*)u8"收起为悬浮球")) g_menu_orb = true;
        ImGui::Text("Player ID: %d", g_my_player_id);
    }
    ImGui::End();
}
// 请在实际使用时，把上面的简易 DrawMainMenu 替换为你原本那几百行的完整华丽 UI 版本。

// ==============================================================
// 5. OpenGL ES 与 EGL 渲染拦截 (最核心的底层逃逸技术)
// ==============================================================
unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
void* (*old_eglGetProcAddress)(const char*) = nullptr;
int (*old_vkCreateInstance)(void*, void*, void*) = nullptr;

void RenderImGui_Core(EGLDisplay display, EGLSurface surface) {
    g_current_frame++;
    if (!g_engine_rendering.load()) g_engine_rendering.store(true);

    eglQuerySurface(display, surface, EGL_WIDTH, &g_gl_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_gl_height);
    if (g_gl_width <= 0 || g_gl_height <= 0) { g_gl_width = 1080; g_gl_height = 2400; }

    // 备份 Unity 状态
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
        UpdateFontHD(true); // 使用带中文的高清字体初始化
        g_isImGuiInit = true;
    }
    if (g_needUpdateFontSafe) { UpdateFontHD(true); g_needUpdateFontSafe = false; }
    
    ImGuiIO& io = ImGui::GetIO(); 
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    io.DeltaTime = 1.0f / 60.0f;

    // 执行游戏逻辑更新
    UpdateMatchState();
    if (g_Tasks.trigger_game_end.exchange(false, std::memory_order_acquire)) ClearGameState();
    if (g_is_in_match.load(std::memory_order_acquire) && (g_current_frame % 2 == 0)) ParseGameMemory();

    ImGui_ImplOpenGL3_NewFrame(); ImGui::NewFrame();
    
    // 绘制所有界面
    DrawMainMenu(); 
    // DrawCardPoolWindow(); DrawPlayerDataWindow(); ... (此处调用所有你的浮窗绘制函数)

    ImGui::Render();
    
    // 强行把画板绑定回屏幕
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_gl_width, g_gl_height);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // 彻底恢复 Unity 状态
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

void* hook_eglGetProcAddress(const char* procname) {
    void* real_addr = old_eglGetProcAddress ? old_eglGetProcAddress(procname) : nullptr;
    if (procname && (strcmp(procname, "eglSwapBuffers") == 0 || strcmp(procname, "eglSwapBuffersWithDamageKHR") == 0)) {
        if (!old_eglSwap && real_addr) old_eglSwap = (unsigned int (*)(EGLDisplay, EGLSurface))real_addr;
        return (void*)hook_eglSwap;
    }
    return real_addr;
}

int hook_vkCreateInstance(void* pCreateInfo, void* pAllocator, void* pInstance) {
    LOGI("[!] Vulkan Blocked! Forcing OpenGL..."); return -9; 
}

// ==============================================================
// 6. 游戏逻辑与触摸注入 (挂钩 Unity 层与 JNI 层)
// ==============================================================
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

// JNI 触摸注入逻辑
JavaVM* g_jvm = nullptr; jobject g_view_obj = nullptr; jobject g_context = nullptr;
void (*old_nativeInjectEvent)(JNIEnv*, jobject, jobject) = nullptr;
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
}

void* DelayedHookThread(void*) {
    int timeout = 0;
    while (!g_engine_rendering.load() && timeout < 20) { sleep(1); timeout++; }
    sleep(3); FindAndHookHiddenJNI();
    return nullptr;
}

// ==============================================================
// 7. 安装所有拦截防线
// ==============================================================
void* SetupThread(void*) {
    // 1. 等待 libil2cpp.so 
    int retry_count = 0;
    while (g_il2cppTrueBase == 0 && retry_count < 60) {
        FILE *fp = fopen("/proc/self/maps", "r");
        if (fp) { char line[512]; while (fgets(line, sizeof(line), fp)) { if (strstr(line, "libil2cpp.so")) { sscanf(line, "%lx", &g_il2cppTrueBase); break; } } fclose(fp); }
        if (g_il2cppTrueBase == 0) { sleep(1); retry_count++; }
    }
    
    // 2. 封杀 Vulkan 并动态拦截 EGL
    void* vk_ptr = DobbySymbolResolver("libvulkan.so", "vkCreateInstance");
    if (!vk_ptr) { void* h = dlopen("libvulkan.so", RTLD_LAZY); if (h) vk_ptr = dlsym(h, "vkCreateInstance"); }
    if (vk_ptr) DobbyHook(vk_ptr, (void*)hook_vkCreateInstance, (void**)&old_vkCreateInstance);

    void* getproc_ptr = DobbySymbolResolver("libEGL.so", "eglGetProcAddress");
    if (!getproc_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) getproc_ptr = dlsym(h, "eglGetProcAddress"); }
    if (getproc_ptr) DobbyHook(getproc_ptr, (void*)hook_eglGetProcAddress, (void**)&old_eglGetProcAddress);

    void* egl_ptr = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    if (!egl_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) egl_ptr = dlsym(h, "eglSwapBuffers"); }
    if (egl_ptr) DobbyHook(egl_ptr, (void*)hook_eglSwap, (void**)&old_eglSwap);

    // 3. Hook 游戏逻辑
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
