#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <atomic>
#include <algorithm>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "dobby.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JKDiagnostic", __VA_ARGS__)

bool g_show_menu = true;
bool g_isImGuiInit = false;
int g_gl_width = 1080;
int g_gl_height = 2400;
std::atomic<bool> g_engine_rendering{false};
int g_current_frame = 0;

// 原始指针备份
unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
void* (*old_eglGetProcAddress)(const char*) = nullptr;
int (*old_vkCreateInstance)(void*, void*, void*) = nullptr;

// -------------------------------------------------------------
// 核心 ImGui 极简画板 (带严格状态保护)
// -------------------------------------------------------------
void RenderImGui_Core(EGLDisplay display, EGLSurface surface) {
    g_current_frame++;
    if (!g_engine_rendering.load()) g_engine_rendering.store(true);

    eglQuerySurface(display, surface, EGL_WIDTH, &g_gl_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_gl_height);
    if (g_gl_width <= 0 || g_gl_height <= 0) { g_gl_width = 1080; g_gl_height = 2400; }

    // ★ 极其关键：备份 Unity 当前的 OpenGL 状态
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

    // 初始化 ImGui
    if (!g_isImGuiInit) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        const char* gl_ver = (const char*)glGetString(GL_VERSION);
        const char* glsl_ver = "#version 300 es";
        if (gl_ver && strstr(gl_ver, "OpenGL ES 2.")) glsl_ver = "#version 100";
        ImGui_ImplOpenGL3_Init(glsl_ver);
        io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
        io.Fonts->AddFontDefault(); 
        io.Fonts->Build();
        ImGui_ImplOpenGL3_CreateDeviceObjects(); 
        g_isImGuiInit = true;
        LOGI("[+] ImGui Context Created. Resolution: %dx%d", g_gl_width, g_gl_height);
    }
    
    ImGuiIO& io = ImGui::GetIO(); 
    io.DisplaySize = ImVec2((float)g_gl_width, (float)g_gl_height);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame(); 
    ImGui::NewFrame();

    // 绘制测试面板
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("渲染极简测试面板", &g_show_menu)) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "如果你能看到我，说明渲染 Hook 完美！");
        ImGui::Separator();
        ImGui::Text("当前分辨率: %d x %d", g_gl_width, g_gl_height);
        ImGui::Text("当前 FBO: %d", last_fbo);
        ImGui::Text("当前帧数: %d", g_current_frame);
    }
    ImGui::End();

    ImGui::ShowDemoWindow(&g_show_menu);

    ImGui::Render();
    
    // ★ 强行画在屏幕表面
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_gl_width, g_gl_height);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // ★ 彻底恢复游戏状态
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

// -------------------------------------------------------------
// 拦截器 1：拦截 eglSwapBuffers
// -------------------------------------------------------------
unsigned int hook_eglSwap(EGLDisplay display, EGLSurface surface) {
    RenderImGui_Core(display, surface);
    if (old_eglSwap) return old_eglSwap(display, surface);
    return 1;
}

// -------------------------------------------------------------
// 拦截器 2：拦截动态分配 eglGetProcAddress (针对 Houdini 伪装)
// -------------------------------------------------------------
void* hook_eglGetProcAddress(const char* procname) {
    void* real_addr = old_eglGetProcAddress ? old_eglGetProcAddress(procname) : nullptr;
    
    if (procname && (strcmp(procname, "eglSwapBuffers") == 0 || strcmp(procname, "eglSwapBuffersWithDamageKHR") == 0)) {
        LOGI("[!] Intercepted dynamic request for: %s", procname);
        
        // 如果我们还没拿到原指针，趁机保存一份
        if (!old_eglSwap && real_addr) {
            old_eglSwap = (unsigned int (*)(EGLDisplay, EGLSurface))real_addr;
        }
        
        // 无论你要什么，强行塞给引擎我们自己的 Hook 后的画笔！
        return (void*)hook_eglSwap;
    }
    return real_addr;
}

// -------------------------------------------------------------
// 拦截器 3：物理消灭 Vulkan，强逼引擎回滚 OpenGL
// -------------------------------------------------------------
int hook_vkCreateInstance(void* pCreateInfo, void* pAllocator, void* pInstance) {
    LOGI("[!] Vulkan Creation Blocked! Forcing Unity to fallback to OpenGL...");
    return -9; // 强行返回 VK_ERROR_INCOMPATIBLE_DRIVER 错误码
}

// -------------------------------------------------------------
// 安装防线
// -------------------------------------------------------------
void* SetupThread(void*) {
    LOGI("[+] Ultimate Rendering Bypass Setup Started...");

    // 1. 物理消灭 Vulkan
    void* vk_ptr = DobbySymbolResolver("libvulkan.so", "vkCreateInstance");
    if (!vk_ptr) {
        void* h = dlopen("libvulkan.so", RTLD_LAZY);
        if (h) vk_ptr = dlsym(h, "vkCreateInstance");
    }
    if (vk_ptr) {
        DobbyHook(vk_ptr, (void*)hook_vkCreateInstance, (void**)&old_vkCreateInstance);
        LOGI("[+] Vulkan Blocked Successfully.");
    }

    // 2. 拦截动态指针分配
    void* getproc_ptr = DobbySymbolResolver("libEGL.so", "eglGetProcAddress");
    if (!getproc_ptr) {
        void* h = dlopen("libEGL.so", RTLD_LAZY);
        if (h) getproc_ptr = dlsym(h, "eglGetProcAddress");
    }
    if (getproc_ptr) {
        DobbyHook(getproc_ptr, (void*)hook_eglGetProcAddress, (void**)&old_eglGetProcAddress);
        LOGI("[+] eglGetProcAddress Hooked.");
    }

    // 3. 基础静态拦截
    void* egl_ptr = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    if (!egl_ptr) {
        void* h = dlopen("libEGL.so", RTLD_LAZY);
        if (h) egl_ptr = dlsym(h, "eglSwapBuffers");
    }
    if (egl_ptr) {
        DobbyHook(egl_ptr, (void*)hook_eglSwap, (void**)&old_eglSwap);
        LOGI("[+] eglSwapBuffers Hooked.");
    }

    LOGI("[+] All rendering traps set! Waiting for Unity to render...");
    return nullptr;
}

__attribute__((constructor)) void Init() { 
    pthread_t t; 
    pthread_create(&t, 0, SetupThread, 0); 
    pthread_detach(t); 
}

