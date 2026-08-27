#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <elf.h>
#include <dlfcn.h>
#include <errno.h>

// Android NDK 常不声明 process_vm_*，用 syscall 号直接调
#ifndef __NR_process_vm_readv
#  if defined(__x86_64__)
#    define __NR_process_vm_readv 310
#    define __NR_process_vm_writev 311
#  elif defined(__aarch64__)
#    define __NR_process_vm_readv 270
#    define __NR_process_vm_writev 271
#  endif
#endif

static ssize_t ProcessVmReadv(pid_t pid, const struct iovec* local_iov, unsigned long liovcnt,
                              const struct iovec* remote_iov, unsigned long riovcnt, unsigned long flags) {
#ifdef __NR_process_vm_readv
    return (ssize_t)syscall(__NR_process_vm_readv, pid, local_iov, liovcnt, remote_iov, riovcnt, flags);
#else
    (void)pid; (void)local_iov; (void)liovcnt; (void)remote_iov; (void)riovcnt; (void)flags;
    errno = ENOSYS;
    return -1;
#endif
}

static ssize_t ProcessVmWritev(pid_t pid, const struct iovec* local_iov, unsigned long liovcnt,
                               const struct iovec* remote_iov, unsigned long riovcnt, unsigned long flags) {
#ifdef __NR_process_vm_writev
    return (ssize_t)syscall(__NR_process_vm_writev, pid, local_iov, liovcnt, remote_iov, riovcnt, flags);
#else
    (void)pid; (void)local_iov; (void)liovcnt; (void)remote_iov; (void)riovcnt; (void)flags;
    errno = ENOSYS;
    return -1;
#endif
}

#if defined(__aarch64__)
#  define INJECTOR_ARCH_NAME "ARM64"
#  define pt_regs user_pt_regs
#  ifndef PTRACE_GETREGS
#    define PTRACE_GETREGS 12
#    define PTRACE_SETREGS 13
#  endif
#elif defined(__x86_64__)
#  define INJECTOR_ARCH_NAME "x86_64"
#  define pt_regs user_regs_struct
#else
#  error "Unsupported architecture. Build for aarch64 or x86_64."
#endif

#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif

#ifndef EM_AARCH64
#define EM_AARCH64 183
#endif
#ifndef EM_X86_64
#define EM_X86_64 62
#endif

// Android NativeBridgeCallbacks（按指针自然对齐，与 ART 头文件一致）
struct NativeBridgeCallbacksRemote {
    uint32_t version;
    void* initialize;
    void* loadLibrary;
    void* getTrampoline;
    void* isSupported;
    void* getAppEnv;
    void* isCompatibleWith;
    void* getSignalHandler;
    void* unloadLibrary;
    void* getError;
    void* isPathSupported;
    void* initAnonymousNamespace;
    void* createNamespace;
    void* linkNamespaces;
    void* loadLibraryExt;
    void* getVendorNamespace;
    void* getExportedNamespace;
};

const char* TARGET_PROCESS = "com.tencent.jkchess";
const char* PAYLOAD_PATH = "/data/1/libMyMenu.so";

static const char* ErrnoName(int e) {
    switch (e) {
        case ENOSYS: return "ENOSYS(功能未实现)";
        case EPERM:  return "EPERM(权限不足)";
        case ESRCH:  return "ESRCH(进程不存在)";
        case EIO:    return "EIO";
        case EINVAL: return "EINVAL(参数/架构不匹配)";
        case EBUSY:  return "EBUSY";
        default:     return "unknown";
    }
}

static bool ContainsIgnoreCase(const char* hay, const char* needle) {
    if (!hay || !needle || !*needle) return false;
    for (const char* h = hay; *h; ++h) {
        const char* a = h;
        const char* b = needle;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
            if (ca != cb) break;
            ++a; ++b;
        }
        if (!*b) return true;
    }
    return false;
}

static const char* ElfMachineName(uint16_t m) {
    switch (m) {
        case EM_AARCH64: return "ARM64";
        case EM_X86_64:  return "x86_64";
        case EM_ARM:     return "ARM32";
        case EM_386:     return "x86";
        default:         return "unknown";
    }
}

static uint16_t ReadElfMachineFromFd(int fd) {
    unsigned char hdr[64];
    if (lseek(fd, 0, SEEK_SET) < 0) return 0;
    if (read(fd, hdr, sizeof(hdr)) < 20) return 0;
    if (hdr[0] != 0x7f || hdr[1] != 'E' || hdr[2] != 'L' || hdr[3] != 'F') return 0;
    uint16_t em = 0;
    memcpy(&em, hdr + 18, sizeof(em));
    return em;
}

static uint16_t GetProcessElfMachine(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/exe", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    uint16_t em = ReadElfMachineFromFd(fd);
    close(fd);
    return em;
}

static uint16_t GetFileElfMachine(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    uint16_t em = ReadElfMachineFromFd(fd);
    close(fd);
    return em;
}

static uint16_t InjectorElfMachine() {
#if defined(__aarch64__)
    return EM_AARCH64;
#elif defined(__x86_64__)
    return EM_X86_64;
#else
    return 0;
#endif
}

