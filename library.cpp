#include <iostream>
#include "hasp.hpp"
#include "hook.h"
#include <fcntl.h>
#include <cstring>
#include <cstdarg>
#include <termios.h>
#include <csignal>
#include <thread>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "jvs.h"
#include "touch.h"
#include "sysmonitor.hpp"
#include "nsadrv.hpp"
#include "bana.h"
#include "config.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include "memory.h"
#include "ssl.hpp"
#include "input.h"
#include "str400.hpp"
#include "limiter.hpp"
#include <GL/gl.h>
#include <GL/glx.h>

int jvsFd = 144444443;
int touchFd = 144444444;
int strFd = 144444445;

int ffbState = 0;

void jvsThread() {
    init_input();
    initJvs();

    while (!inputStopped) {
        jvsMutex.lock();
        update_input();
        jvsMutex.unlock();
        usleep(1 * 1000);
    }
}

defineHook(int, system, const char* command) {
    printf("system(\"%s\")\n", command);
    return -1;
}

#define YA_CARD_EMU_FIFO "/tmp/yacardemu-mt4"

defineHook(int, open, const char *pathname, int flags, ...) {
    //printf("open(\"%s\", %d)\n", pathname, flags);

    int mode = 0;
    bool twoArgs = true;

    if (flags & O_CREAT) {
        va_list arg;
        va_start (arg, flags);
        mode = va_arg (arg, int);
        va_end (arg);

        twoArgs = false;
    }

    if (strstr(pathname, "/dev/tty") == pathname) {
        printf("open(\"%s\", %d)\n", pathname, flags);
    }

    if (isMt4 && strcmp(pathname, "/dev/ttyS1") == 0 && redirectMagneticCard) {
        printf("open magnetic card %s\n", redirectMagneticCard);
        int ret = callOld(open, redirectMagneticCard, flags);
        if (ret == -1) {
            printf("cant open magnetic card %d\n", errno);
        }
        return ret;
    }

    if (strcmp(pathname, "/dev/ttyS3") == 0 && redirectBanaReader){
        printf("open banareader %s\n", redirectBanaReader);
        int ret = callOld(open, redirectBanaReader, flags);
        if (ret == -1) {
            printf("cant open banareader %d\n", errno);
        }
        return ret;
    }

    if ((strcmp(pathname, "/dev/ttyS2") == 0 || strcmp(pathname, "/dev/tnt0") == 0) && useJvs)
        return jvsFd;

    if (strcmp(pathname, "/dev/ttyS0") == 0) {
        if (isTerminal) {
            if (useTouch) {
                initTouch();
                return touchFd;
            }
        } else if (useStr3) {
            return strFd;
        }
    }

    if(twoArgs)
        return callOld(open, pathname, flags);
    return callOld(open, pathname, flags, mode);
}

defineHook(int, close, int fd) {
    if (fd == jvsFd)
        return 0;
    if (fd == touchFd)
        return 0;
    if (fd == strFd)
        return 0;
    return callOld(close, fd);
}

defineHook(int, ioctl, int fd, unsigned long request) {
    if (fd == jvsFd) {
        switch (request) {
            case 0x545D: {
                return -1;
            }
            case 0x5415: {
                return -1;
            }
            case 0x540b: {
                return -1;
            }
        }
        return 0;
    }
    if (fd == touchFd)
        return 0;
    if (fd == strFd)
        return 0;

    return callOld(ioctl, fd, request);
}

defineHook(int, fcntl, int fd, int cmd, void* arg) {
    if (fd == touchFd)
        return 0;
    if (fd == strFd)
        return 0;
    return callOld(fcntl, fd, cmd, arg);
}

defineHook(int, fcntl64, int fd, int cmd, int arg) {
    if (fd == touchFd)
        return 0;
    if (fd == strFd)
        return 0;
    return callOld(fcntl64, fd, cmd, arg);
}

defineHook(int, tcgetattr, int fildes, struct termios *termios_p) {
    if (fildes == jvsFd)
        return 0;
    if (fildes == touchFd)
        return 0;
    if (fildes == strFd)
        return 0;
    return callOld(tcgetattr, fildes, termios_p);
}

defineHook(int, tcsetattr, int fildes, int optional_actions,
           const struct termios *termios_p) {
    if (fildes == jvsFd)
        return 0;
    if (fildes == touchFd)
        return 0;
    if (fildes == strFd)
        return 0;
    return callOld(tcsetattr, fildes, optional_actions, termios_p);
}

defineHook(int, epoll_ctl, int epfd, int op, int fd, struct epoll_event *event) {
    if (fd == touchFd || fd == strFd)
        return 0;
    return callOld(epoll_ctl, epfd, op, fd, event);
}

