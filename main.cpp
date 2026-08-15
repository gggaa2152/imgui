#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <dlfcn.h> // 【修复】：加回被误删的 dlfcn.h
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
#include <EGL/egl.h>    
#include <jni.h>
#include "dobby.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JKInternal", __VA_ARGS__)

uintptr_t g_il2cppTrueBase = 0;
bool g_show_menu = true;

// -------------------------------------------------------------
// UI State & Global Variables (精简版不包含游戏逻辑)
// -------------------------------------------------------------
bool g_isImGuiInit = false; 
ImFont* g_mainFont = nullptr;
float g_autoScale = 1.0f;
float g_current_rendered_size = 0.0f;
bool g_needUpdateFontSafe = false;

int g_gl_width = 1080;
int g_gl_height = 2400;

unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
unsigned int (*old_eglSwapWithDamage)(EGLDisplay, EGLSurface, EGLint*, EGLint) = nullptr;
std::atomic<bool> g_engine_rendering{false};
int g_current_frame = 0;

// -------------------------------------------------------------
// 字体加载与 UI 初始化
// -------------------------------------------------------------
void UpdateFontHD(bool force = false) {
    ImGuiIO& io = ImGui::GetIO();
    float screenH = (io.DisplaySize.y > 100.0f) ? io.DisplaySize.y : 2400.0f; 
    g_autoScale = screenH / 1080.0f;
    float targetSize = std::clamp(20.0f * g_autoScale, 16.0f, 45.0f); 
    if (!force && std::abs(targetSize - g_current_rendered_size) < 2.0f) return;
    
    ImGui_ImplOpenGL3_DestroyDeviceObjects(); 
    io.Fonts->Clear(); 
    g_mainFont = nullptr; 
    
    ImFontConfig configMain; 
    configMain.OversampleH = 2; 
    configMain.OversampleV = 2; 
    configMain.PixelSnapH = false; 
    
    float mainResFactor = 1.5f; 
    // 尽量加载系统字体，加载失败则 fallback 到自带的默认小字体
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

// -------------------------------------------------------------
// 核心 ImGui 渲染逻辑（带有完整的 OpenGL 状态保护）
// -------------------------------------------------------------
void RenderImGui_Core(EGLDisplay display, EGLSurface surface) {
    g_current_frame++;
    if (!g_engine_rendering.load()) g_engine_rendering.store(true);

    // 1. 获取屏幕宽高
    eglQuerySurface(display, surface, EGL_WIDTH, &g_gl_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_gl_height);
    if (g_gl_width <= 0 || g_gl_height <= 0) { g_gl_width = 1080; g_gl_height = 2400; }

    // 2. ★ 极其关键：备份 Unity 当前的 OpenGL 状态，防止互相污染导致隐形！
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

    // 3. ImGui 初始化
    if (!g_isImGuiInit) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        
        // 自动降级适配版本
        const char* gl_ver = (const char*)glGetString(GL_VERSION);
        const char* glsl_ver = "#version 300 es";
        if (gl_ver && strstr(gl_ver, "OpenGL ES 2.")) glsl_ver = "#version 100";
        
        ImGui_ImplOpenGL3_Init(glsl_ver);
        io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
        
        // 我们在极简模式里不搞花里胡哨的样式，直接默认就行
        UpdateFontHD(true);
        g_isImGuiInit = true;
        LOGI("[+] ImGui Context Created Successfully!");
    }
    
    if (g_needUpdateFontSafe) { UpdateFontHD(true); g_needUpdateFontSafe = false; }
    
    ImGuiIO& io = ImGui::GetIO(); 
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    io.DeltaTime = 1.0f / 60.0f;

    // 4. ImGui 构建帧
    ImGui_ImplOpenGL3_NewFrame(); 
    ImGui::NewFrame();

    // ==========================================
    // 只画一个极其简单的诊断面板和官方 Demo
    // ==========================================
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("渲染极简测试面板", &g_show_menu)) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "如果你能看到我，说明渲染 Hook 完美！");
        ImGui::Separator();
        ImGui::Text("当前分辨率: %d x %d", g_gl_width, g_gl_height);
        ImGui::Text("原始 FBO 编号: %d", last_fbo);
        ImGui::Text("当前帧数: %d", g_current_frame);
    }
    ImGui::End();

    // 召唤出无敌的官方测试面板
    ImGui::ShowDemoWindow(&g_show_menu);
    // ==========================================

    ImGui::Render();
    
    // 5. 强行把画板绑定回屏幕（FBO 0）
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_gl_width, g_gl_height);
    
    // 绘制！
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // 6. ★ 彻底恢复 Unity 原始的 OpenGL 状态
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo); // 极其关键：把 Unity 自己的 FBO 还给它！
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);

    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);

    if (g_current_frame % 180 == 0) {
        LOGI("[*] Diagnostic Render Heartbeat | Frame: %d | FBO: %d | Res: %dx%d", g_current_frame, last_fbo, g_gl_width, g_gl_height);
    }
}