pid_t GetPID(const char *process_name) {
    pid_t pid = -1;
    DIR *dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        int current_pid = atoi(entry->d_name);
        if (current_pid == 0) continue;

        char cmdline_path[256];
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", current_pid);

        FILE *fp = fopen(cmdline_path, "r");
        if (fp) {
            char cmdline[256];
            if (fgets(cmdline, sizeof(cmdline), fp) != NULL) {
                if (strcmp(cmdline, process_name) == 0) {
                    pid = current_pid;
                    fclose(fp);
                    break;
                }
            }
            fclose(fp);
        }
    }
    closedir(dir);
    return pid;
}

uintptr_t GetModuleBase(pid_t pid, const char *module_name) {
    uintptr_t exec_base = 0;
    uintptr_t any_base = 0;
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);

    FILE *fp = fopen(maps_path, "r");
    if (!fp) return 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, module_name)) continue;
        if (!strstr(line, "r-xp") && !strstr(line, "r--p") && !strstr(line, "rw-p")) continue;
        uintptr_t addr = 0;
        if (sscanf(line, "%lx", &addr) != 1) continue;
        if (any_base == 0 || addr < any_base) any_base = addr;
        if (strstr(line, "x") && exec_base == 0) exec_base = addr;
    }
    fclose(fp);
    // NativeBridgeItf 在数据段：必须用该 so 的最低映射作为 load bias
    return any_base ? any_base : exec_base;
}

// 从 maps 取模块在磁盘上的真实路径
static bool GetModulePath(pid_t pid, const char* module_name, char* out, size_t out_sz) {
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    FILE* fp = fopen(maps_path, "r");
    if (!fp) return false;
    char line[1024];
    bool ok = false;
    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, module_name)) continue;
        char* path = strchr(line, '/');
        if (!path) continue;
        size_t n = strcspn(path, "\r\n");
        if (n == 0 || n >= out_sz) continue;
        memcpy(out, path, n);
        out[n] = 0;
        ok = true;
        break;
    }
    fclose(fp);
    return ok;
}

static bool AddrInMaps(pid_t pid, uintptr_t addr) {
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    FILE* fp = fopen(maps_path, "r");
    if (!fp) return false;
    char line[1024];
    bool ok = false;
    while (fgets(line, sizeof(line), fp)) {
        uintptr_t start = 0, end = 0;
        if (sscanf(line, "%lx-%lx", &start, &end) != 2) continue;
        if (addr >= start && addr < end) { ok = true; break; }
    }
    fclose(fp);
    return ok;
}

static bool MapsContain(pid_t pid, const char* needle) {
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    FILE* fp = fopen(maps_path, "r");
    if (!fp) return false;
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, needle)) { found = true; break; }
    }
    fclose(fp);
    return found;
}

static bool DetectNativeBridge(pid_t pid) {
    char nb_prop[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.dalvik.vm.native.bridge", nb_prop);
    if (nb_prop[0] && strcmp(nb_prop, "0") != 0) return true;
    return MapsContain(pid, "libhoudini") || MapsContain(pid, "libndk_translation")
        || MapsContain(pid, "libnb.so");
}

static void GetNativeBridgeLibName(char* out, size_t out_sz) {
    char nb_prop[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.dalvik.vm.native.bridge", nb_prop);
    if (nb_prop[0] && strcmp(nb_prop, "0") != 0) {
        snprintf(out, out_sz, "%s", nb_prop);
        return;
    }
    snprintf(out, out_sz, "libhoudini.so");
}

// 解析 ELF64 动态符号，返回符号相对模块基址的虚拟地址偏移
static bool FindElfDynSymOffset(const char* elf_path, const char* sym_name, uintptr_t* out_off) {
    int fd = open(elf_path, O_RDONLY);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) { close(fd); return false; }
    size_t sz = (size_t)st.st_size;
    uint8_t* buf = (uint8_t*)malloc(sz);
    if (!buf) { close(fd); return false; }
    if ((size_t)read(fd, buf, sz) != sz) { free(buf); close(fd); return false; }
    close(fd);

    if (sz < sizeof(Elf64_Ehdr) || buf[0] != 0x7f) { free(buf); return false; }
    Elf64_Ehdr* eh = (Elf64_Ehdr*)buf;
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) { free(buf); return false; }

    Elf64_Shdr* sh = (Elf64_Shdr*)(buf + eh->e_shoff);
    Elf64_Shdr* dynsym = nullptr;
    Elf64_Shdr* dynstr = nullptr;
    for (int i = 0; i < eh->e_shnum; i++) {
        if (sh[i].sh_type == SHT_DYNSYM) dynsym = &sh[i];
    }
    if (!dynsym) { free(buf); return false; }
    dynstr = &sh[dynsym->sh_link];

    size_t count = dynsym->sh_size / dynsym->sh_entsize;
    Elf64_Sym* syms = (Elf64_Sym*)(buf + dynsym->sh_offset);
    const char* strs = (const char*)(buf + dynstr->sh_offset);
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        const char* name = strs + syms[i].st_name;
        if (strcmp(name, sym_name) == 0 && syms[i].st_value != 0) {
            *out_off = (uintptr_t)syms[i].st_value;
            found = true;
            break;
        }
    }
    free(buf);
    return found;
}

