#pragma once
#include <stdio.h>

#include "list.h"
#include "kernel_structs.h"

/* Taken from kernel/rtmutex_common.h and stripped down to essentials */

struct rt_mutex {
    struct plist_head wait_list;
    struct task_struct *owner;
};

struct rt_mutex_waiter {
    struct plist_node list_entry;
    struct plist_node pi_list_entry;
    struct task_struct *task;
    struct rt_mutex *lock;
};
