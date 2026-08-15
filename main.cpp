#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <atomic>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "dobby.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JKDiagnostic", __VA_ARGS__)

// 渲染全局标志
static std::atomic<bool> g_isImGuiInit{false};
static bool g_show_diagnostic_window = true;
static int g_gl_width = 1080;
static int g_gl_height = 2400;

// EGL 原始函数指针
static unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
static unsigned int (*old_eglSwapWithDamage)(EGLDisplay, EGLSurface, EGLint*, EGLint) = nullptr;

// -------------------------------------------------------------
// 核心 ImGui 最简渲染逻辑（带完整的 OpenGL 状态备份与还原）
// -------------------------------------------------------------
static void RenderDiagnosticUI(EGLDisplay display, EGLSurface surface) {
    // 1. 获取屏幕真实宽高
    eglQuerySurface(display, surface, EGL_WIDTH, &g_gl_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_gl_height);
    if (g_gl_width <= 0 || g_gl_height <= 0) {
        g_gl_width = 1080;
        g_gl_height = 2400;
    }

    // 2. 备份 Unity 当前的 OpenGL 状态（防止 ImGui 与 Unity 互相污染状态导致黑屏或隐形）
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
