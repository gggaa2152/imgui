#include <sys/syscall.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
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

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"
#include "HeroImages.h"

#define IMGUI_DEFINE_MATH_OPERATORS 
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include <dirent.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>    
#include <vulkan/vulkan.h>
#include <jni.h>
#include "dobby.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JKInternal", __VA_ARGS__)

uintptr_t g_il2cppTrueBase = 0;
bool g_show_menu = true;

// 采用了你最新调试出的精准偏移
struct Offsets {
    // 主线寻址链
    uint32_t func_get_Instance = 0x9339D64;
    uint32_t addr2 = 0x10;
    uint32_t addr3 = 0x20;
    uint32_t addra = 0x10;
    uint32_t segmentcsogame = 0x20;
    
    uint32_t func_quit = 0x8292D94;
    uint32_t segment_my_player_id = 0x10C;
    uint32_t next_opponents_list = 0x248;
    uint32_t func_shop_listen = 0xA63FC44;
    uint32_t func_buy_hero_new = 0xA644B48;
    uint32_t func_set_IsGameEnd = 0x8EE7564;
    uint32_t func_SendWillRenderCanvases = 0x79BAD18;
    
    // 牌库字典链 (addr4~9)
    uint32_t addr4 = 0x250;
    uint32_t addr5 = 0x18;
    uint32_t addr6 = 0x18;
    uint32_t addr7 = 0x10;
    
    uint32_t addr7_struct_size = 0x20; 
    uint32_t addr7_ptr_offset = 0x10;
    
    uint32_t addr9 = 0x10;
    
    uint32_t addr9_struct_size = 0x20;
    uint32_t addr9_ptr_offset = 0x10;
    
    // 牌库底部 (addr10)
    uint32_t ph_heroId = 0x10;
    uint32_t ph_remaining = 0x1c; 
    uint32_t ph_total = 0x20;     
    
    // 字典 (addr11~12)
    uint32_t addr11 = 0x38;
    uint32_t addr12 = 0x18;
    
    uint32_t addr12_struct_size = 0x08;
    uint32_t addr12_ptr_offset = 0x08;
    
    // 玩家 (addr13)
    uint32_t addr13 = 0x50;
    uint32_t pi_name = 0x18;
    uint32_t pi_id = 0x20;
    uint32_t pi_is_bot = 0x58;
    uint32_t pi_money = 0x5c;
    uint32_t pi_win_streak = 0xdc;
    uint32_t pi_lose_streak = 0xe0;
    uint32_t pi_level = 0x100;
    
    // 商店、备战与场上
    uint32_t addr14 = 0x118;
    uint32_t addr15 = 0x10;
    uint32_t addr16 = 0x10;
    uint32_t shop_hero_id = 0x14; 
    
    uint32_t addr17 = 0x398;
    uint32_t addr18 = 0x10;
    uint32_t bench_hero_id = 0x108; 
    
    uint32_t addr19 = 0x3A0;
    uint32_t addr20 = 0x10; 
    uint32_t board_hero_id = 0x118; 
    uint32_t board_x = 0x3C;
    uint32_t board_y = 0x40;
    
    // 海克斯与排位
    uint32_t addr21 = 0x148;
    uint32_t addr22 = 0x18;
    uint32_t addr23 = 0x10;
    uint32_t addr23_struct_size = 0x20;
    uint32_t addr23_ptr_offset = 0x10;
    uint32_t addr26 = 0x68;              
    uint32_t pi_avatar_rank = 0x2DC;     
    uint32_t pi_avatar_player_id = 0x248; 
    uint32_t hexctrl = 0x60;
    uint32_t func_get_hex = 0x8BBA7FC;
};

Offsets g_off;

uintptr_t g_dbg_addr1 = 0, g_dbg_addr2 = 0, g_dbg_addr3 = 0, g_dbg_addra = 0, g_dbg_segmentcsogame = 0;
uintptr_t g_dbg_addr4 = 0, g_dbg_addr5 = 0, g_dbg_addr6 = 0, g_dbg_addr7 = 0, g_dbg_addr9 = 0;
uintptr_t g_dbg_addr11 = 0, g_dbg_addr12 = 0, g_dbg_addr13 = 0;
uintptr_t g_dbg_addr21 = 0, g_dbg_addr22 = 0, g_dbg_addr23 = 0;
uintptr_t g_dbg_addr14 = 0, g_dbg_addr15 = 0, g_dbg_addr16 = 0;
uintptr_t g_dbg_addr17 = 0, g_dbg_addr18 = 0;
uintptr_t g_dbg_addr19 = 0, g_dbg_addr20 = 0;
bool g_dbg_shop_ok = false;
bool g_dbg_bench_ok = false;
bool g_dbg_board_ok = false;
bool g_dbg_board_pos_ok = false;
std::vector<uintptr_t> g_dbg_list23_addrs;
struct AvatarRankProbe { uintptr_t entry = 0, addr26 = 0; int raw_rank = 0, rank = 0, pid = 0, matched_id = -1; };
std::vector<AvatarRankProbe> g_dbg_avatar_ranks;
uintptr_t g_dbg_addr26 = 0;
uintptr_t g_dbg_hexctrl = 0;

std::vector<uintptr_t> g_dbg_list7_addrs;
std::map<uintptr_t, std::vector<uintptr_t>> g_dbg_list9_map;
std::vector<uintptr_t> g_dbg_player_addrs;

int g_my_player_id = -1;
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
void AddActionLog(const char* format, ...);

static float g_realtime_fps = 60.0f;
static float g_fps_frametime_ms = 16.6f;

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
static float g_custom_font_scale = 0.85f;
int g_ui_theme = 0;
float g_ui_anim[32] = {0};
bool g_menu_orb = false;
float g_orb_x = 120.0f, g_orb_y = 120.0f;
float g_orb_r = 34.0f;

// Floating Window Toggles & Settings
bool g_win_cardpool = true;
bool g_win_playerdata = true;
bool g_win_hextech = true;
bool g_win_path_trace = true;
float g_path_trace_scale = 1.0f;
float g_alpha_pt = 1.0f;
float g_float_pt_x = -1.0f, g_float_pt_y = -1.0f;
// Per-float opacity (1 = opaque)
float g_alpha_cp = 1.0f;
float g_alpha_pd = 1.0f;
float g_alpha_opp = 1.0f;
float g_alpha_hex = 1.0f;

// Card Pool View Settings
int g_cp_columns = 6;
int g_cp_rows = 0; // 0 = auto (no row cap)
float g_cp_box_size = 65.0f;
float g_cp_scale = 1.0f;
bool g_cp_show_cost[6] = { false, true, true, true, true, true }; // index 1-5
bool g_cp_warning_enable = true;
int g_cp_warning_thres = 3;

// Player Data View Settings
float g_pd_line_spacing = 0.0f;
float g_pd_vert_spacing = 0.0f;
float g_pd_arrow_spacing = 15.0f;
float g_pd_font_size = 1.0f;
bool g_pd_hero_summary_enable = true;
int g_pd_hero_count_min[6] = {0, 1, 1, 1, 1, 1}; // index 1-5 = per-cost threshold

// Opponent View Settings
bool g_opp_show_board = true;
bool g_opp_show_shop = true;
bool g_opp_show_bench = true;
float g_opp_hex_size = 25.0f;
float g_opp_scale = 1.0f;

// Hextech View Settings
float g_hextech_scale = 1.0f;

// My Hero Warning Float Settings
bool g_win_hero_warn = true;
int g_hero_warn_thres = 3;
float g_hero_warn_scale = 1.0f;
float g_alpha_hero_warn = 1.0f;
float g_float_hw_x = -1.0f, g_float_hw_y = -1.0f;

// View dimensions for scaling
int g_cached_view_width = 0;
int g_cached_view_height = 0;
int g_gl_width = 0, g_gl_height = 0;

// Quit capsule (independent float)
float g_quit_x = 80.0f, g_quit_y = 520.0f;
int g_quit_confirm = 0;
float g_quit_timer = 0.0f;

// Lock floats capsule
float g_lock_x = 80.0f, g_lock_y = 460.0f;
bool g_floats_locked = false;

// Card pool toggle capsule
float g_cpbtn_x = 80.0f, g_cpbtn_y = 400.0f;

// Saved float window positions (-1 = unset)
float g_float_cp_x = -1.0f, g_float_cp_y = -1.0f;
float g_float_pd_x = -1.0f, g_float_pd_y = -1.0f;
float g_float_opp_x = -1.0f, g_float_opp_y = -1.0f;
float g_float_hex_x = -1.0f, g_float_hex_y = -1.0f;
static bool g_apply_saved_float_pos = false;
static std::unordered_set<std::string> s_pos_initialized;

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
    struct BuySlotTask { uintptr_t slot_addr; int hero_id; };
    std::vector<BuySlotTask> buy_slots;
} g_Tasks;

std::vector<uintptr_t> g_shop_slots;
void* old_shop_listen = nullptr;
std::atomic<bool> g_shop_listen_done{false};

std::atomic<uint64_t> g_count_shop_listen{0};
std::atomic<uint64_t> g_count_set_IsGameEnd{0};
std::atomic<uint64_t> g_count_SendWillRenderCanvases{0};
std::atomic<uint64_t> g_count_buy_hero_new{0};
std::atomic<uint64_t> g_count_func_get_Instance{0};
std::atomic<uint64_t> g_count_func_quit{0};
std::atomic<uint64_t> g_count_func_get_hex{0};

uintptr_t hook_shop_listen(uintptr_t x0, uintptr_t x1, uintptr_t x2, uintptr_t x3, uintptr_t x4, uintptr_t x5, uintptr_t x6, uintptr_t x7) { g_count_shop_listen++;
    if (g_is_in_match.load(std::memory_order_relaxed) && !g_shop_listen_done.load() && x0 != 0) {
        if (std::find(g_shop_slots.begin(), g_shop_slots.end(), x0) == g_shop_slots.end()) {
            g_shop_slots.push_back(x0);
                        if (g_shop_slots.size() >= 5) {
                g_shop_listen_done.store(true);
                AddActionLog((const char*)u8"-> [商店] 已获取本局全部 5 个卡槽地址，自动停止监听!");
            }
        }
    }
    if (old_shop_listen) {
        return ((uintptr_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))old_shop_listen)(x0, x1, x2, x3, x4, x5, x6, x7);
    }
    return 0;
}

// Forward declaration for SafeReadMemory (used by SafeDobbyHook below)
bool SafeReadMemory(uintptr_t addr, void* buffer, size_t size);

// -------------------- Anti-Crash Signal Guard & Safe Execution --------------------
static thread_local sigjmp_buf g_segv_jmp_buf;
static thread_local bool g_segv_guard_active = false;

static void SegvSignalHandler(int sig, siginfo_t* info, void* ucontext) {
    if (g_segv_guard_active) {
        g_segv_guard_active = false;
        siglongjmp(g_segv_jmp_buf, 1);
    }
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, nullptr);
    raise(sig);
}

inline void InitCrashGuard() {
    static std::atomic<bool> inited{false};
    if (inited.exchange(true)) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = SegvSignalHandler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    LOGI("[+] Anti-Crash Signal Guard Installed.");
}

#define SAFE_CALL(call_expr, fallback_val) [&]() { \
    InitCrashGuard(); \
    g_segv_guard_active = true; \
    if (sigsetjmp(g_segv_jmp_buf, 1) == 0) { \
        auto res = (call_expr); \
        g_segv_guard_active = false; \
        return res; \
    } else { \
        g_segv_guard_active = false; \
        LOGI("[!] Crash prevented during call: " #call_expr); \
        return fallback_val; \
    } \
}()

#define SAFE_CALL_VOID(call_expr) [&]() { \
    InitCrashGuard(); \
    g_segv_guard_active = true; \
    if (sigsetjmp(g_segv_jmp_buf, 1) == 0) { \
        call_expr; \
        g_segv_guard_active = false; \
    } else { \
        g_segv_guard_active = false; \
        LOGI("[!] Crash prevented during void call: " #call_expr); \
    } \
}()

struct MemRange { uintptr_t start; uintptr_t end; };
static std::vector<MemRange> g_il2cpp_exec_regions;
static std::mutex g_exec_regions_mutex;

inline void UpdateIl2CppExecRegions() {
    std::lock_guard<std::mutex> lock(g_exec_regions_mutex);
    g_il2cpp_exec_regions.clear();
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "libil2cpp.so") && strstr(line, "r-xp")) {
            uintptr_t start = 0, end = 0;
            if (sscanf(line, "%lx-%lx", &start, &end) == 2 && start < end) {
                g_il2cpp_exec_regions.push_back({start, end});
            }
        }
    }
    fclose(fp);
}

inline bool IsValidExecutableAddr(void* addr) {
    if (!addr) return false;
    uintptr_t ptr = (uintptr_t)addr;
    if (ptr % 4 != 0) return false;
    std::lock_guard<std::mutex> lock(g_exec_regions_mutex);
    for (const auto& r : g_il2cpp_exec_regions) {
        if (ptr >= r.start && ptr < r.end) return true;
    }
    return false;
}
inline void EnsureIl2CppThreadAttached() {
    typedef void* (*il2cpp_domain_get_t)();
    typedef void* (*il2cpp_thread_attach_t)(void*);
    static auto domain_get = (il2cpp_domain_get_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_domain_get");
    static auto thread_attach = (il2cpp_thread_attach_t)DobbySymbolResolver("libil2cpp.so", "il2cpp_thread_attach");
    if (!domain_get) {
        void* h = dlopen("libil2cpp.so", RTLD_LAZY);
        if (h) {
            domain_get = (il2cpp_domain_get_t)dlsym(h, "il2cpp_domain_get");
            thread_attach = (il2cpp_thread_attach_t)dlsym(h, "il2cpp_thread_attach");
        }
    }
    if (domain_get && thread_attach) {
        void* domain = domain_get();
        if (domain) thread_attach(domain);
    }
}

inline int SafeDobbyHook(void* target, void* replace, void** origin) {
    if (!target || !replace) return -1;
    if (!IsValidExecutableAddr(target)) {
        LOGI("[!] SafeDobbyHook rejected invalid target address: %p", target);
        return -1;
    }
    int ret = DobbyHook(target, replace, origin);
    if (ret == 0) {
        LOGI("[+] SafeDobbyHook successfully hooked target: %p", target);
    } else {
        LOGI("[!] SafeDobbyHook failed with code %d for target: %p", ret, target);
    }
    return ret;
}











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
#define IsValidPtr(ptr) (((uintptr_t)(ptr)) > 0x10000000 && ((uintptr_t)(ptr)) < 0x00007FFFFFFFFFFF && (((uintptr_t)(ptr)) & 0x3) == 0)

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
    int len = 0;
    if (!SafeReadMemory(strAddr + 0x10, &len, sizeof(int))) return "";
    if (len <= 0 || len > 300) return "";
    std::wstring wstr;
    wstr.reserve(len);
    for (int i = 0; i < len; i++) {
        uint16_t ch = 0;
        if (!SafeReadMemory(strAddr + 0x14 + i * 2, &ch, sizeof(uint16_t))) break;
        wstr += (wchar_t)ch;
    }
    return utf16_to_utf8(wstr);
}

std::vector<uintptr_t> GetStructArrayPointers(uintptr_t arrayAddr, int maxCount, int structSize, int ptrOffset) {
    std::vector<uintptr_t> res;
    if (!IsValidPtr(arrayAddr)) return res;
    if (structSize < 4 || structSize > 0x1000) return res;
    if (ptrOffset < 0 || ptrOffset >= structSize) return res;
    int count = SAFE_READ_INT(arrayAddr, 0x18);
    if (count <= 0 || count > 200) return res;
    int readLimit = (maxCount <= 0) ? count : std::min(count, std::min(maxCount, 80));
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
    if (count <= 0 || count > 200) return res; 
    int readLimit = std::min(count, std::min(maxCount, 60));
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
    if (count <= 0 || count > 200) return res; 
    int readLimit = std::min(count, std::min(maxCount, 60));
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
    if (ImGui::GetCurrentContext() == nullptr) return;
    ImGuiWindow* w = ImGui::FindWindowByName(name);
    if (w) { x = w->Pos.x; y = w->Pos.y; }
}