// -------------------------------------------------------------
// 第一扇门：老版 EGL 渲染钩子
// -------------------------------------------------------------
unsigned int hook_eglSwap(EGLDisplay display, EGLSurface surface) {
    RenderImGui_Core(display, surface);
    if (old_eglSwap) return old_eglSwap(display, surface);
    return 1;
}

// -------------------------------------------------------------
// 第二扇门：新版局部刷新 EGL 渲染钩子 (带 KHR)
// -------------------------------------------------------------
unsigned int hook_eglSwapWithDamage(EGLDisplay display, EGLSurface surface, EGLint* rects, EGLint n_rects) {
    RenderImGui_Core(display, surface);
    if (old_eglSwapWithDamage) return old_eglSwapWithDamage(display, surface, rects, n_rects);
    return 1;
}

// -------------------------------------------------------------
// 寻找地址并下钩子
// -------------------------------------------------------------
void* SetupThread(void*) {
    LOGI("[+] Diagnostic SetupThread Started...");
    
    // 采用最标准、最干净的 API 获取函数地址（无论模拟器还是真机）
    void* egl_ptr = (void*)eglGetProcAddress("eglSwapBuffers");
    void* egl_damage_ptr = (void*)eglGetProcAddress("eglSwapBuffersWithDamageKHR");

    if (!egl_ptr) {
        void* handle = dlopen("libEGL.so", RTLD_LAZY);
        if (handle) egl_ptr = dlsym(handle, "eglSwapBuffers");
    }
    
    if (!egl_damage_ptr) {
        void* handle = dlopen("libEGL.so", RTLD_LAZY);
        if (handle) egl_damage_ptr = dlsym(handle, "eglSwapBuffersWithDamageKHR");
    }

    int hook_cnt = 0;
    if (egl_ptr) {
        LOGI("[+] Found eglSwapBuffers at %p, Hooking...", egl_ptr);
        DobbyHook(egl_ptr, (void*)hook_eglSwap, (void**)&old_eglSwap);
        hook_cnt++;
    }

    if (egl_damage_ptr) {
        LOGI("[+] Found eglSwapBuffersWithDamageKHR at %p, Hooking...", egl_damage_ptr);
        DobbyHook(egl_damage_ptr, (void*)hook_eglSwapWithDamage, (void**)&old_eglSwapWithDamage);
        hook_cnt++;
    }

    if (hook_cnt > 0) {
        LOGI("[+] EGL Diagnostic Hooked Successfully! (%d interfaces protected)", hook_cnt);
    } else {
        LOGI("[-] Diagnostic SetupThread abort: No EGL functions found! Is the game using Vulkan?");
    }

    return nullptr;
}

