#pragma once

#include <sys/types.h>
#include "kernel_structs.h"

#define BARKER (0xdeadbeef)

enum command_code {
    #define CFG(x) CMD_##x,
    #include "client_commands.def"
    CMD_MAX_CMD
    #undef CFG
};

struct client_command_header {
    /*uint32_t barker;*/
    enum command_code cmd;
    /*uint32_t len;*/
};

void client_message_loop(int sock);
void handle_client(int sock, struct thread_info *k_thread_info);
