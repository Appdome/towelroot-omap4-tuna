#include <android/log.h>

#include "list.h"
#include "rtmutex.h"
#include "kernel_rw.h"
#include "kernel_structs.h"

#include "restore_execution.h"

static void show_rt_mutex_waiter(struct rt_mutex_waiter *rt_waiter)
{
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "list_entry.prio = %d", rt_waiter->list_entry.prio);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "list_entry.prio_list.next = %p",
            rt_waiter->list_entry.prio_list.next);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "list_entry.prio_list.prev = %p",
            rt_waiter->list_entry.prio_list.prev);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "list_entry.node_list.next = %p",
            rt_waiter->list_entry.node_list.next);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "list_entry.node_list.prev = %p",
            rt_waiter->list_entry.node_list.prev);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "pi_list_entry.prio = %d", rt_waiter->pi_list_entry.prio);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "pi_list_entry.prio_list.next = %p",
            rt_waiter->pi_list_entry.prio_list.next);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "pi_list_entry.prio_list.prev = %p",
            rt_waiter->pi_list_entry.prio_list.prev);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "pi_list_entry.node_list.next = %p",
            rt_waiter->pi_list_entry.node_list.next);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "pi_list_entry.node_list.prev = %p",
            rt_waiter->pi_list_entry.node_list.prev);
    __android_log_print(ANDROID_LOG_DEBUG, "Root", "task = %p",
            rt_waiter->task);
    __android_log_print(ANDROID_LOG_DEBUG, "Root", "lock = %p",
            rt_waiter->lock);
}

