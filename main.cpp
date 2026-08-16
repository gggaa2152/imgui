#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <atomic>
#include <algorithm>
#include <string>
#include <dirent.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "dobby.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JKAutoHook", __VA_ARGS__)

bool g_show_menu = true;
bool g_isImGuiInit = false;
ImFont* g_mainFont = nullptr;
float g_autoScale = 1.0f;
float g_current_rendered_size = 0.0f;

int g_gl_width = 1080;
int g_gl_height = 2400;
int g_current_frame = 0;

// 渲染引擎识别标志：0=未知, 1=OpenGL ES, 2=Vulkan
std::atomic<int> g_active_renderer{0};

// 1. OpenGL / EGL 原始函数指针
unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
void* (*old_eglGetProcAddress)(const char*) = nullptr;

// 2. Vulkan 原始函数指针
VkResult (*old_vkQueuePresentKHR)(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) = nullptr;
VkResult (*old_vkCreateInstance)(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) = nullptr;

std::string FindChineseFontPath() {
    const char* known_paths[] = {
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/NotoSansTC-Regular.otf",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/SysSans-Hans-Regular.ttf",
        "/system/fonts/Miui-Regular.ttf",
        "/system/fonts/SourceHanSansCN-Regular.otf",
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
                    name.find("Hans") != std::string::npos || name.find("Fallback") != std::string::npos) {
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
        if (g_mainFont) g_mainFont->Scale = 1.0f / 1.5f;
    }

    if (!g_mainFont) {
        g_mainFont = io.Fonts->AddFontDefault();
        if (g_mainFont) g_mainFont->Scale = 1.0f / 1.5f;
    }

    io.Fonts->Build();
    ImGui_ImplOpenGL3_CreateDeviceObjects();
    g_current_rendered_size = targetSize;
}

void DrawMainMenu() {
    ImGui::SetNextWindowPos(ImVec2(100.0f * g_autoScale, 100.0f * g_autoScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500.0f * g_autoScale, 340.0f * g_autoScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin((const char*)u8"自适应双引擎控制菜单", &g_show_menu)) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), (const char*)u8"✓ 菜单已成功渲染上屏！");
        ImGui::Separator();

        int mode = g_active_renderer.load();
        if (mode == 1) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), (const char*)u8"当前捕获渲染引擎: OpenGL ES 3.0");
        } else if (mode == 2) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), (const char*)u8"当前捕获渲染引擎: Vulkan");
        } else {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), (const char*)u8"当前捕获渲染引擎: 检测中...");
        }

        ImGui::Text((const char*)u8"实时渲染帧数: %d", g_current_frame);
        ImGui::Text((const char*)u8"屏幕分辨率: %d x %d", g_gl_width, g_gl_height);

        ImGui::Separator();
        static bool toggle1 = true;
        static float slider_val = 1.0f;
        ImGui::Checkbox((const char*)u8"测试开关 1", &toggle1);
        ImGui::SliderFloat((const char*)u8"测试数值", &slider_val, 0.0f, 10.0f, "%.1f");

        if (ImGui::Button((const char*)u8"测试点击按钮", ImVec2(-1, 38.0f * g_autoScale))) {
            LOGI("[+] Test button clicked!");
        }
    }
    ImGui::End();

    ImGui::ShowDemoWindow(&g_show_menu);
}

void RenderImGui_Core_GLES(EGLDisplay display, EGLSurface surface) {
    g_current_frame++;
    if (g_active_renderer.load() == 0) g_active_renderer.store(1);

    eglQuerySurface(display, surface, EGL_WIDTH, &g_gl_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_gl_height);
    if (g_gl_width <= 0 || g_gl_height <= 0) { g_gl_width = 1080; g_gl_height = 2400; }

    // 备份 OpenGL 状态
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
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        const char* gl_ver = (const char*)glGetString(GL_VERSION);
        const char* glsl_ver = "#version 300 es";
        if (gl_ver && strstr(gl_ver, "OpenGL ES 2.")) glsl_ver = "#version 100";

        ImGui_ImplOpenGL3_Init(glsl_ver);
        io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
        UpdateFontHD(true);
        g_isImGuiInit = true;
        LOGI("[+] GLES ImGui Initialized. Resolution: %dx%d", g_gl_width, g_gl_height);
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    DrawMainMenu();

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
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);

    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

unsigned int hook_eglSwap(EGLDisplay display, EGLSurface surface) {
    RenderImGui_Core_GLES(display, surface);
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

VkResult hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    g_current_frame++;
    if (g_active_renderer.load() == 0) g_active_renderer.store(2);

    // 如果游戏跑在 Vulkan 上，触发帧计数
    if (g_current_frame % 180 == 0) {
        LOGI("[*] Vulkan Present Heartbeat | Frame: %d", g_current_frame);
    }

    if (old_vkQueuePresentKHR) return old_vkQueuePresentKHR(queue, pPresentInfo);
    return VK_SUCCESS;
}

VkResult hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    LOGI("[+] Vulkan Instance creation intercepted! Allowing Vulkan normally...");
    
    VkResult res = VK_SUCCESS;
    if (old_vkCreateInstance) {
        res = old_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    }

    // 动态在 Vulkan Instance 中寻找 vkQueuePresentKHR 函数地址并进行 Hook
    if (res == VK_SUCCESS && pInstance && *pInstance) {
        void* present_ptr = DobbySymbolResolver("libvulkan.so", "vkQueuePresentKHR");
        if (present_ptr && !old_vkQueuePresentKHR) {
            DobbyHook(present_ptr, (void*)hook_vkQueuePresentKHR, (void**)&old_vkQueuePresentKHR);
            LOGI("[+] Vulkan QueuePresent Hooked Successfully.");
        }
    }
    return res;
}

void* SetupThread(void*) {
    LOGI("[+] Adaptive Dual-Engine Setup Thread Started...");

    // 1. 尝试对 Vulkan 通道进行挂钩（不强行阻断）
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

    // 2. 尝试对 OpenGL / EGL 通道进行挂钩
    void* getproc_ptr = DobbySymbolResolver("libEGL.so", "eglGetProcAddress");
    if (!getproc_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) getproc_ptr = dlsym(h, "eglGetProcAddress"); }
    if (getproc_ptr) {
        DobbyHook(getproc_ptr, (void*)hook_eglGetProcAddress, (void**)&old_eglGetProcAddress);
        LOGI("[+] EGL GetProcAddress Hook Set.");
    }

    void* egl_ptr = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    if (!egl_ptr) { void* h = dlopen("libEGL.so", RTLD_LAZY); if (h) egl_ptr = dlsym(h, "eglSwapBuffers"); }
    if (egl_ptr) {
        DobbyHook(egl_ptr, (void*)hook_eglSwap, (void**)&old_eglSwap);
        LOGI("[+] EGL SwapBuffers Hook Set.");
    }

    LOGI("[+] All adaptive hooks installed. Waiting for game render pipeline...");
    return nullptr;
}

__attribute__((constructor)) void Init() {
    pthread_t t;
    pthread_create(&t, 0, SetupThread, 0);
    pthread_detach(t);
}