void SaveConfig() {
    CaptureWindowPos("##CardPoolFloat", g_float_cp_x, g_float_cp_y);
    CaptureWindowPos("##PlayerDataFloat", g_float_pd_x, g_float_pd_y);
    CaptureWindowPos("##OpponentFloat", g_float_opp_x, g_float_opp_y);
    CaptureWindowPos("##HextechFloat", g_float_hex_x, g_float_hex_y);
    CaptureWindowPos("##PathTraceFloat", g_float_pt_x, g_float_pt_y);

    std::ofstream out(GetConfigPath());
    if (out.is_open()) {
        out << "# [完美UI版] 在此处或菜单内修改十六进制偏移并保存，会自动生效！\n";
        
        #define WRITE_OFF_32(name) out << #name << "=0x" << std::hex << g_off.name << "\n"
        
        out << "[主线基础寻址与全局功能]\n";
        WRITE_OFF_32(func_get_Instance); WRITE_OFF_32(addr2); WRITE_OFF_32(addr3); WRITE_OFF_32(addra); WRITE_OFF_32(segmentcsogame);
        WRITE_OFF_32(func_quit); WRITE_OFF_32(segment_my_player_id); WRITE_OFF_32(next_opponents_list);
        WRITE_OFF_32(func_shop_listen); WRITE_OFF_32(func_buy_hero_new);
        WRITE_OFF_32(func_set_IsGameEnd); WRITE_OFF_32(func_SendWillRenderCanvases);
        
        out << "\n[牌库字典链]\n";
        WRITE_OFF_32(addr4); WRITE_OFF_32(addr5); WRITE_OFF_32(addr6); WRITE_OFF_32(addr7);
        WRITE_OFF_32(addr7_struct_size); WRITE_OFF_32(addr7_ptr_offset);
        WRITE_OFF_32(addr9); WRITE_OFF_32(addr9_struct_size); WRITE_OFF_32(addr9_ptr_offset);
        
        out << "\n[牌库底层数据]\n";
        WRITE_OFF_32(ph_heroId); WRITE_OFF_32(ph_remaining); WRITE_OFF_32(ph_total);
        
        out << "\n[玩家字典链]\n";
        WRITE_OFF_32(addr11); WRITE_OFF_32(addr12); WRITE_OFF_32(addr12_struct_size); WRITE_OFF_32(addr12_ptr_offset);
        
        out << "\n[玩家基本属性]\n";
        WRITE_OFF_32(addr13); WRITE_OFF_32(pi_name); WRITE_OFF_32(pi_id); WRITE_OFF_32(pi_is_bot); WRITE_OFF_32(pi_money); 
        WRITE_OFF_32(pi_win_streak); WRITE_OFF_32(pi_lose_streak); WRITE_OFF_32(pi_level);
        
        out << "\n[场上商店备战区]\n";
        WRITE_OFF_32(addr14); WRITE_OFF_32(addr15); WRITE_OFF_32(addr16); WRITE_OFF_32(shop_hero_id);
        WRITE_OFF_32(addr17); WRITE_OFF_32(addr18); WRITE_OFF_32(bench_hero_id);
        WRITE_OFF_32(addr19); WRITE_OFF_32(addr20); WRITE_OFF_32(board_hero_id); WRITE_OFF_32(board_x); WRITE_OFF_32(board_y);
        
        out << "\n[海克斯预测]\n";
        WRITE_OFF_32(addr21); WRITE_OFF_32(addr22); WRITE_OFF_32(addr23); WRITE_OFF_32(addr23_struct_size); WRITE_OFF_32(addr23_ptr_offset);
        WRITE_OFF_32(addr26); WRITE_OFF_32(pi_avatar_rank); WRITE_OFF_32(pi_avatar_player_id); WRITE_OFF_32(hexctrl); WRITE_OFF_32(func_get_hex);
        out << "\n[UI设置]\n";
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
                        out << "float_cp_x=" << g_float_cp_x << "\n";
        out << "float_cp_y=" << g_float_cp_y << "\n";
        out << "float_pd_x=" << g_float_pd_x << "\n";
        out << "float_pd_y=" << g_float_pd_y << "\n";
        out << "float_opp_x=" << g_float_opp_x << "\n";
        out << "float_opp_y=" << g_float_opp_y << "\n";
        out << "float_hex_x=" << g_float_hex_x << "\n";
        out << "float_hex_y=" << g_float_hex_y << "\n";
        out << "win_path_trace=" << (g_win_path_trace ? 1 : 0) << "\n";
        out << "path_trace_scale=" << g_path_trace_scale << "\n";
        out << "alpha_pt=" << g_alpha_pt << "\n";
        out << "float_pt_x=" << g_float_pt_x << "\n";
        out << "float_pt_y=" << g_float_pt_y << "\n";
        out << "auto_buy_ids=";
        bool first_ab = true;
        for (const auto& kv : g_heroAutoBuyChecked) {
            if (kv.second) {
                if (!first_ab) out << ",";
                out << kv.first;
                first_ab = false;
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
                PARSE_OFF_32(func_quit) PARSE_OFF_32(segment_my_player_id) PARSE_OFF_32(next_opponents_list) PARSE_OFF_32(func_shop_listen) PARSE_OFF_32(func_buy_hero_new) PARSE_OFF_32(func_set_IsGameEnd) PARSE_OFF_32(func_SendWillRenderCanvases)
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
                                                else if (key == "float_cp_x") g_float_cp_x = std::stof(valStr);
                else if (key == "float_cp_y") g_float_cp_y = std::stof(valStr);
                else if (key == "float_pd_x") g_float_pd_x = std::stof(valStr);
                else if (key == "float_pd_y") g_float_pd_y = std::stof(valStr);
                else if (key == "float_opp_x") g_float_opp_x = std::stof(valStr);
                else if (key == "float_opp_y") g_float_opp_y = std::stof(valStr);
                else if (key == "float_hex_x") g_float_hex_x = std::stof(valStr);
                else if (key == "float_hex_y") g_float_hex_y = std::stof(valStr);
                else if (key == "win_path_trace") g_win_path_trace = (std::stoi(valStr) != 0);
                else if (key == "path_trace_scale") g_path_trace_scale = std::stof(valStr);
                else if (key == "alpha_pt") g_alpha_pt = std::stof(valStr);
                else if (key == "float_pt_x") g_float_pt_x = std::stof(valStr);
                else if (key == "float_pt_y") g_float_pt_y = std::stof(valStr);
                                                                
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
    g_apply_saved_float_pos = true; s_pos_initialized.clear();
    if (!has_full) SaveConfig();
}

void ClearGameState() {
    g_poolHeroes.clear();
    g_heroesByCost.clear();
    g_players.clear();
    g_next_opponents.clear();
    g_my_player_id = -1;
    g_hex_qualities[0] = g_hex_qualities[1] = g_hex_qualities[2] = 0;

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

void ResolveDiagnosticPointers() {
    if (g_il2cppTrueBase == 0 || g_off.func_get_Instance == 0) {
        g_dbg_addr1 = 0; g_dbg_addr2 = 0; g_dbg_addr3 = 0; g_dbg_addra = 0; g_dbg_segmentcsogame = 0;
        return;
    }

    if (!IsValidPtr(g_dbg_addr1)) {
        typedef void* (*func_get_Instance_t)(void* method_info);
        func_get_Instance_t get_Instance = (func_get_Instance_t)(g_il2cppTrueBase + (uintptr_t)g_off.func_get_Instance);
        if (get_Instance && IsValidExecutableAddr((void*)get_Instance)) {
            g_dbg_addr1 = SAFE_CALL((uintptr_t)(g_count_func_get_Instance++, get_Instance(nullptr)), (uintptr_t)0);
        } else {
            g_dbg_addr1 = 0;
        }
    }

    if (IsValidPtr(g_dbg_addr1)) {
        g_dbg_addr2 = SAFE_READ_PTR(g_dbg_addr1, g_off.addr2);
    } else {
        g_dbg_addr2 = 0;
    }

    if (IsValidPtr(g_dbg_addr2)) {
        g_dbg_addr3 = SAFE_READ_PTR(g_dbg_addr2, g_off.addr3);
        g_dbg_addr11 = SAFE_READ_PTR(g_dbg_addr2, g_off.addr11);
    } else {
        g_dbg_addr3 = 0;
        g_dbg_addr11 = 0;
    }

    if (IsValidPtr(g_dbg_addr3)) {
        g_dbg_addra = SAFE_READ_PTR(g_dbg_addr3, g_off.addra);
    } else {
        g_dbg_addra = 0;
    }

    if (IsValidPtr(g_dbg_addra)) {
        g_dbg_segmentcsogame = SAFE_READ_PTR(g_dbg_addra, g_off.segmentcsogame);
    } else {
        g_dbg_segmentcsogame = 0;
    }

    if (IsValidPtr(g_dbg_segmentcsogame)) {
        g_my_player_id = SAFE_READ_INT(g_dbg_segmentcsogame, g_off.segment_my_player_id);
        g_dbg_addr4 = SAFE_READ_PTR(g_dbg_segmentcsogame, g_off.addr4);
        g_dbg_addr21 = SAFE_READ_PTR(g_dbg_segmentcsogame, g_off.addr21);
    } else {
        g_dbg_addr4 = 0;
        g_dbg_addr21 = 0;
    }

    if (IsValidPtr(g_dbg_addr4)) {
        g_dbg_addr5 = SAFE_READ_PTR(g_dbg_addr4, g_off.addr5);
    } else { g_dbg_addr5 = 0; }

    if (IsValidPtr(g_dbg_addr5)) {
        g_dbg_addr6 = SAFE_READ_PTR(g_dbg_addr5, g_off.addr6);
    } else { g_dbg_addr6 = 0; }

    if (IsValidPtr(g_dbg_addr6)) {
        g_dbg_addr7 = SAFE_READ_PTR(g_dbg_addr6, g_off.addr7);
    } else { g_dbg_addr7 = 0; }

    if (IsValidPtr(g_dbg_addr11)) {
        g_dbg_addr12 = SAFE_READ_PTR(g_dbg_addr11, g_off.addr12);
    } else { g_dbg_addr12 = 0; }

    if (IsValidPtr(g_dbg_addr21)) {
        g_dbg_addr22 = SAFE_READ_PTR(g_dbg_addr21, g_off.addr22);
    } else { g_dbg_addr22 = 0; }

    if (IsValidPtr(g_dbg_addr22)) {
        g_dbg_addr23 = SAFE_READ_PTR(g_dbg_addr22, g_off.addr23);
    } else { g_dbg_addr23 = 0; }
}

bool TryResolveSegmentCSOGame(uintptr_t* out_segment = nullptr) {
    ResolveDiagnosticPointers();
    if (IsValidPtr(g_dbg_segmentcsogame)) {
        if (out_segment) *out_segment = g_dbg_segmentcsogame;
        return true;
    }
    return false;
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

    // 中途注入：segment 已连续有效且上一局 segment 已失效过
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

// addr26+0x248 读出玩家 id，与玩家列表里已有 id 对上即该玩家
static int MatchPlayerIndexByAvatarPid(int pid) {
    if (pid < 0) return -1;
    for (size_t i = 0; i < g_players.size(); i++)
        if (g_players[i].id == pid) return (int)i;
    return -1;
}

// 遍历 addr23 全部条目：每条 +0x68 -> addr26；addr26+0x2DC=头像排位，+0x248=玩家id
static void ApplyAvatarRanksFromList23() {
    g_dbg_avatar_ranks.clear();
    for (auto& pi : g_players) pi.avatar_rank = 0;

    for (uintptr_t entry : g_dbg_list23_addrs) {
        if (!IsValidPtr(entry)) continue;
        uintptr_t addr26 = SAFE_READ_PTR(entry, g_off.addr26); // +0x68
        if (!IsValidPtr(addr26)) continue;

        int raw_rank = SAFE_READ_INT(addr26, g_off.pi_avatar_rank); // +0x2DC
        int rank = NormalizeAvatarRank(raw_rank);
        if (rank < 0) continue;

        int pid = SAFE_READ_INT(addr26, g_off.pi_avatar_player_id); // +0x248
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

        if (!IsValidPtr(g_dbg_addr1)) {
        typedef void* (*func_get_Instance_t)(void* method_info);
        func_get_Instance_t get_Instance = (func_get_Instance_t)(g_il2cppTrueBase + (uintptr_t)g_off.func_get_Instance);
        if (get_Instance && IsValidExecutableAddr((void*)get_Instance)) {
            g_dbg_addr1 = SAFE_CALL((uintptr_t)(g_count_func_get_Instance++, get_Instance(nullptr)), (uintptr_t)0);
        }
    }
    if (!IsValidPtr(g_dbg_addr1)) return; // Anti-crash protected
    g_dbg_addr2 = SAFE_READ_PTR(g_dbg_addr1, g_off.addr2);
    g_dbg_addr3 = SAFE_READ_PTR(g_dbg_addr2, g_off.addr3); 
    g_dbg_addra = SAFE_READ_PTR(g_dbg_addr3, g_off.addra); 
    g_dbg_segmentcsogame = SAFE_READ_PTR(g_dbg_addra, g_off.segmentcsogame); 

    if (IsValidPtr(g_dbg_segmentcsogame))
        g_my_player_id = SAFE_READ_INT(g_dbg_segmentcsogame, g_off.segment_my_player_id);
    else
        g_my_player_id = -1;
    uintptr_t next_opp_addr = SAFE_READ_PTR(g_dbg_addr2, g_off.next_opponents_list);
    uintptr_t next_opp_list = SAFE_READ_PTR(next_opp_addr, 0x10);
    int next_opp_count = SAFE_READ_INT(next_opp_addr, 0x18); // List._size
    g_next_opponents = GetIntsInArray(next_opp_list, next_opp_count > 0 && next_opp_count < 16 ? next_opp_count : 8);

    // [牌库显示]
    g_dbg_addr4 = SAFE_READ_PTR(g_dbg_segmentcsogame, g_off.addr4);
    g_dbg_addr5 = SAFE_READ_PTR(g_dbg_addr4, g_off.addr5);
    g_dbg_addr6 = SAFE_READ_PTR(g_dbg_addr5, g_off.addr6);
    g_dbg_addr7 = SAFE_READ_PTR(g_dbg_addr6, g_off.addr7);
    
    auto list7 = GetStructArrayPointers(g_dbg_addr7, 60, g_off.addr7_struct_size, g_off.addr7_ptr_offset);
    g_dbg_list7_addrs = list7;
    g_dbg_list9_map.clear();

    int total_hero_reads = 0;
    for (auto addr8 : list7) {
        if (++total_hero_reads > 80) break; // 熔断保护：防止野指针下无限读取卡死
        uintptr_t addr9 = SAFE_READ_PTR(addr8, g_off.addr9);
        if (g_dbg_addr9 == 0 && IsValidPtr(addr9)) g_dbg_addr9 = addr9;
        auto list9 = GetStructArrayPointers(addr9, 60, g_off.addr9_struct_size, g_off.addr9_ptr_offset);
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

    // ★ 核心修复：【内存全量暴力扫描算法】
    g_dbg_addr11 = SAFE_READ_PTR(g_dbg_addr2, g_off.addr11);
    g_dbg_addr12 = SAFE_READ_PTR(g_dbg_addr11, g_off.addr12);
    
    g_players.clear();
    g_dbg_player_addrs.clear();
    
    if (IsValidPtr(g_dbg_addr12)) {
        int capacity = SAFE_READ_INT(g_dbg_addr12, 0x18);
        if (capacity > 0 && capacity <= 32) {
            for (int offset = 0x20; offset < 0x20 + capacity * 0x20 && offset < 0x20 + 32 * 0x20; offset += 8) {
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

    g_dbg_addr14 = 0; g_dbg_addr15 = 0; g_dbg_addr16 = 0;
    g_dbg_addr17 = 0; g_dbg_addr18 = 0;
    g_dbg_addr19 = 0; g_dbg_addr20 = 0;
    g_dbg_shop_ok = false;
    g_dbg_bench_ok = false;
    g_dbg_board_ok = false;
    g_dbg_board_pos_ok = false;

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
        
        // 商店链条
        uintptr_t addr14 = SAFE_READ_PTR(val, g_off.addr14);
        if (IsValidPtr(addr14)) g_dbg_addr14 = addr14;
        uintptr_t addr15 = SAFE_READ_PTR(addr14, g_off.addr15);
        if (IsValidPtr(addr15)) g_dbg_addr15 = addr15;
        auto shopItems = GetPointersInArray(addr15, 5);
        for (size_t i = 0; i < shopItems.size(); i++) {
            uintptr_t addr16 = SAFE_READ_PTR(shopItems[i], g_off.addr16);
            if (IsValidPtr(addr16)) g_dbg_addr16 = addr16;
            int shop_hero_id = SAFE_READ_INT(addr16, g_off.shop_hero_id);
            if (shop_hero_id > 0 && shop_hero_id < 200000) g_dbg_shop_ok = true;
            pi.shop.push_back(shop_hero_id);
            
            // 自动购买
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
                        g_Tasks.buy_slots.push_back({ slot_addr, shop_hero_id });
                    }
                }
            }
        }
        
        // 备战席链条
        uintptr_t addr17 = SAFE_READ_PTR(val, g_off.addr17);
        if (IsValidPtr(addr17)) g_dbg_addr17 = addr17;
        uintptr_t addr18 = SAFE_READ_PTR(addr17, g_off.addr18);
        if (IsValidPtr(addr18)) g_dbg_addr18 = addr18;
        auto benchItems = GetPointersInArray(addr18, 10);
        for (auto b_item : benchItems) {
            int b_hid = SAFE_READ_INT(b_item, g_off.bench_hero_id);
            if (b_hid > 0 && b_hid < 200000) g_dbg_bench_ok = true;
            pi.bench.push_back(b_hid);
        }
        
        // 场上棋盘链条
        uintptr_t addr19 = SAFE_READ_PTR(val, g_off.addr19);
        if (IsValidPtr(addr19)) g_dbg_addr19 = addr19;
        uintptr_t addr20 = SAFE_READ_PTR(addr19, g_off.addr20);
        if (IsValidPtr(addr20)) g_dbg_addr20 = addr20;
        auto boardItems = GetPointersInArray(addr20, 30);
        for (auto bd_item : boardItems) {
            BoardHero bh;
            bh.heroId = SAFE_READ_INT(bd_item, g_off.board_hero_id);
            bh.x = SAFE_READ_INT(bd_item, g_off.board_x);
            bh.y = SAFE_READ_INT(bd_item, g_off.board_y);
            if (bh.heroId > 0 && bh.heroId < 200000) g_dbg_board_ok = true;
            if (bh.x >= 0 && bh.x <= 8 && bh.y >= 0 && bh.y <= 8) g_dbg_board_pos_ok = true;
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
                        if (get_hex && IsValidExecutableAddr((void*)get_hex) && IsValidPtr(g_dbg_hexctrl)) {
                            int q0 = SAFE_CALL((g_count_func_get_hex++, get_hex(g_dbg_hexctrl, 0)), 0);
                            int q1 = SAFE_CALL((g_count_func_get_hex++, get_hex(g_dbg_hexctrl, 1)), 0);
                            int q2 = SAFE_CALL((g_count_func_get_hex++, get_hex(g_dbg_hexctrl, 2)), 0);
                                
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

// 负宽度 = 为右侧 label 预留像素；-1 几乎不留空，会导致「排布列数」等只露出一个字
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

void DrawAmbientOrbs() {
    // 不再绘制全屏灰蒙蒙遮罩，避免遮挡游戏画面
}

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
    case 0: // 浮窗
        dl->AddRect(ImVec2(c.x - s, c.y - s * 0.75f), ImVec2(c.x + s, c.y + s * 0.75f), col, 2.5f, 0, 1.5f);
        dl->AddLine(ImVec2(c.x - s * 0.5f, c.y - s * 0.75f), ImVec2(c.x - s * 0.5f, c.y + s * 0.75f), col, 1.2f);
        break;
    case 1: // 拿牌
        dl->AddRect(ImVec2(c.x - s * 0.65f, c.y - s), ImVec2(c.x + s * 0.65f, c.y + s), col, 2.0f, 0, 1.5f);
        dl->AddLine(ImVec2(c.x - s * 0.2f, c.y - s * 0.55f), ImVec2(c.x + s * 0.35f, c.y - s * 0.55f), col, 1.2f);
        break;
    case 2: // 监视
        dl->AddCircle(c, s * 0.55f, col, 16, 1.5f);
        dl->AddLine(ImVec2(c.x + s * 0.35f, c.y + s * 0.35f), ImVec2(c.x + s * 0.85f, c.y + s * 0.85f), col, 1.5f);
        break;
        case 3: // 调试
        dl->AddCircle(c, s * 0.55f, col, 16, 1.5f);
        for (int i = 0; i < 6; i++) {
            float a = (float)(M_PI * 2.0 * i / 6.0);
            ImVec2 p1(c.x + cosf(a) * s * 0.55f, c.y + sinf(a) * s * 0.55f);
            ImVec2 p2(c.x + cosf(a) * s * 0.85f, c.y + sinf(a) * s * 0.85f);
            dl->AddLine(p1, p2, col, 1.2f);
        }
        break;
    case 4: // 符号反查 (放大镜图标)
        dl->AddCircle(ImVec2(c.x - s * 0.2f, c.y - s * 0.2f), s * 0.5f, col, 16, 1.5f);
        dl->AddLine(ImVec2(c.x + s * 0.18f, c.y + s * 0.18f), ImVec2(c.x + s * 0.75f, c.y + s * 0.75f), col, 2.2f);
        break;
    case 5: // 对象检查 (多维网格层级图标)
        dl->AddRect(ImVec2(c.x - s * 0.7f, c.y - s * 0.7f), ImVec2(c.x + s * 0.7f, c.y + s * 0.7f), col, 2.0f, 0, 1.5f);
        dl->AddLine(ImVec2(c.x - s * 0.7f, c.y), ImVec2(c.x + s * 0.7f, c.y), col, 1.2f);
        dl->AddLine(ImVec2(c.x, c.y - s * 0.7f), ImVec2(c.x, c.y + s * 0.7f), col, 1.2f);
        break;
    default:
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

    // 1. 右侧状态徽章 (对局中 / 等待连接)
    bool online = g_is_in_match.load(std::memory_order_relaxed);
    const char* stTxt = online ? (const char*)u8"● 对局中" : (const char*)u8"● 等待连接";
    ImVec2 stSz = ImGui::CalcTextSize(stTxt);
    float badgeW = stSz.x + 20.0f * sc;
    ImVec2 bMin(mx.x - badgeW - 14.0f * sc, mn.y + (h - 26.0f * sc) * 0.5f);
    ImVec2 bMax(bMin.x + badgeW, bMin.y + 26.0f * sc);
    ImU32 badgeBg = online ? IM_COL32(52, 211, 153, 30) : IM_COL32(248, 113, 113, 30);
    ImU32 badgeBorder = online ? IM_COL32(52, 211, 153, 120) : IM_COL32(248, 113, 113, 120);
    ImU32 badgeText = online ? IM_COL32(110, 231, 183, 255) : IM_COL32(252, 165, 165, 255);
    dl->AddRectFilled(bMin, bMax, badgeBg, 13.0f * sc);
    dl->AddRect(bMin, bMax, badgeBorder, 13.0f * sc, 0, 1.2f * sc);
    dl->AddText(ImVec2(bMin.x + 10.0f * sc, bMin.y + (26.0f * sc - stSz.y) * 0.5f), badgeText, stTxt);

    // 2. ★ 实时全量真实帧率徽章 (如 144.0 FPS / 120.0 FPS / 90.0 FPS 自适应高刷高亮)
    char fpsBuf[48];
    snprintf(fpsBuf, sizeof(fpsBuf), "%.1f FPS", g_realtime_fps);
    ImVec2 fpsSz = ImGui::CalcTextSize(fpsBuf);
    float fpsBadgeW = fpsSz.x + 20.0f * sc;
    ImVec2 fpsMin(bMin.x - fpsBadgeW - 8.0f * sc, bMin.y);
    ImVec2 fpsMax(fpsMin.x + fpsBadgeW, bMax.y);
    
    // 满血高刷 120Hz/144Hz 呈现高光霓虹青，60Hz 呈现翠绿，<50Hz 呈现暖黄
    ImU32 fpsBg = (g_realtime_fps >= 115.0f) ? IM_COL32(6, 182, 212, 35) :
                  (g_realtime_fps >= 55.0f)  ? IM_COL32(52, 211, 153, 30) :
                                              IM_COL32(245, 158, 11, 30);
    ImU32 fpsBorder = (g_realtime_fps >= 115.0f) ? IM_COL32(6, 182, 212, 140) :
                      (g_realtime_fps >= 55.0f)  ? IM_COL32(52, 211, 153, 120) :
                                                  IM_COL32(245, 158, 11, 120);
    ImU32 fpsText = (g_realtime_fps >= 115.0f) ? IM_COL32(103, 232, 249, 255) :
                    (g_realtime_fps >= 55.0f)  ? IM_COL32(110, 231, 183, 255) :
                                                IM_COL32(253, 230, 138, 255);
    dl->AddRectFilled(fpsMin, fpsMax, fpsBg, 13.0f * sc);
    dl->AddRect(fpsMin, fpsMax, fpsBorder, 13.0f * sc, 0, 1.2f * sc);
    dl->AddText(ImVec2(fpsMin.x + 10.0f * sc, fpsMin.y + (26.0f * sc - fpsSz.y) * 0.5f), fpsText, fpsBuf);

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
    // 禁用 BringWindowToDisplayFront，防止在帧内遍历窗口列表时引起自循环递归/堆栈溢出 (SIGSEGV code -6)
}

inline void ExecuteRapidQuit() {
    EnsureIl2CppThreadAttached();
    uintptr_t seg = g_dbg_segmentcsogame;
    int pid = g_my_player_id;
    
    // 如果全局缓存未就绪，则现场实时通过寻址链获取一次
    if (!IsValidPtr(seg) || pid < 0) {
        typedef void* (*func_get_Instance_t)(void*);
        func_get_Instance_t get_Instance = (func_get_Instance_t)(g_il2cppTrueBase + g_off.func_get_Instance);
        if (get_Instance && IsValidExecutableAddr((void*)get_Instance)) {
            uintptr_t a1 = SAFE_CALL((uintptr_t)(g_count_func_get_Instance++, get_Instance(nullptr)), (uintptr_t)0);
            uintptr_t a2 = SAFE_READ_PTR(a1, g_off.addr2);
            uintptr_t a3 = SAFE_READ_PTR(a2, g_off.addr3);
            uintptr_t aa = SAFE_READ_PTR(a3, g_off.addra);
            seg = SAFE_READ_PTR(aa, g_off.segmentcsogame);
            if (IsValidPtr(seg)) {
                pid = SAFE_READ_INT(seg, g_off.segment_my_player_id);
            }
        }
    }

    typedef void (*func_quit_t)(uintptr_t, int, int, void*);
    func_quit_t quit_func = (func_quit_t)(g_il2cppTrueBase + g_off.func_quit);

    if (quit_func && IsValidExecutableAddr((void*)quit_func) && IsValidPtr(seg)) {
        AddActionLog((const char*)u8"-> [极速退游] 执行 func_quit(seg=0x%lx, pid=%d, mode=0)", seg, pid);
        g_count_func_quit++; SAFE_CALL_VOID(quit_func(seg, pid, 0, nullptr));
    } else {
        AddActionLog((const char*)u8"-> [退游失败] func_quit(0x%lx) 或 seg(0x%lx) 无效!", (uintptr_t)quit_func, seg);
    }
}

void DrawQuitCapsule() {
    ImGuiIO& io = ImGui::GetIO();
    float sc = g_autoScale;
    const char* label = (g_quit_confirm == 0) ? (const char*)u8"极速退游" : (const char*)u8"再次点击确认";
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
            AddActionLog((const char*)u8"-> [极速退游] 请在3秒内再次点击以确认退游...");
        } else {
            g_quit_confirm = 0;
            ExecuteRapidQuit();











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
    // 每帧强制显示置顶：仅靠后绘制无法压过已在最前的牌库浮窗
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
}

static bool BeginContentFloatWindow(const char* id, bool* open, float* pos_x = nullptr, float* pos_y = nullptr, float alpha = 1.0f) {
    if (open && !*open) return false;
    if (pos_x && pos_y && *pos_x >= 0.0f && *pos_y >= 0.0f) {
        if (s_pos_initialized.find(id) == s_pos_initialized.end()) {
            ImGui::SetNextWindowPos(ImVec2(*pos_x, *pos_y), ImGuiCond_Always);
            s_pos_initialized.insert(id);
        }
    }
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * g_autoScale, 10.0f * g_autoScale));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(alpha, 0.1f, 1.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration;
    // 锁定后：不可拖动、不抢层级；鼠标穿透，避免盖住锁定胶囊导致无法解锁
    if (g_floats_locked)
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMouseInputs;
    bool vis = ImGui::Begin(id, open, flags);
    if (!vis) {
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
        return false;
        }
    if (pos_x && pos_y) {
        ImVec2 curPos = ImGui::GetWindowPos();
        *pos_x = curPos.x;
        *pos_y = curPos.y;
    }
    return true;
}

static void DrawFloatScaleGrip(const char* grip_id, float* scale, float min_s, float max_s) {
    if (!scale || g_floats_locked) return;
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    if (!win) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushID(grip_id);
    ImGuiID gid = ImGui::GetID("##scale_grip");
    ImGuiStorage* st = ImGui::GetStateStorage();
    if (!st) { ImGui::PopID(); return; }

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
        win->DrawList->AddText(ImVec2(bb.Min.x + pad + 22.0f * sc, bb.Min.y + (size.y - ImGui::GetFontSize()) * 0.5f),
            selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 190, 205, 255), label);
    }
    return pressed;
}

void SetupImGuiStyle() { ApplyFrostedTheme(); }

// 暴力扫描 /system/fonts/ 目录，尝试所有字体文件直到找到能加载中文的
// 原因: MuMu 12 (Android 12+) 的中文字体都是 CFF/OTF 格式，stb_truetype 只支持 TrueType 轮廓
// 所以需要暴力尝试所有 .ttf 文件，找到真正的 TrueType 格式字体
ImFont* TryLoadChineseFont(ImGuiIO& io, const char* path, int fontNo, float size) {
    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    cfg.FontNo = fontNo;
    ImFont* f = io.Fonts->AddFontFromFileTTF(path, size, &cfg, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    if (f) {
        if (io.Fonts->Build()) {
            LOGI("[+] Font OK: %s (FontNo: %d)", path, fontNo);
            return f;
        }
        LOGI("[!] Font Build() failed: %s (FontNo: %d)", path, fontNo);
    } else {
        LOGI("[!] AddFontFromFileTTF NULL: %s (FontNo: %d)", path, fontNo);
    }
    io.Fonts->Clear();
    return nullptr;
}

void UpdateFontHD(bool force = false) {
    ImGuiIO& io = ImGui::GetIO();
    // 采用屏幕短边 (std::min(w, h)) 计算基准缩放，彻底消除横竖屏切换与启动阶段闪屏导致的超大菜单 Bug！
    float short_edge = (g_gl_width > 0 && g_gl_height > 0) ? std::min((float)g_gl_width, (float)g_gl_height) : ((io.DisplaySize.x > 0 && io.DisplaySize.y > 0) ? std::min(io.DisplaySize.x, io.DisplaySize.y) : 1080.0f);
    if (short_edge < 300.0f) short_edge = 1080.0f;
    g_autoScale = short_edge / 1080.0f;
    float targetSize = std::clamp(22.0f * g_autoScale, 18.0f, 32.0f);
    if (!force && std::abs(targetSize - g_current_rendered_size) < 2.0f && g_mainFont != nullptr) return;

    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    io.Fonts->Clear();
    g_mainFont = nullptr;

    // Phase 1: 优先尝试已知的纯 TrueType 中文字体路径
    const char* priority_paths[] = {
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/DroidSansFallbackFull.ttf",
        "/system/fonts/NotoSansSC-Regular.ttf",
        "/system/fonts/NotoSansHans-Regular.ttf",
        "/system/fonts/SysSans-Hans-Regular.ttf",
        "/system/fonts/Miui-Regular.ttf",
        "/system/fonts/SourceHanSansCN-Regular.ttf",
        "/system/fonts/HarmonyOS_Sans_SC.ttf",
        "/system/fonts/OplusSC-Regular.ttf",
        "/system/fonts/VivoSansSC-Regular.ttf",
    };

    for (const char* path : priority_paths) {
        if (access(path, R_OK) != 0) continue;
        LOGI("[*] Phase1 trying: %s", path);
        g_mainFont = TryLoadChineseFont(io, path, 0, targetSize);
        if (g_mainFont) goto font_done;
    }

    // Phase 2: 尝试 TTC 集合字体（索引 0~4）
    {
        const char* ttc_paths[] = {
            "/system/fonts/NotoSansCJK-Regular.ttc",
            "/system/fonts/NotoSerifCJK-Regular.ttc",
        };
        for (const char* path : ttc_paths) {
            if (access(path, R_OK) != 0) continue;
            for (int idx = 0; idx < 5; idx++) {
                LOGI("[*] Phase2 trying TTC: %s (FontNo: %d)", path, idx);
                g_mainFont = TryLoadChineseFont(io, path, idx, targetSize);
                if (g_mainFont) goto font_done;
            }
        }
    }

    // Phase 3: 暴力扫描 /system/fonts/ 目录，尝试所有 .ttf 文件
    {
        DIR* dir = opendir("/system/fonts");
        if (dir) {
            LOGI("[*] Phase3: Scanning /system/fonts/ for ANY TrueType font with Chinese glyphs...");
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL) {
                std::string name = ent->d_name;
                // 只尝试 .ttf 文件（.otf 是 CFF 格式，stb_truetype 不支持）
                if (name.size() < 5) continue;
                std::string ext = name.substr(name.size() - 4);
                if (ext != ".ttf" && ext != ".TTF") continue;
                std::string full = "/system/fonts/" + name;
                if (access(full.c_str(), R_OK) != 0) continue;
                LOGI("[*] Phase3 trying: %s", full.c_str());
                g_mainFont = TryLoadChineseFont(io, full.c_str(), 0, targetSize);
                if (g_mainFont) { closedir(dir); goto font_done; }
            }
            closedir(dir);
        }

        // 也扫描 /product/fonts/
        dir = opendir("/product/fonts");
        if (dir) {
            LOGI("[*] Phase3: Scanning /product/fonts/...");
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL) {
                std::string name = ent->d_name;
                if (name.size() < 5) continue;
                std::string ext = name.substr(name.size() - 4);
                if (ext != ".ttf" && ext != ".TTF") continue;
                std::string full = "/product/fonts/" + name;
                if (access(full.c_str(), R_OK) != 0) continue;
                LOGI("[*] Phase3 trying: %s", full.c_str());
                g_mainFont = TryLoadChineseFont(io, full.c_str(), 0, targetSize);
                if (g_mainFont) { closedir(dir); goto font_done; }
            }
            closedir(dir);
        }
    }

    // Phase 4: 尝试所有 .ttc 文件的所有子字体索引
    {
        DIR* dir = opendir("/system/fonts");
        if (dir) {
            LOGI("[*] Phase4: Trying ALL .ttc files with indices 0-6...");
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL) {
                std::string name = ent->d_name;
                if (name.size() < 5) continue;
                std::string ext = name.substr(name.size() - 4);
                if (ext != ".ttc" && ext != ".TTC") continue;
                std::string full = "/system/fonts/" + name;
                if (access(full.c_str(), R_OK) != 0) continue;
                for (int idx = 0; idx < 7; idx++) {
                    LOGI("[*] Phase4 trying: %s (FontNo: %d)", full.c_str(), idx);
                    g_mainFont = TryLoadChineseFont(io, full.c_str(), idx, targetSize);
                    if (g_mainFont) { closedir(dir); goto font_done; }
                }
            }
            closedir(dir);
        }
    }

    // Phase 5: 完全失败，使用默认英文字体
    LOGI("[!] ALL FONT PHASES FAILED. No TrueType Chinese font found on this system.");
    LOGI("[!] Falling back to default ASCII font. Chinese will show as '?'.");
    g_mainFont = io.Fonts->AddFontDefault();
    io.Fonts->Build();

font_done:
    if (g_mainFont) {
        io.FontDefault = g_mainFont;
    }

    ImGui_ImplOpenGL3_CreateDeviceObjects();
    g_current_rendered_size = targetSize;
    LOGI("[+] Font setup complete. g_mainFont=%p", (void*)g_mainFont);
}

// --------------------------------------------------------
// MODULE: Hex Virtual Keypad Popup (十六进制虚拟小键盘)
// --------------------------------------------------------
struct HexKeypadState {
    bool open = false;
    std::string target_name;
    uint32_t* target_val_ptr = nullptr;
    char input_buf[32] = {0};
} g_hexKeypad;

inline void OpenHexKeypad(const char* name, uint32_t* value_ptr) {
    g_hexKeypad.open = true;
    g_hexKeypad.target_name = name ? name : "偏移调整";
    g_hexKeypad.target_val_ptr = value_ptr;
    if (value_ptr) {
        snprintf(g_hexKeypad.input_buf, sizeof(g_hexKeypad.input_buf), "0x%X", *value_ptr);
    } else {
        g_hexKeypad.input_buf[0] = '\0';
    }
}

void DrawHexKeypadModal() {
    if (!g_hexKeypad.open || !g_hexKeypad.target_val_ptr) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460 * g_autoScale * g_scale, 440 * g_autoScale * g_scale), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
    std::string title = (const char*)u8"✏️ 输入十六进制偏移###HexKeypadModal";

    if (ImGui::Begin(title.c_str(), &g_hexKeypad.open, flags)) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), (const char*)u8"正在编辑: %s", g_hexKeypad.target_name.c_str());
        ImGui::Spacing();

        // Display input box
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8 * g_autoScale, 8 * g_autoScale));
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##hex_input_str", g_hexKeypad.input_buf, sizeof(g_hexKeypad.input_buf), ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Quick add/sub step buttons
        float avail_w = ImGui::GetContentRegionAvail().x;
        float quick_btn_w = (avail_w - 5 * 6 * g_autoScale) / 6.0f;
        auto QuickStep = [&](int delta) {
            uint32_t current = (uint32_t)strtoul(g_hexKeypad.input_buf, nullptr, 16);
            current += delta;
            snprintf(g_hexKeypad.input_buf, sizeof(g_hexKeypad.input_buf), "0x%X", current);
        };

        if (ImGui::Button("-0x1000", ImVec2(quick_btn_w, 28 * g_autoScale))) QuickStep(-0x1000);
        ImGui::SameLine();
        if (ImGui::Button("-0x100", ImVec2(quick_btn_w, 28 * g_autoScale))) QuickStep(-0x100);
        ImGui::SameLine();
        if (ImGui::Button("-0x10", ImVec2(quick_btn_w, 28 * g_autoScale))) QuickStep(-0x10);
        ImGui::SameLine();
        if (ImGui::Button("+0x10", ImVec2(quick_btn_w, 28 * g_autoScale))) QuickStep(0x10);
        ImGui::SameLine();
        if (ImGui::Button("+0x100", ImVec2(quick_btn_w, 28 * g_autoScale))) QuickStep(0x100);
        ImGui::SameLine();
        if (ImGui::Button("+0x1000", ImVec2(quick_btn_w, 28 * g_autoScale))) QuickStep(0x1000);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Hex Keypad Grid (4x4)
        float pad_btn_w = (avail_w - 3 * 6 * g_autoScale) / 4.0f;
        float pad_btn_h = 38 * g_autoScale * g_scale;

        auto AppendChar = [&](char c) {
            size_t len = strlen(g_hexKeypad.input_buf);
            if (len < sizeof(g_hexKeypad.input_buf) - 2) {
                g_hexKeypad.input_buf[len] = c;
                g_hexKeypad.input_buf[len + 1] = '\0';
            }
        };

        const char* hex_keys[4][4] = {
            {"1", "2", "3", "A"},
            {"4", "5", "6", "B"},
            {"7", "8", "9", "C"},
            {"0", "D", "E", "F"}
        };

        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (c > 0) ImGui::SameLine();
                if (ImGui::Button(hex_keys[r][c], ImVec2(pad_btn_w, pad_btn_h))) {
                    AppendChar(hex_keys[r][c][0]);
                }
            }
        }

        ImGui::Spacing();
        float action_btn_w = (avail_w - 2 * 6 * g_autoScale) / 3.0f;
        if (ImGui::Button("0x", ImVec2(action_btn_w, 32 * g_autoScale))) {
            if (strncmp(g_hexKeypad.input_buf, "0x", 2) != 0 && strncmp(g_hexKeypad.input_buf, "0X", 2) != 0) {
                char temp[32];
                snprintf(temp, sizeof(temp), "0x%s", g_hexKeypad.input_buf);
                strncpy(g_hexKeypad.input_buf, temp, sizeof(g_hexKeypad.input_buf));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button((const char*)u8"← 退格", ImVec2(action_btn_w, 32 * g_autoScale))) {
            size_t len = strlen(g_hexKeypad.input_buf);
            if (len > 0) {
                g_hexKeypad.input_buf[len - 1] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button((const char*)u8"清空 CE", ImVec2(action_btn_w, 32 * g_autoScale))) {
            g_hexKeypad.input_buf[0] = '\0';
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float confirm_w = (avail_w - 8 * g_autoScale) / 2.0f;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.6f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.75f, 0.35f, 1.0f));
        if (ImGui::Button((const char*)u8"✓ 确定应用并保存", ImVec2(confirm_w, 40 * g_autoScale))) {
            uint32_t val = (uint32_t)strtoul(g_hexKeypad.input_buf, nullptr, 16);
            *g_hexKeypad.target_val_ptr = val;
            g_hexKeypad.open = false;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button((const char*)u8"✗ 取消", ImVec2(confirm_w, 40 * g_autoScale))) {
            g_hexKeypad.open = false;
        }
    }
    ImGui::End();
}

void DrawOffsetAdjuster(const char* label, uint32_t* value, uintptr_t resolved_addr = 0, bool show_resolved = false) {
    bool is_valid = false;
    std::string s(label);
    if (s.find("segment_my_player_id") != std::string::npos) {
        is_valid = IsValidPtr(g_dbg_segmentcsogame) && (g_my_player_id >= 0 && g_my_player_id < 16);
    } else if (show_resolved) {
        is_valid = IsValidPtr(resolved_addr) || (resolved_addr > 0 && resolved_addr < 0xFFFFFFFF);
    } else {
        std::string s(label);
        if (s.find("func_") == 0) is_valid = IsValidExecutableAddr((void*)(g_il2cppTrueBase + *value));
        else if (s.find("pi_name") != std::string::npos) is_valid = !g_players.empty() && !g_players[0].name.empty();
        else if (s.find("pi_id") != std::string::npos) is_valid = !g_players.empty() && g_players[0].id >= 0;
        else if (s.find("pi_level") != std::string::npos) is_valid = !g_players.empty() && g_players[0].level >= 1 && g_players[0].level <= 11;
        else if (s.find("pi_money") != std::string::npos) is_valid = !g_players.empty() && g_players[0].money >= 0 && g_players[0].money < 500;
        else if (s.find("pi_win_streak") != std::string::npos || s.find("pi_lose_streak") != std::string::npos || s.find("pi_is_bot") != std::string::npos) is_valid = !g_players.empty();
        else if (s.find("pi_avatar_rank") != std::string::npos || s.find("pi_avatar_player_id") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr26);
        else if (s.find("ph_") == 0) is_valid = !g_poolHeroes.empty() || IsValidPtr(g_dbg_addr7);
        else if (s.find("dict struct_size") != std::string::npos || s.find("dict ptr_offset") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr12) && !g_players.empty();
        else if (s.find("addr7 ") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr7) && !g_dbg_list7_addrs.empty();
        else if (s.find("addr9 ") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr9);
        else if (s.find("addr23 ") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr23) && !g_dbg_list23_addrs.empty();
        
        // 商店链条判定
        else if (s.find("addr14") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr14);
        else if (s.find("addr15") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr15);
        else if (s.find("addr16") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr16);
        else if (s.find("shop_hero_id") != std::string::npos) is_valid = g_dbg_shop_ok;

        // 备战席链条判定
        else if (s.find("addr17") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr17);
        else if (s.find("addr18") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr18);
        else if (s.find("bench_hero_id") != std::string::npos) is_valid = g_dbg_bench_ok;

        // 场上棋盘链条判定
        else if (s.find("addr19") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr19);
        else if (s.find("addr20") != std::string::npos) is_valid = IsValidPtr(g_dbg_addr20);
        else if (s.find("board_hero_id") != std::string::npos) is_valid = g_dbg_board_ok;
        else if (s.find("board_x") != std::string::npos || s.find("board_y") != std::string::npos) is_valid = g_dbg_board_pos_ok;

        else if (s.find("my_player_id") != std::string::npos) is_valid = g_my_player_id >= 0;
        else if (s.find("next_opponents_list") != std::string::npos) is_valid = !g_next_opponents.empty();
        else if (s.find("hexctrl") != std::string::npos) is_valid = IsValidPtr(g_dbg_hexctrl);
        else is_valid = true;
    }
    ImVec4 label_col = is_valid ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    
    ImGui::PushID(label);
    std::string disp_label = label; size_t hash_pos = disp_label.find("##"); if (hash_pos != std::string::npos) disp_label = disp_label.substr(0, hash_pos); ImGui::TextColored(label_col, "%s", disp_label.c_str()); 
    
    if (show_resolved) {
        ImGui::SameLine();
        if (s.find("segment_my_player_id") != std::string::npos) {
            if (is_valid) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "-> ID: %d [OK]", g_my_player_id);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "-> %d [x]", g_my_player_id);
            }
        } else if (IsValidPtr(resolved_addr)) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "-> 0x%lx [OK]", (unsigned long)resolved_addr);
        } else if (resolved_addr != 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "-> %lu [OK]", (unsigned long)resolved_addr);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "-> 0x0 [x]");
        }
    }
    char hex_str[32];
    snprintf(hex_str, sizeof(hex_str), "0x%X", *value);

    float btn_w = 34 * g_autoScale * g_scale;
    float hex_btn_w = 95 * g_autoScale * g_scale;
    float controls_width = (btn_w * 4 + hex_btn_w + 4 * 4 * g_autoScale * g_scale); 
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - controls_width);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * g_autoScale * g_scale, 0));
    if (ImGui::Button("-10", ImVec2(btn_w, 0))) { *value -= 0x10; }
    ImGui::SameLine();
    if (ImGui::Button("-1", ImVec2(btn_w, 0))) { *value -= 0x1; }
    ImGui::SameLine();
    
    // Hex button that opens the keypad on click!
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.35f, 0.5f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
    if (ImGui::Button(hex_str, ImVec2(hex_btn_w, 0))) {
        OpenHexKeypad(label, value);
    }
    ImGui::PopStyleColor(2);
    
    ImGui::SameLine();
    if (ImGui::Button("+1", ImVec2(btn_w, 0))) { *value += 0x1; }
    ImGui::SameLine();
    if (ImGui::Button("+10", ImVec2(btn_w, 0))) { *value += 0x10; }
    ImGui::PopStyleVar();
    ImGui::PopID();
}