static void PrintEnvHints(pid_t pid, uint16_t payload_em) {
    char brand[PROP_VALUE_MAX] = {0};
    char model[PROP_VALUE_MAX] = {0};
    char abi[PROP_VALUE_MAX] = {0};
    char nb_prop[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.product.brand", brand);
    __system_property_get("ro.product.model", model);
    __system_property_get("ro.product.cpu.abi", abi);
    __system_property_get("ro.dalvik.vm.native.bridge", nb_prop);

    bool bridge = DetectNativeBridge(pid);
    printf("[*] 注入器架构: %s\n", INJECTOR_ARCH_NAME);
    printf("[*] 设备: brand=%s model=%s abi=%s\n", brand, model, abi);
    printf("[*] 目标进程 ELF: %s\n", ElfMachineName(GetProcessElfMachine(pid)));
    printf("[*] 载荷 SO ELF: %s (%s)\n", ElfMachineName(payload_em), PAYLOAD_PATH);
    printf("[*] native.bridge=%s detected=%s\n", nb_prop[0] ? nb_prop : "(empty)", bridge ? "yes" : "no");

#if defined(__x86_64__)
    if (bridge && payload_em == EM_AARCH64) {
        printf("[*] 模式: Houdini/NativeBridge 注入 (x86_64 injector -> ARM64 SO)\n");
    } else {
        printf("[*] 模式: 原生 x86_64 dlopen\n");
    }
#else
    if (bridge) {
        printf("[!] 检测到原生桥。MuMu 请改用 x86_64 版 injector + 仍用 ARM64 的 libMyMenu.so\n");
    } else {
        printf("[*] 模式: 真机 ARM64 ptrace dlopen\n");
    }
#endif
}

uintptr_t GetRemoteFuncAddr(pid_t target_pid, const char *module_name, void *local_func_addr) {
    uintptr_t local_base = GetModuleBase(getpid(), module_name);
    uintptr_t remote_base = GetModuleBase(target_pid, module_name);
    if (local_base == 0 || remote_base == 0) return 0;
    return (uintptr_t)local_func_addr - local_base + remote_base;
}

bool PtraceRead(pid_t pid, uintptr_t addr, void *buf, size_t size) {
    // 1) process_vm_readv（模拟器上比 PEEKTEXT 更稳）
    struct iovec local_iov = { buf, size };
    struct iovec remote_iov = { (void*)(uintptr_t)addr, size };
    errno = 0;
    ssize_t n = ProcessVmReadv(pid, &local_iov, 1, &remote_iov, 1, 0);
    if (n == (ssize_t)size) return true;
    int e_vm = errno;

    // 2) /proc/pid/mem
    char mem_path[64];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
    int fd = open(mem_path, O_RDONLY);
    if (fd >= 0) {
        errno = 0;
        ssize_t rn = pread(fd, buf, size, (off_t)addr);
        int e_mem = errno;
        close(fd);
        if (rn == (ssize_t)size) return true;
        printf("[-] RemoteRead pread(0x%lx) failed rn=%zd errno=%d/%s\n",
               (unsigned long)addr, rn, e_mem, ErrnoName(e_mem));
    } else {
        printf("[-] 打开 %s 失败 errno=%d\n", mem_path, errno);
    }

    // 3) ptrace peek
    uintptr_t i = 0;
    while (i < size) {
        errno = 0;
        long data = ptrace(PTRACE_PEEKTEXT, pid, (void*)(addr + i), NULL);
        if (data == -1 && errno != 0) {
            printf("[-] RemoteRead peek(0x%lx) errno=%d/%s (vm_readv errno=%d)\n",
                   (unsigned long)(addr + i), errno, ErrnoName(errno), e_vm);
            return false;
        }
        size_t copy_size = (size - i) > sizeof(long) ? sizeof(long) : (size - i);
        memcpy((uint8_t*)buf + i, &data, copy_size);
        i += copy_size;
    }
    return true;
}

bool PtraceWrite(pid_t pid, uintptr_t addr, const void *buf, size_t size) {
    // 1) process_vm_writev
    struct iovec local_iov = { (void*)buf, size };
    struct iovec remote_iov = { (void*)(uintptr_t)addr, size };
    errno = 0;
    ssize_t n = ProcessVmWritev(pid, &local_iov, 1, &remote_iov, 1, 0);
    if (n == (ssize_t)size) return true;
    int e_vm = errno;

    // 2) /proc/pid/mem
    char mem_path[64];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
    int fd = open(mem_path, O_RDWR);
    if (fd < 0) fd = open(mem_path, O_WRONLY);
    if (fd >= 0) {
        errno = 0;
        ssize_t wn = pwrite(fd, buf, size, (off_t)addr);
        int e_mem = errno;
        close(fd);
        if (wn == (ssize_t)size) return true;
        printf("[-] RemoteWrite pwrite(0x%lx) wn=%zd errno=%d/%s\n",
               (unsigned long)addr, wn, e_mem, ErrnoName(e_mem));
    }

    // 3) ptrace poke
    uintptr_t i = 0;
    while (i < size) {
        size_t copy_size = (size - i) > sizeof(long) ? sizeof(long) : (size - i);
        long data = 0;
        if (copy_size < sizeof(long)) {
            errno = 0;
            data = ptrace(PTRACE_PEEKTEXT, pid, (void*)(addr + i), NULL);
            if (data == -1 && errno != 0) {
                printf("[-] RemoteWrite peek(0x%lx) errno=%d (vm_writev errno=%d)\n",
                       (unsigned long)(addr + i), errno, e_vm);
                return false;
            }
            memcpy(&data, (uint8_t*)buf + i, copy_size);
        } else {
            memcpy(&data, (uint8_t*)buf + i, sizeof(long));
        }
        if (ptrace(PTRACE_POKETEXT, pid, (void*)(addr + i), (void*)data) == -1) {
            printf("[-] RemoteWrite poke(0x%lx) errno=%d/%s (vm_writev errno=%d)\n",
                   (unsigned long)(addr + i), errno, ErrnoName(errno), e_vm);
            return false;
        }
        i += copy_size;
    }
    return true;
}

static bool PtraceGetRegs(pid_t pid, struct pt_regs* regs) {
    struct iovec io;
    io.iov_base = regs;
    io.iov_len = sizeof(*regs);
    errno = 0;
    if (ptrace(PTRACE_GETREGSET, pid, (void*)(uintptr_t)NT_PRSTATUS, &io) == 0)
        return true;
    int e1 = errno;
    errno = 0;
    if (ptrace(PTRACE_GETREGS, pid, NULL, regs) == 0) {
        printf("[*] GETREGSET 不可用(errno=%d/%s)，已回退 GETREGS\n", e1, ErrnoName(e1));
        return true;
    }
    printf("[-] 读取寄存器失败: GETREGSET errno=%d/%s, GETREGS errno=%d/%s\n",
           e1, ErrnoName(e1), errno, ErrnoName(errno));
#if defined(__aarch64__)
    if (DetectNativeBridge(pid)) {
        printf("[-] MuMu/Houdini 上请使用 x86_64 版 injector（载荷仍用 ARM64 libMyMenu.so）。\n");
    }
#endif
    return false;
}

static bool PtraceSetRegs(pid_t pid, struct pt_regs* regs) {
    struct iovec io;
    io.iov_base = regs;
    io.iov_len = sizeof(*regs);
    errno = 0;
    if (ptrace(PTRACE_SETREGSET, pid, (void*)(uintptr_t)NT_PRSTATUS, &io) == 0)
        return true;
    int e1 = errno;
    errno = 0;
    if (ptrace(PTRACE_SETREGS, pid, NULL, regs) == 0) {
        printf("[*] SETREGSET 不可用(errno=%d/%s)，已回退 SETREGS\n", e1, ErrnoName(e1));
        return true;
    }
    printf("[-] 写入寄存器失败: SETREGSET errno=%d/%s, SETREGS errno=%d/%s\n",
           e1, ErrnoName(e1), errno, ErrnoName(errno));
    return false;
}

static uintptr_t RegsGetPC(const struct pt_regs* r) {
#if defined(__aarch64__)
    return (uintptr_t)r->pc;
#else
    return (uintptr_t)r->rip;
#endif
}

static uintptr_t RegsGetSP(const struct pt_regs* r) {
#if defined(__aarch64__)
    return (uintptr_t)r->sp;
#else
    return (uintptr_t)r->rsp;
#endif
}

static uintptr_t RegsGetRet(const struct pt_regs* r) {
#if defined(__aarch64__)
    return (uintptr_t)r->regs[0];
#else
    return (uintptr_t)r->rax;
#endif
}

static bool SetupRemoteCall(pid_t pid, struct pt_regs* regs, uintptr_t func_addr, long* params, int num_params) {
#if defined(__aarch64__)
    for (int i = 0; i < num_params && i < 8; i++)
        regs->regs[i] = (uint64_t)params[i];
    regs->sp = (regs->sp - 0x100) & ~0xFULL;
    regs->regs[30] = 0;
    regs->pc = func_addr;
    (void)pid;
    return true;
#else
    if (num_params > 0) regs->rdi = (unsigned long long)params[0];
    if (num_params > 1) regs->rsi = (unsigned long long)params[1];
    if (num_params > 2) regs->rdx = (unsigned long long)params[2];
    if (num_params > 3) regs->rcx = (unsigned long long)params[3];
    if (num_params > 4) regs->r8  = (unsigned long long)params[4];
    if (num_params > 5) regs->r9  = (unsigned long long)params[5];

    regs->rsp = (regs->rsp - 0x100) & ~0xFULL;
    regs->rsp -= 8;
    unsigned long long ret_addr = 0;
    if (!PtraceWrite(pid, (uintptr_t)regs->rsp, &ret_addr, sizeof(ret_addr))) {
        printf("[-] 写入 x86_64 返回地址失败\n");
        return false;
    }
    regs->rip = func_addr;
    return true;
#endif
}

bool PtraceCall(pid_t pid, uintptr_t func_addr, long *params, int num_params, struct pt_regs *regs) {
    if (!SetupRemoteCall(pid, regs, func_addr, params, num_params))
        return false;
    if (!PtraceSetRegs(pid, regs))
        return false;
    if (ptrace(PTRACE_CONT, pid, NULL, 0) == -1) {
        printf("[-] PTRACE_CONT 失败, errno=%d/%s\n", errno, ErrnoName(errno));
        return false;
    }

    int status = 0;
    waitpid(pid, &status, WUNTRACED);
    while (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) == SIGSEGV) {
            if (!PtraceGetRegs(pid, regs)) return false;
            uintptr_t pc = RegsGetPC(regs);
            if (pc != 0) {
                printf("[-] 远程调用在 0x%lx 处发生异常崩溃 (非正常返回)\n", (unsigned long)pc);
                return false;
            }
            return true;
        }
        ptrace(PTRACE_CONT, pid, NULL, 0);
        waitpid(pid, &status, WUNTRACED);
    }
    return false;
}