void restore_execution(struct task_struct *task, pid_t bugged_tid,
        pid_t main_tid, uint32_t rt_waiter_offset)
{
    struct task_struct *iter = NULL, *leader = NULL, *main = NULL;
    struct task_struct current;
    struct rt_mutex_waiter_meta waiters[100];
    struct rt_mutex_waiter *rt_waiter = NULL, *next_k_rt_waiter = NULL,
                           *prev_k_rt_waiter = NULL, *k_rt_waiter = NULL,
                           *first_k_rt_waiter = NULL, *last_k_rt_waiter = NULL,
                           *first_rt_waiter = NULL, *last_rt_waiter = NULL;
    struct rt_mutex lock, *k_lock = NULL;
    int i = 0, num_waiters = 0;

    iter = leader = (struct task_struct *)task->group_leader;
    do {
        read_kern(iter, &current, sizeof(current));
        __android_log_print(ANDROID_LOG_DEBUG, "Root", "Task @%p", iter);
        __android_log_print(ANDROID_LOG_DEBUG, "Root",
                "task pid = %d, tgid = %d", current.pid, current.tgid);
        __android_log_print(ANDROID_LOG_DEBUG, "Root",
                "task.pi_waiters.node_list.next = %p",
                current.pi_waiters.node_list.next);
        __android_log_print(ANDROID_LOG_DEBUG, "Root",
                "task.pi_waiters.node_list.prev = %p",
                current.pi_waiters.node_list.prev);
        __android_log_print(ANDROID_LOG_DEBUG, "Root",
                "task.pi_blocked_on = %p", current.pi_blocked_on);
        if (current.pid == main_tid) {
            /* We are interested in the pointer to the task_struct of the
             * thread which owns the lock - I call it the "main" thread even
             * though it's not the main thread per-se in the context of the
             * application */
            main = iter;
        }
        if (current.pid != main_tid && current.pid != bugged_tid) {
            /* Don't add the main thread, it does not have an rt_waiter */
            waiters[i].k_task = iter;
            waiters[i].k_rt_waiter = (struct rt_mutex_waiter *)(current.stack + rt_waiter_offset);
            read_kern(waiters[i].k_rt_waiter, &waiters[i].u_rt_waiter,
                    sizeof(struct rt_mutex_waiter));
            __android_log_print(ANDROID_LOG_DEBUG, "Root", "rt_waiter @%p",
                    waiters[i].k_rt_waiter);
            show_rt_mutex_waiter(&waiters[i].u_rt_waiter);
            if (waiters[i].k_rt_waiter == current.pi_blocked_on) {
                __android_log_print(ANDROID_LOG_DEBUG, "Root",
                        "Task should not be blocked on its own waiter");
                current.pi_blocked_on = NULL;
                write_kern(&iter->pi_blocked_on, &current.pi_blocked_on,
                        sizeof(current.pi_blocked_on));
            }
            if (waiters[i].u_rt_waiter.task == iter) {
                /* Only acknowledge rt_mutex_waiters objects which appear
                 * "real" by comparing the alleged "task" field to the
                 * task on which the object was found */
                __android_log_print(ANDROID_LOG_DEBUG, "Root", "Good waiter");
                k_lock = waiters[i].u_rt_waiter.lock;
                ++i;
            }
        }
        iter = list_entry(current.thread_group.next, struct task_struct,
                thread_group);
    } while (iter != leader);

    num_waiters = i;
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "Need to fix a total of %d waiters", num_waiters);

    for (i = 0; i < num_waiters; ++i) {
        rt_waiter = &waiters[i].u_rt_waiter;
        k_rt_waiter = waiters[i].k_rt_waiter;
        __android_log_print(ANDROID_LOG_DEBUG, "Root",
                "waiters[%d].k_rt_waiter = %p", i, k_rt_waiter);
        next_k_rt_waiter = waiters[(i + 1) % num_waiters].k_rt_waiter;
        prev_k_rt_waiter = waiters[(i + num_waiters - 1) % num_waiters].k_rt_waiter;

        rt_waiter->task = waiters[i].k_task;
        rt_waiter->lock = k_lock;

        rt_waiter->list_entry.prio = 120 + i;
        rt_waiter->list_entry.prio_list.next = &next_k_rt_waiter->list_entry.prio_list;
        rt_waiter->list_entry.prio_list.prev = &prev_k_rt_waiter->list_entry.prio_list;
        rt_waiter->list_entry.node_list.next = &next_k_rt_waiter->list_entry.node_list;
        rt_waiter->list_entry.node_list.prev = &prev_k_rt_waiter->list_entry.node_list;

        /* For the time being, all the "pi_list_entry"s will be emptied and
         * set to the same priority as the "list_entry" nodes */
        rt_waiter->pi_list_entry.prio = 120 + i;
        rt_waiter->pi_list_entry.prio_list.next = &k_rt_waiter->pi_list_entry.prio_list;
        rt_waiter->pi_list_entry.prio_list.prev = &k_rt_waiter->pi_list_entry.prio_list;
        rt_waiter->pi_list_entry.node_list.next = &k_rt_waiter->pi_list_entry.node_list;
        rt_waiter->pi_list_entry.node_list.prev = &k_rt_waiter->pi_list_entry.node_list;
    }
    
    /* The main task owns the lock, and so its pi_waiters list should contain
     * the highest priority waiters which block on its locks. Seeing as it has
     * only one lock, there should be only one waiter in its list */
    first_k_rt_waiter = waiters[0].k_rt_waiter;
    first_rt_waiter = &waiters[0].u_rt_waiter;

    read_kern(main, &current, sizeof(current));
    __android_log_print(ANDROID_LOG_DEBUG, "Root", "Before main fix");
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "main.pi_waiters.node_list.next = %p",
            current.pi_waiters.node_list.next);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "main.pi_waiters.node_list.prev = %p",
            current.pi_waiters.node_list.prev);
    current.pi_waiters.node_list.next = &first_k_rt_waiter->pi_list_entry.node_list;
    current.pi_waiters.node_list.prev = &first_k_rt_waiter->pi_list_entry.node_list;
    __android_log_print(ANDROID_LOG_DEBUG, "Root", "After main fix");
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "main.pi_waiters.node_list.next = %p",
            current.pi_waiters.node_list.next);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "main.pi_waiters.node_list.prev = %p",
            current.pi_waiters.node_list.prev);
    first_rt_waiter->pi_list_entry.node_list.next = &main->pi_waiters.node_list;
    first_rt_waiter->pi_list_entry.node_list.prev = &main->pi_waiters.node_list;
    write_kern(&main->pi_waiters, &current.pi_waiters,
            sizeof(current.pi_waiters));
    
    /* This leaves the list_entry.prio_list and list_entry.node_list linked in
     * a circle (each), however, list_entry.node_list should be chained with
     * the lock's wait_list member (a plist_head) */
    /* Link the lock on which the waiters are blocking to the "new" list */
    read_kern(k_lock, &lock, sizeof(lock));

    first_k_rt_waiter = waiters[0].k_rt_waiter;
    first_rt_waiter = &waiters[0].u_rt_waiter;
    /* lock's "next" points to the first waiter's node_list */
    lock.wait_list.node_list.next = &first_k_rt_waiter->list_entry.node_list;
    /* First waiter's node_list.prev points to the lock's head */
    first_rt_waiter->list_entry.node_list.prev = &k_lock->wait_list.node_list;

    last_k_rt_waiter = waiters[num_waiters - 1].k_rt_waiter;
    last_rt_waiter = &waiters[num_waiters - 1].u_rt_waiter;
    /* lock's "prev" points to the last waiter's node_list */
    lock.wait_list.node_list.prev = &last_k_rt_waiter->list_entry.node_list;
    /* Last waiter's node_list.next points to the lock's head */
    last_rt_waiter->list_entry.node_list.next = &k_lock->wait_list.node_list;

    __android_log_print(ANDROID_LOG_DEBUG, "Root", "After fix");
    /* Write back everything to the kernel */
    __android_log_print(ANDROID_LOG_DEBUG, "Root", "lock @%p", k_lock);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "lock.wait_list.node_list.next = %p",
            lock.wait_list.node_list.next);
    __android_log_print(ANDROID_LOG_DEBUG, "Root",
            "lock.wait_list.node_list.prev = %p",
            lock.wait_list.node_list.prev);
    __android_log_print(ANDROID_LOG_DEBUG, "Root", "lock.owner = %p",
            lock.owner);
    write_kern(k_lock, &lock, sizeof(lock));
    for (i = 0; i < num_waiters; ++i) {
        rt_waiter = &waiters[i].u_rt_waiter;
        __android_log_print(ANDROID_LOG_DEBUG, "Root", "rt_waiter @%p",
                waiters[i].k_rt_waiter);
        show_rt_mutex_waiter(rt_waiter);
        write_kern(waiters[i].k_rt_waiter, rt_waiter, sizeof(*rt_waiter));
    }
}
