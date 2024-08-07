#ifndef AIRD_H
#define AIRD_H

#include "../common.h"
#include <stdio.h>

#define SV_SOCK_PATH "/tmp/ud_ucase"
#define BUF_SIZE 1024
#define BACKLOG 5
#define MAXIMUM_MESSAGE_COUNT 10

static const char *LOG_FILE = "/var/log/aird.log";
static const char *BACKUP_FILE = "/var/log/aird_backup.log";

int ioctl_set_msg(int file_desc);
int ioctl_func();
char* dump_data(char* data);
void dump_to_module();
void backup_shutdown();
void shutdown_handler();
void backup_start();
static void skeleton_daemon();
int create_socket();
void execute_command(daemon_command command, char* response, ssize_t *command_len);
FILE *set_log_levels();



#endif