static int g_remote_str_slot = 0;

static bool RemoteMmapString(pid_t pid, struct pt_regs* original_regs, const char* s, uintptr_t* out_addr) {
    size_t len = strlen(s) + 1;

    // 优先写到目标线程栈上（不同字符串错开槽位，避免互相覆盖）
    uintptr_t sp = RegsGetSP(original_regs);
    if (sp > 0x2000) {
        uintptr_t stack_addr = (sp - 0x900 - (uintptr_t)g_remote_str_slot * 0x100) & ~0xFULL;
        g_remote_str_slot++;
        if (PtraceWrite(pid, stack_addr, s, len)) {
            char check[512];
            if (len <= sizeof(check) && PtraceRead(pid, stack_addr, check, len) && memcmp(check, s, len) == 0) {
                printf("[+] 字符串已写入远程栈 0x%lx\n", (unsigned long)stack_addr);
                *out_addr = stack_addr;
                return true;
            }
        }
        printf("[*] 栈写入失败，回退远程 mmap...\n");
    }

    struct pt_regs regs;
    memcpy(&regs, original_regs, sizeof(regs));
    uintptr_t remote_mmap = GetRemoteFuncAddr(pid, "libc.so", (void*)mmap);
    if (!remote_mmap) {
        printf("[-] 找不到远程 mmap (libc base 解析可能不对)\n");
        return false;
    }
    printf("[*] 远程 mmap @ 0x%lx\n", (unsigned long)remote_mmap);

    long parameters[6] = {
        0, 0x1000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0
    };
    if (!PtraceCall(pid, remote_mmap, parameters, 6, &regs)) {
        printf("[-] 远程 mmap 调用失败\n");
        return false;
    }
    uintptr_t addr = RegsGetRet(&regs);
    printf("[*] mmap 返回 0x%lx\n", (unsigned long)addr);
    if (!addr || addr == (uintptr_t)-1) {
        printf("[-] mmap 返回无效地址\n");
        return false;
    }
    if (!PtraceWrite(pid, addr, s, len)) {
        printf("[-] 写入远程字符串失败 addr=0x%lx len=%zu\n", (unsigned long)addr, len);
        return false;
    }
    *out_addr = addr;
    return true;
}

