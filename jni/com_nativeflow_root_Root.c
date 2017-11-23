#define _GNU_SOURCE
#include <jni.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "root.h"
#include "client.h"
#include "logging.h"
#include "com_nativeflow_root_Root.h"

#define COMM_PORT (6666)
#define BACKOFF_TIMEOUT_MAX (30)

static int daemon_main(void)
{
    int sock;
    struct sockaddr_in addr = {0};
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len;
    int client_sock;
    int ret;
    pid_t child;
    struct thread_info *k_thread_info;

    ALOGD("rootd", "daemon_main");

    sock = socket(AF_INET, SOCK_STREAM, SOL_TCP);
    if (sock < 0) {
        ALOGE("rootd", "socket(2) error: %s", strerror(errno));
        goto Exit;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6666);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret) {
        ALOGE("rootd", "bind(2) error: %s", strerror(errno));
        goto Cleanup;
    }

    ret = listen(sock, 1);
    if (ret < 0) {
        ALOGE("rootd", "listen(2) error: %s", strerror(errno));
        goto Cleanup;
    }

    k_thread_info = do_root();

    ALOGD("rootd", "Waiting for connection");

    while (1) {
        client_addr_len = sizeof(client_addr);
        client_sock = accept(sock, (struct sockaddr *)&client_addr,
                &client_addr_len);
        if (client_sock < 0) {
            ALOGE("rootd", "accept(2) error: %s", strerror(errno));
        }
        ALOGD("rootd", "Client connected");
        handle_client(client_sock, k_thread_info);
        close(client_sock);
    }

Cleanup:
    close(sock);
Exit:
    return ret;
}

static int handle_daemon(void)
{
    pid_t pid = daemon(0, 0);
    if (0 == pid) {
        ALOGD("rootd", "In daemon");
        daemon_main();
        return 0;
    } else {
        ALOGD("Root", "Spawn failed");
        return -1;
    }
}

JNIEXPORT jlong JNICALL Java_com_nativeflow_root_Root_root(JNIEnv *env, jclass cls)
{
    pid_t pid;
    int sock;
    struct sockaddr_in addr = {0};
    int backoff = 1;
    int status = -1;
    int ret;

    ALOGD("Root", "Enter Java_com_nativeflow_root_Root_root");

    sock = socket(AF_INET, SOCK_STREAM, SOL_TCP);
    if (sock < 0) {
        ALOGE("Root", "socket(2) error: %s", strerror(errno));
        goto Exit;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6666);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

Retry:
    ALOGD("Root", "Trying to connect to daemon");
    /* Try to connect to the daemon, if the connection succeeds, that means
     * the deamon exists and is running and there is no need to spawn it */
    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        /* Daemon probably does not exist, we can go on to spawn it */
        switch (errno) {
            case EINTR:
            case EAGAIN:
            case EALREADY:
            case ETIMEDOUT:
            case EADDRINUSE:
            case EINPROGRESS:
                ALOGD("Root", "connect(2) error: %s", strerror(errno));
                backoff *= 2;
                if (backoff <= BACKOFF_TIMEOUT_MAX) {
                    ALOGD("Root", "Retrying in %d seconds", backoff);
                    sleep(backoff);
                    goto Retry;
                } else {
                    ALOGE("Root", "Maximum retries exceeded, bailing");
                    status = -1;
                    goto Cleanup;
                }
            case EPERM:
            case EBADF:
            case EACCES:
            case EFAULT:
            case EISCONN:
            case ENOTSOCK:
            case ENETUNREACH:
                ALOGE("Root", "connect(2) error: %s", strerror(errno));
                status = -1;
                goto Cleanup;
            case ECONNREFUSED:
            default:
                break;
        }
        pid = fork();
        switch (pid) {
            case 0:
                /* In child */
                ALOGD("Root", "Spawning a daemon");
                return handle_daemon();
                break;
            case -1:
                /* Error */
                ALOGE("Root", "fork error: %s", strerror(errno));
                goto Exit;
            default:
                /* Parent */
                wait(NULL);
                ALOGD("Root", "Child reaped");
                break;
        }
    } else {
        ALOGD("Root", "Connection successful");
    }

    /* The flow which passes here is the OK flow, all the other flows indicate
     * a failure */
    status = 0;

Cleanup:
    close(sock);
Exit:
    ALOGD("Root", "Leave Java_com_nativeflow_root_Root_root");
    return (jlong)status;
}
