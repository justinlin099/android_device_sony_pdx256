// SPDX-FileCopyrightText: 2026 Justin Lin and contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <asm/ptrace.h>
#include <cutils/sockets.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <private/android_logger.h>
#include <private/android_filesystem_config.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int collect_logs(int log) {
    int socket = android_get_control_socket("logdw");
    if (socket < 0) {
        dprintf(log, "logdw socket unavailable: %s\n", strerror(errno));
        return 74;
    }
    dprintf(log, "logdw collector started\n");
    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
        return 74;
    }
    unsigned written = 0;
    unsigned gatekeeper_written = 0;
    unsigned security_written = 0;
    struct timespec now = start;
    while (now.tv_sec - start.tv_sec < 600) {
        struct pollfd ready = {.fd = socket, .events = POLLIN};
        if (poll(&ready, 1, 500) > 0 && (ready.revents & POLLIN)) {
            char packet[sizeof(android_log_header_t) + LOGGER_ENTRY_MAX_PAYLOAD];
            union {
                struct cmsghdr alignment;
                char buffer[CMSG_SPACE(sizeof(struct ucred))];
            } control;
            struct iovec iov = {.iov_base = packet, .iov_len = sizeof(packet)};
            struct msghdr message = {.msg_iov = &iov, .msg_iovlen = 1,
                                     .msg_control = control.buffer,
                                     .msg_controllen = sizeof(control.buffer)};
            ssize_t length = recvmsg(socket, &message, MSG_DONTWAIT);
            const struct cmsghdr *cred = CMSG_FIRSTHDR(&message);
            if (length > (ssize_t)sizeof(android_log_header_t) + 2 &&
                !(message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) && cred &&
                cred->cmsg_level == SOL_SOCKET && cred->cmsg_type == SCM_CREDENTIALS &&
                cred->cmsg_len >= CMSG_LEN(sizeof(struct ucred))) {
                struct ucred sender;
                memcpy(&sender, CMSG_DATA(cred), sizeof(sender));
                unsigned char buffer_id = packet[0];
                unsigned char priority = packet[sizeof(android_log_header_t)];
                if ((sender.uid == AID_ROOT || sender.uid == AID_SYSTEM || sender.uid == AID_KEYSTORE) &&
                    priority >= ANDROID_LOG_DEBUG &&
                    (buffer_id == LOG_ID_MAIN || buffer_id == LOG_ID_SYSTEM ||
                     buffer_id == LOG_ID_CRASH)) {
                    char *tag = packet + sizeof(android_log_header_t) + 1;
                    char *end = memchr(tag, '\0', packet + length - tag);
                    if (end) {
                        const int gatekeeper = strcasestr(tag, "gatekeeper") != NULL;
                        const int security_error = priority >= ANDROID_LOG_ERROR &&
                            (strcmp(tag, "KeymasterUtils") == 0 ||
                             strcasestr(tag, "qsee") != NULL);
                        unsigned* count = NULL;
                        if (gatekeeper && gatekeeper_written < 180) {
                            count = &gatekeeper_written;
                        } else if (security_error && security_written < 120) {
                            count = &security_written;
                        } else if (priority >= ANDROID_LOG_INFO && written < 300 &&
                                   now.tv_sec - start.tv_sec < 65) {
                            count = &written;
                        }
                        if (count) {
                            char *text = end + 1;
                            dprintf(log, "logdw pid=%d priority=%u %s: %.*s\n", sender.pid, priority,
                                    tag, (int)(packet + length - text), text);
                            ++*count;
                        }
                    }
                }
            }
        }
        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
            return 74;
        }
    }
    dprintf(log, "logdw collector received %u startup, %u Gatekeeper, and %u security messages\n",
            written, gatekeeper_written, security_written);
    return 0;
}

static void log_code_location(int log, pid_t tid, const char *kind, unsigned long address) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", tid);
    FILE *maps = fopen(path, "re");
    if (!maps) {
        dprintf(log, "%s=%#lx, maps unavailable: %s\n", kind, address, strerror(errno));
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), maps)) {
        unsigned long start, end, offset;
        char permissions[5];
        int consumed = 0;
        if (sscanf(line, "%lx-%lx %4s %lx %*s %*s %n", &start, &end, permissions,
                   &offset, &consumed) == 4 && address >= start && address < end &&
            permissions[2] == 'x') {
            char *name = line + consumed;
            name[strcspn(name, "\n")] = '\0';
            dprintf(log, "%s=%#lx %s file offset %#lx\n", kind, address, name,
                    offset + address - start);
            fclose(maps);
            return;
        }
    }
    fclose(maps);
    dprintf(log, "%s=%#lx has no executable mapping\n", kind, address);
}

static void log_keystore_fault(int log, pid_t tid, int signal_number) {
    siginfo_t info = {0};
    if (ptrace(PTRACE_GETSIGINFO, tid, NULL, &info) == 0) {
        dprintf(log, "Keystore2 tid=%d signal=%d code=%d address=%p\n", tid, signal_number,
                info.si_code, info.si_addr);
    }
    struct user_pt_regs regs;
    struct iovec io = {.iov_base = &regs, .iov_len = sizeof(regs)};
    if (ptrace(PTRACE_GETREGSET, tid, (void *)(uintptr_t)NT_PRSTATUS, &io) < 0 ||
        io.iov_len != sizeof(regs)) {
        dprintf(log, "Keystore2 register read failed: %s\n", strerror(errno));
        return;
    }
    log_code_location(log, tid, "PC", regs.pc);
    log_code_location(log, tid, "LR", regs.regs[30]);

    unsigned long frame = regs.regs[29];
    for (int depth = 0; depth < 8 && frame >= regs.sp && frame - regs.sp < 65536 &&
         (frame & 15) == 0; ++depth) {
        errno = 0;
        long next = ptrace(PTRACE_PEEKDATA, tid, (void *)frame, NULL);
        if (errno != 0) break;
        long address = ptrace(PTRACE_PEEKDATA, tid, (void *)(frame + sizeof(long)), NULL);
        if (errno != 0) break;
        log_code_location(log, tid, "frame LR", (unsigned long)address);
        if ((unsigned long)next <= frame) break;
        frame = (unsigned long)next;
    }
}