#if defined(__x86_64__)
static bool TryReadNbItfAt(pid_t pid, uintptr_t itf_addr, NativeBridgeCallbacksRemote* out_cb) {
    memset(out_cb, 0, sizeof(*out_cb));
    if (!AddrInMaps(pid, itf_addr)) {
        printf("[-] NativeBridgeItf 候选地址 0x%lx 不在目标 maps 中\n", (unsigned long)itf_addr);
        return false;
    }
    if (!PtraceRead(pid, itf_addr, out_cb, sizeof(*out_cb)))
        return false;

    // libnb 有时导出的是「指向真实 callbacks 的指针」
    if (out_cb->version == 0 || out_cb->version > 10) {
        uintptr_t maybe_ptr = 0;
        memcpy(&maybe_ptr, out_cb, sizeof(maybe_ptr));
        if (maybe_ptr && AddrInMaps(pid, maybe_ptr)) {
            printf("[*] NativeBridgeItf 像是指针 -> 0x%lx，改读真实结构\n", (unsigned long)maybe_ptr);
            memset(out_cb, 0, sizeof(*out_cb));
            if (!PtraceRead(pid, maybe_ptr, out_cb, sizeof(*out_cb)))
                return false;
        }
    }
    if (out_cb->version == 0 || out_cb->version > 10) {
        printf("[-] NativeBridgeItf@version 异常: %u @0x%lx\n",
               out_cb->version, (unsigned long)itf_addr);
        return false;
    }
    if (!out_cb->loadLibrary && !out_cb->loadLibraryExt) {
        printf("[-] NativeBridgeItf 无 loadLibrary/loadLibraryExt\n");
        return false;
    }
    return true;
}