// --------------------------------------------------------
// MODULE: Hero Image Textures (HeroImages.h + stb_image)
// --------------------------------------------------------

inline int GetBaseHeroImageId(int rawHeroId) {
    if (rawHeroId < 10) return rawHeroId;
    if (rawHeroId >= 10000) return rawHeroId - (rawHeroId / 10000) * 10000 + 10000;
    if (rawHeroId >= 1000) return rawHeroId - (rawHeroId / 1000) * 1000 + 1000;
    if (rawHeroId >= 100) return rawHeroId - (rawHeroId / 100) * 100 + 100;
    return rawHeroId - (rawHeroId / 10) * 10 + 10;
}

// 英雄 ID 首位 = 星级，例如 23546 → 2 星
inline int GetHeroStarLevel(int rawHeroId) {
    if (rawHeroId <= 0) return 0;
    int star = rawHeroId;
    while (star >= 10) star /= 10;
    return std::clamp(star, 1, 3);
}

static void DrawStarGlyph(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 tip[5];
    for (int i = 0; i < 6; i++) {
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
        // GetColorU32 乘上当前 Style.Alpha，透明度滑条对图片同样生效
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

// --------------------------------------------------------
// MODULE: Float Windows (Card Pool, Player Data, Opponent, Hextech)
// --------------------------------------------------------


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
    // 按星级换算实际卡牌数：1★=1张, 2★=3张, 3★=9张
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

            // 预测对手：名字使用动态彩虹闪烁色
            ImU32 rainbow_col = 0;
            if (is_next_opp) {
                float t = ImGui::GetTime() * 4.0f;
                name_col = ImVec4(
                    0.5f + 0.5f * sinf(t),
                    0.5f + 0.5f * sinf(t + 2.094f),
                    0.5f + 0.5f * sinf(t + 4.189f),
                    1.0f);
                rainbow_col = ImGui::GetColorU32(name_col);
            }

            ImVec2 np = ImGui::GetCursorScreenPos();
            ImU32 nc = ImGui::GetColorU32(name_col);
            ImU32 outline = ImGui::GetColorU32(IM_COL32(0, 0, 0, 200));
            const float o = 1.2f * g_autoScale;
            // 8方向描边+伪加粗
            dl->AddText(ImVec2(np.x - o, np.y), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x + o, np.y), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x, np.y - o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x, np.y + o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x - o, np.y - o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x + o, np.y + o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x - o, np.y + o), outline, disp_name_storage.c_str());
            dl->AddText(ImVec2(np.x + o, np.y - o), outline, disp_name_storage.c_str());
            // 核心字体重叠绘制两次实现加粗
            dl->AddText(ImVec2(np.x + 0.5f, np.y), nc, disp_name_storage.c_str());
            dl->AddText(np, nc, disp_name_storage.c_str());
            
            ImVec2 ts = ImGui::CalcTextSize(disp_name_storage.c_str());
            
            if (is_next_opp) {
                // 画指向左侧的彩色大箭头
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

    // 找到自己
    PlayerInfo* me = nullptr;
    for (auto& p : g_players) { if (p.id == g_my_player_id) { me = &p; break; } }
    if (!me) return;

    // 收集自己棋盘+备战席的英雄 baseId
    std::map<int, int> myHeroIds; // baseId -> star level (for display)
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

    // 检查牌库中哪些英雄余量低于阈值
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

    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), (const char*)u8"\u4f59\u91cf\u9884\u8b66"); // 余量预警

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

        // 背景
        dl->AddRectFilled(pMin, pMax, ImGui::GetColorU32(IM_COL32(255, 255, 255, 14)), 8.0f);

        // 英雄图标
        DrawHeroIcon(dl, w.baseId, pMin, pMax, 8.0f * sc, IM_COL32(255, 255, 255, 240));

        // 星级
        DrawHeroStars(dl, ImVec2((pMin.x + pMax.x) * 0.5f, pMax.y - 6.0f * sc), w.star, 5.5f * sc);

        // 彩色闪烁边框
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

        // 余量文字
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

// ==================== IL2CPP Dynamic Symbol & Live Instance Resolver ====================
struct Il2CppApis {
    typedef void* (*domain_get_t)();
    typedef void** (*domain_get_assemblies_t)(void* domain, size_t* size);
    typedef void* (*assembly_get_image_t)(void* assembly);
    typedef const char* (*image_get_name_t)(void* image);
    typedef size_t (*image_get_class_count_t)(void* image);
    typedef void* (*image_get_class_t)(void* image, size_t index);
    typedef const char* (*class_get_name_t)(void* klass);
    typedef const char* (*class_get_namespace_t)(void* klass);
    typedef void* (*class_get_methods_t)(void* klass, void** iter);
    typedef void* (*class_get_fields_t)(void* klass, void** iter);
    typedef const char* (*method_get_name_t)(void* method);
    typedef uint32_t (*method_get_param_count_t)(void* method);
    typedef const char* (*field_get_name_t)(void* field);
    typedef size_t (*field_get_offset_t)(void* field);
    typedef void* (*field_get_type_t)(void* field);
    typedef const char* (*type_get_name_t)(void* type);
    typedef void (*field_static_get_value_t)(void* field, void* value);
    typedef uint32_t (*field_get_flags_t)(void* field);
    typedef void* (*class_get_parent_t)(void* klass);
    typedef void* (*runtime_invoke_t)(void* method, void* obj, void** params, void** exc);
    typedef void* (*object_unbox_t)(void* obj);

    domain_get_t domain_get = nullptr;
    domain_get_assemblies_t domain_get_assemblies = nullptr;
    assembly_get_image_t assembly_get_image = nullptr;
    image_get_name_t image_get_name = nullptr;
    image_get_class_count_t image_get_class_count = nullptr;
    image_get_class_t image_get_class = nullptr;
    class_get_name_t class_get_name = nullptr;
    class_get_namespace_t class_get_namespace = nullptr;
    class_get_methods_t class_get_methods = nullptr;
    class_get_fields_t class_get_fields = nullptr;
    method_get_name_t method_get_name = nullptr;
    method_get_param_count_t method_get_param_count = nullptr;
    field_get_name_t field_get_name = nullptr;
    field_get_offset_t field_get_offset = nullptr;
    field_get_type_t field_get_type = nullptr;
    type_get_name_t type_get_name = nullptr;
    field_static_get_value_t field_static_get_value = nullptr;
    field_get_flags_t field_get_flags = nullptr;
    class_get_parent_t class_get_parent = nullptr;
    runtime_invoke_t runtime_invoke = nullptr;
    object_unbox_t object_unbox = nullptr;
    bool inited = false;

    bool init() {
        if (inited) return true;
        void* h = dlopen("libil2cpp.so", RTLD_LAZY);
        auto resolve = [h](const char* sym) -> void* {
            void* p = DobbySymbolResolver("libil2cpp.so", sym);
            if (!p && h) p = dlsym(h, sym);
            return p;
        };

        domain_get = (domain_get_t)resolve("il2cpp_domain_get");
        domain_get_assemblies = (domain_get_assemblies_t)resolve("il2cpp_domain_get_assemblies");
        assembly_get_image = (assembly_get_image_t)resolve("il2cpp_assembly_get_image");
        image_get_name = (image_get_name_t)resolve("il2cpp_image_get_name");
        image_get_class_count = (image_get_class_count_t)resolve("il2cpp_image_get_class_count");
        image_get_class = (image_get_class_t)resolve("il2cpp_image_get_class");
        class_get_name = (class_get_name_t)resolve("il2cpp_class_get_name");
        class_get_namespace = (class_get_namespace_t)resolve("il2cpp_class_get_namespace");
        class_get_methods = (class_get_methods_t)resolve("il2cpp_class_get_methods");
        class_get_fields = (class_get_fields_t)resolve("il2cpp_class_get_fields");
        method_get_name = (method_get_name_t)resolve("il2cpp_method_get_name");
        method_get_param_count = (method_get_param_count_t)resolve("il2cpp_method_get_param_count");
        field_get_name = (field_get_name_t)resolve("il2cpp_field_get_name");
        field_get_offset = (field_get_offset_t)resolve("il2cpp_field_get_offset");
        field_get_type = (field_get_type_t)resolve("il2cpp_field_get_type");
        type_get_name = (type_get_name_t)resolve("il2cpp_type_get_name");
        field_static_get_value = (field_static_get_value_t)resolve("il2cpp_field_static_get_value");
        field_get_flags = (field_get_flags_t)resolve("il2cpp_field_get_flags");
        class_get_parent = (class_get_parent_t)resolve("il2cpp_class_get_parent");

        inited = (domain_get != nullptr && assembly_get_image != nullptr && class_get_name != nullptr);
        return inited;
    }
};

static Il2CppApis g_il2cpp_api;

static std::unordered_set<void*> g_valid_classes;
static bool g_classes_cached = false;

static void CacheValidClasses() {
    if (g_classes_cached || !g_il2cpp_api.init()) return;
    
    void* domain = g_il2cpp_api.domain_get();
    if (!domain) return;
    
    size_t asm_count = 0;
    void** assemblies = g_il2cpp_api.domain_get_assemblies(domain, &asm_count);
    if (!assemblies) return;
    
    for (size_t a = 0; a < asm_count; a++) {
        void* img = g_il2cpp_api.assembly_get_image(assemblies[a]);
        if (!img) continue;
        size_t cls_count = g_il2cpp_api.image_get_class_count ? g_il2cpp_api.image_get_class_count(img) : 0;
        for (size_t c = 0; c < cls_count; c++) {
            void* klass = g_il2cpp_api.image_get_class(img, c);
            if (klass) g_valid_classes.insert(klass);
        }
    }
    g_classes_cached = true;
}

static std::string SafeReadCString(const char* ptr, size_t maxLen = 128) {
    // 注意: 字符串指针来自 IL2CPP 元数据字符串堆, 其中各字符串紧挨零终止存放,
    // 当上一条字符串长度为奇数时, 下一条字符串的指针地址低 2 位并非 0, 即不满足
    // 4 字节对齐. 因此这里不能用 IsValidPtr(要求 (ptr&0x3)==0) 来过滤, 否则会导致
    // 部分字段名/类型名被误判为无效指针而返回空串, 进而整条字段被丢弃(不显示).
    // 改为只校验空指针与合理地址范围, 真正的字节读取交给 SafeReadMemory 容错.
    if (!ptr || ((uintptr_t)ptr) < 0x1000 || ((uintptr_t)ptr) > 0x00007FFFFFFFFFFF) return "";
    char buf[128] = {0};
    for (size_t i = 0; i < maxLen - 1; i++) {
        char c = 0;
        if (!SafeReadMemory((uintptr_t)ptr + i, &c, 1) || c == 0) {
            buf[i] = 0;
            break;
        }
        buf[i] = c;
    }
    return std::string(buf);
}

static bool IsValidIl2CppClass(void* klass) {
    if (!klass || !IsValidPtr((uintptr_t)klass)) return false;
    if (((uintptr_t)klass & 0x7) != 0) return false;
    
    // 快速缓存匹配
    if (g_valid_classes.find(klass) != g_valid_classes.end()) return true;

    // 动态验证泛型类、实例化类与数组类 (支持 List<T>, Dictionary<K,V>, T[] 等运行时动态生成的泛型类)
    if (g_il2cpp_api.init() && g_il2cpp_api.class_get_name) {
        const char* c_name = g_il2cpp_api.class_get_name(klass);
        if (IsValidPtr((uintptr_t)c_name)) {
            char first_char = 0;
            if (SafeReadMemory((uintptr_t)c_name, &first_char, sizeof(char))) {
                if (first_char != 0) {
                    g_valid_classes.insert(klass);
                    return true;
                }
            }
        }
    }
    return IsValidPtr((uintptr_t)klass);
}

static bool StringEqualsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (::tolower(a[i]) != ::tolower(b[i])) return false;
    }
    return true;
}

static uintptr_t GetSingletonInstance(const char* className) {
    if (!g_il2cpp_api.init() || !g_il2cpp_api.field_static_get_value || !g_il2cpp_api.field_get_flags) return 0;

    void* domain = g_il2cpp_api.domain_get();
    if (!domain) return 0;

    size_t asm_count = 0;
    void** assemblies = g_il2cpp_api.domain_get_assemblies(domain, &asm_count);
    if (!assemblies) return 0;

    std::string searchName = className;
    void* target_klass = nullptr;
    for (size_t a = 0; a < asm_count; a++) {
        void* img = g_il2cpp_api.assembly_get_image(assemblies[a]);
        if (!img) continue;
        size_t cls_count = g_il2cpp_api.image_get_class_count(img);
        for (size_t c = 0; c < cls_count; c++) {
            void* klass = g_il2cpp_api.image_get_class(img, c);
            if (!klass) continue;
            const char* c_name = g_il2cpp_api.class_get_name(klass);
            const char* c_ns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(klass) : "";
            std::string full_class = (c_ns && c_ns[0]) ? (std::string(c_ns) + "." + c_name) : (c_name ? std::string(c_name) : "");

            if ((c_name && StringEqualsIgnoreCase(c_name, searchName)) || StringEqualsIgnoreCase(full_class, searchName)) {
                target_klass = klass;
                break;
            }
        }
        if (target_klass) break;
    }

    if (!target_klass) return 0;

    // 遍历当前类以及父类 (支持 MonoSingleton<T>, Singleton<T> 等通用单例基类)
    for (void* cur_k = target_klass; cur_k != nullptr; cur_k = g_il2cpp_api.class_get_parent ? g_il2cpp_api.class_get_parent(cur_k) : nullptr) {
        void* iter = nullptr;
        while (void* field = g_il2cpp_api.class_get_fields(cur_k, &iter)) {
            uint32_t flags = g_il2cpp_api.field_get_flags(field);
            if ((flags & 0x0010) != 0) { // FIELD_ATTRIBUTE_STATIC
                const char* f_name = g_il2cpp_api.field_get_name(field);
                void* f_type = g_il2cpp_api.field_get_type(field);
                const char* t_name = f_type && g_il2cpp_api.type_get_name ? g_il2cpp_api.type_get_name(f_type) : "";

                if (f_name && (StringEqualsIgnoreCase(f_name, "instance") || StringEqualsIgnoreCase(f_name, "instance_") || 
                               StringEqualsIgnoreCase(f_name, "_instance") || StringEqualsIgnoreCase(f_name, "m_instance") || 
                               StringEqualsIgnoreCase(f_name, "s_instance") || StringEqualsIgnoreCase(f_name, "current") ||
                               StringEqualsIgnoreCase(f_name, "_current") || StringEqualsIgnoreCase(f_name, "s_current") ||
                               (t_name && StringEqualsIgnoreCase(t_name, className)))) {
                    
                    uintptr_t inst_ptr = 0;
                    g_il2cpp_api.field_static_get_value(field, &inst_ptr);
                    if (IsValidPtr(inst_ptr)) {
                        return inst_ptr;
                    }
                }
            }
        }
    }
    return 0;
}

std::string CleanIl2CppTypeName(const std::string& raw) {
    if (raw.empty()) return "";
    std::string s = raw;
    while (!s.empty() && s.back() == '*') s.pop_back();
    const char* prefixes[] = {
        "System.Collections.Generic.",
        "System.Collections.",
        "System.",
        "UnityEngine."
    };
    for (const char* p : prefixes) {
        size_t plen = strlen(p);
        size_t pos = 0;
        while ((pos = s.find(p, pos)) != std::string::npos) {
            s.erase(pos, plen);
        }
    }
    return s;
}

struct ObjectPathStep {
    uintptr_t fromObj;
    std::string fromClass;
    std::string fieldName;
    std::string rawType;
    std::string cleanType;
    size_t offset;
    uintptr_t toObj;
    std::string toClass;
    bool isValueField;
    uintptr_t rawVal;
    std::string formattedVal;
};

struct FoundPath {
    std::string matchDesc;
    uintptr_t targetInstance;
    std::vector<ObjectPathStep> steps;
    std::string fieldName;
    std::string cleanType;
    size_t offset;
    uintptr_t fieldAddress;
    std::string formattedVal;
    int32_t intVal;
    bool isIntField;
};

struct ObjectPathFindingResult {
    bool found;
    std::string targetKeyword;
    std::vector<FoundPath> paths;
    std::vector<FoundPath> exploredChains; // 记录所有深度探索过的分支链条 (即使未搜到也全量呈现)
};

static static std::atomic<bool> g_is_searching_path{false};
static std::atomic<int> g_search_progress_nodes{0};
ObjectPathFindingResult g_lastPathResult;

static int g_search_max_depth = 12;

static std::string SafeToLower(const std::string& str) {
    std::string res = str;
    for (size_t i = 0; i < res.size(); i++) {
        unsigned char c = (unsigned char)res[i];
        if (c >= 'A' && c <= 'Z') res[i] = (char)(c + 32);
    }
    return res;
}

static bool SmartStringMatch(const std::string& candidate, const std::string& target) {
    if (target.empty()) return true;
    if (candidate.empty()) return false;
    
    std::string c_low = SafeToLower(candidate);
    std::string t_low = SafeToLower(target);
    
    // 1. 标准子串不区分大小写包含 (如 "ZGameChess.ChessModelManager" 匹配 "Chess", "Model", "Manager", "zgame")
    // 或者 "我是小红" 包含 "我是", "小红"
    if (c_low.find(t_low) != std::string::npos) return true;
    if (t_low.find(c_low) != std::string::npos && c_low.length() >= 3) return true;
    
    // 2. 中文字符单字模糊匹配 (★ 严格限定仅对 UTF-8 中文多字节汉字字符生效，绝不能对单个英文字母 'e'/'a' 乱匹配！)
    size_t i = 0;
    while (i < target.length()) {
        unsigned char lead = (unsigned char)target[i];
        size_t charLen = 1;
        if (lead >= 0xF0) charLen = 4;
        else if (lead >= 0xE0) charLen = 3;
        else if (lead >= 0xC0) charLen = 2;
        else charLen = 1;
        
        if (i + charLen <= target.length()) {
            // 只有多字节字符（UTF-8 中文字符 charLen >= 2）才允许单字匹配
            if (charLen >= 2) {
                std::string singleChineseChar = target.substr(i, charLen);
                if (candidate.find(singleChineseChar) != std::string::npos) return true;
            }
        }
        i += charLen;
    }

    // 3. 空格分词英文匹配 (例如输入 "Chess Manager"，若 candidate 包含 "Chess" 且包含 "Manager")
    if (t_low.find(' ') != std::string::npos) {
        std::istringstream iss(t_low);
        std::string token;
        bool allTokensMatch = true;
        int tokenCount = 0;
        while (iss >> token) {
            tokenCount++;
            if (c_low.find(token) == std::string::npos) {
                allTokensMatch = false;
                break;
            }
        }
        if (tokenCount > 0 && allTokensMatch) return true;
    }

    return false;
}