defineHook(int, read, int fd, void* buf, size_t count) {
    if (fd == jvsFd) {
        jvsMutex.lock();
        updateJvs();
        jvsMutex.unlock();

        int size;
        if (!readJvs(buf, &size)) {
            printf("no packet jvs\n");
            errno = EINVAL;
            return -1;
        }
        return size;
    }

    if (fd == strFd) {
        switch (ffbState) {
            case 1: {
                memcpy(buf, "C01", 3);
                ffbState = 3;
                return 3;
            }
            case 2: {
                memcpy(buf, "C06", 3);
                ffbState = 0;
                return 3;
            }
            case 3: {
                ((char*)buf)[0] = 'H';

                int v = htons(0);
                memcpy((uint8_t*)buf + 1, &v, 2);

                return 3;
            }
        }

        return 0;
    }

    if (fd == touchFd) {
        int size;
        if (!readTouch(buf, &size))
            return 0;
        return size;
    }

    return callOld(read, fd, buf, count);
}

defineHook(int, write, int fd, const void* buf, size_t count) {
    if (fd == jvsFd) {
        writeJvs((void*)buf, count);

        jvsMutex.lock();
        updateJvs();
        jvsMutex.unlock();
        return count;
    }

    if (fd == strFd) {
        // TODO: handle steering packet for ffb
        return count;
    }

    if (fd == touchFd) {
        writeTouch((void*)buf, count);
        return count;
    }

    return callOld(write, fd, buf, count);
}

defineHook(ssize_t, writev, int fd, const struct iovec *iov, int iovcnt) {
    if (fd == touchFd) {
        writeTouch((void*)iov->iov_base, iov->iov_len);
        return iov->iov_len;
    }

    if (fd == strFd)
        return iov->iov_len;

    return callOld(writev, fd, iov, iovcnt);
}

defineHook(ssize_t, readv, int fd, const struct iovec *iov, int iovcnt) {
    if (fd == touchFd) {
        int size;
        if (!readTouch(iov->iov_base, &size))
            return 0;
        return size;
    }

    if (fd == strFd) { // im a bit lazy
        // super hacky
        if (ffbState == 0)
            ffbState = 3;

        return jmp_read(strFd, (void*)iov->iov_base, iov->iov_len);
    }

    return callOld(writev, fd, iov, iovcnt);
}

defineHook(FILE*, popen, const char* command, const char* type) {
    printf("popen %s\n", command);
    if (strstr(command, "LANG=C;/usr/bin/sudo ") == command) {
        return callOld(popen, command + 21, type);
    }
    return callOld(popen, command, type);
}

char logBuffer[0x10000];

defineHook(void, log, void* a1, int type, const char* msg, ...) {
    va_list va;
    va_start(va, msg);
    int len = vsnprintf(logBuffer, sizeof(logBuffer), msg, va);
    va_end(va);
    if (len < 1)
        return;
    switch (type) {
        case 1: {
            printf("[INF] ");
            break;
        } case 2: {
            printf("[WRN] ");
            break;
        } case 3: {
            printf("[SYS] ");
            break;
        } case 4: {
            printf("[ERR] ");
            break;
        }
    }
    printf("%s", logBuffer);
}

defineHook(void, logmt4, int type, const char* msg, ...) {
    va_list va;
    va_start(va, msg);
    int len = vsnprintf(logBuffer, sizeof(logBuffer), msg, va);
    va_end(va);
    if (len < 1)
        return;
    switch (type) {
        case 1: {
            printf("[INF] ");
            break;
        } case 2: {
            printf("[WRN] ");
            break;
        } case 3: {
            printf("[SYS] ");
            break;
        } case 4: {
            printf("[ERR] ");
            break;
        }
    }
    printf("%s", logBuffer);
}

defineHook(int, getContentRouter)
{
    return inet_addr("192.168.92.254");
}

defineHook(int, isTerminal)
{
    return 1;
}

defineHook(int, isTerminalMt4, int* a1)
{
    // @Function bool Mode::StartUp::state_TerminalIoCoinCheckWait(Mode::StartUp *const this)
    a1[1] = 0x84819F0;
    return 0;
}

defineHook(int, touchPanelFix) {
    return 1;
}

defineHook(int, bind, int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    struct sockaddr_in* in = ((struct sockaddr_in*) addr);
    int port = ntohs(in->sin_port);

    if (port == 50765) {
        int one = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

        // bind on 0.0.0.0 for terminal
        in->sin_addr.s_addr = 0;
    }

    return callOld(bind, sockfd, addr, addrlen);
}