static bool ResolveNativeBridgeItf(pid_t pid, NativeBridgeCallbacksRemote* out_cb, uintptr_t* out_itf_addr) {
    char nb_name[64];
    GetNativeBridgeLibName(nb_name, sizeof(nb_name));
    printf("[*] NativeBridge 库名: %s\n", nb_name);

    // libnb 是壳，真实 Itf 常在 libhoudini / libndk_translation
    const char* candidates[] = {
        "libhoudini.so", "libndk_translation.so", nb_name, "libnb.so", nullptr
    };

    for (int i = 0; candidates[i]; i++) {
        uintptr_t remote_base = GetModuleBase(pid, candidates[i]);
        if (remote_base == 0) continue;

        char disk_path[512] = {0};
        if (!GetModulePath(pid, candidates[i], disk_path, sizeof(disk_path))) {
            const char* try_paths[] = {
                "/system/lib64/", "/system/lib/", "/apex/com.android.art/lib64/", nullptr
            };
            for (int p = 0; try_paths[p]; p++) {
                snprintf(disk_path, sizeof(disk_path), "%s%s", try_paths[p], candidates[i]);
                if (access(disk_path, R_OK) == 0) break;
                disk_path[0] = 0;
            }
        }
        if (!disk_path[0]) continue;

        uintptr_t sym_off = 0;
        if (!FindElfDynSymOffset(disk_path, "NativeBridgeItf", &sym_off)) {
            printf("[*] %s 无 NativeBridgeItf 符号，跳过\n", candidates[i]);
            continue;
        }

        uintptr_t itf_addr = remote_base + sym_off;
        printf("[+] 尝试 %s base=0x%lx Itf=0x%lx (off=0x%lx) path=%s\n",
               candidates[i], (unsigned long)remote_base, (unsigned long)itf_addr,
               (unsigned long)sym_off, disk_path);

        if (!TryReadNbItfAt(pid, itf_addr, out_cb))
            continue;

        printf("[+] NativeBridge version=%u loadLibrary=%p loadLibraryExt=%p\n",
               out_cb->version, out_cb->loadLibrary, out_cb->loadLibraryExt);
        if (out_itf_addr) *out_itf_addr = itf_addr;
        return true;
    }

    printf("[-] 读取 NativeBridgeItf 失败（已尝试 houdini/ndk/nb）\n");
    return false;
}

static void PrintNativeBridgeError(pid_t pid, const NativeBridgeCallbacksRemote* cb,
                                   struct pt_regs* original_regs) {
    if (!cb->getError) return;
    struct pt_regs regs;
    memcpy(&regs, original_regs, sizeof(regs));
    long params[2] = {0};
    if (!PtraceCall(pid, (uintptr_t)cb->getError, params, 0, &regs)) return;
    uintptr_t err_ptr = RegsGetRet(&regs);
    if (!err_ptr) {
        printf("[*] NativeBridge getError() = NULL\n");
        return;
    }
    char err[512];
    memset(err, 0, sizeof(err));
    if (PtraceRead(pid, err_ptr, err, sizeof(err) - 1)) {
        // 试错命名空间时的常见噪音，不是最终失败
        if (strstr(err, "neuralnetworks") || strstr(err, "not accessible for the namespace"))
            printf("[*] 跳过不适用命名空间: %s\n", err);
        else
            printf("[-] NativeBridge error: %s\n", err);
    }
}

static bool NbCallLoad(pid_t pid, const NativeBridgeCallbacksRemote* cb, struct pt_regs* original_regs,
                       uintptr_t remote_path, uintptr_t ns, bool use_ext, uintptr_t* out_handle) {
    struct pt_regs regs;
    memcpy(&regs, original_regs, sizeof(regs));
    long params[6] = {0};
    params[0] = (long)remote_path;
    params[1] = RTLD_NOW;
    if (use_ext) {
        if (!cb->loadLibraryExt) return false;
        params[2] = (long)ns;
        printf("[+] loadLibraryExt(path, RTLD_NOW, ns=%p)\n", (void*)ns);
        if (!PtraceCall(pid, (uintptr_t)cb->loadLibraryExt, params, 3, &regs)) {
            printf("[-] loadLibraryExt 远程调用失败\n");
            return false;
        }
    } else {
        if (!cb->loadLibrary) return false;
        printf("[+] loadLibrary(path, RTLD_NOW)\n");
        if (!PtraceCall(pid, (uintptr_t)cb->loadLibrary, params, 2, &regs)) {
            printf("[-] loadLibrary 远程调用失败\n");
            return false;
        }
    }
    *out_handle = RegsGetRet(&regs);
    return true;
}

// 把 SO 拷到应用可访问目录作回退（部分 MuMu/Houdini 会拒绝直接 load /data/1）
// 加载顺序：先试 /data/1 原路径，再试覆盖后的 files/、data 根、tmp。
static int PreparePayloadPaths(const char* src, char out_paths[][512], int max_paths) {
    int n = 0;
    auto add = [&](const char* path) {
        if (n >= max_paths) return;
        // 去重
        for (int i = 0; i < n; i++) {
            if (strcmp(out_paths[i], path) == 0) return;
        }
        snprintf(out_paths[n], 512, "%s", path);
        n++;
    };

    // 1) 优先直接用用户放的 /data/1/libMyMenu.so
    add(src);

    char dst[512];
    snprintf(dst, sizeof(dst), "/data/data/%s/files/libMyMenu.so", TARGET_PROCESS);
    add(dst);
    snprintf(dst, sizeof(dst), "/data/data/%s/libMyMenu.so", TARGET_PROCESS);
    add(dst);
    add("/data/local/tmp/libMyMenu.so");

    // 覆盖所有非 src 副本，避免残留旧 SO
    for (int i = 0; i < n; i++) {
        if (strcmp(out_paths[i], src) == 0)
            continue;
        if (strstr(out_paths[i], "/files/")) {
            char dir[512];
            snprintf(dir, sizeof(dir), "/data/data/%s/files", TARGET_PROCESS);
            mkdir(dir, 0755);
        }
        char cmd[1280];
        snprintf(cmd, sizeof(cmd), "cp -f '%s' '%s'", src, out_paths[i]);
        int rc = system(cmd);
        if (rc == 0) {
            chmod(out_paths[i], 0755);
            if (access(out_paths[i], R_OK) == 0)
                printf("[+] 已覆盖复制载荷到 %s\n", out_paths[i]);
            else
                printf("[*] 复制后无法访问 %s\n", out_paths[i]);
        } else {
            printf("[*] 复制到 %s 失败 (rc=%d)\n", out_paths[i], rc);
        }
    }
    return n;
}

