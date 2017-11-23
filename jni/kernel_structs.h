#pragma once

#include <time.h>
#include <sys/types.h>

#include "list.h"

typedef long long unsigned int u64;
typedef long long signed int s64;
typedef long unsigned int cputime_t;
typedef long unsigned int mm_segment_t;

/* Forward declarations */
struct mm_struct;
struct pid;
struct rt_q;
struct rt_mutex_waiter;

typedef struct atomic_s {
    int counter;
} atomic_t;

typedef struct cpumask {
    long unsigned int bits[1];
} cpumask_t;

struct sched_info {
    long unsigned int pcount;
    long long unsigned int run_delay;
    long long unsigned int last_arrival;
    long long unsigned int last_queued;
};

struct hlist_node {
    struct hlist_node *next, **pprev;
};

struct pid_link {
    struct hlist_node node;
    struct pid *pid;
};

struct task_cputime {
    cputime_t utime;
    cputime_t stime;
    long long unsigned int sum_exec_runtime;
};

struct sched_statistics {
    u64 wait_start;
    u64 wait_max;
    u64 wait_count;
    u64 wait_sum;
    u64 iowait_count;
    u64 iowait_sum;
    u64 sleep_start;
    u64 sleep_max;
    s64 sum_sleep_runtime;
    u64 block_start;
    u64 block_max;
    u64 exec_max;
    u64 slice_max;
    u64 nr_migrations_cold;
    u64 nr_failed_migrations_affine;
    u64 nr_failed_migrations_running;
    u64 nr_failed_migrations_hot;
    u64 nr_forced_migrations;
    u64 nr_wakeups;
    u64 nr_wakeups_sync;
    u64 nr_wakeups_migrate;
    u64 nr_wakeups_local;
    u64 nr_wakeups_remote;
    u64 nr_wakeups_affine;
    u64 nr_wakeups_affine_attempts;
    u64 nr_wakeups_passive;
    u64 nr_wakeups_idle;
};

struct load_weight {
    long unsigned int weight;
    long unsigned int inv_weight;
};

struct rb_node {
    long unsigned int rb_parent_color;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
};

struct sched_entity {
    struct load_weight load;
    struct rb_node run_node;
    struct list_head group_node;
    unsigned int on_rq;
    u64 exec_start;
    u64 sum_exec_runtime;
    u64 vruntime;
    u64 prev_sum_exec_runtime;
    u64 nr_migrations;
    struct sched_statistics statistics;
    struct sched_entity *parent;
    struct cfs_rq *cfs_rq;
    struct cfs_rq *my_q;
};

struct kernel_cap_struct {
    uint32_t cap[2];
};

typedef struct kernel_cap_struct kernel_cap_t;

struct cred {
    atomic_t usage;
    uid_t uid;
    gid_t gid;
    uid_t suid;
    gid_t sgid;
    uid_t euid;
    gid_t egid;
    uid_t fsuid;
    gid_t fsgid;
    unsigned int securebits;
    kernel_cap_t cap_inheritable;
    kernel_cap_t cap_permitted;
    kernel_cap_t cap_effective;
    kernel_cap_t cap_bset;
    void *security;
    struct user_struct *user;
    struct user_namespace *user_ns;
    struct group_info *group_info;
    /* redacted */
};

struct sched_rt_entity {
    struct list_head run_list;
    long unsigned int timeout;
    unsigned int time_slice;
    int nr_cpus_allowed;
    struct sched_rt_entity *back;
    struct sched_rt_entity *parent;
    struct rt_rq *rt_rq;
    struct rt_rq *my_q;
};

struct thread_info {
    long unsigned int flags;
    int preempt_count;
    mm_segment_t addr_limit;
    struct task_struct *task;
    /* ... redacted ... */
};


struct task_struct {
    volatile long int state;
    struct thread_info *stack;
    atomic_t usage;
    unsigned int flags;
    char field_10[268];
    pid_t pid;
    pid_t tgid;
    struct task_struct *real_parent;
    struct task_struct *parent;
    struct list_head children;
    struct list_head sibling;
    struct task_struct *group_leader;
    char field_140[52];
    struct list_head thread_group;
    char field_17c[116];
    const struct cred *real_cred;
    const struct cred *cred;
    uint32_t replacement_session_keyring;
    char field_1fc[180];
    uint32_t nsproxy;
    char field_2b8[100];
    struct plist_head pi_waiters;
    struct rt_mutex_waiter *pi_blocked_on;
    /* ... redacted ... */
};