ObjectPathFindingResult AutoFindPath(uintptr_t rootObj, const std::string& targetName, int maxDepth = 12) {
    ObjectPathFindingResult result;
    result.found = false;
    result.targetKeyword = targetName;

    if (!IsValidPtr(rootObj) || !g_il2cpp_api.init()) return result;

    std::string target_raw = targetName;
    while (!target_raw.empty() && isspace((unsigned char)target_raw.front())) target_raw.erase(target_raw.begin());
    while (!target_raw.empty() && isspace((unsigned char)target_raw.back())) target_raw.pop_back();

    std::string target_lower = SafeToLower(target_raw);

    // 数值解析 (支持 10进制、16进制)
    bool hasTargetInt = false;
    int64_t targetIntVal = 0;
    if (!target_raw.empty()) {
        try {
            size_t idx = 0;
            targetIntVal = (int64_t)std::stoll(target_raw, &idx, 0);
            if (idx == target_raw.length()) hasTargetInt = true;
        } catch (...) {}
    }

    bool hasTargetHexPtr = false;
    uintptr_t targetHexPtr = 0;
    if (target_lower.find("0x") == 0) {
        try {
            targetHexPtr = (uintptr_t)std::stoull(target_raw, nullptr, 16);
            hasTargetHexPtr = true;
        } catch (...) {}
    }

    struct QueueItem {
        uintptr_t obj;
        std::string className;
        std::vector<ObjectPathStep> path;
        int depth;
    };

    std::deque<QueueItem> queue;
    std::unordered_set<uintptr_t> visited;
    std::unordered_set<std::string> recordedPaths;
    std::unordered_set<std::string> recordedExplored;

    auto GetObjClassName = [](uintptr_t ptr) -> std::string {
        if (!IsValidPtr(ptr)) return "";
        void* klass_ptr = nullptr;
        if (!SafeReadMemory(ptr, &klass_ptr, sizeof(void*)) || !IsValidIl2CppClass(klass_ptr)) return "";
        const char* c_name = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(klass_ptr) : nullptr;
        const char* c_ns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(klass_ptr) : nullptr;
        std::string name_str = SafeReadCString(c_name);
        std::string ns_str = SafeReadCString(c_ns);
        return (!ns_str.empty()) ? (ns_str + "." + name_str) : name_str;
    };

    std::string rootClass = GetObjClassName(rootObj);
    if (rootClass.empty()) rootClass = "RootObject";

    // 检查根对象自身是否直接匹配
    if (!target_raw.empty() && SmartStringMatch(rootClass, target_raw)) {
        FoundPath fp;
        fp.matchDesc = (const char*)u8"[起点类名匹配] " + rootClass;
        fp.targetInstance = rootObj;
        fp.fieldName = rootClass;
        fp.cleanType = CleanIl2CppTypeName(rootClass);
        fp.offset = 0;
        fp.fieldAddress = rootObj;
        char buf[64]; snprintf(buf, sizeof(buf), "0x%lx", rootObj);
        fp.formattedVal = buf;
        fp.isIntField = false;
        result.paths.push_back(fp);
        result.found = true;
    }

    queue.push_back({ rootObj, rootClass, {}, 0 });
    visited.insert(rootObj);

    int nodesProcessed = 0;
    const int maxNodes = 35000;

    while (!queue.empty() && nodesProcessed < maxNodes && result.paths.size() < 1500) {
        QueueItem item = queue.front();
        queue.pop_front();
        nodesProcessed++;
        g_search_progress_nodes = nodesProcessed;

        if (item.depth >= maxDepth) continue;
        if (!IsValidPtr(item.obj)) continue;

        void* klass_ptr = nullptr;
        SafeReadMemory(item.obj, &klass_ptr, sizeof(void*));

        // 记录探测过的分支链条 (Explored Chain)
        if (!item.path.empty() && result.exploredChains.size() < 60) {
            std::string sig = "";
            for (const auto& s : item.path) { sig += s.fromClass + ":" + std::to_string(s.offset) + "->"; }
            if (recordedExplored.find(sig) == recordedExplored.end()) {
                recordedExplored.insert(sig);
                FoundPath fp;
                fp.matchDesc = (const char*)u8"[探测分支] " + item.className;
                fp.targetInstance = item.obj;
                fp.steps = item.path;
                fp.fieldName = item.className;
                fp.cleanType = CleanIl2CppTypeName(item.className);
                fp.offset = item.path.back().offset;
                fp.fieldAddress = item.obj;
                char buf[64]; snprintf(buf, sizeof(buf), "0x%lx", item.obj);
                fp.formattedVal = buf;
                fp.isIntField = false;
                result.exploredChains.push_back(fp);
            }
        }

        std::unordered_set<size_t> exploredOffsets;

        // ==========================================
        // 1. 数组类型全量解包 (Array / T[])
        // ==========================================
        bool isArray = (item.className.find("[]") != std::string::npos || item.className.find("Array") != std::string::npos || item.className == "System.Array");
        if (isArray) {
            uint32_t len_18 = 0, len_10 = 0;
            SafeReadMemory(item.obj + 0x18, &len_18, sizeof(uint32_t));
            SafeReadMemory(item.obj + 0x10, &len_10, sizeof(uint32_t));
            
            uint32_t arr_len = 0;
            size_t elem_start_off = 0x20;
            if (len_18 > 0 && len_18 <= 5000) { arr_len = len_18; elem_start_off = 0x20; }
            else if (len_10 > 0 && len_10 <= 5000) { arr_len = len_10; elem_start_off = 0x18; }

            if (arr_len > 0) {
                for (uint32_t i = 0; i < arr_len && i < 64; i++) {
                    size_t elem_off = elem_start_off + i * sizeof(uintptr_t);
                    uintptr_t elem_ptr = 0;
                    SafeReadMemory(item.obj + elem_off, &elem_ptr, sizeof(uintptr_t));
                    int32_t elem_val32 = 0;
                    SafeReadMemory(item.obj + elem_off, &elem_val32, sizeof(int32_t));

                    size_t elem_off4 = elem_start_off + i * 4;
                    int32_t elem_val32_stride4 = 0;
                    SafeReadMemory(item.obj + elem_off4, &elem_val32_stride4, sizeof(int32_t));

                    std::string idx_name = "[" + std::to_string(i) + "]";
                    bool intMatch = hasTargetInt && (elem_val32 == targetIntVal || (int64_t)elem_ptr == targetIntVal || elem_val32_stride4 == targetIntVal);

                    if (IsValidPtr(elem_ptr)) {
                        std::string elem_str = ReadIl2CppString(elem_ptr);
                        bool strMatch = (!elem_str.empty() && !target_raw.empty() && SmartStringMatch(elem_str, target_raw));

                        std::string elemClass = GetObjClassName(elem_ptr);
                        std::string elem_clean = CleanIl2CppTypeName(elemClass);
                        bool classMatch = (!target_raw.empty() && SmartStringMatch(elemClass, target_raw));

                        char fbuf[96];
                        if (!elem_str.empty()) snprintf(fbuf, sizeof(fbuf), "\"%s\"", elem_str.c_str());
                        else snprintf(fbuf, sizeof(fbuf), "0x%lx (%s)", elem_ptr, elem_clean.c_str());

                        std::vector<ObjectPathStep> nextPath = item.path;
                        nextPath.push_back({ item.obj, item.className, idx_name, elem_str.empty() ? elemClass : "String", elem_str.empty() ? elem_clean : "String", elem_off, elem_ptr, elem_str.empty() ? (elemClass.empty() ? elem_clean : elemClass) : ("\"" + elem_str + "\""), false, elem_ptr, fbuf });

                        if (classMatch || intMatch || strMatch) {
                            std::string sig = "";
                            for (const auto& s : nextPath) { sig += s.fromClass + ":" + std::to_string(s.offset) + "->"; }
                            if (recordedPaths.find(sig) == recordedPaths.end()) {
                                recordedPaths.insert(sig);
                                FoundPath fp;
                                if (strMatch) fp.matchDesc = (const char*)u8"[文本值命中: \"" + elem_str + "\"] " + idx_name;
                                else if (intMatch) fp.matchDesc = (const char*)u8"[数组数值命中: " + target_raw + "] " + idx_name;
                                else fp.matchDesc = (const char*)u8"[数组元素匹配] " + idx_name + " (" + elem_clean + ")";
                                fp.targetInstance = elem_ptr;
                                fp.steps = nextPath;
                                fp.fieldName = idx_name;
                                fp.cleanType = elem_str.empty() ? elem_clean : "String";
                                fp.offset = elem_off;
                                fp.fieldAddress = item.obj + elem_off;
                                fp.formattedVal = fbuf;
                                fp.intVal = elem_val32;
                                fp.isIntField = intMatch;

                                result.paths.push_back(fp);
                                result.found = true;
                            }
                        }

                        if (!elemClass.empty() && elem_str.empty() && visited.find(elem_ptr) == visited.end() && (item.depth + 1 < maxDepth)) {
                            visited.insert(elem_ptr);
                            queue.push_back({ elem_ptr, elemClass, nextPath, item.depth + 1 });
                        }
                    }
                }
            }
        }

        // ==========================================
        // 2. 列表 List<T> 快速穿透底层 _items
        // ==========================================
        if (item.className.find("List") != std::string::npos) {
            uintptr_t items_arr = 0;
            SafeReadMemory(item.obj + 0x10, &items_arr, sizeof(uintptr_t));
            if (!IsValidPtr(items_arr)) SafeReadMemory(item.obj + 0x18, &items_arr, sizeof(uintptr_t));

            if (IsValidPtr(items_arr) && visited.find(items_arr) == visited.end()) {
                visited.insert(items_arr);
                std::vector<ObjectPathStep> nextPath = item.path;
                nextPath.push_back({ item.obj, item.className, "_items", "Array", "Array", 0x10, items_arr, "Array", false, items_arr, "ItemsArray" });
                queue.push_back({ items_arr, "Array", nextPath, item.depth });
            }
        }

        // ==========================================
        // 3. 字典 Dictionary<TKey, TValue> 专用 Entry 结构体解包
        // ==========================================
        if (item.className.find("Dictionary") != std::string::npos) {
            uintptr_t entries_arr = 0;
            int32_t dict_count = 0;
            SafeReadMemory(item.obj + 0x18, &entries_arr, sizeof(uintptr_t));
            SafeReadMemory(item.obj + 0x20, &dict_count, sizeof(int32_t));
            if (!IsValidPtr(entries_arr)) {
                SafeReadMemory(item.obj + 0x10, &entries_arr, sizeof(uintptr_t));
                SafeReadMemory(item.obj + 0x18, &dict_count, sizeof(int32_t));
            }

            if (IsValidPtr(entries_arr) && dict_count > 0 && dict_count <= 2000) {
                size_t entry_stride = 24;
                for (int i = 0; i < dict_count && i < 64; i++) {
                    uintptr_t entry_addr = entries_arr + 0x20 + i * entry_stride;
                    int32_t hash = 0;
                    SafeReadMemory(entry_addr, &hash, sizeof(int32_t));
                    uintptr_t key_ptr = 0;
                    SafeReadMemory(entry_addr + 0x08, &key_ptr, sizeof(uintptr_t));
                    uintptr_t val_ptr = 0;
                    SafeReadMemory(entry_addr + 0x10, &val_ptr, sizeof(uintptr_t));
                    int32_t val_int = 0;
                    SafeReadMemory(entry_addr + 0x10, &val_int, sizeof(int32_t));

                    std::string key_label = "Dict[" + std::to_string(i) + "]";
                    std::string k_str = "";
                    if (IsValidPtr(key_ptr)) {
                        k_str = ReadIl2CppString(key_ptr);
                        if (!k_str.empty()) key_label = "[\"" + k_str + "\"]";
                    }

                    bool keyMatch = (!target_raw.empty() && (SmartStringMatch(key_label, target_raw) || (!k_str.empty() && SmartStringMatch(k_str, target_raw))));
                    bool valIntMatch = hasTargetInt && (val_int == targetIntVal || (int64_t)val_ptr == targetIntVal);

                    if (IsValidPtr(val_ptr)) {
                        std::string val_str = ReadIl2CppString(val_ptr);
                        bool valStrMatch = (!val_str.empty() && !target_raw.empty() && SmartStringMatch(val_str, target_raw));

                        std::string valClass = GetObjClassName(val_ptr);
                        std::string val_clean = CleanIl2CppTypeName(valClass);
                        bool classMatch = (!target_raw.empty() && SmartStringMatch(valClass, target_raw));

                        char fbuf[96];
                        if (!val_str.empty()) snprintf(fbuf, sizeof(fbuf), "\"%s\"", val_str.c_str());
                        else snprintf(fbuf, sizeof(fbuf), "0x%lx (%s)", val_ptr, val_clean.c_str());

                        std::vector<ObjectPathStep> nextPath = item.path;
                        nextPath.push_back({ item.obj, item.className, key_label, val_str.empty() ? valClass : "String", val_str.empty() ? val_clean : "String", 0x18, val_ptr, val_str.empty() ? (valClass.empty() ? val_clean : valClass) : ("\"" + val_str + "\""), false, val_ptr, fbuf });

                        if (keyMatch || classMatch || valIntMatch || valStrMatch) {
                            std::string sig = "";
                            for (const auto& s : nextPath) { sig += s.fromClass + ":" + std::to_string(s.offset) + "->"; }
                            if (recordedPaths.find(sig) == recordedPaths.end()) {
                                recordedPaths.insert(sig);
                                FoundPath fp;
                                if (valStrMatch) fp.matchDesc = (const char*)u8"[字典值文本命中: \"" + val_str + "\"] " + key_label;
                                else if (valIntMatch) fp.matchDesc = (const char*)u8"[字典数值命中: " + target_raw + "] " + key_label;
                                else if (classMatch) fp.matchDesc = (const char*)u8"[字典值类名匹配] " + key_label + " -> " + val_clean;
                                else fp.matchDesc = (const char*)u8"[字典键匹配] " + key_label;

                                fp.targetInstance = val_ptr;
                                fp.steps = nextPath;
                                fp.fieldName = key_label;
                                fp.cleanType = val_str.empty() ? val_clean : "String";
                                fp.offset = 0x18;
                                fp.fieldAddress = val_ptr;
                                fp.formattedVal = fbuf;
                                fp.intVal = val_int;
                                fp.isIntField = valIntMatch;

                                result.paths.push_back(fp);
                                result.found = true;
                            }
                        }

                        if (!valClass.empty() && val_str.empty() && visited.find(val_ptr) == visited.end() && (item.depth + 1 < maxDepth)) {
                            visited.insert(val_ptr);
                            queue.push_back({ val_ptr, valClass, nextPath, item.depth + 1 });
                        }
                    }
                }
            }
        }

        // ==========================================
        // 4. 原生反射遍历类与全部父类字段
        // ==========================================
        if (klass_ptr && IsValidIl2CppClass(klass_ptr) && g_il2cpp_api.class_get_fields) {
            int p_depth = 0;
            for (void* cur_klass = klass_ptr; cur_klass != nullptr && IsValidIl2CppClass(cur_klass) && p_depth < 20; p_depth++, cur_klass = (g_il2cpp_api.class_get_parent ? g_il2cpp_api.class_get_parent(cur_klass) : nullptr)) {
                void* iter = nullptr;
                int field_count = 0;
                while (field_count < 8000 && (field_count++, true)) {
                    void* field = g_il2cpp_api.class_get_fields(cur_klass, &iter);
                    if (!field) break;

                    uint32_t flags = g_il2cpp_api.field_get_flags ? g_il2cpp_api.field_get_flags(field) : 0;
                    if ((flags & 0x0010) != 0) continue; // 跳过静态字段

                    const char* f_name = g_il2cpp_api.field_get_name ? g_il2cpp_api.field_get_name(field) : nullptr;
                    size_t f_offset = g_il2cpp_api.field_get_offset ? g_il2cpp_api.field_get_offset(field) : 0;
                    void* f_type = g_il2cpp_api.field_get_type ? g_il2cpp_api.field_get_type(field) : nullptr;
                    const char* t_name = (f_type && g_il2cpp_api.type_get_name) ? g_il2cpp_api.type_get_name(f_type) : nullptr;

                    std::string field_str = SafeReadCString(f_name);
                    std::string type_str = SafeReadCString(t_name);
                    std::string clean_type = CleanIl2CppTypeName(type_str);

                    if (field_str.empty()) continue;
                    exploredOffsets.insert(f_offset);

                    uintptr_t rawVal = 0;
                    SafeReadMemory(item.obj + f_offset, &rawVal, sizeof(uintptr_t));
                    int32_t val32 = 0;
                    SafeReadMemory(item.obj + f_offset, &val32, sizeof(int32_t));
                    int64_t val64 = 0;
                    SafeReadMemory(item.obj + f_offset, &val64, sizeof(int64_t));

                    std::string formattedVal = "";
                    std::string str_content = "";
                    bool isIntField = false;

                    if (IsValidPtr(rawVal)) {
                        str_content = ReadIl2CppString(rawVal);
                        if (!str_content.empty()) {
                            formattedVal = "\"" + str_content + "\"";
                        }
                    }

                    if (str_content.empty()) {
                        if (clean_type == "String" || type_str.find("String") != std::string::npos) {
                            formattedVal = IsValidPtr(rawVal) ? "\"\"" : "Null";
                        } else if (clean_type == "Int32" || clean_type == "UInt32") {
                            isIntField = true;
                            char buf[32]; snprintf(buf, sizeof(buf), "%d", val32);
                            formattedVal = buf;
                        } else if (clean_type == "Int64" || clean_type == "UInt64") {
                            isIntField = true;
                            char buf[32]; snprintf(buf, sizeof(buf), "%ld", (int64_t)rawVal);
                            formattedVal = buf;
                        } else if (clean_type == "Boolean") {
                            formattedVal = (rawVal & 0xFF) ? "true" : "false";
                        } else if (clean_type == "Single") {
                            float fval = 0; memcpy(&fval, &rawVal, sizeof(float));
                            char buf[32]; snprintf(buf, sizeof(buf), "%.2f", fval);
                            formattedVal = buf;
                        } else {
                            if (rawVal == 0) formattedVal = "Null";
                            else {
                                char buf[64]; snprintf(buf, sizeof(buf), "0x%lx", rawVal);
                                formattedVal = buf;
                            }
                        }
                    }

                    bool fieldMatch = false;
                    bool typeMatch = false;
                    bool stringValMatch = false;
                    bool intValMatch = false;
                    bool classMatch = false;
                    bool isAnyMatch = false;

                    if (target_raw.empty()) {
                        isAnyMatch = true;
                    } else {
                        if (SmartStringMatch(field_str, target_raw)) fieldMatch = true;
                        if (SmartStringMatch(clean_type, target_raw)) typeMatch = true;
                        if (!str_content.empty() && SmartStringMatch(str_content, target_raw)) stringValMatch = true;

                        if (hasTargetInt) {
                            if (val32 == targetIntVal || (int64_t)rawVal == targetIntVal) intValMatch = true;
                        }

                        bool offsetMatch = (target_lower == ("0x" + std::to_string(f_offset)) || target_lower == ("+" + std::to_string(f_offset)));
                        bool ptrMatch = hasTargetHexPtr && (rawVal == targetHexPtr || (item.obj + f_offset) == targetHexPtr);
                        bool boolMatch = (target_lower == "true" && (rawVal & 0xFF) != 0) || (target_lower == "false" && (rawVal & 0xFF) == 0 && clean_type == "Boolean");
                        
                        isAnyMatch = fieldMatch || typeMatch || stringValMatch || intValMatch || offsetMatch || ptrMatch || boolMatch;
                    }

                    uintptr_t child_ptr = 0;
                    bool isObjectRef = (IsValidPtr(item.obj + f_offset) && SafeReadMemory(item.obj + f_offset, &child_ptr, sizeof(uintptr_t)) && IsValidPtr(child_ptr));

                    if (isObjectRef) {
                        std::string childClass = GetObjClassName(child_ptr);

                        if (!target_raw.empty() && SmartStringMatch(childClass, target_raw)) {
                            classMatch = true;
                            isAnyMatch = true;
                        }

                        std::vector<ObjectPathStep> nextPath = item.path;
                        nextPath.push_back({ item.obj, item.className, field_str, type_str, clean_type, f_offset, child_ptr, childClass.empty() ? clean_type : childClass, false, rawVal, formattedVal });

                        if (isAnyMatch) {
                            std::string sig = "";
                            for (const auto& s : nextPath) { sig += s.fromClass + ":" + std::to_string(s.offset) + "->"; }
                            if (recordedPaths.find(sig) == recordedPaths.end()) {
                                recordedPaths.insert(sig);
                                FoundPath fp;
                                if (stringValMatch) fp.matchDesc = (const char*)u8"[文本值命中: \"" + str_content + "\"] " + field_str;
                                else if (intValMatch) fp.matchDesc = (const char*)u8"[数值/金币命中: " + target_raw + "] " + field_str;
                                else if (classMatch) fp.matchDesc = (const char*)u8"[类名匹配] " + (childClass.empty() ? clean_type : childClass);
                                else if (fieldMatch) fp.matchDesc = (const char*)u8"[字段匹配] " + field_str + " (" + clean_type + ")";
                                else if (typeMatch) fp.matchDesc = (const char*)u8"[类型匹配] " + clean_type;
                                else fp.matchDesc = (const char*)u8"[匹配成功] " + field_str;

                                fp.targetInstance = child_ptr;
                                fp.steps = nextPath;
                                fp.fieldName = field_str;
                                fp.cleanType = clean_type;
                                fp.offset = f_offset;
                                fp.fieldAddress = item.obj + f_offset;
                                fp.formattedVal = formattedVal;
                                fp.intVal = val32;
                                fp.isIntField = isIntField;

                                result.paths.push_back(fp);
                                result.found = true;
                            }
                        }

                        if (!childClass.empty() && str_content.empty() && visited.find(child_ptr) == visited.end() && (item.depth + 1 < maxDepth)) {
                            visited.insert(child_ptr);
                            queue.push_back({ child_ptr, childClass, nextPath, item.depth + 1 });
                        }
                    } else {
                        if (isAnyMatch) {
                            std::vector<ObjectPathStep> nextPath = item.path;
                            nextPath.push_back({ item.obj, item.className, field_str, type_str, clean_type, f_offset, item.obj + f_offset, clean_type.empty() ? "(值字段)" : clean_type, true, rawVal, formattedVal });

                            std::string sig = "";
                            for (const auto& s : nextPath) { sig += s.fromClass + ":" + std::to_string(s.offset) + "->"; }
                            if (recordedPaths.find(sig) == recordedPaths.end()) {
                                recordedPaths.insert(sig);
                                FoundPath fp;
                                if (stringValMatch) fp.matchDesc = (const char*)u8"[文本值命中: \"" + str_content + "\"] " + field_str;
                                else if (intValMatch) fp.matchDesc = (const char*)u8"[数值/金币命中: " + target_raw + "] " + field_str;
                                else if (fieldMatch) fp.matchDesc = (const char*)u8"[字段属性] " + field_str + " (" + (clean_type.empty() ? "value" : clean_type) + ")";
                                else if (typeMatch) fp.matchDesc = (const char*)u8"[类型匹配] " + clean_type;
                                else fp.matchDesc = (const char*)u8"[匹配成功] " + field_str;

                                fp.targetInstance = item.obj + f_offset;
                                fp.steps = nextPath;
                                fp.fieldName = field_str;
                                fp.cleanType = clean_type;
                                fp.offset = f_offset;
                                fp.fieldAddress = item.obj + f_offset;
                                fp.formattedVal = formattedVal;
                                fp.intVal = val32;
                                fp.isIntField = isIntField;

                                result.paths.push_back(fp);
                                result.found = true;
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 5. 穿透所有未映射物理内存槽位 (+0x10 ~ +0x1E0) 全量深钻 (保证穿透 +0x10 addr2 等非反射主线指针)
        // ==========================================
        for (size_t raw_off = 0x00; raw_off <= 0x2E0; raw_off += sizeof(uintptr_t)) {
            if (exploredOffsets.find(raw_off) != exploredOffsets.end()) continue;

            uintptr_t r_ptr = 0;
            SafeReadMemory(item.obj + raw_off, &r_ptr, sizeof(uintptr_t));
            int32_t r_val32 = 0;
            SafeReadMemory(item.obj + raw_off, &r_val32, sizeof(int32_t));

            std::string slot_name = "Slot[+0x" + std::to_string(raw_off) + "]";
            std::string raw_str = "";
            bool strMatch = false;

            if (IsValidPtr(r_ptr)) {
                raw_str = ReadIl2CppString(r_ptr);
                if (!raw_str.empty() && !target_raw.empty() && SmartStringMatch(raw_str, target_raw)) {
                    strMatch = true;
                }
            }

            bool intMatch = hasTargetInt && (r_val32 == targetIntVal || (int64_t)r_ptr == targetIntVal);

            if (IsValidPtr(r_ptr)) {
                std::string childClass = GetObjClassName(r_ptr);
                std::string cleanType = CleanIl2CppTypeName(childClass);
                bool classMatch = (!target_raw.empty() && !childClass.empty() && SmartStringMatch(childClass, target_raw));

                char fbuf[96];
                if (!raw_str.empty()) snprintf(fbuf, sizeof(fbuf), "\"%s\"", raw_str.c_str());
                else snprintf(fbuf, sizeof(fbuf), "0x%lx (%s)", r_ptr, cleanType.c_str());

                std::vector<ObjectPathStep> nextPath = item.path;
                nextPath.push_back({ item.obj, item.className, slot_name, raw_str.empty() ? childClass : "String", raw_str.empty() ? cleanType : "String", raw_off, r_ptr, raw_str.empty() ? (childClass.empty() ? cleanType : childClass) : ("\"" + raw_str + "\""), false, r_ptr, fbuf });

                if (strMatch || intMatch || classMatch) {
                    std::string sig = "";
                    for (const auto& s : nextPath) { sig += s.fromClass + ":" + std::to_string(s.offset) + "->"; }
                    if (recordedPaths.find(sig) == recordedPaths.end()) {
                        recordedPaths.insert(sig);
                        FoundPath fp;
                        if (strMatch) fp.matchDesc = (const char*)u8"[内存物理文本命中: \"" + raw_str + "\"] " + slot_name;
                        else if (intMatch) fp.matchDesc = (const char*)u8"[内存物理数值命中: " + target_raw + "] " + slot_name;
                        else fp.matchDesc = (const char*)u8"[未反射类指针匹配] " + cleanType + " " + slot_name;

                        fp.targetInstance = r_ptr;
                        fp.steps = nextPath;
                        fp.fieldName = slot_name;
                        fp.cleanType = raw_str.empty() ? cleanType : "String";
                        fp.offset = raw_off;
                        fp.fieldAddress = item.obj + raw_off;
                        fp.formattedVal = fbuf;
                        fp.intVal = r_val32;
                        fp.isIntField = intMatch;

                        result.paths.push_back(fp);
                        result.found = true;
                    }
                }

                // 将未反射的指针槽位（如 +0x10）也加入 BFS 队列深钻！
                if (!childClass.empty() && raw_str.empty() && visited.find(r_ptr) == visited.end() && (item.depth + 1 < maxDepth)) {
                    visited.insert(r_ptr);
                    queue.push_back({ r_ptr, childClass, nextPath, item.depth + 1 });
                }
            } else if (intMatch) {
                std::vector<ObjectPathStep> nextPath = item.path;
                char fbuf[32]; snprintf(fbuf, sizeof(fbuf), "%d", r_val32);
                nextPath.push_back({ item.obj, item.className, slot_name, "Int32", "Int32", raw_off, item.obj + raw_off, "Int32", true, (uintptr_t)r_val32, fbuf });

                std::string sig = "";
                for (const auto& s : nextPath) { sig += s.fromClass + ":" + std::to_string(s.offset) + "->"; }
                if (recordedPaths.find(sig) == recordedPaths.end()) {
                    recordedPaths.insert(sig);
                    FoundPath fp;
                    fp.matchDesc = (const char*)u8"[内存物理数值命中: " + target_raw + "] " + slot_name;
                    fp.targetInstance = item.obj + raw_off;
                    fp.steps = nextPath;
                    fp.fieldName = slot_name;
                    fp.cleanType = "Int32";
                    fp.offset = raw_off;
                    fp.fieldAddress = item.obj + raw_off;
                    fp.formattedVal = fbuf;
                    fp.intVal = r_val32;
                    fp.isIntField = true;

                    result.paths.push_back(fp);
                    result.found = true;
                }
            }
        }
    }

    // 严格按偏移升序排序
    std::sort(result.paths.begin(), result.paths.end(), [](const FoundPath& a, const FoundPath& b) {
        return a.offset < b.offset;
    });

    return result;
}

struct ClassInspectInfo {
    std::string className;
    std::string imageName;
    uintptr_t classAddress;
    uintptr_t classRva;
    struct MethodEntry { std::string name; uintptr_t rva; };
    struct FieldEntry { std::string name; std::string typeName; size_t offset; };
    std::vector<MethodEntry> methods;
    std::vector<FieldEntry> fields;
    bool valid;
};

static ClassInspectInfo g_inspectedClass;
static char g_class_search_input[64] = "ZGameChess.ChessModelManager";

ClassInspectInfo InspectClassByFullName(const std::string& targetName) {
    ClassInspectInfo info;
    info.className = targetName;
    info.classAddress = 0;
    info.classRva = 0;
    info.valid = false;

    if (targetName.empty() || !g_il2cpp_api.init()) return info;

    void* domain = g_il2cpp_api.domain_get();
    if (!domain) return info;

    size_t asm_count = 0;
    void** assemblies = g_il2cpp_api.domain_get_assemblies(domain, &asm_count);
    if (!assemblies) return info;

    std::string target_lower = targetName;
    std::transform(target_lower.begin(), target_lower.end(), target_lower.begin(), ::tolower);

    for (size_t a = 0; a < asm_count; a++) {
        void* img = g_il2cpp_api.assembly_get_image(assemblies[a]);
        if (!img) continue;
        const char* img_name = g_il2cpp_api.image_get_name ? g_il2cpp_api.image_get_name(img) : "";
        size_t cls_count = g_il2cpp_api.image_get_class_count ? g_il2cpp_api.image_get_class_count(img) : 0;

        for (size_t c = 0; c < cls_count; c++) {
            void* klass = g_il2cpp_api.image_get_class(img, c);
            if (!klass) continue;

            const char* c_name = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(klass) : "";
            const char* c_ns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(klass) : "";
            std::string full_class = (c_ns && c_ns[0]) ? (std::string(c_ns) + "." + c_name) : std::string(c_name);
            std::string full_class_lower = full_class;
            std::transform(full_class_lower.begin(), full_class_lower.end(), full_class_lower.begin(), ::tolower);

            if (full_class_lower == target_lower || full_class_lower.find(target_lower) != std::string::npos || target_lower.find(full_class_lower) != std::string::npos) {
                info.className = full_class;
                info.imageName = img_name ? img_name : "";
                info.classAddress = (uintptr_t)klass;
                info.classRva = info.classAddress > g_il2cppTrueBase ? (info.classAddress - g_il2cppTrueBase) : 0;
                info.valid = true;

                if (g_il2cpp_api.class_get_methods && g_il2cpp_api.method_get_name) {
                    void* iter = nullptr;
                    while (void* method = g_il2cpp_api.class_get_methods(klass, &iter)) {
                        const char* m_name = g_il2cpp_api.method_get_name(method);
                        uintptr_t func_ptr = *(uintptr_t*)method;
                        uintptr_t rva = func_ptr > g_il2cppTrueBase ? (func_ptr - g_il2cppTrueBase) : 0;
                        info.methods.push_back({ m_name ? m_name : "", rva });
                    }
                }

                if (g_il2cpp_api.class_get_fields && g_il2cpp_api.field_get_name && g_il2cpp_api.field_get_offset) {
                    void* iter = nullptr;
                    while (void* field = g_il2cpp_api.class_get_fields(klass, &iter)) {
                        const char* f_name = g_il2cpp_api.field_get_name(field);
                        size_t f_offset = g_il2cpp_api.field_get_offset(field);
                        void* f_type = g_il2cpp_api.field_get_type ? g_il2cpp_api.field_get_type(field) : nullptr;
                        const char* t_name = (f_type && g_il2cpp_api.type_get_name) ? g_il2cpp_api.type_get_name(f_type) : "var";
                        info.fields.push_back({ f_name ? f_name : "", t_name ? t_name : "", f_offset });
                    }
                }
                return info;
            }
        }
    }
    return info;
}

static char g_root_class_input[128] = "ChessModelManager";

// ==================== ImGui Virtual Keyboard ====================
static char* g_vkbd_target = nullptr;
static size_t g_vkbd_target_size = 0;
static bool g_show_vkbd = false;
static bool g_vkbd_caps = false;

// ==========================================
// 中文拼音输入法引擎 (Embedded Chinese Pinyin IME)
// ==========================================
static bool g_vkbd_chinese_mode = true;
static std::string g_vkbd_pinyin_buf = "";

struct PinyinEntry {
    const char* pinyin;
    const char* candidates;
};

static const PinyinEntry g_pinyin_table[] = {
    {"woshi", "我是"},
    {"wode", "我的"},
    {"jinbi", "金币"},
    {"wanjia", "玩家"},
    {"mingzi", "名字"},
    {"xiaoming", "小明"},
    {"xueliang", "血量"},
    {"gongji", "攻击"},
    {"dengji", "等级"},
    {"yingxiong", "英雄"},
    {"qizi", "棋子"},
    {"shuxing", "属性"},
    {"moxing", "模型"},
    {"shuju", "数据"},
    {"duiwu", "队伍"},
    {"zhuangbei", "装备"},
    {"a", "啊 阿 呵"},
    {"ai", "爱 矮 挨 哎 碍"},
    {"an", "按 安 暗 案"},
    {"ba", "把 八 吧 爸 拔"},
    {"bai", "百 白 败 拜"},
    {"ban", "半 办 班 版 板"},
    {"bao", "包 宝 保 报 暴"},
    {"bei", "被 北 倍 杯 备"},
    {"ben", "本 奔 笨"},
    {"bi", "币 比 必 闭 笔 壁 避"},
    {"bian", "变 边 便 编"},
    {"biao", "表 标 飙"},
    {"bing", "兵 冰 并 病"},
    {"bo", "波 博 拨 播"},
    {"bu", "不 部 步 补 布"},
    {"ca", "擦"},
    {"cai", "财 才 彩 采 踩 裁"},
    {"can", "参 残 餐"},
    {"ce", "策 测 侧 册"},
    {"cha", "查 差 插 茶"},
    {"chang", "长 场 常 唱"},
    {"che", "车 撤 扯"},
    {"chen", "沉 陈 晨"},
    {"cheng", "成 城 称 程 乘"},
    {"chi", "吃 持 迟 池 尺"},
    {"chu", "出 初 除 处 储"},
    {"chuan", "穿 传 船"},
    {"chuang", "创 床 闯"},
    {"ci", "此 次 词 刺"},
    {"cong", "从 聪 匆"},
    {"cu", "粗 促 醋"},
    {"cui", "催 脆 翠"},
    {"cun", "存 村 寸"},
    {"cuo", "错 挫 措"},
    {"da", "大 打 达 答"},
    {"dai", "带 代 待 袋 呆"},
    {"dan", "但 单 蛋 淡 担"},
    {"dang", "当 党 挡 荡"},
    {"dao", "到 道 倒 刀 岛"},
    {"de", "的 得 德"},
    {"deng", "等 灯 登 邓"},
    {"di", "第 地 低 底 敌 帝"},
    {"dian", "点 电 店 典 垫"},
    {"diao", "掉 调 吊 钓"},
    {"ding", "定 顶 订 丁"},
    {"dong", "动 东 懂 洞 冬"},
    {"dou", "斗 都 豆 抖"},
    {"du", "度 读 独 毒 渡"},
    {"duan", "段 断 短 端"},
    {"dui", "对 队 堆 追"},
    {"duo", "多 夺 躲 朵"},
    {"e", "饿 恶 额 俄"},
    {"en", "嗯 恩"},
    {"er", "二 儿 而 耳"},
    {"fa", "发 法 罚 伐"},
    {"fan", "反 范 犯 翻 烦"},
    {"fang", "放 方 防 房 访"},
    {"fei", "非 飞 费 废 肥"},
    {"fen", "分 份 粉 纷 奋"},
    {"feng", "封 风 峰 锋 凤"},
    {"fo", "佛"},
    {"fou", "否"},
    {"fu", "服 复 副 富 负 付 符"},
    {"ga", "嘎 尬"},
    {"gai", "该 改 盖 概"},
    {"gan", "干 敢 感 赶 甘"},
    {"gang", "刚 钢 港 岗"},
    {"gao", "高 告 搞 稿"},
    {"ge", "个 各 歌 格 哥 割"},
    {"gei", "给"},
    {"gen", "跟 根"},
    {"geng", "更 耕 梗"},
    {"gong", "工 公 共 功 攻 弓"},
    {"gou", "够 狗 购 构 勾"},
    {"gu", "古 股 骨 顾 故 孤"},
    {"gua", "挂 刮 瓜"},
    {"guai", "怪 乖 拐"},
    {"guan", "关 管 官 馆 贯 冠"},
    {"guang", "光 广 逛"},
    {"gui", "规 贵 鬼 归 轨"},
    {"gun", "滚 棍"},
    {"guo", "国 过 果 裹 锅"},
    {"ha", "哈 蛤"},
    {"hai", "海 害 还 孩"},
    {"han", "喊 寒 汗 汉 含"},
    {"hao", "好 号 毫 豪 耗"},
    {"he", "和 何 合 喝 盒 河"},
    {"hei", "黑 嘿"},
    {"hen", "很 狠 恨"},
    {"heng", "横 恒 哼"},
    {"hong", "红 宏 洪 轰"},
    {"hou", "后 候 厚 猴"},
    {"hu", "护 户 互 湖 呼 忽"},
    {"hua", "话 画 华 化 花 滑"},
    {"huai", "坏 怀 淮"},
    {"huan", "换 还 环 缓 幻 欢"},
    {"huang", "黄 慌 皇 荒"},
    {"hui", "会 回 灰 挥 辉 毁"},
    {"hun", "混 魂 婚"},
    {"huo", "或 活 火 获 货"},
    {"ji", "机 级 基 极 击 集 计 记 技 即 急 给"},
    {"jia", "加 家 价 假 架 甲 夹"},
    {"jian", "件 见 间 建 检 减 尖 简 剑"},
    {"jiang", "将 奖 降 讲 江"},
    {"jiao", "角 交 叫 脚 教 较 焦"},
    {"jie", "结 节 界 接 解 阶 借 截"},
    {"jin", "金 进 近 仅 今 紧 禁 斤"},
    {"jing", "精 经 警 竞 境 静 惊 净"},
    {"jiu", "就 九 酒 旧 久 救"},
    {"ju", "局 具 据 巨 聚 剧 居 举"},
    {"juan", "卷 捐 券"},
    {"jue", "决 绝 觉 角色 掘"},
    {"jun", "军 均 君 俊"},
    {"ka", "卡 咖"},
    {"kai", "开 凯 楷"},
    {"kan", "看 砍 刊"},
    {"kang", "抗 康 扛"},
    {"kao", "考 靠 烤"},
    {"ke", "可 克 科 客 课 刻"},
    {"ken", "肯 啃"},
    {"keng", "坑"},
    {"kong", "空 控 孔 恐"},
    {"kou", "口 扣"},
    {"ku", "苦 库 哭 裤"},
    {"kua", "跨 夸"},
    {"kuai", "快 块 筷 会"},
    {"kuan", "宽 款"},
    {"kuang", "况 框 狂 矿"},
    {"kui", "亏 愧 盔"},
    {"kun", "困 捆 昆"},
    {"kuo", "阔 扩 括"},
    {"la", "拉 啦 辣 落"},
    {"lai", "来 赖"},
    {"lan", "蓝 览 拦 栏 懒"},
    {"lang", "浪 狼 郎"},
    {"lao", "老 劳 捞 落"},
    {"le", "了 乐 勒"},
    {"lei", "类 雷 累 泪"},
    {"leng", "冷 愣"},
    {"li", "力 利 立 里 理 历 例 离 离"},
    {"lia", "俩"},
    {"lian", "连 练 脸 联 恋 链"},
    {"liang", "量 两 亮 良 粮 凉"},
    {"liao", "了 料 聊 疗"},
    {"lie", "列 烈 猎 劣"},
    {"lin", "林 临 邻 琳"},
    {"ling", "令 另 零 领 灵 凌"},
    {"liu", "六 流 留 溜 柳"},
    {"long", "龙 隆 垄 笼"},
    {"lou", "楼 漏 露"},
    {"lu", "路 录 陆 露 鲁"},
    {"luan", "乱 卵"},
    {"lun", "论 轮 伦"},
    {"luo", "落 罗 络 洛"},
    {"lv", "率 绿 滤 旅 履"},
    {"ma", "吗 妈 马 嘛 码 麻"},
    {"mai", "买 卖 迈 麦"},
    {"man", "慢 满 漫 蛮"},
    {"mang", "忙 盲 芒"},
    {"mao", "猫 毛 冒 贸 矛"},
    {"me", "么 麽"},
    {"mei", "没 美 每 妹 煤 梅"},
    {"men", "们 门 闷"},
    {"meng", "梦 猛 蒙 盟"},
    {"mi", "米 密 迷 秘 蜜"},
    {"mian", "面 免 棉 眠"},
    {"miao", "秒 妙 描 苗"},
    {"mie", "灭"},
    {"min", "民 敏 闵"},
    {"ming", "名 明 命 鸣"},
    {"mo", "模 末 莫 摩 魔 墨"},
    {"mou", "某 谋"},
    {"mu", "目 木 母 墓 幕 暮"},
    {"na", "那 拿 哪 呐"},
    {"nai", "乃 奶 耐"},
    {"nan", "难 南 男"},
    {"nao", "脑 闹"},
    {"ne", "呢 哪"},
    {"nei", "内 那"},
    {"nen", "嫩"},
    {"neng", "能"},
    {"ni", "你 泥 拟 逆 呢"},
    {"nian", "年 念 粘"},
    {"niang", "娘"},
    {"niao", "鸟 尿"},
    {"nie", "捏 聂"},
    {"nin", "您"},
    {"ning", "宁 凝 拧"},
    {"niu", "牛 纽 扭"},
    {"nong", "农 弄 浓"},
    {"nu", "奴 怒 努"},
    {"nv", "女"},
    {"o", "哦 噢"},
    {"ou", "偶 欧 殴"},
    {"pa", "怕 爬 帕"},
    {"pai", "排 派 牌 拍"},
    {"pan", "判 盘 盼"},
    {"pang", "旁 胖"},
    {"pao", "跑 炮 抛 泡"},
    {"pei", "配 陪 赔 培"},
    {"pen", "喷 盆"},
    {"peng", "碰 朋 膨 棚"},
    {"pi", "皮 匹 批 披 辟 脾"},
    {"pian", "片 篇 偏 骗"},
    {"piao", "票 漂 飘"},
    {"pie", "撇"},
    {"pin", "品 拼 频 贫"},
    {"ping", "平 评 凭 屏 瓶"},
    {"po", "破 迫 坡 婆"},
    {"pou", "剖"},
    {"pu", "普 铺 扑 谱"},
    {"qi", "起 其 七 期 奇 气 器 妻 齐 骑 棋"},
    {"qia", "卡 恰"},
    {"qian", "前 千 钱 浅 签 欠"},
    {"qiang", "强 枪 墙 抢"},
    {"qiao", "桥 巧 敲 乔 瞧"},
    {"qie", "切 且 窃"},
    {"qin", "亲 琴 侵 勤"},
    {"qing", "清 情 请 青 轻 倾"},
    {"qiong", "穷"},
    {"qiu", "求 秋 球"},
    {"qu", "去 取 区 曲 趋 驱"},
    {"quan", "全 权 圈 券 劝"},
    {"que", "确 却 缺 雀"},
    {"qun", "群 裙"},
    {"ran", "然 燃 染"},
    {"rang", "让 嚷"},
    {"rao", "绕 扰"},
    {"re", "热 惹"},
    {"ren", "人 任 认 忍 刃"},
    {"reng", "仍 扔"},
    {"ri", "日"},
    {"rong", "容 荣 融 绒"},
    {"rou", "肉 柔"},
    {"ru", "如 入 乳 儒"},
    {"ruan", "软"},
    {"rui", "瑞 锐"},
    {"run", "润"},
    {"ruo", "若 弱"},
    {"sa", "撒 萨"},
    {"sai", "赛 塞"},
    {"san", "三 散 伞"},
    {"sang", "桑 丧"},
    {"sao", "扫 骚"},
    {"se", "色 瑟"},
    {"sen", "森"},
    {"seng", "僧"},
    {"sha", "杀 沙 傻 刹"},
    {"shai", "晒 筛"},
    {"shan", "山 闪 善 单 扇 删"},
    {"shang", "上 伤 商 尚 赏"},
    {"shao", "少 烧 稍 绍"},
    {"she", "设 社 射 舍 蛇"},
    {"shen", "身 深 神 甚 申 沈"},
    {"sheng", "生 胜 声 省 升 盛 圣"},
    {"shi", "是 时 十 事 市 实 使 世 式 视 试 示 识 师 失 士"},
    {"shou", "手 首 受 收 守 售 兽"},
    {"shu", "书 数 术 属 树 输 熟 束"},
    {"shua", "刷 耍"},
    {"shuai", "帅 衰 摔 甩"},
    {"shuan", "栓"},
    {"shuang", "双 爽 霜"},
    {"shui", "水 谁 税 睡"},
    {"shun", "顺 瞬"},
    {"shuo", "说 硕"},
    {"si", "四 死 思 司 私 丝 寺"},
    {"song", "送 松 宋 诵"},
    {"sou", "搜 艘"},
    {"su", "速 素 苏 诉 塑"},
    {"suan", "算 酸"},
    {"sui", "随 岁 碎 虽"},
    {"sun", "损 孙"},
    {"suo", "所 锁 索 缩"},
    {"ta", "他 她 它 踏 塔"},
    {"tai", "太 台 态 泰 抬"},
    {"tan", "谈 探 弹 坦 叹"},
    {"tang", "糖 堂 躺 趟 汤"},
    {"tao", "逃 套 淘 桃 讨"},
    {"te", "特"},
    {"teng", "疼 腾"},
    {"ti", "体 提 题 替 踢 梯"},
    {"tian", "天 田 填 甜 添"},
    {"tiao", "条 跳 调 挑"},
    {"tie", "铁 贴 帖"},
    {"ting", "听 停 厅 庭 挺"},
    {"tong", "同 通 统 痛 童 铜"},
    {"tou", "头 投 透 偷"},
    {"tu", "图 土 突 途 徒 兔"},
    {"tuan", "团"},
    {"tui", "推 退 腿"},
    {"tun", "吞 屯"},
    {"tuo", "托 脱 拖 妥"},
    {"wa", "挖 娃 瓦 哇"},
    {"wai", "外 歪"},
    {"wan", "玩 完 万 晚 碗 弯"},
    {"wang", "网 王 望 往 忘"},
    {"wei", "为 位 微 未 唯 危 围 卫 尾 味"},
    {"wen", "问 文 温 闻 稳"},
    {"weng", "翁"},
    {"wo", "我 握 窝 卧 喔 沃"},
    {"wu", "无 五 物 武 舞 务 屋 误 悟"},
    {"xi", "西 息 系 希 吸 喜 细 戏 洗 席"},
    {"xia", "下 夏 吓 峡 瞎"},
    {"xian", "现 先 线 限 显 险 献 县"},
    {"xiang", "想 相 向 象 项 响 详 香 降"},
    {"xiao", "小 效 笑 消 校 肖 销 削"},
    {"xie", "写 些 谢 斜 血 鞋 携"},
    {"xin", "心 新 信 辛 欣"},
    {"xing", "行 性 形 星 型 幸 兴 姓"},
    {"xiong", "胸 雄 兄 熊 凶"},
    {"xiu", "修 秀 休 袖"},
    {"xu", "需 许 续 须 虚 序 叙"},
    {"xuan", "选 宣 玄 悬 旋"},
    {"xue", "学 血 雪 削"},
    {"xun", "寻 迅 训 讯 巡"},
    {"ya", "呀 压 亚 鸭 牙 押"},
    {"yan", "眼 言 演 严 验 延 研 烟 盐 岩"},
    {"yang", "样 阳 养 洋 羊 央"},
    {"yao", "要 药 耀 摇 咬 妖 腰"},
    {"ye", "也 页 夜 业 野 叶 爷"},
    {"yi", "一 以 已 意 义 移 易 医 异 亿 依 役 艺"},
    {"yin", "因 引 银 音 印 阴 隐"},
    {"ying", "应 英 影 营 赢 硬 映"},
    {"yo", "哟"},
    {"yong", "用 勇 永 拥 涌"},
    {"you", "有 右 又 友 游 幼 油 由 优"},
    {"yu", "与 于 语 育 雨 预 遇 域 余 鱼 羽 誉 玉"},
    {"yuan", "元 原 远 员 院 愿 源 圆 缘"},
    {"yue", "月 越 约 乐 跃 阅"},
    {"yun", "运 云 允 匀 陨"},
    {"za", "砸 杂"},
    {"zai", "在 再 载 灾"},
    {"zan", "赞 暂"},
    {"zang", "藏 脏"},
    {"zao", "造 早 噪 遭 藻"},
    {"ze", "则 责 择"},
    {"zei", "贼"},
    {"zen", "怎"},
    {"zeng", "增 赠"},
    {"zha", "炸 扎 查 眨"},
    {"zhai", "摘 宅 窄"},
    {"zhan", "站 占 战 展 斩 粘"},
    {"zhang", "张 章 长 掌 障 涨"},
    {"zhao", "找 照 招 召 赵 兆"},
    {"zhe", "这 着 者 折 哲 遮"},
    {"zhen", "真 阵 震 针 镇 侦"},
    {"zheng", "正 政 证 争 整 征 整"},
    {"zhi", "直 指 知 只 之 支 制 值 质 志 置 至 止"},
    {"zhong", "中 重 种 众 终 忠 钟"},
    {"zhou", "周 州 舟 洲 宙"},
    {"zhu", "主 住 注 助 猪 朱 著 祝 逐"},
    {"zhua", "抓"},
    {"zhuai", "拽"},
    {"zhuan", "转 专 传 赚 砖"},
    {"zhuang", "装 壮 状 撞 庄"},
    {"zhui", "追 坠 缀"},
    {"zhun", "准"},
    {"zhuo", "着 桌 捉 拙"},
    {"zi", "自 字 子 资 紫 姿"},
    {"zong", "总 纵 宗 踪"},
    {"zou", "走 奏 揍"},
    {"zu", "组 足 族 阻 祖"},
    {"zuan", "钻"},
    {"zui", "最 嘴 醉 罪"},
    {"zun", "尊 遵"},
    {"zuo", "做 作 坐 左 座"}
};

static std::vector<std::string> GetPinyinCandidates(const std::string& py) {
    std::vector<std::string> res;
    if (py.empty()) return res;

    std::string py_lower = py;
    std::transform(py_lower.begin(), py_lower.end(), py_lower.begin(), ::tolower);

    auto SplitCandidates = [&](const char* str) {
        if (!str) return;
        std::stringstream ss(str);
        std::string token;
        while (ss >> token && res.size() < 12) {
            res.push_back(token);
        }
    };

    // 1. 完全匹配
    for (size_t i = 0; i < sizeof(g_pinyin_table) / sizeof(g_pinyin_table[0]); i++) {
        if (py_lower == g_pinyin_table[i].pinyin) {
            SplitCandidates(g_pinyin_table[i].candidates);
            break;
        }
    }

    // 2. 前缀匹配
    if (res.size() < 6) {
        for (size_t i = 0; i < sizeof(g_pinyin_table) / sizeof(g_pinyin_table[0]); i++) {
            if (std::string(g_pinyin_table[i].pinyin).find(py_lower) == 0 && py_lower != g_pinyin_table[i].pinyin) {
                SplitCandidates(g_pinyin_table[i].candidates);
                if (res.size() >= 12) break;
            }
        }
    }

    return res;
}

void DrawVirtualKeyboard() {
    if (!g_show_vkbd || !g_vkbd_target) return;

    ImGuiIO& io = ImGui::GetIO();
    
    // Height of keyboard is about 40% of screen height
    float kbd_h = io.DisplaySize.y * 0.40f;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, kbd_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.16f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.22f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.35f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.5f, 0.65f, 1.0f));
    
    ImGui::Begin("VKBD", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(0.95f);

    // 顶部状态栏：显示当前输入内容 + 模式切换 + 粘贴 + 清空
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), (const char*)u8"输入内容:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s_", g_vkbd_target);
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 250.0f * g_autoScale);
    // [中/英] 切换按钮
    if (ImGui::Button(g_vkbd_chinese_mode ? (const char*)u8"[模式: 中文]" : (const char*)u8"[模式: 英文]", ImVec2(90.0f * g_autoScale, 0))) {
        g_vkbd_chinese_mode = !g_vkbd_chinese_mode;
        g_vkbd_pinyin_buf.clear();
    }
    ImGui::SameLine();
    // [粘贴] 按钮 (支持直接从系统剪贴板粘贴中文)
    if (ImGui::Button((const char*)u8"[粘贴]", ImVec2(55.0f * g_autoScale, 0))) {
        const char* clip = ImGui::GetClipboardText();
        if (clip && strlen(clip) > 0) {
            size_t cur_len = strlen(g_vkbd_target);
            strncat(g_vkbd_target, clip, g_vkbd_target_size - cur_len - 1);
        }
    }
    ImGui::SameLine();
    // [清空] 按钮
    if (ImGui::Button((const char*)u8"[清空]", ImVec2(55.0f * g_autoScale, 0))) {
        g_vkbd_target[0] = '\0';
        g_vkbd_pinyin_buf.clear();
    }

    ImGui::Separator();

    // 候选字/词栏 (在中文模式下输入拼音时显示候选词，否则显示常用快捷短语)
    std::vector<std::string> candidates = GetPinyinCandidates(g_vkbd_pinyin_buf);
    
    if (g_vkbd_chinese_mode && !g_vkbd_pinyin_buf.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "拼音: %s", g_vkbd_pinyin_buf.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "| 候选字:");
        ImGui::SameLine();

        if (candidates.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(无匹配)");
        } else {
            for (size_t c = 0; c < candidates.size() && c < 8; c++) {
                char cand_btn[32]; snprintf(cand_btn, sizeof(cand_btn), "%zu.%s##cand_%zu", c + 1, candidates[c].c_str(), c);
                if (ImGui::Button(cand_btn)) {
                    size_t cur_len = strlen(g_vkbd_target);
                    strncat(g_vkbd_target, candidates[c].c_str(), g_vkbd_target_size - cur_len - 1);
                    g_vkbd_pinyin_buf.clear();
                }
                ImGui::SameLine();
            }
        }
        ImGui::NewLine();
    } else if (g_vkbd_chinese_mode && g_vkbd_pinyin_buf.empty()) {
        // 常用中文快捷字词条
        const char* quick_words[] = {(const char*)u8"我是", (const char*)u8"小明", (const char*)u8"金币", (const char*)u8"玩家", (const char*)u8"名称", (const char*)u8"段位", (const char*)u8"英雄", (const char*)u8"棋子", (const char*)u8"血量", (const char*)u8"等级"};
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), (const char*)u8"常用词:");
        ImGui::SameLine();
        for (int w = 0; w < 10; w++) {
            if (ImGui::Button(quick_words[w])) {
                size_t cur_len = strlen(g_vkbd_target);
                strncat(g_vkbd_target, quick_words[w], g_vkbd_target_size - cur_len - 1);
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }

    // Responsive Button sizes
    float avail_w = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float base_btn_w = (avail_w - (spacing * 13.0f)) / 13.5f; // 13 keys wide approx
    float btn_h = (ImGui::GetContentRegionAvail().y - (spacing * 4.0f)) / 4.2f; // 4 rows

    static float g_bs_hold_time = 0.0f;
    static float g_bs_repeat_interval = 0.0f;

    auto KeyBtn = [&](const char* key, float width_mult = 1.0f) {
        if (strcmp(key, "BS") == 0) {
            ImGui::Button("BS", ImVec2(base_btn_w * width_mult, btn_h));
            bool is_clicked = ImGui::IsItemClicked();
            bool is_active = ImGui::IsItemActive();

            auto DoDelete = [&]() {
                if (g_vkbd_chinese_mode && !g_vkbd_pinyin_buf.empty()) {
                    g_vkbd_pinyin_buf.pop_back();
                } else {
                    size_t len = strlen(g_vkbd_target);
                    if (len > 0) {
                        // UTF-8 multi-byte character deletion
                        size_t i = len - 1;
                        while (i > 0 && (g_vkbd_target[i] & 0xC0) == 0x80) { i--; }
                        g_vkbd_target[i] = '\0';
                    }
                }
            };

            if (is_clicked) {
                DoDelete();
                g_bs_hold_time = 0.0f;
                g_bs_repeat_interval = 0.0f;
            } else if (is_active) {
                g_bs_hold_time += io.DeltaTime;
                if (g_bs_hold_time > 0.28f) {
                    g_bs_repeat_interval += io.DeltaTime;
                    if (g_bs_repeat_interval >= 0.04f) {
                        g_bs_repeat_interval = 0.0f;
                        DoDelete();
                    }
                }
            } else {
                g_bs_hold_time = 0.0f;
                g_bs_repeat_interval = 0.0f;
            }
            return;
        }

        if (ImGui::Button(key, ImVec2(base_btn_w * width_mult, btn_h))) {
            if (strcmp(key, "ENTER") == 0) {
                if (g_vkbd_chinese_mode && !g_vkbd_pinyin_buf.empty()) {
                    // Enter commits current pinyin as raw English
                    size_t cur_len = strlen(g_vkbd_target);
                    strncat(g_vkbd_target, g_vkbd_pinyin_buf.c_str(), g_vkbd_target_size - cur_len - 1);
                    g_vkbd_pinyin_buf.clear();
                } else {
                    g_show_vkbd = false;
                }
            } else if (strcmp(key, "SPACE") == 0) {
                if (g_vkbd_chinese_mode && !g_vkbd_pinyin_buf.empty() && !candidates.empty()) {
                    // Space selects 1st candidate character
                    size_t cur_len = strlen(g_vkbd_target);
                    strncat(g_vkbd_target, candidates[0].c_str(), g_vkbd_target_size - cur_len - 1);
                    g_vkbd_pinyin_buf.clear();
                } else {
                    size_t cur_len = strlen(g_vkbd_target);
                    if (cur_len < g_vkbd_target_size - 1) {
                        g_vkbd_target[cur_len] = ' ';
                        g_vkbd_target[cur_len + 1] = '\0';
                    }
                }
            } else if (strcmp(key, "CAPS") == 0) {
                g_vkbd_caps = !g_vkbd_caps;
            } else {
                if (g_vkbd_chinese_mode && isalpha((unsigned char)key[0])) {
                    // In Chinese mode, lowercase alpha adds to pinyin
                    char c = tolower((unsigned char)key[0]);
                    g_vkbd_pinyin_buf += c;
                } else {
                    size_t cur_len = strlen(g_vkbd_target);
                    if (cur_len < g_vkbd_target_size - 1) {
                        g_vkbd_target[cur_len] = key[0];
                        g_vkbd_target[cur_len + 1] = '\0';
                    }
                }
            }
        }
    };

    const char* row1_low[] = {"1","2","3","4","5","6","7","8","9","0","-","="};
    const char* row2_low[] = {"q","w","e","r","t","y","u","i","o","p","[","]"};
    const char* row3_low[] = {"a","s","d","f","g","h","j","k","l",";","'","\\"};
    const char* row4_low[] = {"z","x","c","v","b","n","m",",",".","/","_","@"};

    const char* row2_up[] = {"Q","W","E","R","T","Y","U","I","O","P","{","}"};
    const char* row3_up[] = {"A","S","D","F","G","H","J","K","L",":","\"","|"};
    const char* row4_up[] = {"Z","X","C","V","B","N","M","<",">","?","_","@"};

    // Row 1
    for (int i = 0; i < 12; i++) { KeyBtn(row1_low[i]); ImGui::SameLine(); }
    KeyBtn("BS", 1.5f);

    // Row 2
    for (int i = 0; i < 12; i++) { KeyBtn(g_vkbd_caps ? row2_up[i] : row2_low[i]); ImGui::SameLine(); }
    KeyBtn("SPACE", 1.5f);

    // Row 3
    for (int i = 0; i < 12; i++) { KeyBtn(g_vkbd_caps ? row3_up[i] : row3_low[i]); ImGui::SameLine(); }
    KeyBtn("ENTER", 1.5f);

    // Row 4
    KeyBtn("CAPS", 1.5f); ImGui::SameLine();
    for (int i = 0; i < 12; i++) { KeyBtn(g_vkbd_caps ? row4_up[i] : row4_low[i]); ImGui::SameLine(); }
    KeyBtn(g_vkbd_chinese_mode ? (const char*)u8"中" : (const char*)u8"英", 1.5f);

    ImGui::End();
    ImGui::PopStyleColor(4);
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

bool ModernAnimatedFolder(const char* label, bool* state) {
    ImGuiWindow* win = ImGui::GetCurrentWindow(); ImGuiID id = win->GetID(label); ImVec2 pos = win->DC.CursorPos; ImVec2 size(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 1.3f);
    const ImRect bb(pos, pos + size); ImGui::ItemSize(bb); bool hovered = false, held = false; 
    if (ImGui::ItemAdd(bb, id)) if (ImGui::ButtonBehavior(bb, id, &hovered, &held)) *state = !(*state); 
    float* p_anim = win->StateStorage.GetFloatRef(id, *state ? 1.0f : 0.0f); float anim = (*p_anim = ImLerp(*p_anim, *state ? 1.0f : 0.0f, 1.0f - expf(-18.0f * ImGui::GetIO().DeltaTime)));
    float sc = g_autoScale * g_scale;
    FrostTheme& th = UITheme();
    ImU32 bg = hovered ? IM_COL32(255, 255, 255, 28) : IM_COL32(255, 255, 255, 14);
    if (*state) bg = ImGui::GetColorU32(ImVec4(th.primary.x, th.primary.y, th.primary.z, 0.22f));
    win->DrawList->AddRectFilled(bb.Min, bb.Max, bg, 10.0f * sc);
    win->DrawList->AddRect(bb.Min, bb.Max, IM_COL32(255, 255, 255, *state ? 45 : 22), 10.0f * sc, 0, 1.0f);
    float cx = bb.Min.x + 15.0f * sc; float cy = bb.Min.y + size.y * 0.5f; float ang = anim * 1.5708f; float arr = 5.0f * sc;
    win->DrawList->AddTriangleFilled(ImVec2(cx+cosf(ang)*arr, cy+sinf(ang)*arr), ImVec2(cx+cosf(ang+2.094f)*arr, cy+sinf(ang+2.094f)*arr), ImVec2(cx+cosf(ang-2.094f)*arr, cy+sinf(ang-2.094f)*arr), ImGui::GetColorU32(th.primary));
    ImFont* font = ImGui::GetFont(); float fSz = ImGui::GetFontSize();
    win->DrawList->AddText(ImVec2(cx + 25.0f * g_autoScale * g_scale, bb.Min.y + (size.y - fSz)*0.5f), IM_COL32_WHITE, label);
    if (*state) { ImGui::Indent(15.0f * g_autoScale * g_scale); return true; } return false;
}

void EndModernAnimatedFolder() { ImGui::Unindent(15.0f * g_autoScale * g_scale); }

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
        win->DrawList->AddText(pos + ImVec2((bb.GetWidth() - t_sz.x)*0.5f, (bb.GetHeight() - t_sz.y)*0.5f), tierArray[i] ? IM_COL32(0,0,0,255) : IM_COL32_WHITE, buf);
    }
}

