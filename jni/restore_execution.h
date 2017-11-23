#pragma once

#include "rtmutex.h"
#include "kernel_structs.h"

struct rt_mutex_waiter_meta {
    struct rt_mutex_waiter *k_rt_waiter;
    struct task_struct *k_task;
    struct rt_mutex_waiter u_rt_waiter;
};

void restore_execution(struct task_struct *task, pid_t bugged_tid,
        pid_t main_tid, uint32_t rt_waiter_offset);