static bool InjectViaNativeBridge(pid_t pid, const char* library_path,
                                  struct pt_regs* original_regs) {
    g_remote_str_slot = 0;
    NativeBridgeCallbacksRemote cb;
    if (!ResolveNativeBridgeItf(pid, &cb, nullptr))
        return false;

    char paths[4][512];
    int path_count = PreparePayloadPaths(library_path, paths, 4);

    // 收集 namespace 候选
    uintptr_t ns_list[8];
    int ns_count = 0;
    auto add_ns = [&](uintptr_t ns) {
        if (ns_count >= 8) return;
        for (int i = 0; i < ns_count; i++) if (ns_list[i] == ns) return;
        ns_list[ns_count++] = ns;
    };
    // ns=0 在 MuMu 上已验证可用，优先；再试 3 / vendor / exported
    add_ns(0);
    add_ns(3);
    if (cb.getVendorNamespace) {
        struct pt_regs regs;
        memcpy(&regs, original_regs, sizeof(regs));
        long params[2] = {0};
        if (PtraceCall(pid, (uintptr_t)cb.getVendorNamespace, params, 0, &regs)) {
            uintptr_t got = RegsGetRet(&regs);
            if (got) {
                printf("[+] getVendorNamespace -> %p\n", (void*)got);
                add_ns(got);
            }
        }
    }
    if (cb.getExportedNamespace) {
        const char* ns_names[] = {
            "classloader-namespace", "default", nullptr
        };
        for (int i = 0; ns_names[i]; i++) {
            uintptr_t name_addr = 0;
            if (!RemoteMmapString(pid, original_regs, ns_names[i], &name_addr))
                continue;
            struct pt_regs regs;
            memcpy(&regs, original_regs, sizeof(regs));
            long params[2] = { (long)name_addr, 0 };
            printf("[+] getExportedNamespace(\"%s\")...\n", ns_names[i]);
            if (PtraceCall(pid, (uintptr_t)cb.getExportedNamespace, params, 1, &regs)) {
                uintptr_t got = RegsGetRet(&regs);
                if (got) {
                    printf("[+]   -> %p\n", (void*)got);
                    add_ns(got);
                }
            }
        }
    }

    for (int pi = 0; pi < path_count; pi++) {
        if (access(paths[pi], R_OK) != 0) continue;
        printf("[*] 尝试载荷路径: %s\n", paths[pi]);

        uintptr_t remote_path = 0;
        if (!RemoteMmapString(pid, original_regs, paths[pi], &remote_path))
            continue;

        if (cb.isPathSupported) {
            struct pt_regs regs;
            memcpy(&regs, original_regs, sizeof(regs));
            long params[2] = { (long)remote_path, 0 };
            if (PtraceCall(pid, (uintptr_t)cb.isPathSupported, params, 1, &regs)) {
                printf("[*] isPathSupported(%s) -> %lld\n",
                       paths[pi], (long long)RegsGetRet(&regs));
            }
        }

        uintptr_t handle = 0;

        if (cb.version >= 3 && cb.loadLibraryExt) {
            for (int ni = 0; ni < ns_count; ni++) {
                if (!NbCallLoad(pid, &cb, original_regs, remote_path, ns_list[ni], true, &handle))
                    continue;
                if (handle) {
                    printf("[+] Houdini 注入成功! handle=0x%lx path=%s ns=%p\n",
                           handle, paths[pi], (void*)ns_list[ni]);
                    return true;
                }
                PrintNativeBridgeError(pid, &cb, original_regs);
            }
        }

        if (cb.loadLibrary) {
            if (NbCallLoad(pid, &cb, original_regs, remote_path, 0, false, &handle) && handle) {
                printf("[+] Houdini 注入成功(loadLibrary)! handle=0x%lx path=%s\n",
                       handle, paths[pi]);
                return true;
            }
            PrintNativeBridgeError(pid, &cb, original_regs);
        }
    }

    printf("[-] NativeBridge 返回 NULL。可试:\n");
    printf("    1) 先手动: cp /data/1/libMyMenu.so /data/data/%s/\n", TARGET_PROCESS);
    printf("    2) 游戏完全进大厅/对局后再注入\n");
    printf("    3) chmod 755 目标 so，确认是 ARM64 ELF\n");
    return false;
}
#endif // __x86_64__