extern void (*old_nativeInjectEvent)(JNIEnv*, jobject, jobject);
typedef void (*func_set_IsGameEnd_t)(void* thisObj, uint8_t isEnd);
extern func_set_IsGameEnd_t orig_set_IsGameEnd;
typedef void* (*func_SendWillRenderCanvases_t)();
extern func_SendWillRenderCanvases_t orig_SendWillRenderCanvases;

// ===== 悬浮调用链与参数日志 =====
struct ActionLog {
    std::string message;
    std::chrono::time_point<std::chrono::steady_clock> spawn_time;
};
std::deque<ActionLog> g_action_logs;
std::mutex g_log_mutex;

void AddActionLog(const char* format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_action_logs.push_back({std::string(buf), std::chrono::steady_clock::now()});
    if (g_action_logs.size() > 8) {
        g_action_logs.pop_front();
    }
}

void DrawActionLogOverlay() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_action_logs.empty()) return;

    auto now = std::chrono::steady_clock::now();
    while (!g_action_logs.empty()) {
        float elapsed = std::chrono::duration<float>(now - g_action_logs.front().spawn_time).count();
        if (elapsed > 6.0f) g_action_logs.pop_front();
        else break;
    }
    
    if (g_action_logs.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    float dispW = io.DisplaySize.x > 0 ? io.DisplaySize.x : (float)g_gl_width;
    float posX = dispW * 0.5f;
    float posY = 75.0f * g_autoScale;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * g_autoScale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f) * g_autoScale);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.16f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.7f, 1.0f, 0.6f));
    
    if (ImGui::Begin("##ActionLogToastOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", (const char*)u8"⚡ 调用链与参数实时监视");
        DrawGlassSeparator();
        for (const auto& log : g_action_logs) {
            float elapsed = std::chrono::duration<float>(now - log.spawn_time).count();
            float fade = 1.0f;
            if (elapsed > 4.5f) fade = 1.0f - ((elapsed - 4.5f) / 1.5f);
            if (fade < 0.1f) fade = 0.1f;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 1.0f, 0.65f, fade));
            ImGui::TextUnformatted(log.message.c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}



// ==========================================
// 符号反查与动态反射模块 (Symbol Reflection & Inspector)
struct LiveFieldInfo {
    std::string name;
    std::string typeName;
    std::string cleanTypeName;
    size_t offset;
    uintptr_t rawValue;
    std::string childClassName;
    std::string strValue;
    bool isString;
    bool isPointer;
    bool matchesKnown;
    std::string matchDesc;
};

struct LiveInstanceDump {
    std::string label;
    uintptr_t address;
    std::string fullClassName;
    std::string shortClassName;
    std::vector<LiveFieldInfo> fields;
    bool valid;
};

LiveInstanceDump InspectLiveInstance(const char* label, uintptr_t ptr) {
    LiveInstanceDump dump;
    dump.label = label ? label : "Unknown";
    dump.address = ptr;
    dump.valid = false;

    if (!IsValidPtr(ptr) || !g_il2cpp_api.init()) return dump;

    void* klass_ptr = nullptr;
    if (!SafeReadMemory((uintptr_t)ptr, &klass_ptr, sizeof(void*)) || !IsValidIl2CppClass(klass_ptr)) {
        return dump;
    }

    const char* c_name = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(klass_ptr) : nullptr;
    const char* c_ns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(klass_ptr) : nullptr;
    if (!c_name || !c_name[0]) return dump;

    dump.shortClassName = c_name;
    dump.fullClassName = (c_ns && c_ns[0]) ? (std::string(c_ns) + "." + c_name) : std::string(c_name);
    dump.valid = true;

    if (g_il2cpp_api.class_get_fields) {
        void* iter = nullptr;
        while (void* field = g_il2cpp_api.class_get_fields(klass_ptr, &iter)) {
            const char* f_name = g_il2cpp_api.field_get_name ? g_il2cpp_api.field_get_name(field) : "";
            size_t f_offset = g_il2cpp_api.field_get_offset ? g_il2cpp_api.field_get_offset(field) : 0;
            void* f_type = g_il2cpp_api.field_get_type ? g_il2cpp_api.field_get_type(field) : nullptr;
            const char* t_name = (f_type && g_il2cpp_api.type_get_name) ? g_il2cpp_api.type_get_name(f_type) : "var";

            LiveFieldInfo fInfo;
            fInfo.name = f_name ? f_name : "";
            fInfo.typeName = t_name ? t_name : "";
            fInfo.cleanTypeName = CleanIl2CppTypeName(fInfo.typeName);
            fInfo.offset = f_offset;
            fInfo.rawValue = 0;
            fInfo.strValue = "";
            fInfo.isString = false;
            fInfo.isPointer = false;
            fInfo.matchesKnown = false;

            if (IsValidPtr(ptr + f_offset)) {
                SafeReadMemory(ptr + f_offset, &fInfo.rawValue, sizeof(uintptr_t));
                
                // 字符串解析
                if (fInfo.typeName == "System.String" || fInfo.typeName.find("String") != std::string::npos) {
                    fInfo.isString = true;
                    if (IsValidPtr(fInfo.rawValue)) {
                        fInfo.strValue = ReadIl2CppString(fInfo.rawValue);
                    }
                }
                // 指针对象解析
                else if (IsValidPtr(fInfo.rawValue)) {
                    fInfo.isPointer = true;
                    void* child_klass = nullptr;
                    if (SafeReadMemory(fInfo.rawValue, &child_klass, sizeof(void*)) && IsValidIl2CppClass(child_klass)) {
                        const char* child_cname = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(child_klass) : "";
                        const char* child_ns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(child_klass) : "";
                        fInfo.childClassName = (child_ns && child_ns[0]) ? (std::string(child_ns) + "." + child_cname) : std::string(child_cname);
                    }
                    if (fInfo.childClassName.empty()) {
                        fInfo.childClassName = fInfo.cleanTypeName;
                    }
                }
            }

            // 智能关联已知配置项
            std::string lstr = dump.label;
            if (f_offset == g_off.addr2 && lstr.find("addr1") != std::string::npos) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 addr2 下级指针】"; }
            else if (f_offset == g_off.addr3 && lstr.find("addr2") != std::string::npos) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 addr3 下级指针】"; }
            else if (f_offset == g_off.segmentcsogame && lstr.find("addr1") != std::string::npos) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 segmentcsogame 对局基址】"; }
            else if (f_offset == g_off.segment_my_player_id) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 segment_my_player_id 我的ID】"; }
            else if (f_offset == g_off.board_hero_id) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 board_hero_id 棋盘英雄ID】"; }
            else if (f_offset == g_off.board_x) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 board_x 棋盘X坐标】"; }
            else if (f_offset == g_off.board_y) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 board_y 棋盘Y坐标】"; }
            else if (f_offset == g_off.pi_name) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 pi_name 玩家名字】"; }
            else if (f_offset == g_off.pi_money) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 pi_money 玩家金币】"; }
            else if (f_offset == g_off.pi_level) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 pi_level 玩家等级】"; }
            else if (f_offset == g_off.shop_hero_id) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 shop_hero_id 商店卡牌ID】"; }
            else if (f_offset == g_off.bench_hero_id) { fInfo.matchesKnown = true; fInfo.matchDesc = (const char*)u8"===> 【对应 bench_hero_id 备战席英雄ID】"; }

            dump.fields.push_back(fInfo);
        }
    }

    return dump;
}

struct KwSearchResult {
    std::string className;
    std::string memberName;
    std::string typeName;
    uintptr_t offsetOrRva;
    bool isFunc;
};

static std::vector<KwSearchResult> g_kwResults;
static std::mutex g_kwMutex;
static char g_kw_search_input[64] = "Player";
static std::atomic<bool> g_kwSearching{false};

