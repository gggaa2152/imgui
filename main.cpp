#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <vector>
#include <string>
#include <atomic>
#include <algorithm>
#include <cmath>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "dobby.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JKPureMenu", __VA_ARGS__)

bool g_show_menu = true;
bool g_isImGuiInit = false;
ImFont* g_mainFont = nullptr;
float g_autoScale = 1.0f;
float g_current_rendered_size = 0.0f;

int g_gl_width = 1080;
int g_gl_height = 2400;
int g_current_frame = 0;
std::atomic<bool> g_engine_rendering{false};

// 原始函数指针备份
unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
void* (*old_eglGetProcAddress)(const char*) = nullptr;
int (*old_vkCreateInstance)(void*, void*, void*) = nullptr;

// 自动在系统目录中寻找有效的中文字体，杜绝中文问号 (????) 乱码
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

// 高清中文字体加载与 DPI 动态缩放
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

    std::string fontPath = FindChineseFontPath();
    if (!fontPath.empty()) {
        g_mainFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), targetSize * 1.5f, &configMain, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        if (g_mainFont) {
            g_mainFont->Scale = 1.0f / 1.5f;
            LOGI("[+] Loaded Chinese Font: %s", fontPath.c_str());
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

// 极简纯菜单界面
void DrawMainMenu() {
    ImGui::SetNextWindowPos(ImVec2(100.0f * g_autoScale, 100.0f * g_autoScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(480.0f * g_autoScale, 320.0f * g_autoScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin((const char*)u8"功能控制菜单 (纯渲染测试版)", &g_show_menu)) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), (const char*)u8"✓ 菜单已成功在模拟器上绘制！");
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), (const char*)u8"✓ 中文字体加载正常，无问号乱码");
        ImGui::Separator();

        ImGui::Text((const char*)u8"当前渲染帧数: %d", g_current_frame);
        ImGui::Text((const char*)u8"屏幕实时分辨率: %d x %d", g_gl_width, g_gl_height);
        ImGui::Text((const char*)u8"UI 自动缩放比例: %.2f", g_autoScale);

        ImGui::Separator();
        static bool test_toggle1 = true;
        static bool test_toggle2 = false;
        ImGui::Checkbox((const char*)u8"测试功能开关 1", &test_toggle1);
        ImGui::Checkbox((const char*)u8"测试功能开关 2", &test_toggle2);

        static float test_slider = 1.0f;
        ImGui::SliderFloat((const char*)u8"测试滑动条", &test_slider, 0.1f, 5.0f, "%.1f");

        if (ImGui::Button((const char*)u8"点击测试按钮", ImVec2(-1, 36.0f * g_autoScale))) {
            LOGI("[+] Test button clicked inside ImGui menu!");
        }
    }
    ImGui::End();

    // 显示 Demo Window
    ImGui::ShowDemoWindow(&g_show_menu);
}

// 核心 ImGui 渲染管线 (带严格的 OpenGL ES 状态恢复)
void RenderImGui_Core(EGLDisplay display, EGLSurface surface) {
    g_current_frame++;
    if (!g_engine_rendering.load()) g_engine_rendering.store(true);

    eglQuerySurface(display, surface, EGL_WIDTH, &g_gl_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_gl_height);
    if (g_gl_width <= 0 || g_gl_height <= 0) { g_gl_width = 1080; g_gl_height = 2400; }

    // 1. 完整备份 Unity 当前 OpenGL 状态
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

    // 2. 初始化 ImGui 上下文
    if (!g_isImGuiInit) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        const char* gl_ver = (const char*)glGetString(GL_VERSION);
        const char* glsl_ver = "#version 300 es";
        if (gl_ver && strstr(gl_ver, "OpenGL ES 2.")) glsl_ver = "#version 100";

        ImGui_ImplOpenGL3_Init(glsl_ver);
        io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
        UpdateFontHD(true);
        g_isImGuiInit = true;
        LOGI("[+] ImGui Context Created. Resolution: %dx%d", g_gl_width, g_gl_height);
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    io.DeltaTime = 1.0f / 60.0f;

    // 3. 构建 ImGui 帧
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    DrawMainMenu();

    ImGui::Render();

    // 4. 复位状态机
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_gl_width, g_gl_height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // 5. 还原 Unity 原始 OpenGL 状态
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

    if (g_current_frame % 180 == 0) {
        LOGI("[*] Render Heartbeat | Frame: %d | FBO: %d", g_current_frame, last_fbo);
    }
}

// 全屏刷新 Hook
unsigned int hook_eglSwap(EGLDisplay display, EGLSurface surface) {
    RenderImGui_Core(display, surface);
    if (old_eglSwap) return old_eglSwap(display, surface);
    return 1;
}

// 动态寻址 Hook (直接重定向到 hook_eglSwap，不写多余的 eglSwapBuffersWithDamageKHR)
void* hook_eglGetProcAddress(const char* procname) {
    void* real_addr = old_eglGetProcAddress ? old_eglGetProcAddress(procname) : nullptr;
    if (procname && (strcmp(procname, "eglSwapBuffers") == 0 || strcmp(procname, "eglSwapBuffersWithDamageKHR") == 0)) {
        if (!old_eglSwap && real_addr) old_eglSwap = (unsigned int (*)(EGLDisplay, EGLSurface))real_addr;
        return (void*)hook_eglSwap;
    }
    return real_addr;
}

// 屏蔽 Vulkan 拦截点
int hook_vkCreateInstance(void* pCreateInfo, void* pAllocator, void* pInstance) {
    LOGI("[!] Vulkan Blocked! Forcing OpenGL...");
    return -9;
}

// 安装 Hook 防线
void* SetupThread(void*) {
    LOGI("[+] SetupThread started. Installing render hooks...");

    // 1. 强杀 Vulkan 强制让游戏回滚 OpenGL
    void* vk_ptr = DobbySymbolResolver("libvulkan.so", "vkCreateInstance");
    if (!vk_ptr) { void* h = dlopen("libvulkan.so", RTLD_LAZY); if (h) vk_ptr = dlsym(h, "vkCreateInstance"); }
    if (vk_ptr) DobbyHook(vk_ptr, (void*)hook_vkCreateInstance, (void**)&old_vkCreateInstance);

    // 2. 动态拦截 eglGetProcAddress 劫持指针
    void* getproc_ptr = DobbySymbolResolver("libEGL.so", "eglGetProcAddress");
    if (!getproc_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) getproc_ptr = dlsym(h, "eglGetProcAddress"); }
    if (getproc_ptr) DobbyHook(getproc_ptr, (void*)hook_eglGetProcAddress, (void**)&old_eglGetProcAddress);

    // 3. 拦截基础 eglSwapBuffers
    void* egl_ptr = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    if (!egl_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) egl_ptr = dlsym(h, "eglSwapBuffers"); }
    if (egl_ptr) DobbyHook(egl_ptr, (void*)hook_eglSwap, (void**)&old_eglSwap);

    LOGI("[+] Render hooks installed successfully.");
    return nullptr;
}

__attribute__((constructor)) void Init() {
    pthread_t t;
    pthread_create(&t, 0, SetupThread, 0);
    pthread_detach(t);
}
