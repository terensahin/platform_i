#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/un.h>
#include "c_vector.h"

static const char *LOG_FILE = "/home/baranbolo/Desktop/platform_i/daemon.log";
static const char *ERR_FILE = "home/baranbolo/Desktop/platform_i/error.log";
static const char *BACKUP_FILE = "home/baranbolo/Desktop/platform_i/backup.log";

#define SV_SOCK_PATH "/tmp/platform"
#define BUF_SIZE 100
#define BACKLOG 5
#define MAXIMUM_MESSAGE_COUNT 10

static FILE *logfp;

int *vector;

ssize_t logMessage(const char *format)
{
    return fprintf(logfp, "%s", format); /* Writes to opened log file */
}

void logOpen(const char *logFilename)
{
    mode_t m;

    m = umask(077);                  /* To recover old mask */
    logfp = fopen(logFilename, "a"); /* File is opened with permissions 700 */
    umask(m);

    if (logfp == NULL) /* If opening the log fails */
        exit(EXIT_FAILURE);

    setbuf(logfp, NULL); /* Disable stdio buffering */

}

void logClose(void)
{
    fclose(logfp);
}

void backup(){
    logOpen(BACKUP_FILE);
    char buf[16];
    for(int i = 0; i < vector_get_size(vector); i++){
        if(i == vector_get_size(vector) - 1)
            snprintf(buf, sizeof(int)+1, "%d", *((int *)vector_at(vector, i)));
        else
            snprintf(buf, sizeof(int)+1, "%d,", *((int *)vector_at(vector, i)));
        logMessage(buf);
    }
    logClose()
}

static void skeleton_daemon()
{
    pid_t pid; /* Used pid_t for portability */

    pid = fork(); /* This fork is to leave from process group */

    if (pid < 0) /* fork error */
        exit(EXIT_FAILURE);

    if (pid > 0)
    { /* Parent exits */
        exit(EXIT_SUCCESS);
    }

    /*child continues*/
    if (setsid() < 0) /* Child process becomes leader of his own session */
        exit(EXIT_FAILURE);

    struct sigaction sa; /* Ignore SIGCHLD and SIGHUP signals */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    sigemptyset(&sa.sa_mask); /* Set handler for SIGTERM signal (backup on shutdown) */
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = backup;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    pid = fork(); /* This fork is to prevent access to the terminal */

    if (pid < 0) /* fork error*/
        exit(EXIT_FAILURE);

    if (pid > 0) /* Parent exits */
        exit(EXIT_SUCCESS);

    /* Grandchild continues */

    umask(077); /* Newly opened files will have permission 700
                 so that only deamon process can rwe them */

    chdir("/"); /* Changing working directory to be able to unmount the initial directory */

    int maxfd = sysconf(_SC_OPEN_MAX); /* Get maximum possible file desciptor a process can have */
    if (maxfd == -1)                   /* If not defined, make a guess */
        maxfd = 8192;

    for (int i = maxfd - 1; i >= 0; i--) /* Closed all desciptors */
    {
        close(i);
    }

    int fd = open("/dev/null", O_RDWR); /* 0,1,2 file desciptors are pointed to null, so functions using stdin or stdout does not generate error */
    if (fd != STDIN_FILENO)             /* 'fd' should be 0 */
        exit(EXIT_FAILURE);
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
        exit(EXIT_FAILURE);
    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
        exit(EXIT_FAILURE);
}

int create_socket()
{
    struct sockaddr_un socket_address;
    int socket_fd;

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("Error creating socket!");
        exit(EXIT_FAILURE);
    }

    if (strlen(SV_SOCK_PATH) > sizeof(socket_address.sun_path) - 1)
        exit(EXIT_FAILURE);

    if (remove(SV_SOCK_PATH) == -1 && errno != ENOENT)
        exit(EXIT_FAILURE);

    memset(&socket_address, 0, sizeof(struct sockaddr_un)); /* Cleared the socket_address */
    socket_address.sun_family = AF_UNIX;                    /* UNIX domain address */
    strncpy(socket_address.sun_path, SV_SOCK_PATH, sizeof(socket_address.sun_path) - 1);

    if (bind(socket_fd, (struct sockaddr *)&socket_address, sizeof(struct sockaddr_un)) == -1)
    {
        if (errno == EADDRINUSE)
        {
            perror("socket in use");
            exit(EXIT_FAILURE);
        }
        perror("binding error ");

        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, BACKLOG) == -1)
        exit(EXIT_FAILURE);

    return socket_fd;
}


int main(int argc, char *argv[])
{
    skeleton_daemon();
    int error_int = open(ERR_FILE, O_APPEND | O_CREAT);
    dup2(error_int, 2);
    close(error_int);
    int socket_fd = create_socket();
 
    int connection_fd;
    ssize_t numRead, numWrite;
    char buf[BUF_SIZE];
    char output_buf[BUF_SIZE];
    char *token;

    vector = vector_initialize(vector, sizeof(int), NULL);
    int message_count = 0;
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        connection_fd = accept(socket_fd, NULL, NULL);
        if (connection_fd == -1){
            logOpen(LOG_FILE);
            char str[20];
            snprintf(str, 20, "%d-%d", 1,1);
            logMessage(str);
            logClose();
            exit(EXIT_FAILURE);
        }

        while ((numRead = read(connection_fd, buf, BUF_SIZE)) > 0){
            logOpen(LOG_FILE);
            numWrite = logMessage(buf);
	    
            /* get the first token */
            token = strtok(buf, ",");
            
            if (strcmp(token, "-a") == 0){
                token = strtok(NULL, ",");
                int number = atoi(token);
                snprintf(output_buf, BUF_SIZE, "Succesfully added %d\n", number);
                vector = vector_push_back(vector, &number);                                
            }

            else if(strcmp(token, "-d") == 0){
                int number = atoi(strtok(NULL, ","));
                int i;
                int is_deleted = 0;
                for (i = vector_get_size(vector) - 1; i >= 0 ; i--){
                    if (*((int *)vector_at(vector, i)) == number){
                        vector_erase(vector, i);
                        snprintf(output_buf, BUF_SIZE, "Succesfully deleted all elements equal to %d\n", number);
                        is_deleted = 1;
                    }
                }
                if (!is_deleted) 
                    snprintf(output_buf, BUF_SIZE, "There is no element equal to %d\n", number);
            }

            else if(strcmp(token, "-s") == 0){
                char buf[16];
                strcpy(output_buf, "Vector: ");
                for(int i = 0; i < vector_get_size(vector); i++){
                    if(i == vector_get_size(vector) - 1)
                        snprintf(buf, sizeof(int)+1, "%d", *((int *)vector_at(vector, i)));
                    else
                        snprintf(buf, sizeof(int)+1, "%d,", *((int *)vector_at(vector, i)));

                    logMessage(buf);
                    strcat(output_buf, buf);
                }
                strcat(output_buf, "\n");
            }

            if (numRead != numWrite){
                logOpen(LOG_FILE);
                char str[20];
                snprintf(str, 20, "%ld-%ld\n", numWrite, numRead);
                logMessage(str);
                logClose();
                exit(EXIT_FAILURE);
            }            

            logMessage("\n");
            logClose();
            message_count++;
            memset(buf, 0, sizeof(buf));

            if (write(connection_fd, output_buf, BUF_SIZE) == -1){
                perror("write");
                exit(EXIT_FAILURE);
            }
        }

        if (numRead == -1){
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (close(connection_fd) == -1){
            perror("close");
            exit(EXIT_FAILURE);
        }

        if (message_count >= MAXIMUM_MESSAGE_COUNT) exit(EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}
