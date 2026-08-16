#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <atomic>
#include <algorithm>
#include <string>
#include <vector>
#include <dirent.h>
#include <jni.h>

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
int g_cached_view_width = 0;
int g_cached_view_height = 0;
int g_current_frame = 0;

// 渲染引擎识别标志：0=未知, 1=OpenGL ES, 2=Vulkan
std::atomic<int> g_active_renderer{0};

// 1. OpenGL / EGL 原始函数指针
unsigned int (*old_eglSwap)(EGLDisplay, EGLSurface) = nullptr;
void* (*old_eglGetProcAddress)(const char*) = nullptr;

// 2. Vulkan 原始函数指针
VkResult (*old_vkQueuePresentKHR)(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) = nullptr;
VkResult (*old_vkCreateInstance)(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) = nullptr;

// 3. JNI 触摸原始函数指针
JavaVM* g_jvm = nullptr;
jobject g_view_obj = nullptr;
void (*old_nativeInjectEvent)(JNIEnv*, jobject, jobject) = nullptr;

std::string FindChineseFontPath() {
    const char* known_paths[] = {
        "/system/fonts/NotoSansSC-VF.ttf",          // Android 12/13/14 可变中文字体 (如 MuMu 12)
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/NotoSansSC-Regular.ttf",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/SysSans-Hans-Regular.ttf",
        "/system/fonts/Miui-Regular.ttf",
        "/system/fonts/SourceHanSansCN-Regular.otf",
        "/system/fonts/HarmonyOS_Sans_SC.ttf",
        "/system/fonts/OplusSC-Regular.ttf",
        "/system/fonts/VivoSansSC-Regular.ttf",
        "/system/fonts/NotoSansCJK-Regular.ttc"
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
    float targetSize = std::clamp(20.0f * g_autoScale, 18.0f, 48.0f);
    if (!force && std::abs(targetSize - g_current_rendered_size) < 2.0f) return;

    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    io.Fonts->Clear();
    g_mainFont = nullptr;

    // 禁用过高的采样率，控制字体纹理图集在 2048x2048 以内，防止 GLES 纹理溢出
    ImFontConfig configMain;
    configMain.OversampleH = 1;
    configMain.OversampleV = 1;
    configMain.PixelSnapH = true;

    std::string fontPath = FindChineseFontPath();
    if (!fontPath.empty()) {
        // 改用常用简体中文字库，完美平衡汉字覆盖率与纹理内存占用
        g_mainFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), targetSize, &configMain, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        if (g_mainFont) {
            LOGI("[+] Loaded Chinese Font successfully from: %s", fontPath.c_str());
        }
    }

    if (!g_mainFont) {
        LOGI("[!] Warning: Chinese font file failed to build, falling back to default font.");
        g_mainFont = io.Fonts->AddFontDefault();
    }

    io.Fonts->Build();
    ImGui_ImplOpenGL3_CreateDeviceObjects();
    g_current_rendered_size = targetSize;
}

void DrawMainMenu() {
    ImGui::SetNextWindowPos(ImVec2(100.0f * g_autoScale, 100.0f * g_autoScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520.0f * g_autoScale, 360.0f * g_autoScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin((const char*)u8"自适应双引擎控制菜单", &g_show_menu)) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), (const char*)u8"✓ 菜单已成功渲染并支持拖动点击！");
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
            LOGI("[+] Test button clicked successfully!");
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

extern "C" void hook_nativeInjectEvent(JNIEnv* env, jobject obj, jobject event) {
    if (!g_jvm) env->GetJavaVM(&g_jvm);
    if (obj && (!g_view_obj || !env->IsSameObject(g_view_obj, obj))) {
        if (g_view_obj) env->DeleteGlobalRef(g_view_obj);
        g_view_obj = env->NewGlobalRef(obj);
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
            static jmethodID getWidthMid = nullptr, getHeightMid = nullptr, getActionMid = nullptr, getXMid = nullptr, getYMid = nullptr;
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
                    void** fnPtr_addr = (void**)(p + 16);
                    void* real_function_addr = *fnPtr_addr;
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

void* SetupThread(void*) {
    LOGI("[+] Adaptive Dual-Engine Setup Thread Started...");

    // 1. 尝试对 Vulkan 通道进行挂钩
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

    // 3. 3 秒后寻找 libunity.so 并 Hook nativeInjectEvent，绑定手指/鼠标输入
    sleep(3);
    FindAndHookHiddenJNI();

    LOGI("[+] All adaptive hooks and touch interceptors installed successfully!");
    return nullptr;
}

__attribute__((constructor)) void Init() {
    pthread_t t;
    pthread_create(&t, 0, SetupThread, 0);
    pthread_detach(t);
}