void DoKeywordSearch(std::string kw) {
    if (kw.empty() || !g_il2cpp_api.init()) {
        g_kwSearching.store(false);
        return;
    }

    std::string kw_lower = kw;
    std::transform(kw_lower.begin(), kw_lower.end(), kw_lower.begin(), ::tolower);

    {
        std::lock_guard<std::mutex> lock(g_kwMutex);
        g_kwResults.clear();
    }

    void* domain = g_il2cpp_api.domain_get();
    if (!domain) { g_kwSearching.store(false); return; }

    size_t asm_count = 0;
    void** assemblies = g_il2cpp_api.domain_get_assemblies(domain, &asm_count);
    if (!assemblies) { g_kwSearching.store(false); return; }

    for (size_t a = 0; a < asm_count; a++) {
        void* img = g_il2cpp_api.assembly_get_image(assemblies[a]);
        if (!img) continue;
        size_t cls_count = g_il2cpp_api.image_get_class_count ? g_il2cpp_api.image_get_class_count(img) : 0;

        for (size_t c = 0; c < cls_count; c++) {
            void* klass = g_il2cpp_api.image_get_class(img, c);
            if (!klass) continue;

            const char* c_name = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(klass) : "";
            const char* c_ns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(klass) : "";
            std::string full_class = (c_ns && c_ns[0]) ? (std::string(c_ns) + "." + c_name) : std::string(c_name);
            std::string full_class_lower = full_class;
            std::transform(full_class_lower.begin(), full_class_lower.end(), full_class_lower.begin(), ::tolower);

            bool class_matches = (full_class_lower.find(kw_lower) != std::string::npos);

            if (g_il2cpp_api.class_get_fields && g_il2cpp_api.field_get_name && g_il2cpp_api.field_get_offset) {
                void* iter = nullptr;
                while (void* field = g_il2cpp_api.class_get_fields(klass, &iter)) {
                    const char* f_name = g_il2cpp_api.field_get_name(field);
                    std::string f_str = f_name ? f_name : "";
                    std::string f_lower = f_str;
                    std::transform(f_lower.begin(), f_lower.end(), f_lower.begin(), ::tolower);

                    if (class_matches || f_lower.find(kw_lower) != std::string::npos) {
                        size_t f_offset = g_il2cpp_api.field_get_offset(field);
                        void* f_type = g_il2cpp_api.field_get_type ? g_il2cpp_api.field_get_type(field) : nullptr;
                        const char* t_name = (f_type && g_il2cpp_api.type_get_name) ? g_il2cpp_api.type_get_name(f_type) : "var";

                        std::lock_guard<std::mutex> lock(g_kwMutex);
                        if (g_kwResults.size() < 400) {
                            g_kwResults.push_back({ full_class, f_str, CleanIl2CppTypeName(t_name ? t_name : ""), (uintptr_t)f_offset, false });
                        }
                    }
                }
            }

            if (g_il2cpp_api.class_get_methods && g_il2cpp_api.method_get_name) {
                void* iter = nullptr;
                while (void* method = g_il2cpp_api.class_get_methods(klass, &iter)) {
                    const char* m_name = g_il2cpp_api.method_get_name(method);
                    std::string m_str = m_name ? m_name : "";
                    std::string m_lower = m_str;
                    std::transform(m_lower.begin(), m_lower.end(), m_lower.begin(), ::tolower);

                    if (class_matches || m_lower.find(kw_lower) != std::string::npos) {
                        uintptr_t func_ptr = *(uintptr_t*)method;
                        uintptr_t rva = func_ptr > g_il2cppTrueBase ? (func_ptr - g_il2cppTrueBase) : 0;

                        std::lock_guard<std::mutex> lock(g_kwMutex);
                        if (g_kwResults.size() < 400) {
                            g_kwResults.push_back({ full_class, m_str + "()", "Method", rva, true });
                        }
                    }
                }
            }
        }
    }

    g_kwSearching.store(false);
    AddActionLog((const char*)u8"-> [关键词搜索] 搜索 '%s' 找到 %zu 条匹配元数据!", kw.c_str(), g_kwResults.size());
}

static int g_menu_current_tab = 0;

// ==========================================
// 商业级 IL2CPP 运行时对象检查器 (Object Inspector)
// ==========================================
struct InspectBreadcrumb {
    std::string displayName;
    std::string className;
    uintptr_t objectAddress;
};

static std::vector<InspectBreadcrumb> g_inspect_breadcrumbs;
static char g_inspector_filter[128] = "";
static bool g_inspector_include_props = true;
static bool g_inspector_include_base = true;
static bool g_inspector_hide_null = false;
static bool g_show_standalone_inspector = false;
static char g_manual_inspect_input[128] = "";

void PushInspectObject(const std::string& name, const std::string& cls, uintptr_t addr) {
    if (!IsValidPtr(addr)) return;
    void* test_klass = nullptr;
    if (!SafeReadMemory(addr, &test_klass, sizeof(void*))) return;
    g_inspect_breadcrumbs.push_back({ name, cls, addr });
}

struct InspectorFieldRow {
    size_t offset;
    std::string fieldName;
    std::string typeName;
    std::string cleanType;
    std::string baseClassName;
    bool isBaseClass;
    bool isPropBacking;
    uintptr_t rawVal;
    int32_t val32;
    int64_t val64;
    std::string liveVal;
    bool isInt;
    bool isObjectPtr;
    bool isNull;
    std::string childClassName;
};

void DrawObjectInspectorContent(float avail_x, float avail_y) {
    if (!g_il2cpp_api.init()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), (const char*)u8"[提示] IL2CPP API 尚未初始化完成，请等待游戏进入主界面...");
        return;
    }

    float btn_w = 60.0f * g_autoScale;

    // 1. 快捷对象快捷载入栏
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), (const char*)u8"单例快速载入:");
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8"ChessModelManager")) {
        uintptr_t ptr = GetSingletonInstance("ChessModelManager");
        if (ptr == 0) ptr = g_dbg_addr1;
        if (IsValidPtr(ptr)) {
            g_inspect_breadcrumbs.clear();
            PushInspectObject("ChessModelManager", "ChessModelManager", ptr);
        } else {
            AddActionLog((const char*)u8"-> [提示] 未找到 ChessModelManager 实例");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8"CSOGame")) {
        uintptr_t ptr = GetSingletonInstance("CSOGame");
        if (ptr == 0) ptr = g_dbg_segmentcsogame;
        if (IsValidPtr(ptr)) {
            g_inspect_breadcrumbs.clear();
            PushInspectObject("CSOGame", "CSOGame", ptr);
        } else {
            AddActionLog((const char*)u8"-> [提示] 未找到 CSOGame 实例");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8"我的玩家")) {
        uintptr_t my_ptr = 0;
        for (const auto& pi : g_players) { if (pi.id == g_my_player_id && pi.val_ptr != 0) { my_ptr = pi.val_ptr; break; } }
        if (IsValidPtr(my_ptr)) {
            g_inspect_breadcrumbs.clear();
            PushInspectObject("MyPlayer", "TPlayerInfo", my_ptr);
        } else {
            AddActionLog((const char*)u8"-> [提示] 尚未进入对局或未获取到我的玩家实例");
        }
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f * g_autoScale);
    ImGui::InputTextWithHint("##ManualInspectAddr", "0x... / 类名", g_manual_inspect_input, sizeof(g_manual_inspect_input));
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8"载入##btnManualInspect")) {
        if (strlen(g_manual_inspect_input) > 0) {
            uintptr_t target_addr = 0;
            if (strncmp(g_manual_inspect_input, "0x", 2) == 0 || strncmp(g_manual_inspect_input, "0X", 2) == 0) {
                target_addr = (uintptr_t)strtoull(g_manual_inspect_input, nullptr, 16);
            } else {
                target_addr = GetSingletonInstance(g_manual_inspect_input);
            }
            if (IsValidPtr(target_addr)) {
                g_inspect_breadcrumbs.clear();
                PushInspectObject(g_manual_inspect_input, g_manual_inspect_input, target_addr);
                AddActionLog((const char*)u8"-> [对象检查] 成功载入目标对象: 0x%lx", target_addr);
            } else {
                AddActionLog((const char*)u8"-> [对象检查] 目标地址无效或单例为空: %s", g_manual_inspect_input);
            }
        }
    }

    ImGui::Spacing();

    // 2. 面包屑导航栏 (Breadcrumb Path)
    if (g_inspect_breadcrumbs.empty()) {
        uintptr_t defaultRoot = g_dbg_addr1;
        if (!IsValidPtr(defaultRoot)) defaultRoot = GetSingletonInstance("ChessModelManager");
        if (!IsValidPtr(defaultRoot)) defaultRoot = g_dbg_segmentcsogame;
        if (IsValidPtr(defaultRoot)) {
            PushInspectObject("ChessModelManager", "ChessModelManager", defaultRoot);
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), (const char*)u8"[提示] 请先在上方载入对象或在第4页【符号反查】中点击「全量浏览此单例」！");
            return;
        }
    }

    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), (const char*)u8"路径:");
    ImGui::SameLine();
    for (size_t b = 0; b < g_inspect_breadcrumbs.size(); b++) {
        if (b > 0) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), " > ");
            ImGui::SameLine();
        }
        char b_label[128];
        snprintf(b_label, sizeof(b_label), "%s##crumb_%zu", g_inspect_breadcrumbs[b].displayName.c_str(), b);
        if (b == g_inspect_breadcrumbs.size() - 1) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.6f, 1.0f), "%s (0x%lx)", g_inspect_breadcrumbs[b].displayName.c_str(), g_inspect_breadcrumbs[b].objectAddress);
        } else {
            if (ImGui::SmallButton(b_label)) {
                g_inspect_breadcrumbs.erase(g_inspect_breadcrumbs.begin() + b + 1, g_inspect_breadcrumbs.end());
                break;
            }
        }
    }

    ImGui::Spacing();

    // 3. 控制与过滤选项
    ImGui::Checkbox((const char*)u8"包含父类字段##Base", &g_inspector_include_base);
    ImGui::SameLine();
    ImGui::Checkbox((const char*)u8"计算属性(Getters)##Props", &g_inspector_include_props);
    ImGui::SameLine();
    ImGui::Checkbox((const char*)u8"隐藏Null空值##HideNull", &g_inspector_hide_null);
    ImGui::SameLine();
    ImGui::Text((const char*)u8"  过滤:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f * g_autoScale);
    ImGui::InputText("##InspectorFilter", g_inspector_filter, sizeof(g_inspector_filter));
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8"清空##ClrFilter")) { g_inspector_filter[0] = '\0'; }

    ImGui::Separator();
    ImGui::Spacing();

    // 4. 动态持续读取当前对象 (Dynamic Live 60 FPS Real-time Memory Read)
    uintptr_t currentObj = g_inspect_breadcrumbs.back().objectAddress;
    if (!IsValidPtr(currentObj)) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), (const char*)u8"[状态] 当前对象内存指针已失效 (0x0 / Null) - 请点击面包屑返回上一层！");
        return;
    }

    void* klass_ptr = nullptr;
    if (!SafeReadMemory(currentObj, &klass_ptr, sizeof(void*)) || !IsValidPtr((uintptr_t)klass_ptr)) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), (const char*)u8"[状态] 目标内存地址 (0x%lx) 暂无 IL2CPP 类型头信息，已自动为您开启原始内存槽位探测！", currentObj);
    }

    std::string fullClassName = g_inspect_breadcrumbs.back().className;
    if (klass_ptr && IsValidIl2CppClass(klass_ptr)) {
        const char* c_name = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(klass_ptr) : nullptr;
        const char* c_ns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(klass_ptr) : nullptr;
        std::string s_name = SafeReadCString(c_name);
        std::string s_ns = SafeReadCString(c_ns);
        if (!s_name.empty()) {
            fullClassName = (!s_ns.empty()) ? (s_ns + "." + s_name) : s_name;
            g_inspect_breadcrumbs.back().className = fullClassName;
        }
    }

    std::string filter_lower = SafeToLower(g_inspector_filter);

    std::vector<InspectorFieldRow> fieldRows;

    // ==========================================
    // A. 数组类型专属展开 (当对象本身为 T[] / Array 时)
    // ==========================================
    bool isArray = (fullClassName.find("[]") != std::string::npos || fullClassName.find("Array") != std::string::npos || fullClassName == "System.Array");
    if (isArray) {
        uint32_t len_18 = 0, len_10 = 0;
        SafeReadMemory(currentObj + 0x18, &len_18, sizeof(uint32_t));
        SafeReadMemory(currentObj + 0x10, &len_10, sizeof(uint32_t));
        
        uint32_t arr_len = 0;
        size_t elem_start_off = 0x20;
        if (len_18 > 0 && len_18 <= 5000) { arr_len = len_18; elem_start_off = 0x20; }
        else if (len_10 > 0 && len_10 <= 5000) { arr_len = len_10; elem_start_off = 0x18; }

        if (arr_len > 0) {
            for (uint32_t i = 0; i < arr_len && i < 200; i++) {
                size_t elem_off = elem_start_off + i * sizeof(uintptr_t);
                uintptr_t elem_ptr = 0;
                SafeReadMemory(currentObj + elem_off, &elem_ptr, sizeof(uintptr_t));
                int32_t elem_val32 = 0;
                SafeReadMemory(currentObj + elem_off, &elem_val32, sizeof(int32_t));

                std::string idx_name = "[" + std::to_string(i) + "]";
                std::string elem_type = "Object";
                std::string live_val = "";
                bool isObject = false;
                std::string child_cls = "";

                if (IsValidPtr(elem_ptr)) {
                    std::string str_val = ReadIl2CppString(elem_ptr);
                    if (!str_val.empty()) {
                        elem_type = "String";
                        live_val = "\"" + str_val + "\"";
                        isObject = false;
                    } else {
                        void* c_klass = nullptr;
                        SafeReadMemory(elem_ptr, &c_klass, sizeof(void*));
                        if (c_klass && IsValidIl2CppClass(c_klass)) {
                            const char* cn = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(c_klass) : nullptr;
                            const char* cns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(c_klass) : nullptr;
                            std::string scn = SafeReadCString(cn);
                            std::string scns = SafeReadCString(cns);
                            elem_type = (!scns.empty()) ? (scns + "." + scn) : (scn.empty() ? "Object" : scn);
                            child_cls = elem_type;
                        }
                        char buf[96]; snprintf(buf, sizeof(buf), "0x%lx (%s)", elem_ptr, elem_type.c_str());
                        live_val = buf;
                        isObject = true;
                    }
                } else {
                    char buf[32]; snprintf(buf, sizeof(buf), "%d", elem_val32);
                    live_val = buf;
                    elem_type = "Int32";
                }

                if (g_inspector_hide_null && elem_ptr == 0 && elem_val32 == 0) continue;
                if (!filter_lower.empty() && !SmartStringMatch(idx_name, filter_lower) && !SmartStringMatch(elem_type, filter_lower) && !SmartStringMatch(live_val, filter_lower)) continue;

                fieldRows.push_back({ elem_off, idx_name, elem_type, elem_type, "", false, false, elem_ptr, elem_val32, (int64_t)elem_val32, live_val, !isObject, isObject, (elem_ptr == 0 && elem_val32 == 0), child_cls });
            }
        }
    }

    // ==========================================
    // B. 原生 IL2CPP 字段反射 (实时每帧从物理内存读取最新动态值，受深度保护)
    // ==========================================
    if (klass_ptr && IsValidIl2CppClass(klass_ptr) && g_il2cpp_api.class_get_fields) {
        int p_depth = 0;
        for (void* cur_klass = klass_ptr; cur_klass != nullptr && IsValidIl2CppClass(cur_klass) && p_depth < 20; p_depth++, cur_klass = (g_inspector_include_base && g_il2cpp_api.class_get_parent) ? g_il2cpp_api.class_get_parent(cur_klass) : nullptr) {
            const char* cur_cls_name = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(cur_klass) : nullptr;
            std::string base_cls_str = SafeReadCString(cur_cls_name);

            bool isBaseClass = (cur_klass != klass_ptr);

            void* iter = nullptr;
            int f_count = 0;
            while (f_count < 8000 && (f_count++, true)) {
                void* field = g_il2cpp_api.class_get_fields(cur_klass, &iter);
                if (!field) break;

                uint32_t flags = g_il2cpp_api.field_get_flags ? g_il2cpp_api.field_get_flags(field) : 0;
                if ((flags & 0x0010) != 0) continue; // 跳过静态字段

                const char* f_name = g_il2cpp_api.field_get_name ? g_il2cpp_api.field_get_name(field) : nullptr;
                size_t f_offset = g_il2cpp_api.field_get_offset ? g_il2cpp_api.field_get_offset(field) : 0;
                void* f_type = g_il2cpp_api.field_get_type ? g_il2cpp_api.field_get_type(field) : nullptr;
                const char* t_name = (f_type && g_il2cpp_api.type_get_name) ? g_il2cpp_api.type_get_name(f_type) : nullptr;

                std::string field_str = SafeReadCString(f_name);
                std::string type_str = SafeReadCString(t_name);
                std::string clean_type = CleanIl2CppTypeName(type_str);

                // 即便字段名因元数据布局等原因读不出, 也用偏移兜底占位, 保证该字段仍被显示,
                // 不再因 field_str.empty() 而整条丢弃(这正是此前"不显示所有字段名"的根因).
                if (field_str.empty()) {
                    char fb[32];
                    snprintf(fb, sizeof(fb), "<field@+0x%zx>", f_offset);
                    field_str = fb;
                }
                if (type_str.empty()) {
                    char tb[24];
                    snprintf(tb, sizeof(tb), "*@+0x%zx", f_offset);
                    type_str = tb;
                    clean_type = "";
                }

                uintptr_t rawVal = 0;
                SafeReadMemory(currentObj + f_offset, &rawVal, sizeof(uintptr_t));
                int32_t val32 = 0;
                SafeReadMemory(currentObj + f_offset, &val32, sizeof(int32_t));
                int64_t val64 = 0;
                SafeReadMemory(currentObj + f_offset, &val64, sizeof(int64_t));

                std::string liveVal = "";
                bool isInt = false;
                bool isObjPtr = false;
                bool isNull = false;
                std::string childCls = "";

                std::string str_content = "";
                if (IsValidPtr(rawVal)) {
                    str_content = ReadIl2CppString(rawVal);
                }

                if (!str_content.empty()) {
                    liveVal = "\"" + str_content + "\"";
                } else if (clean_type == "String" || type_str.find("String") != std::string::npos) {
                    liveVal = IsValidPtr(rawVal) ? "\"\"" : "Null";
                    isNull = (rawVal == 0);
                } else if (clean_type == "Int32" || clean_type == "UInt32") {
                    char buf[32]; snprintf(buf, sizeof(buf), "%d", val32);
                    liveVal = buf;
                    isInt = true;
                } else if (clean_type == "Int64" || clean_type == "UInt64") {
                    char buf[32]; snprintf(buf, sizeof(buf), "%ld", (int64_t)rawVal);
                    liveVal = buf;
                    isInt = true;
                } else if (clean_type == "Boolean") {
                    liveVal = (rawVal & 0xFF) ? "true" : "false";
                } else if (clean_type == "Single") {
                    float fval = 0; memcpy(&fval, &rawVal, sizeof(float));
                    char buf[32]; snprintf(buf, sizeof(buf), "%.2f", fval);
                    liveVal = buf;
                } else {
                    if (rawVal == 0) {
                        liveVal = "Null";
                        isNull = true;
                    } else if (IsValidPtr(rawVal)) {
                        void* c_klass = nullptr;
                        SafeReadMemory(rawVal, &c_klass, sizeof(void*));
                        if (c_klass && IsValidIl2CppClass(c_klass)) {
                            const char* cn = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(c_klass) : nullptr;
                            const char* cns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(c_klass) : nullptr;
                            std::string scn = SafeReadCString(cn);
                            std::string scns = SafeReadCString(cns);
                            childCls = (!scns.empty()) ? (scns + "." + scn) : scn;
                        }
                        char buf[96]; snprintf(buf, sizeof(buf), "0x%lx (%s)", rawVal, childCls.empty() ? clean_type.c_str() : childCls.c_str());
                        liveVal = buf;
                        isObjPtr = true;
                    } else {
                        char buf[32]; snprintf(buf, sizeof(buf), "0x%lx", rawVal);
                        liveVal = buf;
                    }
                }

                if (g_inspector_hide_null && isNull) continue;
                if (!filter_lower.empty() && !SmartStringMatch(field_str, filter_lower) && !SmartStringMatch(clean_type, filter_lower) && !SmartStringMatch(liveVal, filter_lower)) continue;

                bool isBacking = (field_str.find("k__BackingField") != std::string::npos);
                std::string display_name = field_str;
                if (isBacking) {
                    size_t p1 = field_str.find('<');
                    size_t p2 = field_str.find('>');
                    if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                        display_name = "[属性] " + field_str.substr(p1 + 1, p2 - p1 - 1);
                    }
                }

                fieldRows.push_back({ f_offset, display_name, type_str, clean_type, isBaseClass ? base_cls_str : "", isBaseClass, isBacking, rawVal, val32, val64, liveVal, isInt, isObjPtr, isNull, childCls });
            }
        }
    }

    // ==========================================
    // C. 动态属性 Getters 实时安全反射
    // ==========================================
    if (g_inspector_include_props && klass_ptr && IsValidIl2CppClass(klass_ptr) && g_il2cpp_api.class_get_methods && g_il2cpp_api.runtime_invoke) {
        int m_p_depth = 0;
        for (void* cur_klass = klass_ptr; cur_klass != nullptr && IsValidIl2CppClass(cur_klass) && m_p_depth < 8; m_p_depth++, cur_klass = (g_inspector_include_base && g_il2cpp_api.class_get_parent) ? g_il2cpp_api.class_get_parent(cur_klass) : nullptr) {
            void* iter = nullptr;
            int m_count = 0;
            while (m_count < 8000 && (m_count++, true)) {
                void* method = g_il2cpp_api.class_get_methods(cur_klass, &iter);
                if (!method) break;

                const char* m_name = g_il2cpp_api.method_get_name ? g_il2cpp_api.method_get_name(method) : nullptr;
                std::string m_str = SafeReadCString(m_name);
                if (m_str.empty()) continue;

                if (m_str.find("get_") != 0) continue;
                uint32_t param_count = g_il2cpp_api.method_get_param_count ? g_il2cpp_api.method_get_param_count(method) : 1;
                if (param_count != 0) continue; // 仅调用无参 Getter

                // 过滤容易触发引擎底层原生崩溃的危险 Unity Internal 属性
                if (m_str == "get_gameObject" || m_str == "get_transform" || m_str == "get_rigidbody" || 
                    m_str == "get_renderer" || m_str == "get_material" || m_str == "get_mesh" || 
                    m_str == "get_canvas" || m_str == "get_hideFlags") continue;

                std::string prop_name = m_str.substr(4);
                std::string prop_display = "[Getter] " + prop_name;

                if (!filter_lower.empty() && !SmartStringMatch(prop_name, filter_lower)) continue;

                void* exc = nullptr;
                void* ret = g_il2cpp_api.runtime_invoke(method, (void*)currentObj, nullptr, &exc);
                if (exc == nullptr && ret != nullptr) {
                    uintptr_t ret_ptr = (uintptr_t)ret;
                    int32_t ret_val32 = 0;
                    SafeReadMemory(ret_ptr, &ret_val32, sizeof(int32_t));

                    std::string live_val = "";
                    bool isObj = IsValidPtr(ret_ptr);
                    std::string p_type = "Property";
                    std::string ch_cls = "";

                    if (isObj) {
                        void* c_k = nullptr;
                        SafeReadMemory(ret_ptr, &c_k, sizeof(void*));
                        if (c_k && IsValidIl2CppClass(c_k)) {
                            const char* cn = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(c_k) : nullptr;
                            std::string scn = SafeReadCString(cn);
                            p_type = scn.empty() ? "Object" : CleanIl2CppTypeName(scn);
                            ch_cls = p_type;
                        }
                        char buf[96]; snprintf(buf, sizeof(buf), "0x%lx (%s)", ret_ptr, p_type.c_str());
                        live_val = buf;
                    } else {
                        char buf[32]; snprintf(buf, sizeof(buf), "%d", ret_val32);
                        live_val = buf;
                        p_type = "Int32";
                    }

                    fieldRows.push_back({ 0x999, prop_display, p_type, p_type, "", (cur_klass != klass_ptr), true, ret_ptr, ret_val32, (int64_t)ret_val32, live_val, !isObj, isObj, false, ch_cls });
                }
            }
        }
    }

    // ==========================================
    // D. 实例物理内存槽位全量融合补充 (自动将 +0x10, +0x18, +0x28 等未被 IL2CPP 反射登记的物理槽位全部合并呈现！)
    // ==========================================
    {
        std::unordered_set<size_t> existing_offsets;
        for (const auto& r : fieldRows) existing_offsets.insert(r.offset);

        for (size_t slot_off = 0x00; slot_off <= 0x300; slot_off += sizeof(uintptr_t)) {
            if (existing_offsets.find(slot_off) != existing_offsets.end()) continue;
            uintptr_t slot_ptr = 0;
            SafeReadMemory(currentObj + slot_off, &slot_ptr, sizeof(uintptr_t));
            int32_t slot_int = 0;
            SafeReadMemory(currentObj + slot_off, &slot_int, sizeof(int32_t));

            if (slot_ptr == 0 && slot_int == 0 && g_inspector_hide_null && slot_off > 0x08) continue;

            char slot_name[64];
            std::string live_val = "";
            bool isObject = false;
            std::string slot_type = "Value";
            std::string child_cls = "";

            if (slot_off == 0x00) {
                snprintf(slot_name, sizeof(slot_name), (const char*)u8"[对象头] _klass");
                std::string full_kname = fullClassName;
                if (IsValidIl2CppClass((void*)slot_ptr)) {
                    const char* cn = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name((void*)slot_ptr) : nullptr;
                    const char* cns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace((void*)slot_ptr) : nullptr;
                    std::string s_name = SafeReadCString(cn);
                    std::string s_ns = SafeReadCString(cns);
                    if (!s_name.empty()) full_kname = (!s_ns.empty()) ? (s_ns + "." + s_name) : s_name;
                }
                slot_type = "Il2CppClass*";
                child_cls = full_kname;
                char buf[96]; snprintf(buf, sizeof(buf), "0x%lx (%s)", slot_ptr, full_kname.c_str());
                live_val = buf;
                isObject = true;
            } else if (slot_off == 0x08) {
                snprintf(slot_name, sizeof(slot_name), (const char*)u8"[同步锁] _monitor");
                slot_type = "MonitorData*";
                char buf[64];
                if (slot_ptr == 0) snprintf(buf, sizeof(buf), "0 (0x0)");
                else snprintf(buf, sizeof(buf), "0x%lx", slot_ptr);
                live_val = buf;
                isObject = IsValidPtr(slot_ptr);
            } else {
                if (IsValidPtr(slot_ptr)) {
                    std::string s_val = ReadIl2CppString(slot_ptr);
                    if (!s_val.empty()) {
                        snprintf(slot_name, sizeof(slot_name), "Slot_+0x%lx", slot_off);
                        slot_type = "String";
                        live_val = "\"" + s_val + "\"";
                        isObject = false;
                    } else {
                        void* c_klass = nullptr;
                        SafeReadMemory(slot_ptr, &c_klass, sizeof(void*));
                        if (c_klass && IsValidIl2CppClass(c_klass)) {
                            const char* cn = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(c_klass) : nullptr;
                            const char* cns = g_il2cpp_api.class_get_namespace ? g_il2cpp_api.class_get_namespace(c_klass) : nullptr;
                            std::string scn = SafeReadCString(cn);
                            std::string scns = SafeReadCString(cns);
                            std::string full_cname = (!scns.empty()) ? (scns + "." + scn) : scn;
                            slot_type = full_cname.empty() ? "Object" : full_cname;
                            child_cls = slot_type;

                            // ★ 智能推导字段名：若识别出具体类名（如 SpriteHelperModel），自动生成友好字段标识！
                            std::string clean_cls = CleanIl2CppTypeName(scn.empty() ? full_cname : scn);
                            if (!clean_cls.empty() && clean_cls != "Object" && clean_cls != "ValueType") {
                                std::string inferred_name = clean_cls;
                                if (!inferred_name.empty()) inferred_name[0] = (char)::tolower(inferred_name[0]);
                                snprintf(slot_name, sizeof(slot_name), "[推导] _%s", inferred_name.c_str());
                            } else {
                                snprintf(slot_name, sizeof(slot_name), "Slot_+0x%lx", slot_off);
                            }
                        } else {
                            snprintf(slot_name, sizeof(slot_name), "Slot_+0x%lx", slot_off);
                            slot_type = "NativePtr";
                            child_cls = "NativePtr";
                        }
                        char buf[96]; snprintf(buf, sizeof(buf), "0x%lx (%s)", slot_ptr, CleanIl2CppTypeName(slot_type).c_str());
                        live_val = buf;
                        isObject = true;
                    }
                } else {
                    snprintf(slot_name, sizeof(slot_name), "Slot_+0x%lx", slot_off);
                    char buf[32]; snprintf(buf, sizeof(buf), "%d (0x%x)", slot_int, slot_int);
                    live_val = buf;
                    slot_type = "Int32";
                }
            }

            fieldRows.push_back({ slot_off, slot_name, slot_type, CleanIl2CppTypeName(slot_type), "", false, false, slot_ptr, slot_int, (int64_t)slot_int, live_val, !isObject, isObject, (slot_ptr == 0), child_cls });
        }
    }

    // ★ 核心排序：按字段内存物理偏移从上到下严格升序排列 (+0x00, +0x08, +0x10, +0x18, +0x20...)
    std::sort(fieldRows.begin(), fieldRows.end(), [](const InspectorFieldRow& a, const InspectorFieldRow& b) {
        return a.offset < b.offset;
    });

    // 状态统计栏
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), (const char*)u8"● [实时动态读取中]");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), (const char*)u8"| 当前对象: 0x%lx | 实时字段数: %zu", currentObj, fieldRows.size());
    ImGui::Spacing();

    // 3 列表格 (字段/属性 | 值 | 类型)
    float col0_w = avail_x * 0.40f;
    float col1_w = avail_x * 0.38f;
    float col2_w = avail_x * 0.22f;

    ImGui::Columns(3, "ObjectInspectorColumns", true);
    ImGui::SetColumnWidth(0, col0_w);
    ImGui::SetColumnWidth(1, col1_w);
    ImGui::SetColumnWidth(2, col2_w);

    // 表头
    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), (const char*)u8"字段/属性");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), (const char*)u8"值");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), (const char*)u8"类型");
    ImGui::NextColumn();
    ImGui::Separator();

    std::string dump_buffer = "=== Dump Object: " + fullClassName + " (0x" + std::to_string(currentObj) + ") ===\n";

    for (size_t r = 0; r < fieldRows.size(); r++) {
        const auto& row = fieldRows[r];

        dump_buffer += row.fieldName + " [+0x" + std::to_string(row.offset) + "] = " + row.liveVal + " (" + row.cleanType + ")\n";

        // 列 1: 字段/属性 (带偏移和父类标签)
        if (row.isBaseClass && !row.baseClassName.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 0.8f), "[%s] ", row.baseClassName.c_str());
            ImGui::SameLine();
        }
        if (row.isPropBacking) {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.4f, 1.0f), "%s", row.fieldName.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.95f, 1.0f), "%s", row.fieldName.c_str());
        }
        ImGui::SameLine();
        if (row.offset != 0x999) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 0.8f), "[+0x%lx]", row.offset);
        } else {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 0.8f), "[动态计算]");
        }
        ImGui::NextColumn();

        // 列 2: 值 (支持一键下钻深入 + 设为 pi_money)
        if (row.isObjectPtr && IsValidPtr(row.rawVal)) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "%s", row.liveVal.c_str());
            ImGui::SameLine();
            char drill_btn[64]; snprintf(drill_btn, sizeof(drill_btn), (const char*)u8"下钻 >##drill_%zu", r);
            if (ImGui::Button(drill_btn)) {
                PushInspectObject(row.fieldName, row.childClassName.empty() ? row.cleanType : row.childClassName, row.rawVal);
            }
        } else {
            if (row.liveVal == "Null" || row.liveVal == (const char*)u8"空字段") {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), "%s", row.liveVal.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.4f, 1.0f), "%s", row.liveVal.c_str());
            }

            if (row.isInt) {
                ImGui::SameLine();
                char apply_btn[64]; snprintf(apply_btn, sizeof(apply_btn), (const char*)u8"[设为金币]##ins_%zu", r);
                if (ImGui::Button(apply_btn)) {
                    g_off.pi_money = row.offset;
                    SaveConfig();
                    AddActionLog((const char*)u8"-> [配置应用] 已将 pi_money 成功设为 0x%lx 并保存!", row.offset);
                }
            }
        }
        ImGui::NextColumn();

        // 列 3: 类型
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "%s", row.cleanType.c_str());
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::Spacing();

    // 5. 全量物理内存十六进制透视图 (Hex Memory Dump from 0x00)
    static bool s_show_hex_view = false;
    ImGui::Checkbox((const char*)u8"展开 0x00~0x200 全量物理内存十六进制字节透视表 (Hex Dump)##HexView", &s_show_hex_view);
    if (s_show_hex_view) {
        ImGui::BeginChild("HexDumpRegion", ImVec2(avail_x, 150.0f * g_autoScale), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (size_t h_off = 0; h_off <= 0x240; h_off += 16) {
            uint8_t bytes[16] = {0};
            SafeReadMemory(currentObj + h_off, bytes, 16);
            char hex_line[128];
            char ascii_line[32];
            for (int b = 0; b < 16; b++) {
                ascii_line[b] = (bytes[b] >= 32 && bytes[b] <= 126) ? (char)bytes[b] : '.';
            }
            ascii_line[16] = '\0';
            snprintf(hex_line, sizeof(hex_line), "+0x%03lx:  %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  | %s",
                h_off, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15], ascii_line);
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.9f, 1.0f), "%s", hex_line);
        }
        ImGui::EndChild();
        ImGui::Spacing();
    }

    // 6. 底部操作栏 (Dump Object 按钮)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.15f, 0.45f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.25f, 0.55f, 1.0f));
    if (ImGui::Button((const char*)u8"Dump Object", ImVec2(avail_x, 38.0f * g_autoScale))) {
        ImGui::SetClipboardText(dump_buffer.c_str());
        AddActionLog((const char*)u8"-> [Dump] 成功将对象 %s (0x%lx) 的全量字段数据复制到系统剪贴板!", fullClassName.c_str(), currentObj);
    }
    ImGui::PopStyleColor(2);
}