defineHook(int, connect, int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    struct sockaddr_in* in = ((struct sockaddr_in*) addr);
    int port = ntohs(in->sin_port);

    char ip[20];
    inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip));

    printf("connect %s %d\n", ip, port);

    if (port == 50765 && termSpoof) {
        in->sin_addr.s_addr = inet_addr("127.0.0.1");
    }

    return callOld(connect, sockfd, addr, addrlen);
}

defineHook(int, decryptToken, char* dest, int* destSize, char* src, void* a4) {
    // custom solution for priv server (too lazy to reverse enc)
    if (src[0] == '@') {
        memcpy(dest, src + 1, strlen(src + 1));
        return 0;
    }
    return callOld(decryptToken, dest, destSize, src, a4);
}

int lastType = 0;
defineHook(void, updateTest, uint8_t* a1, int a2) {
    int type = *(int*)(a1 + 568);
    if (type != lastType)
        printf("test=%d\n", type);

    switch (type) {
        case 9: {
            *(int *)(a1 + 568) = 22;
            *(int *)(a1 + 1096) = 1;
            break;
        }
        case 20: {
            *(int *)(a1 + 568) = 22;
            break;
        }
    }

    lastType = type;
    callOld(updateTest, a1, a2);
}

defineHook(int, AA75120) {
    return 0;
}

defineHook(int, A393BE0) {
    return 0;
}

defineHook(void, 84EC560, int * a1) {
    *a1 = 0;
}

defineHook(void, billingSave) {

}

uint8_t ourPcb = 255;
defineHook(ssize_t, recvmsg, int fd, struct msghdr *msg, int flags) {
    int ret = callOld(recvmsg, fd, msg, flags);
    if (ret < 0)
        return ret;

    if (msg->msg_name) {
        struct sockaddr_in* in = ((struct sockaddr_in*) msg->msg_name);
        int port = ntohs(in->sin_port);

        uint8_t* data = (uint8_t*)msg->msg_iov->iov_base;
        if (port == 50765 && msg->msg_iov->iov_len >= 2) {
            if (data[0] == isMt4 ? 1 : 2) {
                //printf("%d %d\n", data[1], ourPcb);
                if (data[1] != ourPcb) {
                    switch (data[1]) {
                        case 0: {
                            in->sin_addr.s_addr = inet_addr("192.168.92.11");
                            break;
                        }
                        case 1: {
                            in->sin_addr.s_addr = inet_addr("192.168.92.12");
                            break;
                        }
                        case 2: {
                            in->sin_addr.s_addr = inet_addr("192.168.92.13");
                            break;
                        }
                        case 3: {
                            in->sin_addr.s_addr = inet_addr("192.168.92.14");
                            break;
                        }
                        case 4: {
                            if (termSpoof)
                                in->sin_addr.s_addr = inet_addr("192.168.92.20");
                            break;
                        }
                    }
                }
            }
        }
    }

    return ret;
}

defineHook(int, alin0) {
    return 0;
}

defineHook(int, sendAlthmand) {
    return 0;
}

defineHook(int, XCreatePixmapCursor) {
    return 0;
}

defineHook(Status, XGetWindowAttributes, Display *display, Window w, XWindowAttributes *window_attributes_return) {
    Status ret = callOld(XGetWindowAttributes, display, w, window_attributes_return);
    window_attributes_return->width = 1360;
    window_attributes_return->height = 768;
    return ret;
}

defineHook(Display*, XOpenDisplay, char* name) {
    Display* ret = callOld(XOpenDisplay, NULL);
    if (!ret)
        ret = callOld(XOpenDisplay, name);
    if (!ret)
        printf("cant open any display\n");
    return ret;
}

defineHook(ssize_t, sendmsg, int fd, struct msghdr *msg, int flags) {
    if (msg->msg_name) {
        struct sockaddr_in* in = ((struct sockaddr_in*) msg->msg_name);
        int port = ntohs(in->sin_port);
        if (port == 50765 && msg->msg_iov->iov_len >= 2) {
            uint8_t* data = (uint8_t*)msg->msg_iov->iov_base;
            if (data[0] == isMt4 ? 1 : 2) {
                ourPcb = data[1];
            }
        }
    }
    return callOld(sendmsg, fd, msg, flags);
}

defineHook(int, refreshNetwork, int a1) {
    int ret = callOld(refreshNetwork, a1);
    printf("refreshNetwork=%d\n", ret);

    if (ret != 1)
        return ret;
    *(int*)(a1 + 20) = htons(inet_addr("192.168.92.254"));
    return 1;
}

defineHook(int, FFBIo_State_PowerOn, int a1) {
    printf("[FFBIo] PowerOn\n");

    ffbState = 1;
    return callOld(FFBIo_State_PowerOn, a1);
}

defineHook(int, FFBIo_State_PowerOff, int a1) {
    printf("[FFBIo] PowerOff\n");

    ffbState = 2;
    return callOld(FFBIo_State_PowerOff, a1);
}

