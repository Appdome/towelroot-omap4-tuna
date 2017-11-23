#include <sys/syscall.h>

#define IPC_PRIVATE ((__kernel_key_t)0)

#define IPC_CREAT (00001000)

/* semctl command definitions */
#define SETALL (17)

/* Max possible semaphor value */
#define SEMVMX (0xFFFF)

#define SEMMSL_FAST (256)
/* Max semaphors allowed */
#define SEMMSL (250)

inline int semget(int key, int nsems, int semflg)
{
    return syscall(__NR_semget, key, nsems, semflg);
}

inline int semctl(int semid, int semnum, int cmd, void *semun)
{
    return syscall(__NR_semctl, semid, semnum, cmd, semun);
}
