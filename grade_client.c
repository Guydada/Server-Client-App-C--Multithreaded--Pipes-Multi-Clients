#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdbool.h>
#define SERVER_NAME_LEN_MAX 100
# define BUFFER_SIZE 256


static void handle_connection(int p[2], int q[2], int socket);
static void handle_input(int p[2], int q[2]);


static void err_exit(const char *fmt, ...)
{
    int errnum = errno;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n%d: %s\n", errnum, strerror(errnum));
    exit(1);
}

int main(int argc, char *argv[])
{
    // Server Variables
    char server_name[SERVER_NAME_LEN_MAX + 1] = {0};
    int server_port, socket_fd;
    struct hostent *server_host;
    struct sockaddr_in server_address;

    /* Get server name from command line arguments or stdin. */
    strncpy(server_name, argv[1], SERVER_NAME_LEN_MAX);
    /* Get server port from command line arguments or stdin. */
    server_port = argc > 2 ? atoi(argv[2]) : 0;
    /* Get server host from server name. */
    server_host = gethostbyname(server_name);

    /* Initialise IPv4 server address with server host. */
    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    memcpy(&server_address.sin_addr.s_addr, server_host->h_addr, server_host->h_length);

    /* Create TCP socket. */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* Connect to socket with server address. */
    if (connect(socket_fd, (struct sockaddr *) &server_address, sizeof server_address) == -1) {
        perror("connect");
        exit(1);
    }

    // Pipe Variables
    int   p[2];     /* Pipe to child */
    int   q[2];     /* Pipe to parent */
    pid_t pid;


    if (pipe(p) != 0)
        err_exit("Failed to create pipe 1");

    if (pipe(q) != 0)
        err_exit("Failed to create pipe 2");

    if ((pid = fork()) < 0)
        err_exit("Failed to create child process");
    else if (pid == 0)
        handle_connection(p, q, socket_fd);
    else
        handle_input(p, q);

    return(0);
}


static int prompt(char *buffer, bool batch, bool kill)
{
    char *c;
    if (!batch){
        printf(">");
        if (fgets(buffer, BUFFER_SIZE, stdin) == 0)
            return EOF;
        if ((c = strchr(buffer, '\n')) != NULL)
            *c = '\0';
    }
    else if (kill){
        return EOF;
    }
    else {
        char *batch_command = "continue";
        strcpy(buffer, batch_command);
    }
    return 0;
}


static void handle_input(int p[2], int q[2])
{
    char  cmd[10] = "";
    bool batch = false;
    bool kill = false;

    if (close(p[0]) != 0 || close(q[1]) != 0)
        err_exit("Parent: failed to close pipe");

    while (prompt(cmd, batch, kill) != EOF)
    {   char    buffer[BUFFER_SIZE];
        ssize_t nbytes;

        if (write(p[1], cmd, strlen(cmd)) != (ssize_t)strlen(cmd))
            err_exit("Write to child failed");

        if (kill){
            exit(0);
        }

        if ((nbytes = read(q[0], buffer, sizeof(buffer))) < 0)
            err_exit("Read from child failed");
        if (nbytes == 0) {
            return;
        }
        buffer[nbytes] = '\0';

        if (strcmp(buffer, "batch") == 0){
            batch = true;
            continue;
        }
        else if (strcmp(buffer, "end") == 0){
            batch = false;
            continue;
        }

        else if (strcmp(buffer, "Exit") == 0){
                kill = true;
                exit(0);
        }

        else if(strcmp(buffer, "skip") != 0) {
            printf("%s\n", buffer);
            if (strcmp(cmd, "Exit") == 0){
                kill = true;
                exit(0);}
        }




    }
}


static void handle_connection(int p[2], int q[2], int socket)
{
    char    cmd[BUFFER_SIZE] = "";
    char    from_server[BUFFER_SIZE] = "";
    ssize_t nbytes;

    if (close(p[1]) != 0 || close(q[0]) != 0)
        err_exit("Child: failed to close pipe");

    while ((nbytes = read(p[0], cmd, sizeof(cmd))) > 0)
    {
        char    buffer[BUFFER_SIZE];
        cmd[nbytes] = '\0';
        /* Process command */
        // send command to server
        write(socket, cmd, BUFFER_SIZE);
        // get response from server
        nbytes = read(socket, from_server, sizeof(from_server));
        from_server[nbytes] = '\0';
        strcpy(buffer, from_server);
        if (write(q[1], buffer, strlen(buffer)) != (ssize_t)strlen(buffer))
            err_exit("Write to parent failed");
    }
}