defineHook(void, glTexParameteri, GLenum target, GLenum pname, GLenum param) {
    if (param == GL_CLAMP) {
        // fix texture rendering issues
        param = GL_CLAMP_TO_EDGE;
    }

    callOld(glTexParameteri, target, pname, param);

}

__attribute__((constructor))
void initialize_wlldr() {
    if (!loadConfig())
        return;

    printf("terminal=%d, mt4=%d\n", isTerminal, isMt4);

    if (redirectBanaReader && useBana) {
        printf("Unable to enable Banapass Emulation and Banapass Reader Redirect, please check config and try again.");
        exit(1);
    }

    if (redirectMagneticCard)
        printf("Redirecting mgcard to %s\n", redirectMagneticCard);

    if (redirectBanaReader)
        printf("Redirecting banareader to %s\n", redirectBanaReader);

    initHasp();
    printf("hasp\n");
    if (!useSurround51) {
        initNsadrv();
        printf("nsadrv\n");
    }
    initSysMonitor();
    printf("monitor\n");
    if (useBana) {
        initBana();
        printf("bana\n");
    }
    if (useStr400) {
        init_str400();
        printf("str400\n");
    }

    // disable ssl verification for mucha
    disableSSLCert();
    printf("ssl\n");

    if (useLimiter) {
        init_limiter();
        printf("limiter\n");
    }

    // Sym_aV388FrontMucha
    patchMemoryString0((void*)0x8FA2FA8, "mucha.local");

    // @Function void Sys::LogMes(nuUINT32 iType, const char *format, ...)
    enableHook(logmt4, 0x80A18C0);

    if (isTerminal)
        // @Function bool Mode::StartUp::state_TerminalIoCheckWait(Mode::StartUp *const this)
        enableHook(isTerminalMt4, 0x84828E0);

    // content router fix
    // @Function bool Sys::Net::Interface::update(Sys::Net::Interface *const this)
    enableHook(refreshNetwork, 0x821D4F0);

    // im not very sure about this function... maybe prevent it call the error handling branch?
    // @Function void Sys::Network::Impl::updateInterface(Sys::Network::Impl *const this)
    // @ASM mov dword ptr [edi+6Ch], offset _ZZN3Sys3Net5Error18GetNetworkCategoryEvE8instance
    // @AS Function Sys::Net::Error::GetNetworkCategory(void)::instance
    patchMemory((void*)0x80A4BD1, { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });

    // @Function int send_command@<eax>(int a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4, char *src)
    enableHook(sendAlthmand, 0x8D2A2E0);

    // fix resolution
    enableHook(XGetWindowAttributes, XGetWindowAttributes);

    // @Function Display *XOpenDisplay(const char *a1)
    enableHook(XOpenDisplay, 0x8058390);

    // @Function bool Sys::Device::FFBIo::State_PowerOn(Sys::Device::FFBIo *const this)
    enableHook(FFBIo_State_PowerOn, 0x83B8BD0);
    // @Function bool Sys::Device::FFBIo::State_PowerOff(Sys::Device::FFBIo *const this)
    enableHook(FFBIo_State_PowerOff, 0x83B8AB0);

    // @Function int open(const char *file, int oflag, ...)
    enableHook(open, 0x8058370);
    // @Function void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
    enableHook(glTexParameteri, 0x8058EA0);
    

    if (isTerminal)
        ourPcb = 4;
    enableHook(recvmsg, recvmsg);
    enableHook(sendmsg, sendmsg);

    enableHook(system, system);
    enableHook(close, close);
    enableHook(tcgetattr, tcgetattr);
    enableHook(tcsetattr, tcsetattr);
    enableHook(epoll_ctl, epoll_ctl);
    enableHook(read, read);
    enableHook(write, write);
    enableHook(writev, writev);
    enableHook(readv, readv);
    enableHook(ioctl, ioctl);
    enableHook(fcntl, fcntl);
    enableHook(fcntl64, fcntl64);
    enableHook(popen, popen);

    enableHook(bind, bind);
    enableHook(connect, connect);

    void *alin_dll = dlopen("./alin.dll", 2);
    enableHook(alin0, dlsym(alin_dll, "alin_init"));
    enableHook(alin0, dlsym(alin_dll, "alin_keyboard"));
    enableHook(alin0, dlsym(alin_dll, "alin_mouse"));
    enableHook(alin0, dlsym(alin_dll, "alin_pad"));
    enableHook(alin0, dlsym(alin_dll, "alin_term"));

    // dont hide cursor
    enableHook(XCreatePixmapCursor, XCreatePixmapCursor);

    std::thread t(jvsThread);
    t.detach();
}