void DrawStandaloneInspectorWindow() {
    if (!g_show_standalone_inspector) return;

    ImGui::SetNextWindowSize(ImVec2(750.0f * g_autoScale, 550.0f * g_autoScale), ImGuiCond_FirstUseEver);
    if (ImGui::Begin((const char*)u8"[SV] IL2CPP Tool - 对象检查器", &g_show_standalone_inspector, ImGuiWindowFlags_NoSavedSettings)) {
        float avail_x = ImGui::GetContentRegionAvail().x;
        float avail_y = ImGui::GetContentRegionAvail().y;
        DrawObjectInspectorContent(avail_x, avail_y);
    }
    ImGui::End();
}

void DrawSymbolResolverUI() {
    float sc = 1.0f;
    ImGui::SetWindowFontScale(g_custom_font_scale);

    float avail_x = ImGui::GetContentRegionAvail().x;
    float btn_w = 60.0f * g_autoScale;

    // 字体大小调节 & 深度控制
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), (const char*)u8"字体大小:");
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8" - ##font", ImVec2(35.0f * g_autoScale, 0))) {
        g_custom_font_scale = std::max(0.40f, g_custom_font_scale - 0.05f);
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%.2fx", g_custom_font_scale);
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8" + ##font", ImVec2(35.0f * g_autoScale, 0))) {
        g_custom_font_scale = std::min(2.00f, g_custom_font_scale + 0.05f);
    }

    ImGui::SameLine(avail_x - 180.0f * g_autoScale);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), (const char*)u8"深度:");
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8" - ##depth", ImVec2(30.0f * g_autoScale, 0))) {
        g_search_max_depth = std::max(4, g_search_max_depth - 1);
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "%d层", g_search_max_depth);
    ImGui::SameLine();
    if (ImGui::Button((const char*)u8" + ##depth", ImVec2(30.0f * g_autoScale, 0))) {
        g_search_max_depth = std::min(20, g_search_max_depth + 1);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 1. 寻址起点
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), (const char*)u8"寻址起点(单例类):");
    ImGui::SetNextItemWidth(avail_x - btn_w - 10.0f);
    ImGui::InputText("##RootClassInput", g_root_class_input, sizeof(g_root_class_input)); 
    ImGui::SameLine(); 
    if (ImGui::Button((const char*)u8"[键盘]##1", ImVec2(btn_w, 0))) { 
        g_vkbd_target = g_root_class_input; 
        g_vkbd_target_size = sizeof(g_root_class_input); 
        g_show_vkbd = true; 
    }

    ImGui::Spacing();

    // 2. 寻址终点 (类名/字段名/数值/金币/字典/列表/字符串等一切内容)
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), (const char*)u8"寻址终点(类名/字段名/金币数值/列表字典，留空则全量探测链条):");
    ImGui::SetNextItemWidth(avail_x - btn_w - 10.0f);
    ImGui::InputText("##TargetClassInput", g_class_search_input, sizeof(g_class_search_input)); 
    ImGui::SameLine(); 
    if (ImGui::Button((const char*)u8"[键盘]##2", ImVec2(btn_w, 0))) { 
        g_vkbd_target = g_class_search_input; 
        g_vkbd_target_size = sizeof(g_class_search_input); 
        g_show_vkbd = true; 
    }

    ImGui::Spacing();
    
    // 3. 开始全量自动寻址按钮
    if (ImGui::Button((const char*)u8"[ 全量浏览 ] 在对象检查器中查看此单例 (逐层展开全部字段并下钻 >)", ImVec2(avail_x, 36 * g_autoScale))) {
        uintptr_t rootObj = g_dbg_addr1;
        if (strlen(g_root_class_input) > 0) {
            if (strncmp(g_root_class_input, "0x", 2) == 0 || strncmp(g_root_class_input, "0X", 2) == 0) {
                rootObj = strtoull(g_root_class_input, nullptr, 16);
            } else {
                rootObj = GetSingletonInstance(g_root_class_input);
                if (rootObj == 0 && strcmp(g_root_class_input, "CSOGame") == 0) rootObj = g_dbg_segmentcsogame;
                if (rootObj == 0 && strcmp(g_root_class_input, "ChessModelManager") == 0) rootObj = g_dbg_addr1;
            }
        }
        if (IsValidPtr(rootObj)) {
            g_inspect_breadcrumbs.clear();
            PushInspectObject(strlen(g_root_class_input) > 0 ? g_root_class_input : "RootSingleton", "Singleton", rootObj);
            g_menu_current_tab = 5;
        }
    }
    ImGui::Spacing();
    if (g_is_searching_path) {
        char search_label[128];
        snprintf(search_label, sizeof(search_label), (const char*)u8"[ 正在深度寻址中... ] (已检索 %d 个节点)", (int)g_search_progress_nodes);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.35f, 0.8f));
        ImGui::Button(search_label, ImVec2(avail_x, 38 * g_autoScale));
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button((const char*)u8"> 开始全量深度寻址！(穿透列表/字典/数组输出全字段)", ImVec2(avail_x, 38 * g_autoScale))) {
            uintptr_t rootObj = g_dbg_addr1;
            if (strlen(g_root_class_input) > 0) {
                if (strncmp(g_root_class_input, "0x", 2) == 0 || strncmp(g_root_class_input, "0X", 2) == 0) {
                    rootObj = strtoull(g_root_class_input, nullptr, 16);
                } else {
                    uintptr_t singletonObj = GetSingletonInstance(g_root_class_input);
                    if (singletonObj != 0) {
                        rootObj = singletonObj;
                        g_dbg_addr1 = singletonObj;
                        AddActionLog((const char*)u8"-> [单例解析] 成功获取 %s 单例实例: 0x%lx", g_root_class_input, singletonObj);
                    } else if (strcmp(g_root_class_input, "CSOGame") == 0 && IsValidPtr(g_dbg_segmentcsogame)) {
                        rootObj = g_dbg_segmentcsogame;
                    } else if (strcmp(g_root_class_input, "ChessModelManager") == 0 && IsValidPtr(g_dbg_addr1)) {
                        rootObj = g_dbg_addr1;
                    } else {
                        rootObj = 0;
                        AddActionLog((const char*)u8"-> [单例解析失败] 无法找到 %s 的单例实例", g_root_class_input);
                    }
                }
            }
            if (rootObj != 0) {
                std::string targetKw = g_class_search_input;
                int depthLimit = g_search_max_depth;
                g_is_searching_path = true;
                g_search_progress_nodes = 0;
                
                std::thread([rootObj, targetKw, depthLimit]() {
                    ObjectPathFindingResult res = AutoFindPath(rootObj, targetKw, depthLimit);
                    g_lastPathResult = res;
                    g_is_searching_path = false;
                    AddActionLog((const char*)u8"-> [深度寻址完成] 搜索完成，共发现 %zu 条匹配链路，已自动按偏移排序!", res.paths.size());
                }).detach();
            } else {
                g_lastPathResult.found = false;
                g_lastPathResult.paths.clear();
                g_lastPathResult.exploredChains.clear();
                AddActionLog((const char*)u8"-> [提示] 寻址起点对象尚未在内存中初始化");
            }
        }
    }

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);

    if (strlen(g_root_class_input) > 0) {
        if (strncmp(g_root_class_input, "0x", 2) == 0 || strncmp(g_root_class_input, "0X", 2) == 0) {
            uintptr_t cptr = strtoull(g_root_class_input, nullptr, 16);
            if (IsValidPtr(cptr)) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), (const char*)u8"[起点就绪 - 内存指针] 0x%lx", cptr);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), (const char*)u8"[起点异常] 内存指针 0x%lx 不可读", cptr);
            }
        } else {
            if (g_dbg_addr1 != 0 && GetSingletonInstance(g_root_class_input) != 0) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), (const char*)u8"[起点就绪] %s = 0x%lx", g_root_class_input, GetSingletonInstance(g_root_class_input));
            } else if (strcmp(g_root_class_input, "CSOGame") == 0 && IsValidPtr(g_dbg_segmentcsogame)) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), (const char*)u8"[起点就绪 - CSOGame] = 0x%lx", g_dbg_segmentcsogame);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), (const char*)u8"[起点异常] 无法获取 %s 单例，请检查拼写或等待游戏加载完成。", g_root_class_input);
            }
        }
    }

    auto RenderPathItem = [&](size_t p, const FoundPath& fp, bool isExplored) {
        ImGui::Spacing();
        ImGui::Separator();
        if (isExplored) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), (const char*)u8"=== 探测链条 [%zu] %s ===", p + 1, fp.matchDesc.c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), (const char*)u8"=== 匹配路径 [%zu] %s ===", p + 1, fp.matchDesc.c_str());
        }
        
        ImGui::Indent(8.0f * g_autoScale);
        
        if (fp.steps.empty()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), (const char*)u8"-> 目标即为起点自身 (0x%lx) [0 层跳跃]", fp.targetInstance);
        } else {
            for (size_t s = 0; s < fp.steps.size(); s++) {
                const auto& st = fp.steps[s];
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "[第 %zu 层] %s", s + 1, st.fromClass.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "-> [+0x%lx: %s] ->", st.offset, st.fieldName.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "%s", st.toClass.c_str());
            }

            std::string off_summary = (const char*)u8"[提取] 偏移链: 起点单例";
            for (const auto& st : fp.steps) {
                char obuf[32]; snprintf(obuf, sizeof(obuf), " -> +0x%lx", st.offset);
                off_summary += obuf;
            }
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.7f, 1.0f), "%s", off_summary.c_str());
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), (const char*)u8"[定位] 字段物理绝对地址: 0x%lx (所属实例: 0x%lx)", fp.fieldAddress, fp.targetInstance);
        }
        
        ImGui::Spacing();

        // 3 列表格 (字段/属性 | 值 | 类型)
        float col0_w = (avail_x - 20.0f * g_autoScale) * 0.40f;
        float col1_w = (avail_x - 20.0f * g_autoScale) * 0.38f;
        float col2_w = (avail_x - 20.0f * g_autoScale) * 0.22f;

        char table_id[32]; snprintf(table_id, sizeof(table_id), "PathFieldCols_%s_%zu", isExplored ? "exp" : "mat", p);
        ImGui::Columns(3, table_id, true);
        ImGui::SetColumnWidth(0, col0_w);
        ImGui::SetColumnWidth(1, col1_w);
        ImGui::SetColumnWidth(2, col2_w);

        // 表头
        ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), (const char*)u8"字段/属性");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), (const char*)u8"值");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), (const char*)u8"类型");
        ImGui::NextColumn();
        ImGui::Separator();

        if (fp.steps.empty()) {
            // 0 层（起点自身）
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s [+0x0]", fp.fieldName.c_str());
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.4f, 1.0f), "%s", fp.formattedVal.c_str());
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "%s", fp.cleanType.c_str());
            ImGui::NextColumn();
        } else {
            // 完整输出第 1 层、第 2 层 ... 所有层级的字段行！
            for (size_t s = 0; s < fp.steps.size(); s++) {
                const auto& st = fp.steps[s];

                uintptr_t cur_raw = 0;
                SafeReadMemory(st.fromObj + st.offset, &cur_raw, sizeof(uintptr_t));
                int32_t cur_val32 = 0;
                SafeReadMemory(st.fromObj + st.offset, &cur_val32, sizeof(int32_t));

                std::string live_val = "";
                bool is_int = false;

                if (st.cleanType == "String") {
                    if (IsValidPtr(cur_raw)) {
                        std::string s_str = ReadIl2CppString(cur_raw);
                        live_val = s_str.empty() ? (const char*)u8"空字段" : ("\"" + s_str + "\"");
                    } else {
                        live_val = (const char*)u8"空字段";
                    }
                } else if (st.cleanType == "Boolean") {
                    live_val = (cur_raw & 0xFF) ? "true" : "false";
                } else if (st.cleanType == "Int32" || st.cleanType == "UInt32") {
                    is_int = true;
                    char buf[32]; snprintf(buf, sizeof(buf), "%d", cur_val32);
                    live_val = buf;
                } else if (st.cleanType == "Int64" || st.cleanType == "UInt64") {
                    is_int = true;
                    char buf[32]; snprintf(buf, sizeof(buf), "%ld", (int64_t)cur_raw);
                    live_val = buf;
                } else if (st.cleanType == "Single") {
                    float fval = 0; memcpy(&fval, &cur_raw, sizeof(float));
                    char buf[32]; snprintf(buf, sizeof(buf), "%.3f", fval);
                    live_val = buf;
                } else {
                    if (cur_raw == 0) {
                        live_val = "Null";
                    } else {
                        char buf[64]; snprintf(buf, sizeof(buf), "0x%lx (%s)", cur_raw, st.cleanType.c_str());
                        live_val = buf;
                    }
                }

                // 列 1: 字段/属性
                char layer_prefix[32]; snprintf(layer_prefix, sizeof(layer_prefix), "[L%zu] ", s + 1);
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", layer_prefix);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s", st.fieldName.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "[+0x%lx]", st.offset);
                ImGui::NextColumn();

                // 列 2: 实时值与快捷设置按钮
                ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.4f, 1.0f), "%s", live_val.c_str());
                if (is_int) {
                    ImGui::SameLine();
                    char btn_apply_id[64]; snprintf(btn_apply_id, sizeof(btn_apply_id), (const char*)u8"[设为金币]##p%zu_s%zu_%s", p, s, isExplored ? "e" : "m");
                    if (ImGui::Button(btn_apply_id)) {
                        g_off.pi_money = st.offset;
                        SaveConfig();
                        AddActionLog((const char*)u8"-> [配置应用] 已将 pi_money 成功设为 0x%lx 并保存本地!", st.offset);
                    }
                }
                ImGui::NextColumn();

                // 列 3: 类型
                ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "%s", st.cleanType.c_str());
                ImGui::NextColumn();
            }
        }

        ImGui::Columns(1);
        ImGui::Separator();

        ImGui::Unindent(8.0f * g_autoScale);
    };

    // 4. 路径输出：
    if (g_lastPathResult.found) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), (const char*)u8"[成功] 共扫描出 %zu 条关联路径与字段：", g_lastPathResult.paths.size());
        
        for (size_t p = 0; p < g_lastPathResult.paths.size(); p++) {
            RenderPathItem(p, g_lastPathResult.paths[p], false);
        }
    } else {
        // ★ 核心改进：即使未完全匹配到目标，也全量展示当前单例探测到的全部底层分支链条与所有层级表格！
        if (!g_lastPathResult.exploredChains.empty()) {
            ImGui::Spacing();
            if (g_class_search_input[0] != '\0') {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), (const char*)u8"[提示] 未直接完全匹配到目标: \"%s\" (搜索深度: %d 层)", g_class_search_input, g_search_max_depth);
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), (const char*)u8"-> 已为您全量解析展开当前单例已探测到的 %zu 条底层链路与全层级表格数据：", g_lastPathResult.exploredChains.size());
            } else {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), (const char*)u8"[就绪] 当前单例全量探测到 %zu 条核心分支链条：", g_lastPathResult.exploredChains.size());
            }

            for (size_t p = 0; p < g_lastPathResult.exploredChains.size(); p++) {
                RenderPathItem(p, g_lastPathResult.exploredChains[p], true);
            }
        }
    }
    
    ImGui::PopTextWrapPos();
}

void DrawMainMenu() {
    ApplyFrostedTheme();
    if (g_menu_orb) {
        DrawMenuOrb();
        return;
    }
    int& current_tab = g_menu_current_tab;

    static bool firstMenuOpen = true;
    if (firstMenuOpen) {
        ImGui::SetNextWindowPos(ImVec2(80.0f * g_autoScale, 80.0f * g_autoScale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(600.0f * g_autoScale, 450.0f * g_autoScale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        firstMenuOpen = false;
    }

    bool menu_visible = true;
    if (ImGui::Begin((const char*)u8"金铲铲助手 Frosted Studio", &menu_visible, ImGuiWindowFlags_NoSavedSettings)) {
        g_menuX = ImGui::GetWindowPos().x;
        g_menuY = ImGui::GetWindowPos().y;
        g_menuCollapsed = ImGui::IsWindowCollapsed();
                if (!menu_visible || g_menuCollapsed) {
            if (g_orb_x <= 0.0f || g_orb_y <= 0.0f) {
                g_orb_x = g_menuX + 28.0f * g_autoScale;
                g_orb_y = g_menuY + 28.0f * g_autoScale;
            }
            g_menu_orb = true;
        }
        if (!g_menuCollapsed) {
            float curW = ImGui::GetWindowSize().x, curH = ImGui::GetWindowSize().y;
            if (std::abs(curW - g_menuW) > 5.0f || std::abs(curH - g_menuH) > 5.0f) {
                g_menuW = curW; g_menuH = curH;
                // Disconnected font scaling from window drag resizing!
            }
        }

        if (!g_menuCollapsed) {
            ImGui::SetWindowFontScale(g_scale);
            DrawStatusHeader();
            DrawGlassSeparator();

            float sidebarW = 132.0f * g_autoScale * g_scale;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::BeginChild("FrostSidebar", ImVec2(sidebarW, 0), true, ImGuiWindowFlags_NoScrollbar);
                        const char* tabLabels[] = { (const char*)u8"视觉透视", (const char*)u8"自动购买", (const char*)u8"链路诊断", (const char*)u8"偏移调试", (const char*)u8"符号反查", (const char*)u8"对象检查" };
            for (int i = 0; i < 6; i++) {
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
                if (ImGui::Button((const char*)u8"保存全部配置", ImVec2(-1, 34 * g_autoScale * g_scale))) { SaveConfig(); AddActionLog((const char*)u8"-> [配置] 已成功保存所有当前改动过的偏移与设置到本地!"); }
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
                {
                DrawSectionTitle((const char*)u8"自动拿牌");
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), (const char*)u8"商店格子捕获进度: %zu / 5", g_shop_slots.size());
                if (g_shop_listen_done.load()) ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), (const char*)u8"已成功获取5个格子地址，就绪！");
                else ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), (const char*)u8"请在游戏内刷新一次商店以捕获...");
                                DrawGlassSeparator();
                int selected_hero_count = 0;
                for (const auto& kv : g_heroAutoBuyChecked) {
                    if (kv.second) selected_hero_count++;
                }

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.22f, 0.22f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
                char clear_btn_txt[64];
                snprintf(clear_btn_txt, sizeof(clear_btn_txt), (const char*)u8"一键清空已选英雄 (已选: %d)", selected_hero_count);
                if (ImGui::Button(clear_btn_txt, ImVec2(-1, 32.0f * g_autoScale * g_scale))) {
                    g_heroAutoBuyChecked.clear();
                    AddActionLog((const char*)u8"-> [自动购买] 已一键清空所有已勾选的英雄!");
                }
                ImGui::PopStyleColor(3);
                ImGui::Spacing();
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
                }
                break;
            case 2:
                {
                    auto PrintCol = [](const char* fmt, int ok, ...) {
                        va_list args;
                        va_start(args, ok);
                        ImVec4 col = ok ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_Text, col);
                        ImGui::TextV(fmt, args);
                        ImGui::PopStyleColor();
                        va_end(args);
                    };

                    DrawSectionTitle((const char*)u8"链路诊断与状态监视");
                    if (ImGui::Button((const char*)u8"⚡ 点击测试悬浮调用日志", ImVec2(-1, 32 * g_autoScale))) { 
                        AddActionLog((const char*)u8"-> [测试] 悬浮调用链与参数监视正常工作!"); 
                    }
                    ImGui::TextColored(UITheme().primary, (const char*)u8"【主线基础寻址链路】");
                    PrintCol("il2cppTrueBase: 0x%lx", g_il2cppTrueBase > 0, g_il2cppTrueBase);
                    PrintCol("addr1 (Instance): 0x%lx %s", IsValidPtr(g_dbg_addr1), g_dbg_addr1, IsValidPtr(g_dbg_addr1) ? "[OK]" : "[x]");
                    PrintCol("addr2: 0x%lx %s", IsValidPtr(g_dbg_addr2), g_dbg_addr2, IsValidPtr(g_dbg_addr2) ? "[OK]" : "[x]");
                    PrintCol("addr3: 0x%lx %s", IsValidPtr(g_dbg_addr3), g_dbg_addr3, IsValidPtr(g_dbg_addr3) ? "[OK]" : "[x]");
                    PrintCol("addra: 0x%lx %s", IsValidPtr(g_dbg_addra), g_dbg_addra, IsValidPtr(g_dbg_addra) ? "[OK]" : "[x]");
                    PrintCol("segmentCSOGame: 0x%lx %s", IsValidPtr(g_dbg_segmentcsogame), g_dbg_segmentcsogame, IsValidPtr(g_dbg_segmentcsogame) ? "[OK]" : "[x]");
                    PrintCol("真实ID: %d | 对局状态: %s", g_is_in_match.load() || g_my_player_id >= 0, g_my_player_id, g_is_in_match.load() ? "对局中" : "未在对局");
                    
                    DrawGlassSeparator();
                    ImGui::TextColored(UITheme().primary, (const char*)u8"【核心Hook点状态】");
                    PrintCol("1. 商店挂载 (func_shop_listen): %s", old_shop_listen != nullptr, old_shop_listen ? "已挂载 [OK]" : "未挂载/偏移需校准 [x]");
                    PrintCol("2. 对局状态 (func_set_IsGameEnd): %s", orig_set_IsGameEnd != nullptr, orig_set_IsGameEnd ? "已挂载 [OK]" : "未挂载/偏移需校准 [x]");
                    PrintCol("3. 线程管道 (SendWillRenderCanvases): %s", orig_SendWillRenderCanvases != nullptr, orig_SendWillRenderCanvases ? "已挂载 [OK]" : "未挂载 [x]");
                    PrintCol("4. 触摸分发 (nativeInjectEvent): %s", old_nativeInjectEvent != nullptr, old_nativeInjectEvent ? "已挂载 [OK]" : "未挂载 [x]");

                    DrawGlassSeparator();
                    ImGui::TextColored(UITheme().primary, (const char*)u8"【牌库字典链】");
                    PrintCol("addr4=0x%lx | addr7=0x%lx (大小: %zu)", IsValidPtr(g_dbg_addr4) && IsValidPtr(g_dbg_addr7), g_dbg_addr4, g_dbg_addr7, g_dbg_list7_addrs.size());
                    ImGui::Indent();
                    for (size_t i = 0; i < std::min((size_t)6, g_dbg_list7_addrs.size()); i++) {
                        uintptr_t a8 = g_dbg_list7_addrs[i];
                        PrintCol("addr7[%zu] -> addr8: 0x%lx", IsValidPtr(a8), i, a8);
                    }
                    if (g_dbg_list7_addrs.size() > 6) ImGui::Text("... (共 %zu 个项)", g_dbg_list7_addrs.size());
                    ImGui::Unindent();

                    DrawGlassSeparator();
                    ImGui::TextColored(UITheme().primary, (const char*)u8"【商店槽位地址 (5个)】");
                    PrintCol("数量: %zu / 5", g_shop_slots.size() == 5, g_shop_slots.size());
                    ImGui::Indent();
                    for (size_t i = 0; i < g_shop_slots.size(); i++)
                        PrintCol("ShopSlot[%zu]: 0x%lx", IsValidPtr(g_shop_slots[i]), i, g_shop_slots[i]);
                    for (size_t i = g_shop_slots.size(); i < 5; i++)
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "ShopSlot[%zu]: (等待刷新...)", i);
                    ImGui::Unindent();

                    DrawGlassSeparator();
                    ImGui::TextColored(UITheme().primary, (const char*)u8"【玩家列表 (addr12)】");
                    PrintCol("addr11=0x%lx | addr12=0x%lx | 数量: %zu", IsValidPtr(g_dbg_addr12) && g_players.size() > 0, g_dbg_addr11, g_dbg_addr12, g_players.size());
                    ImGui::Indent();
                    for (size_t i = 0; i < g_players.size(); i++) {
                        const auto& pi = g_players[i];
                        PrintCol("[%zu] ID=%d 昵称=%s 钱=%d 等级=%d 连胜=%d 连败=%d (addr13=0x%lx)", 
                            IsValidPtr(pi.addr13_ptr), i+1, pi.id, pi.name.c_str(), pi.money, pi.level, pi.win_streak, pi.lose_streak, pi.addr13_ptr);
                    }
                    ImGui::Unindent();

                    DrawGlassSeparator();
                    ImGui::TextColored(UITheme().primary, (const char*)u8"【下回合对手预测】");
                    if (g_next_opponents.empty()) ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "等待对局中获取列表...");
                    else {
                        for (size_t i = 0; i < g_next_opponents.size(); i++) {
                            int opp_id = g_next_opponents[i];
                            std::string opp_name = "未知";
                            for (const auto& p : g_players) { if (p.id == opp_id) { opp_name = p.name; break; } }
                            PrintCol("■[%zu]: ID=%d (%s)", opp_id >= 0, i + 1, opp_id, opp_name.c_str());
                        }
                    }

                    DrawGlassSeparator();
                    ImGui::TextColored(UITheme().primary, (const char*)u8"【海克斯与排位段位 (addr21~26)】");
                    PrintCol("addr21=0x%lx | addr23=0x%lx | addr26=0x%lx | hexctrl=0x%lx", IsValidPtr(g_dbg_addr21) && IsValidPtr(g_dbg_addr26), g_dbg_addr21, g_dbg_addr23, g_dbg_addr26, g_dbg_hexctrl);
                    PrintCol("海克斯品质: [%d, %d, %d]", IsValidPtr(g_dbg_hexctrl), g_hex_qualities[0], g_hex_qualities[1], g_hex_qualities[2]);
                }
                break;
            case 3:
                {
                    DrawSectionTitle((const char*)u8"偏移调试与实时诊断");
                if (ImGui::Button((const char*)u8"保存全部配置", ImVec2(-1, 34 * g_autoScale * g_scale))) { SaveConfig(); AddActionLog((const char*)u8"-> [配置] 已成功保存所有当前改动过的偏移与设置到本地!"); }
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【主线基础寻址链路】（改一个对应地址立刻显示）");
                DrawOffsetAdjuster("func_get_Instance(获取实例) [辅助主动调用]", &g_off.func_get_Instance, g_dbg_addr1, true);
                DrawOffsetAdjuster("addr2(我的玩家对象偏移)", &g_off.addr2, g_dbg_addr2, true);
                DrawOffsetAdjuster("addr3(玩家列表指针)", &g_off.addr3, g_dbg_addr3, true);
                DrawOffsetAdjuster("addra(未知预留偏移)", &g_off.addra, g_dbg_addra, true);
                DrawOffsetAdjuster("segmentcsogame(核心游戏组件)", &g_off.segmentcsogame, g_dbg_segmentcsogame, true);
                DrawOffsetAdjuster("segment_my_player_id(我的玩家ID组件)", &g_off.segment_my_player_id, (uintptr_t)g_my_player_id, true);
                DrawOffsetAdjuster("func_quit(退出游戏) [辅助主动调用]", &g_off.func_quit, (uintptr_t)(g_il2cppTrueBase + g_off.func_quit), true);
                DrawOffsetAdjuster("func_set_IsGameEnd(判断结束) [Hook游戏监听]", &g_off.func_set_IsGameEnd, (uintptr_t)(g_il2cppTrueBase + g_off.func_set_IsGameEnd), true);
                DrawOffsetAdjuster("next_opponents_list(下一回合对手列表)", &g_off.next_opponents_list);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【玩家字典 (addr11~12)】");
                DrawOffsetAdjuster("addr11(玩家字典外层)", &g_off.addr11, g_dbg_addr11, true);
                DrawOffsetAdjuster("addr12(玩家字典对象)", &g_off.addr12, g_dbg_addr12, true);
                DrawOffsetAdjuster(" -> dict struct_size(字典结构大小)", &g_off.addr12_struct_size);
                DrawOffsetAdjuster(" -> dict ptr_offset(字典指针偏移)", &g_off.addr12_ptr_offset);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【玩家基本属性 (addr13)】");
                DrawOffsetAdjuster("addr13(单个玩家信息对象)", &g_off.addr13, g_dbg_addr13, true);
                DrawOffsetAdjuster("pi_name(玩家名字偏移)", &g_off.pi_name);
                DrawOffsetAdjuster("pi_id(玩家ID偏移)", &g_off.pi_id);
                DrawOffsetAdjuster("pi_is_bot(是否机器人)", &g_off.pi_is_bot);
                DrawOffsetAdjuster("pi_money(玩家金币)", &g_off.pi_money);
                DrawOffsetAdjuster("pi_level(玩家等级)", &g_off.pi_level);
                DrawOffsetAdjuster("pi_win_streak(连胜)", &g_off.pi_win_streak);
                DrawOffsetAdjuster("pi_lose_streak(连败)", &g_off.pi_lose_streak);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【牌库字典链 (addr4~7)】");
                DrawOffsetAdjuster("addr4(牌库管理节点4)", &g_off.addr4, g_dbg_addr4, true);
                DrawOffsetAdjuster("addr5(牌库管理节点5)", &g_off.addr5, g_dbg_addr5, true);
                DrawOffsetAdjuster("addr6(牌库管理节点6)", &g_off.addr6, g_dbg_addr6, true);
                DrawOffsetAdjuster("addr7(牌库字典外层)", &g_off.addr7, g_dbg_addr7, true);
                DrawOffsetAdjuster(" -> addr7 struct_size(字典结构大小)", &g_off.addr7_struct_size);
                DrawOffsetAdjuster(" -> addr7 ptr_offset(字典指针偏移)", &g_off.addr7_ptr_offset);
                DrawOffsetAdjuster("addr9(牌库内部数据)", &g_off.addr9, g_dbg_addr9, true);
                DrawOffsetAdjuster(" -> addr9 struct_size(内部结构大小)", &g_off.addr9_struct_size);
                DrawOffsetAdjuster(" -> addr9 ptr_offset(内部指针偏移)", &g_off.addr9_ptr_offset);
                DrawOffsetAdjuster("ph_heroId(牌库英雄ID)", &g_off.ph_heroId);
                DrawOffsetAdjuster("ph_remaining(牌库剩余数量)", &g_off.ph_remaining);
                DrawOffsetAdjuster("ph_total(牌库总数量)", &g_off.ph_total);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【海克斯与排位 (addr21~26)】");
                DrawOffsetAdjuster("addr21(海克斯管理节点1)", &g_off.addr21, g_dbg_addr21, true);
                DrawOffsetAdjuster("addr22(海克斯管理节点2)", &g_off.addr22, g_dbg_addr22, true);
                DrawOffsetAdjuster("addr23(海克斯列表字典)", &g_off.addr23, g_dbg_addr23, true);
                DrawOffsetAdjuster(" -> addr23 struct_size(列表结构大小)", &g_off.addr23_struct_size);
                DrawOffsetAdjuster(" -> addr23 ptr_offset(列表指针偏移)", &g_off.addr23_ptr_offset);
                DrawOffsetAdjuster("addr26(单个海克斯对象)", &g_off.addr26, g_dbg_addr26, true);
                DrawOffsetAdjuster("pi_avatar_rank(海克斯品质/等级)", &g_off.pi_avatar_rank);
                DrawOffsetAdjuster("pi_avatar_player_id(海克斯所属玩家ID)", &g_off.pi_avatar_player_id);
                DrawOffsetAdjuster("hexctrl(海克斯控制对象)", &g_off.hexctrl, g_dbg_hexctrl, true);
                DrawOffsetAdjuster("func_get_hex(获取海克斯) [辅助主动调用]", &g_off.func_get_hex, (uintptr_t)(g_il2cppTrueBase + g_off.func_get_hex), true);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【商店、备战区与场上】");
                DrawOffsetAdjuster("addr14(商店管理对象)", &g_off.addr14, g_dbg_addr14, true);
                DrawOffsetAdjuster("addr15(商店槽位列表)", &g_off.addr15, g_dbg_addr15, true);
                DrawOffsetAdjuster("addr16(单个商店槽位)", &g_off.addr16, g_dbg_addr16, true);
                DrawOffsetAdjuster("shop_hero_id(商店英雄ID)", &g_off.shop_hero_id);
                DrawOffsetAdjuster("addr17(备战席管理对象)", &g_off.addr17, g_dbg_addr17, true);
                DrawOffsetAdjuster("addr18(备战席槽位列表)", &g_off.addr18, g_dbg_addr18, true);
                DrawOffsetAdjuster("bench_hero_id(备战席英雄ID)", &g_off.bench_hero_id);
                DrawOffsetAdjuster("addr19(棋盘管理对象)", &g_off.addr19, g_dbg_addr19, true);
                DrawOffsetAdjuster("addr20(棋盘槽位列表)", &g_off.addr20, g_dbg_addr20, true);
                DrawOffsetAdjuster("board_hero_id(棋盘英雄ID)", &g_off.board_hero_id);
                DrawOffsetAdjuster("board_x(棋盘X坐标)", &g_off.board_x);
                DrawOffsetAdjuster("board_y(棋盘Y坐标)", &g_off.board_y);
                DrawGlassSeparator();
                ImGui::TextColored(UITheme().primary, (const char*)u8"【全局Hook】");
                DrawOffsetAdjuster("func_shop_listen(监听商店) [Hook游戏监听]", &g_off.func_shop_listen, (uintptr_t)(g_il2cppTrueBase + g_off.func_shop_listen), true);
                DrawOffsetAdjuster("func_buy_hero_new(购买英雄) [辅助主动调用]", &g_off.func_buy_hero_new, (uintptr_t)(g_il2cppTrueBase + g_off.func_buy_hero_new), true);
                DrawOffsetAdjuster("func_SendWillRenderCanvases(UI渲染主循环) [Hook游戏监听]", &g_off.func_SendWillRenderCanvases, (uintptr_t)(g_il2cppTrueBase + g_off.func_SendWillRenderCanvases), true);
                break;
            }
            case 4:
                {
                    DrawSectionTitle((const char*)u8"符号反查与全量寻址");
                    DrawSymbolResolverUI();
                    break;
                }
            case 5:
                {
                    DrawSectionTitle((const char*)u8"运行时对象检查器 (Object Inspector)");
                    float avail_w = ImGui::GetContentRegionAvail().x;
                    float avail_h = ImGui::GetContentRegionAvail().y;
                    DrawObjectInspectorContent(avail_w, avail_h);
                    break;
                }
            default:
                current_tab = 0;
                break;
            }

            ImGui::EndChild();
        }
    }
    DrawStandaloneInspectorWindow();
        ImGui::End();
    DrawActionLogOverlay();
}