static bool InjectViaNativeDlopen(pid_t pid, const char* library_path, struct pt_regs* original_regs) {
    g_remote_str_slot = 0;
    struct pt_regs regs;
    memcpy(&regs, original_regs, sizeof(regs));

    uintptr_t remote_mmap = GetRemoteFuncAddr(pid, "libc.so", (void*)mmap);
    uintptr_t remote_dlopen = GetRemoteFuncAddr(pid, "libdl.so", (void*)dlopen);
    if (!remote_mmap || !remote_dlopen) {
        printf("[-] 无法获取远程 mmap/dlopen: mmap=%lx dlopen=%lx\n", remote_mmap, remote_dlopen);
        return false;
    }
    printf("[+] 远程 mmap=0x%lx dlopen=0x%lx\n", remote_mmap, remote_dlopen);

    uintptr_t remote_string_addr = 0;
    if (!RemoteMmapString(pid, original_regs, library_path, &remote_string_addr))
        return false;
    printf("[+] SO 路径已写入: 0x%lx\n", remote_string_addr);

    memcpy(&regs, original_regs, sizeof(regs));
    long parameters[6] = { (long)remote_string_addr, RTLD_NOW | RTLD_LOCAL, 0, 0, 0, 0 };
    printf("[+] 调用远程 dlopen...\n");
    if (!PtraceCall(pid, remote_dlopen, parameters, 2, &regs)) {
        printf("[-] 远程 dlopen 调用失败\n");
        return false;
    }
    uintptr_t so_handle = RegsGetRet(&regs);
    if (so_handle == 0) {
        printf("[-] dlopen 返回 NULL\n");
        return false;
    }
    printf("[+] 注入成功! SO Handle: 0x%lx\n", so_handle);
    return true;
}

bool InjectLibrary(pid_t pid, const char *library_path) {
    printf("[+] 开始注入目标进程 PID: %d\n", pid);

    uint16_t payload_em = GetFileElfMachine(library_path);
    PrintEnvHints(pid, payload_em);

    bool use_houdini = false;
#if defined(__x86_64__)
    use_houdini = DetectNativeBridge(pid) && (payload_em == EM_AARCH64 || payload_em == 0);
    if (payload_em == EM_AARCH64 && !DetectNativeBridge(pid)) {
        printf("[-] 载荷是 ARM64，但未检测到 NativeBridge，无法在 x86_64 进程直接 dlopen\n");
        return false;
    }
#else
    if (DetectNativeBridge(pid)) {
        printf("[-] 当前是 ARM64 injector，但环境有原生桥。\n");
        printf("    MuMu 请推送 x86_64 版 /data/1/injector，载荷保持 ARM64 libMyMenu.so\n");
        return false;
    }
#endif

    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        printf("[-] Ptrace 附加失败 (需要 root), errno=%d/%s\n", errno, ErrnoName(errno));
        return false;
    }
    waitpid(pid, NULL, WUNTRACED);
    printf("[+] 成功附加到进程\n");

    struct pt_regs original_regs;
    memset(&original_regs, 0, sizeof(original_regs));
    if (!PtraceGetRegs(pid, &original_regs)) {
        printf("[-] 附加后无法读取寄存器，注入中止\n");
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return false;
    }
    printf("[+] 当前 PC=0x%llx SP=0x%llx\n",
           (unsigned long long)RegsGetPC(&original_regs),
           (unsigned long long)RegsGetSP(&original_regs));

    bool ok = false;
#if defined(__x86_64__)
    if (use_houdini) {
        printf("[+] 使用 NativeBridge/Houdini 路径注入 ARM64 SO...\n");
        ok = InjectViaNativeBridge(pid, library_path, &original_regs);
    } else {
        ok = InjectViaNativeDlopen(pid, library_path, &original_regs);
    }
#else
    ok = InjectViaNativeDlopen(pid, library_path, &original_regs);
#endif

    PtraceSetRegs(pid, &original_regs);
    ptrace(PTRACE_DETACH, pid, NULL, NULL);
    printf("[+] 已脱离目标进程\n");
    return ok;
}

static void AutoSetPermissiveSELinux() {
    int fd = open("/sys/fs/selinux/enforce", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
    }
    system("setenforce 0 2>/dev/null");
    system("su -c setenforce 0 2>/dev/null");
    system("su 0 setenforce 0 2>/dev/null");
    printf("[+] [SELinux] Auto setenforce 0 (Permissive mode) applied!\n");
}

int main(int argc, char *argv[]) {
    AutoSetPermissiveSELinux();
    (void)argc;
    (void)argv;
    printf("======================================\n");
    printf("  Android %s Ptrace Injector\n", INJECTOR_ARCH_NAME);
    printf("  Phone: arm64 injector + arm64 SO\n");
    printf("  MuMu : x86_64 injector + arm64 SO (Houdini)\n");
    printf("======================================\n");

    pid_t target_pid = GetPID(TARGET_PROCESS);
    if (target_pid == -1) {
        printf("[-] 未找到目标进程: %s，游戏是否已启动？\n", TARGET_PROCESS);
        return -1;
    }

    if (access(PAYLOAD_PATH, F_OK) == -1) {
        printf("[-] 找不到需要注入的文件: %s\n", PAYLOAD_PATH);
        printf("[-] 请 push ARM64 的 libMyMenu.so 到该路径。\n");
        return -1;
    }

    if (!InjectLibrary(target_pid, PAYLOAD_PATH))
        return -1;
    return 0;
}