static int trace_keystore(void) {
    int log = open("/tmp/xperia-keystore-crash.log", O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC);
    if (log < 0) return 74;
    pid_t child = fork();
    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            dprintf(log, "Keystore2 PTRACE_TRACEME failed: %s\n", strerror(errno));
            _exit(74);
        }
        raise(SIGSTOP);
        const char *path = "/tmp/xperia-keystore-bin/xperia_keystore2";
        execl(path, path, "/tmp/xperia-keystore", (char *)NULL);
        dprintf(log, "Keystore2 exec failed: %s\n", strerror(errno));
        _exit(126);
    }
    if (child < 0) {
        dprintf(log, "Keystore2 fork failed: %s\n", strerror(errno));
        close(log);
        return 74;
    }

    int status;
    if (TEMP_FAILURE_RETRY(waitpid(child, &status, 0)) != child || !WIFSTOPPED(status) ||
        ptrace(PTRACE_SETOPTIONS, child, NULL,
               (void *)(uintptr_t)(PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL)) < 0 ||
        ptrace(PTRACE_CONT, child, NULL, NULL) < 0) {
        dprintf(log, "Keystore2 trace startup failed: %s\n", strerror(errno));
        kill(child, SIGKILL);
        close(log);
        return 74;
    }

    dprintf(log, "Tracing Keystore2 pid=%d\n", child);
    int result = 0;
    pid_t tid;
    while ((tid = TEMP_FAILURE_RETRY(waitpid(-1, &status, __WALL))) > 0) {
        if (WIFSTOPPED(status)) {
            int signal_number = WSTOPSIG(status);
            if (signal_number == SIGSEGV || signal_number == SIGBUS ||
                signal_number == SIGILL || signal_number == SIGABRT) {
                log_keystore_fault(log, tid, signal_number);
            }
            if (signal_number == SIGSTOP || (signal_number == SIGTRAP && (status >> 16) != 0)) {
                signal_number = 0;
            }
            if (ptrace(PTRACE_CONT, tid, NULL, (void *)(uintptr_t)signal_number) < 0 &&
                errno != ESRCH) {
                dprintf(log, "Keystore2 continue tid=%d failed: %s\n", tid, strerror(errno));
                result = 74;
                break;
            }
        } else if (tid == child) {
            result = WIFSIGNALED(status) ? 128 + WTERMSIG(status) : WEXITSTATUS(status);
        }
    }
    close(log);
    return result;
}

int main(int argc, char **argv) {
    const char *const services[] = {
        "/vendor/bin/qseecomd",
        "/vendor/bin/hw/vendor.qti.hardware.qseecom@1.0-service",
        "/vendor/bin/hw/android.hardware.security.keymint-service-qti",
        "/vendor/bin/hw/android.hardware.gatekeeper-service-qti",
    };
    if (argc != 2) {
        return 64;
    }
    if (strcmp(argv[1], "--trace-keystore") == 0) return trace_keystore();
    int approved = strcmp(argv[1], "--collect") == 0;
    for (size_t index = 0; index < sizeof(services) / sizeof(services[0]); ++index) {
        if (strcmp(argv[1], services[index]) == 0) {
            approved = 1;
            break;
        }
    }
    if (!approved) {
        return 64;
    }

    int log = open("/tmp/xperia-service.log", O_WRONLY | O_APPEND | O_NOFOLLOW);
    if (log < 0) {
        return 74;
    }
    if (strcmp(argv[1], "--collect") == 0) {
        int status = collect_logs(log);
        close(log);
        return status;
    }
    if (dup2(log, STDOUT_FILENO) < 0 || dup2(log, STDERR_FILENO) < 0) {
        close(log);
        return 74;
    }
    if (log > STDERR_FILENO) {
        close(log);
    }

    char *context = NULL;
    char *label = NULL;
    const int context_status = getcon(&context);
    const int label_status = getfilecon(argv[1], &label);
    dprintf(STDERR_FILENO, "%s uid=%u gid=%u context=%s label=%s\n", argv[1],
            (unsigned)getuid(), (unsigned)getgid(), context_status < 0 ? "unavailable" : context,
            label_status < 0 ? strerror(errno) : label);
    freecon(context);
    freecon(label);

    pid_t child = fork();
    if (child < 0) {
        dprintf(STDERR_FILENO, "%s fork failed: %s\n", argv[1], strerror(errno));
        return 74;
    }
    if (child == 0) {
        dprintf(STDERR_FILENO, "%s exec pid=%d\n", argv[1], getpid());
        execv(argv[1], argv + 1);
        dprintf(STDERR_FILENO, "%s exec failed: %s\n", argv[1], strerror(errno));
        _exit(126);
    }
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(child, &status, 0)) < 0) {
        dprintf(STDERR_FILENO, "%s pid=%d wait failed: %s\n", argv[1], child, strerror(errno));
        return 74;
    }
    if (WIFSIGNALED(status)) {
        int signal = WTERMSIG(status);
        dprintf(STDERR_FILENO, "%s pid=%d signal=%d\n", argv[1], child, signal);
        return 128 + signal;
    }
    int exit_status = WEXITSTATUS(status);
    dprintf(STDERR_FILENO, "%s pid=%d exit=%d\n", argv[1], child, exit_status);
    return exit_status;
}