int g_current_frame = 0;
std::atomic<bool> g_engine_rendering{false};
std::atomic<int> g_active_renderer{0}; // 0=未知, 1=OpenGL ES, 2=Vulkan

// 1. OpenGL / EGL 原始函数指针
unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
void* (*old_eglGetProcAddress)(const char*) = nullptr;

// 2. Vulkan 原始函数指针
VkResult (*old_vkQueuePresentKHR)(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) = nullptr;
VkResult (*old_vkCreateInstance)(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) = nullptr;

// 3. JNI 触摸原始函数指针
JavaVM* g_jvm = nullptr;
jobject g_view_obj = nullptr;
jobject g_context = nullptr;
void (*old_nativeInjectEvent)(JNIEnv*, jobject, jobject) = nullptr;

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
            if (meClass) {
                motionEventClassGlobal = (jclass)env->NewGlobalRef(meClass);
                env->DeleteLocalRef(meClass);
            }
        }

        if (motionEventClassGlobal && env->IsInstanceOf(event, motionEventClassGlobal)) {
            static jmethodID getWidthMid = nullptr, getHeightMid = nullptr;
            static jmethodID getActionMid = nullptr, getXMid = nullptr, getYMid = nullptr;

            if (getWidthMid == nullptr) {
                jclass viewClass = env->GetObjectClass(obj);
                getWidthMid = env->GetMethodID(viewClass, "getWidth", "()I");
                getHeightMid = env->GetMethodID(viewClass, "getHeight", "()I");
                env->DeleteLocalRef(viewClass);

                getActionMid = env->GetMethodID(motionEventClassGlobal, "getAction", "()I");
                getXMid = env->GetMethodID(motionEventClassGlobal, "getX", "()F");
                getYMid = env->GetMethodID(motionEventClassGlobal, "getY", "()F");
            }

            if (getActionMid && getXMid && getYMid) {
                int action = env->CallIntMethod(event, getActionMid) & 255;
                if (g_cached_view_width <= 0 && getWidthMid && getHeightMid) {
                    g_cached_view_width = env->CallIntMethod(obj, getWidthMid);
                    g_cached_view_height = env->CallIntMethod(obj, getHeightMid);
                }

                float raw_x = env->CallFloatMethod(event, getXMid);
                float raw_y = env->CallFloatMethod(event, getYMid);

                float scale_x = 1.0f, scale_y = 1.0f;
                if (g_cached_view_width > 0 && g_gl_width > 0) scale_x = (float)g_gl_width / g_cached_view_width;
                if (g_cached_view_height > 0 && g_gl_height > 0) scale_y = (float)g_gl_height / g_cached_view_height;

                ImGuiIO& io = ImGui::GetIO();
                io.AddMousePosEvent(raw_x * scale_x, raw_y * scale_y);

                if (action == 0) { // ACTION_DOWN
                    io.AddMouseButtonEvent(0, true);
                    
                } else if (action == 1 || action == 3) { // ACTION_UP / ACTION_CANCEL
                    io.AddMouseButtonEvent(0, false);
                }

                // 如果按下的地方处于 ImGui 菜单窗口内部，截断事件不透传给游戏
                if (io.WantCaptureMouse) return;
            }
        }
    }

    if (old_nativeInjectEvent) old_nativeInjectEvent(env, obj, event);
}

void FindAndHookHiddenJNI() {
    FILE* fp = fopen("/proc/self/maps", "r"); if (!fp) return;
    char line[1024];
    struct MemRegion { uintptr_t start, end; bool is_rw; };
    std::vector<MemRegion> regions;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "libunity.so")) {
            bool is_r = strstr(line, "r-") != nullptr;
            bool is_rw = strstr(line, "rw") != nullptr;
            if (is_r || is_rw) {
                uintptr_t start, end;
                sscanf(line, "%lx-%lx", &start, &end);
                regions.push_back({start, end, is_rw});
            }
        }
    }
    fclose(fp);

    const char* target_string = "nativeInjectEvent";
    std::vector<uintptr_t> string_addrs;
    for (const auto& reg : regions) {
        if (!reg.is_rw) {
            for (uintptr_t p = reg.start; p < reg.end - strlen(target_string); p++) {
                {
                    char tmp_buf[32] = {0};
                    size_t tlen = strlen(target_string);
                    if (tlen <= sizeof(tmp_buf) && SafeReadMemory(p, tmp_buf, tlen) && memcmp(tmp_buf, target_string, tlen) == 0)
                        string_addrs.push_back(p);
                }
            }
        }
    }

    if (string_addrs.empty()) return;

    bool found_func = false;
    for (const auto& reg : regions) {
        uintptr_t align_start = (reg.start + 7) & ~7;
        for (uintptr_t p = align_start; p < reg.end - sizeof(void*)*3; p += sizeof(void*)) {
            uintptr_t ptr_val = 0;
                if (!SafeReadMemory(p, &ptr_val, sizeof(ptr_val))) continue;
            for (uintptr_t str_addr : string_addrs) {
                if (ptr_val == str_addr) {
                    uintptr_t real_func_val = 0;
                        if (!SafeReadMemory(p + 16, &real_func_val, sizeof(real_func_val))) continue;
                        void* real_function_addr = (void*)real_func_val;
                    if (real_function_addr != nullptr && (uintptr_t)real_function_addr > 0x100000) {
                        DobbyHook(real_function_addr, (void*)hook_nativeInjectEvent, (void**)&old_nativeInjectEvent);
                        LOGI("[+] nativeInjectEvent Hooked for Touch Input.");
                        found_func = true; break;
                    }
                }
            }
            if (found_func) break;
        }
        if (found_func) break;
    }
}

typedef void (*func_set_IsGameEnd_t)(void* thisObj, uint8_t isEnd);
func_set_IsGameEnd_t orig_set_IsGameEnd = nullptr;

void hook_set_IsGameEnd(void* thisObj, uint8_t isEnd) { g_count_set_IsGameEnd++;
    if (orig_set_IsGameEnd) orig_set_IsGameEnd(thisObj, isEnd);
    if (!thisObj || !IsValidPtr((uintptr_t)thisObj)) return;
    if (isEnd == 0) {
        AddActionLog((const char*)u8"-> [引擎状态] call func_set_IsGameEnd(isEnd=0) | 游戏开始!");
        g_match_enter_pending.store(true, std::memory_order_release);
    } else if (g_is_in_match.load(std::memory_order_acquire)) {
        AddActionLog((const char*)u8"-> [引擎状态] call func_set_IsGameEnd(isEnd=%d) | 游戏结束!", isEnd);
        g_is_in_match.store(false, std::memory_order_release);
        g_match_enter_pending.store(false, std::memory_order_release);
        g_need_segment_gap_before_enter = true;
        g_Tasks.trigger_game_end.store(true, std::memory_order_release);
    }
}

typedef void* (*func_SendWillRenderCanvases_t)();
func_SendWillRenderCanvases_t orig_SendWillRenderCanvases = nullptr;
void* hook_SendWillRenderCanvases() { g_count_SendWillRenderCanvases++;
    {
        std::lock_guard<std::mutex> lock(g_Tasks.buy_mutex);
        if (!g_Tasks.buy_slots.empty()) {
            typedef void (*func_buy_new_t)(void*);
            func_buy_new_t buy_hero = (func_buy_new_t)(g_il2cppTrueBase + g_off.func_buy_hero_new);
            if (buy_hero && IsValidExecutableAddr((void*)buy_hero)) { g_count_buy_hero_new++;
                for (const auto& item : g_Tasks.buy_slots) {
                    if (IsValidPtr(item.slot_addr)) {


                        AddActionLog((const char*)u8"-> [自动购买] call buy_hero_new(slot_addr=0x%lx) | hero_id=%d", item.slot_addr, item.hero_id);
                        SAFE_CALL_VOID(buy_hero((void*)item.slot_addr));
                    } else {
                        AddActionLog((const char*)u8"-> [自动购买失败] slot_addr(0x%lx) 指针无效!", item.slot_addr);
                    }
                }
            } else {
                AddActionLog((const char*)u8"-> [自动购买失败] 函数地址(0x%lx) 无效!", (uintptr_t)buy_hero);
            }
            g_Tasks.buy_slots.clear();
        }
    }
    if (g_Tasks.trigger_quit.load()) {
        g_Tasks.trigger_quit.store(false);
        typedef void (*func_quit_t)(uintptr_t, int, int);
        func_quit_t quit_func = (func_quit_t)(g_il2cppTrueBase + g_off.func_quit);
        if (quit_func && IsValidExecutableAddr((void*)quit_func) && IsValidPtr(g_dbg_segmentcsogame)) {
            AddActionLog((const char*)u8"-> [极速退游] call func_quit(segment=0x%lx, player_id=%d, mode=0)", g_dbg_segmentcsogame, g_my_player_id);
            g_count_func_quit++; SAFE_CALL_VOID(quit_func(g_dbg_segmentcsogame, g_my_player_id, 0));
        } else {
            AddActionLog((const char*)u8"-> [退游失败] func_quit指针或segment无效!");
        }
        g_is_in_match.store(false, std::memory_order_release);
        g_need_segment_gap_before_enter = true;
        g_Tasks.trigger_game_end.store(true, std::memory_order_release);
    }
    if (orig_SendWillRenderCanvases) return orig_SendWillRenderCanvases();
    return nullptr;
}

// ==========================================
// 满血高刷帧率解锁引擎 (强制解除 Android VSync 60Hz 限制与 Unity targetFrameRate 限制)
// ==========================================
static void ForceUnlock144FPS(EGLDisplay display) {
    // 1. 解除 EGL 交换链 60Hz 垂直同步等待
    typedef EGLBoolean (*eglSwapInterval_t)(EGLDisplay dpy, EGLint interval);
    static eglSwapInterval_t p_eglSwapInterval = nullptr;
    if (!p_eglSwapInterval) {
        p_eglSwapInterval = (eglSwapInterval_t)DobbySymbolResolver("libEGL.so", "eglSwapInterval");
        if (!p_eglSwapInterval) {
            void* h = dlopen("libEGL.so", RTLD_LAZY);
            if (h) p_eglSwapInterval = (eglSwapInterval_t)dlsym(h, "eglSwapInterval");
        }
    }
    if (p_eglSwapInterval && display != EGL_NO_DISPLAY) {
        p_eglSwapInterval(display, 0); // 0 = 禁用 VSync 锁 60 帧，允许 144Hz/120Hz 满血刷新
    }

    // 2. 动态反射注入 Unity 引擎 Application.targetFrameRate = 144 / QualitySettings.vSyncCount = 0
    if (g_il2cpp_api.init() && g_il2cpp_api.runtime_invoke) {
        static bool s_unity_fps_unlocked = false;
        static int s_unlock_retries = 0;
        if (!s_unity_fps_unlocked && s_unlock_retries < 10) {
            s_unlock_retries++;
            void* domain = g_il2cpp_api.domain_get ? g_il2cpp_api.domain_get() : nullptr;
            if (domain && g_il2cpp_api.domain_get_assemblies) {
                size_t ass_count = 0;
                void** assemblies = g_il2cpp_api.domain_get_assemblies(domain, &ass_count);
                if (assemblies) {
                    for (size_t i = 0; i < ass_count; i++) {
                        void* img = g_il2cpp_api.assembly_get_image(assemblies[i]);
                        if (!img) continue;
                        const char* img_name = g_il2cpp_api.image_get_name ? g_il2cpp_api.image_get_name(img) : "";
                        if (strstr(img_name, "UnityEngine.CoreModule") || strstr(img_name, "UnityEngine")) {
                            size_t cls_cnt = g_il2cpp_api.image_get_class_count ? g_il2cpp_api.image_get_class_count(img) : 0;
                            for (size_t c = 0; c < cls_cnt; c++) {
                                void* klass = g_il2cpp_api.image_get_class(img, c);
                                if (!klass) continue;
                                const char* c_name = g_il2cpp_api.class_get_name ? g_il2cpp_api.class_get_name(klass) : "";
                                if (strcmp(c_name, "Application") == 0) {
                                    void* iter = nullptr;
                                    while (void* m = g_il2cpp_api.class_get_methods(klass, &iter)) {
                                        const char* m_name = g_il2cpp_api.method_get_name ? g_il2cpp_api.method_get_name(m) : "";
                                        if (strcmp(m_name, "set_targetFrameRate") == 0) {
                                            int targetFps = 144;
                                            void* args[1] = { &targetFps };
                                            void* exc = nullptr;
                                            g_il2cpp_api.runtime_invoke(m, nullptr, args, &exc);
                                            s_unity_fps_unlocked = true;
                                            LOGI("[+] ForceUnlock144FPS: Successfully set Application.targetFrameRate = 144!");
                                            break;
                                        }
                                    }
                                }
                                if (strcmp(c_name, "QualitySettings") == 0) {
                                    void* iter = nullptr;
                                    while (void* m = g_il2cpp_api.class_get_methods(klass, &iter)) {
                                        const char* m_name = g_il2cpp_api.method_get_name ? g_il2cpp_api.method_get_name(m) : "";
                                        if (strcmp(m_name, "set_vSyncCount") == 0) {
                                            int vsync = 0;
                                            void* args[1] = { &vsync };
                                            void* exc = nullptr;
                                            g_il2cpp_api.runtime_invoke(m, nullptr, args, &exc);
                                            LOGI("[+] ForceUnlock144FPS: Successfully set QualitySettings.vSyncCount = 0!");
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void RenderImGui_Core_GLES(EGLDisplay display, EGLSurface surface) {
    g_current_frame++;
    ForceUnlock144FPS(display);
    if (g_active_renderer.load() == 0) g_active_renderer.store(1);
    if (!g_engine_rendering.load()) g_engine_rendering.store(true);

    eglQuerySurface(display, surface, EGL_WIDTH, &g_gl_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_gl_height);
    if (g_gl_width <= 0 || g_gl_height <= 0) { g_gl_width = 2400; g_gl_height = 1080; }

    static int s_last_screen_w = 0, s_last_screen_h = 0;
    if (s_last_screen_w != g_gl_width || s_last_screen_h != g_gl_height) {
        s_last_screen_w = g_gl_width;
        s_last_screen_h = g_gl_height;
        float short_edge = std::min((float)g_gl_width, (float)g_gl_height);
        if (short_edge < 300.0f) short_edge = 1080.0f;
        g_autoScale = short_edge / 1080.0f;
        g_needUpdateFontSafe = true;
    }

    if (g_current_frame % 120 == 0) {
        LOGI("[*] GLES Render Heartbeat | Frame: %d | Resolution: %dx%d", g_current_frame, g_gl_width, g_gl_height);
    }

    // 备份 OpenGL 状态（避免查询不支持的 GLES 枚举）
    GLint last_active_texture = 0; glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLint last_program = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLint last_texture = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    GLint last_array_buffer = 0; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    GLint last_vertex_array = 0; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
    GLint last_viewport[4] = {0}; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4] = {0}; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    GLint last_fbo = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_fbo);

    if (!g_isImGuiInit) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        const char* gl_ver = (const char*)glGetString(GL_VERSION);
        const char* glsl_ver = "#version 300 es";
        if (gl_ver && strstr(gl_ver, "OpenGL ES 2.")) glsl_ver = "#version 100";

        ImGui_ImplOpenGL3_Init(glsl_ver);
        io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
        SetupImGuiStyle();
        UpdateFontHD(true);
        g_isImGuiInit = true;
        LOGI("[+] GLES ImGui Initialized. Resolution: %dx%d", g_gl_width, g_gl_height);
    }
    if (g_needUpdateFontSafe) { UpdateFontHD(true); g_needUpdateFontSafe = false; }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    static auto s_last_frame_time = std::chrono::high_resolution_clock::now();
    auto now_time = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now_time - s_last_frame_time).count();
    s_last_frame_time = now_time;

    if (dt > 0.0001f && dt < 0.5f) {
        io.DeltaTime = dt;
        float instant_fps = 1.0f / dt;
        static float s_smooth_fps = 60.0f;
        s_smooth_fps = s_smooth_fps * 0.90f + instant_fps * 0.10f;
        g_realtime_fps = s_smooth_fps;
        g_fps_frametime_ms = dt * 1000.0f;
    } else {
        io.DeltaTime = 1.0f / 60.0f;
    }

    // 预留前 60 帧缓冲，等待游戏引擎完全加载 il2cpp 单例后再开始读取游戏数据，防止启动加载期空指针闪退
    if (g_current_frame > 60 && g_il2cppTrueBase != 0) {
        ResolveDiagnosticPointers();
        UpdateMatchState();

        if (g_Tasks.trigger_game_end.exchange(false, std::memory_order_acquire))
            ClearGameState();

        if (g_is_in_match.load(std::memory_order_acquire) && (g_current_frame % 2 == 0))
            ParseGameMemory();
    }

    ProcessTextureQueue();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    DrawMainMenu();
    DrawHexKeypadModal();
    DrawCardPoolWindow();
    DrawPlayerDataWindow();
    DrawOpponentBoardWindow();
    DrawMyHeroWarningWindow();
    DrawHextechCapsule(); DrawVirtualKeyboard();
    // 胶囊最后绘制并置顶，避免被牌库等浮窗挡住无法解锁
    DrawQuitCapsule();
    DrawLockCapsule();
    DrawCardPoolCapsule();
    DrawActionLogOverlay();
    if (g_apply_saved_float_pos) g_apply_saved_float_pos = false;

    ImGui::Render();

    // 强行刷新状态机并绑定画面最顶层
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_gl_width, g_gl_height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // 还原 Unity 原始 OpenGL 状态
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);

    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

unsigned int hook_eglSwap(EGLDisplay display, EGLSurface surface) {
    static bool in_render = false;
    if (!in_render) {
        in_render = true;
        RenderImGui_Core_GLES(display, surface);
        in_render = false;
    }
    if (old_eglSwap) return old_eglSwap(display, surface);
    return 1;
}

void* hook_eglGetProcAddress(const char* procname) {
    if (!procname) return nullptr;
    if (strcmp(procname, "eglSwapBuffers") == 0 || strcmp(procname, "eglSwapBuffersWithDamageKHR") == 0) {
        return (void*)hook_eglSwap;
    }
    if (old_eglGetProcAddress) return old_eglGetProcAddress(procname);
    return nullptr;
}

VkResult hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    g_current_frame++;
    if (g_active_renderer.load() == 0) g_active_renderer.store(2);

    if (g_current_frame % 180 == 0) {
        LOGI("[*] Vulkan Present Heartbeat | Frame: %d", g_current_frame);
    }

    if (old_vkQueuePresentKHR) return old_vkQueuePresentKHR(queue, pPresentInfo);
    return VK_SUCCESS;
}

VkResult hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    LOGI("[+] Vulkan Instance creation intercepted!");
    VkResult res = VK_SUCCESS;
    if (old_vkCreateInstance) {
        res = old_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    }

    if (res == VK_SUCCESS && pInstance && *pInstance) {
        void* present_ptr = DobbySymbolResolver("libvulkan.so", "vkQueuePresentKHR");
        if (present_ptr && !old_vkQueuePresentKHR) {
            DobbyHook(present_ptr, (void*)hook_vkQueuePresentKHR, (void**)&old_vkQueuePresentKHR);
            LOGI("[+] Vulkan QueuePresent Hooked Successfully.");
        }
    }
    return res;
}

void* Il2CppInitThread(void*) {
    LOGI("[+] Background Il2Cpp Init Thread Started...");
    while (g_il2cppTrueBase == 0) {
        FILE *fp = fopen("/proc/self/maps", "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "libil2cpp.so")) {
                    sscanf(line, "%lx", &g_il2cppTrueBase); break;
                }
            }
            fclose(fp);
        }
        if (g_il2cppTrueBase == 0) sleep(1);
    }
    LOGI("[+] libil2cpp.so Base Found: 0x%lx", (unsigned long)g_il2cppTrueBase);

    // Wait for il2cpp to fully initialize, then build executable regions cache
    sleep(2);
    UpdateIl2CppExecRegions();

    LoadConfig();
    // 自动挂载核心 Hook
    if (g_off.func_shop_listen != 0 && old_shop_listen == nullptr) {
        void* target = (void*)(g_il2cppTrueBase + g_off.func_shop_listen);
        SafeDobbyHook(target, (void*)hook_shop_listen, (void**)&old_shop_listen);
    }
    if (g_off.func_set_IsGameEnd != 0 && orig_set_IsGameEnd == nullptr) {
        void* target = (void*)(g_il2cppTrueBase + g_off.func_set_IsGameEnd);
        SafeDobbyHook(target, (void*)hook_set_IsGameEnd, (void**)&orig_set_IsGameEnd);
    }
    if (g_off.func_SendWillRenderCanvases != 0 && orig_SendWillRenderCanvases == nullptr) {
        void* target = (void*)(g_il2cppTrueBase + g_off.func_SendWillRenderCanvases);
        SafeDobbyHook(target, (void*)hook_SendWillRenderCanvases, (void**)&orig_SendWillRenderCanvases);
    }









    AddActionLog((const char*)u8"-> [系统] 助手核心与调用监视系统已就绪");
    EnsureTextureWorkerStarted();
    return nullptr;
}

static void AutoSetPermissiveSELinux() {
    // 1. 直接写入 Linux 内核 SELinux 状态节点 (0 = Permissive 宽松模式)
    int fd = open("/sys/fs/selinux/enforce", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
    }
    // 2. 多重 shell / su 兜底执行 setenforce 0
    system("setenforce 0 2>/dev/null");
    system("su -c setenforce 0 2>/dev/null");
    system("su 0 setenforce 0 2>/dev/null");
    LOGI("[+] [SELinux] Auto setenforce 0 (Permissive mode) applied!");
}

void* SetupThread(void*) {
    AutoSetPermissiveSELinux();
    InitCrashGuard();
    LOGI("[+] Adaptive Dual-Engine Setup Thread Started...");

    // 1. 优先对 Vulkan 通道进行挂钩（无阻塞）
    void* vk_create_ptr = DobbySymbolResolver("libvulkan.so", "vkCreateInstance");
    if (!vk_create_ptr) { void* h = dlopen("libvulkan.so", RTLD_LAZY); if (h) vk_create_ptr = dlsym(h, "vkCreateInstance"); }
    if (vk_create_ptr) {
        DobbyHook(vk_create_ptr, (void*)hook_vkCreateInstance, (void**)&old_vkCreateInstance);
        LOGI("[+] Vulkan CreateInstance Interceptor Set.");
    }

    void* vk_present_ptr = DobbySymbolResolver("libvulkan.so", "vkQueuePresentKHR");
    if (!vk_present_ptr) { void* h = dlopen("libvulkan.so", RTLD_LAZY); if (h) vk_present_ptr = dlsym(h, "vkQueuePresentKHR"); }
    if (vk_present_ptr) {
        DobbyHook(vk_present_ptr, (void*)hook_vkQueuePresentKHR, (void**)&old_vkQueuePresentKHR);
        LOGI("[+] Vulkan QueuePresent Direct Hook Set.");
    }

    // 2. 挂钩 OpenGL ES / EGL 双通道（同时支持符号 Hook 与 GetProcAddress 重定向）
    void* egl_ptr = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    if (!egl_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) egl_ptr = dlsym(h, "eglSwapBuffers"); }
    if (egl_ptr) {
        DobbyHook(egl_ptr, (void*)hook_eglSwap, (void**)&old_eglSwap);
        LOGI("[+] EGL SwapBuffers Direct Hook Set.");
    }

    void* getproc_ptr = DobbySymbolResolver("libEGL.so", "eglGetProcAddress");
    if (!getproc_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) getproc_ptr = dlsym(h, "eglGetProcAddress"); }
    if (getproc_ptr) {
        DobbyHook(getproc_ptr, (void*)hook_eglGetProcAddress, (void**)&old_eglGetProcAddress);
        LOGI("[+] EGL GetProcAddress Interceptor Set.");
    }

    // 3. 后台启动 il2cpp 基址探测与配置加载线程，不阻塞主图形管道
    pthread_t t_il2cpp;
    pthread_create(&t_il2cpp, 0, Il2CppInitThread, 0);
    pthread_detach(t_il2cpp);

    // 4. 立即极速挂载触摸事件拦截器（毫秒级就绪，彻底消除注入后无法拖动的延迟！）
    for (int retry = 0; retry < 25; retry++) {
        FindAndHookHiddenJNI();
        if (old_nativeInjectEvent != nullptr) break;
        usleep(80000); // 80ms 快速重试探测
    }

    LOGI("[+] All adaptive hooks and touch interceptors installed successfully!");
    return nullptr;
}

__attribute__((constructor)) void Init() { 
    pthread_t t; 
    pthread_create(&t, 0, SetupThread, 0); 
    pthread_detach(t); 
}
