#pragma once
#include <sys/socket.h>
#include <sys/syscall.h>

struct mmsghdr {
    struct msghdr msg_hdr;
    unsigned int msg_len;
};

inline int sendmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
        unsigned int flags)
{
    return syscall(__NR_sendmmsg, sockfd, msgvec, vlen, flags);
}

inline int recvmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
        unsigned int flags, struct timespec *timeout)
{
    return syscall(__NR_recvmmsg, sockfd, msgvec, vlen, flags, timeout);
}