__attribute__((constructor)) void Init() { 
    LOGI("[+] libMyMenu.so Loaded! (Diagnostic Version)");
    pthread_t t; 
    pthread_create(&t, 0, SetupThread, 0); 
    pthread_detach(t); 
}    GLint last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
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

    // 3. 初始化 ImGui 环境（仅执行一次）
    if (!g_isImGuiInit.load()) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        
        // 自动识别 OpenGL ES 版本
        const char* gl_ver = (const char*)glGetString(GL_VERSION);
        const char* glsl_ver = "#version 300 es";
        if (gl_ver && strstr(gl_ver, "OpenGL ES 2.")) {
            glsl_ver = "#version 100";
        }
        
        ImGui_ImplOpenGL3_Init(glsl_ver);
        io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
        io.Fonts->AddFontDefault(); // 使用系统默认字体，拒绝因外部字体文件加载失败崩溃
        
        g_isImGuiInit.store(true);
        LOGI("[+] ImGui Diagnostic Init Complete | GL Version: %s | Res: %dx%d", gl_ver ? gl_ver : "Unknown", g_gl_width, g_gl_height);
    }

    // 4. ImGui 帧更新
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    io.DeltaTime = 1.0f / 60.0f; // 注入固定伪帧率时间

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // 5. 绘制诊断窗口与官方 Demo 窗口
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("渲染极简测试面板", &g_show_diagnostic_window)) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "渲染 Hook 成功运行中！");
        ImGui::Separator();
        ImGui::Text("当前分辨率: %d x %d", g_gl_width, g_gl_height);
        ImGui::Text("原始 FBO 编号: %d", last_fbo);
        if (ImGui::Button("显示/隐藏 ImGui 官方 Demo 窗口")) {
            g_show_diagnostic_window = !g_show_diagnostic_window;
        }
    }
    ImGui::End();

    // 官方测试窗口（双重保险）
    ImGui::ShowDemoWindow(&g_show_diagnostic_window);

    ImGui::Render();

    // 6. 强制绑定主屏幕画板 FBO 0 绘制
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_gl_width, g_gl_height);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // 7. 彻底恢复 Unity 原始的 OpenGL 状态
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo); 
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);

    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);

    static int frame_count = 0;
    if (++frame_count % 180 == 0) {
        LOGI("[*] Diagnostic Render Heartbeat | Frame: %d | FBO: %d", frame_count, last_fbo);
    }
}

// -------------------------------------------------------------
// 渲染 Hook 通道
// -------------------------------------------------------------
static unsigned int hook_eglSwap(EGLDisplay display, EGLSurface surface) {
    RenderDiagnosticUI(display, surface);
    if (old_eglSwap) return old_eglSwap(display, surface);
    return 1;
}

static unsigned int hook_eglSwapWithDamage(EGLDisplay display, EGLSurface surface, EGLint* rects, EGLint n_rects) {
    RenderDiagnosticUI(display, surface);
    if (old_eglSwapWithDamage) return old_eglSwapWithDamage(display, surface, rects, n_rects);
    return 1;
}

// -------------------------------------------------------------
// 初始化入口 Thread
// -------------------------------------------------------------
static void* SetupDiagnosticThread(void*) {
    LOGI("[+] SetupDiagnosticThread Started. Seeking EGL functions...");

    // 采用官方标准 API 获取函数指针
    void* egl_ptr = (void*)eglGetProcAddress("eglSwapBuffers");
    void* egl_damage_ptr = (void*)eglGetProcAddress("eglSwapBuffersWithDamageKHR");

    if (!egl_ptr) {
        void* handle = dlopen("libEGL.so", RTLD_LAZY);
        if (handle) egl_ptr = dlsym(handle, "eglSwapBuffers");
    }
    if (!egl_damage_ptr) {
        void* handle = dlopen("libEGL.so", RTLD_LAZY);
        if (handle) egl_damage_ptr = dlsym(handle, "eglSwapBuffersWithDamageKHR");
    }

    int hook_count = 0;
    if (egl_ptr) {
        LOGI("[+] eglSwapBuffers found at %p, Hooking...", egl_ptr);
        DobbyHook(egl_ptr, (void*)hook_eglSwap, (void**)&old_eglSwap);
        hook_count++;
    }
    if (egl_damage_ptr) {
        LOGI("[+] eglSwapBuffersWithDamageKHR found at %p, Hooking...", egl_damage_ptr);
        DobbyHook(egl_damage_ptr, (void*)hook_eglSwapWithDamage, (void**)&old_eglSwapWithDamage);
        hook_count++;
    }

    LOGI("[+] EGL Diagnostic Hook Finished! Active Hooks: %d", hook_count);
    return nullptr;
}

__attribute__((constructor)) void Init() {
    LOGI("[+] Barebones Diagnostic SO Loaded!");
    pthread_t t;
    pthread_create(&t, nullptr, SetupDiagnosticThread, nullptr);
    pthread_detach(t);
}
