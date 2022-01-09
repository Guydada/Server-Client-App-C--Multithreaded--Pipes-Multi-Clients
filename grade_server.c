#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#define BACKLOG 10
#define BUFFER_SIZE 256
#define TA "assistants.txt"
#define ST "students.txt"
#define NUM_CLIENTS 5
#define ANT "Action not allowed"
#define EG "UpdateGrade 123456789 0"
#define ER "UpdateGrade 123456789 100"
#define DEFAULT_PASSWORD "password"

typedef struct pthread_arg_t {
    int new_socket_fd;
    struct sockaddr_in client_address;
} pthread_arg_t;

bool check_input(const char *id, const char *pw);
int write_grade(const char *id, const char *grade);

/* Thread routine to serve connection to client. */
void *pthread_routine(void *arg);

/* Signal handler to handle SIGTERM and SIGINT signals. */
void signal_handler(int signal_number);

/* user login */
int user_login(const char *id, const char *pw);

/* Search files */
bool search_id_pass(char filename[15], const char *id, const char *pw);

/* Text File editing */
void init_grades();
void append(FILE *inf, FILE *outf, const char *suffix);
char * read_grade(char *id);

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"


int main(int argc, char *argv[]) {
    int port, socket_fd, new_socket_fd;
    struct sockaddr_in address;
    pthread_attr_t pthread_attr;
    pthread_arg_t *pthread_arg;
    pthread_t pthread;
    socklen_t client_address_len;

    /* Get port from command line arguments or stdin. */
    port = argc > 1 ? atoi(argv[1]) : 0;
    if (!port) {
        printf("Enter Port: ");
        scanf("%d", &port);
    }

    /* Initialise IPv4 address. */
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    /* Create TCP socket. */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* Bind address to socket. */
    if (bind(socket_fd, (struct sockaddr *) &address, sizeof address) == -1) {
        perror("bind");
        exit(1);
    }

    /* Listen on socket. */
    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    /* Assign signal handlers to signals. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    /* Initialise pthread attribute to create detached threads. */
    if (pthread_attr_init(&pthread_attr) != 0) {
        perror("pthread_attr_init");
        exit(1);
    }
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate");
        exit(1);
    }


    /* Initialize Grades */
    init_grades();

    int thread_count = 0;

    while (true) {
        if (thread_count > NUM_CLIENTS) {
            printf("Server is full, clients: %d.\n", thread_count);
        }

        pthread_arg = (pthread_arg_t *) malloc(sizeof *pthread_arg);
        if (!pthread_arg) {
            perror("malloc");
            continue;
        }

        /* Accept connection to client. */
        client_address_len = sizeof pthread_arg->client_address;
        new_socket_fd = accept(socket_fd, (struct sockaddr *) &pthread_arg->client_address, &client_address_len);
        if (new_socket_fd == -1) {
            perror("accept");
            free(pthread_arg);
            continue;
        }

        /* Initialise pthread argument. */
        pthread_arg->new_socket_fd = new_socket_fd;
        /* Create thread to serve connection to client. */
        if (pthread_create(&pthread, &pthread_attr, pthread_routine, (void *) pthread_arg) != 0) {
            perror("pthread_create");
            free(pthread_arg);
            thread_count -= 1;
            continue;
        }
    }
    return 0;
}


#pragma clang diagnostic pop


void *pthread_routine(void *arg) {
    size_t nbytes;
    pthread_arg_t *pthread_arg = (pthread_arg_t *)arg;
    int new_socket_fd = pthread_arg->new_socket_fd;
    struct sockaddr_in client_address = pthread_arg->client_address;
    char buffer[BUFFER_SIZE] = {0};
    char parser[BUFFER_SIZE] = {0};
    char *msg;
    char *tok_buffer;
    char *token;
    char delim[] = " ";
    bool st_login = false;
    bool ta_login = false;
    bool ext_flag = false;
    const char *id, *pw;
    char logged_id[10] = {0};
    char logout[BUFFER_SIZE] = "Goodbye ";


    while (true){

        read(new_socket_fd, buffer, BUFFER_SIZE);
        strcpy(parser, buffer);
        token = strtok_r(parser, delim, &tok_buffer);
        msg = token;

        if ((strcmp(msg, "Login" ) == 0) & !st_login & !ta_login){
            if (strlen(buffer) < 7){
                write(new_socket_fd, "Wrong user information", BUFFER_SIZE);
                continue;}
            token = strtok_r(NULL, delim, &tok_buffer);
            id = token;
            token = strtok_r(NULL, delim, &tok_buffer);
            pw = token;
            char answer[BUFFER_SIZE] = "Welcome ";
            int i = user_login(id, pw);

            if (i == 1 && (!st_login | !ta_login)) {
                strcat(answer, "Student ");
                strcat(answer, id);
                write(new_socket_fd, answer, BUFFER_SIZE);
                strcat(logout, id);
                st_login = true;
                strcat(logged_id, id);
                }
            else if (i == 2 && (!st_login | !ta_login)) {
                strcat(answer, "TA ");
                strcat(answer, id);
                write(new_socket_fd, answer, BUFFER_SIZE);
                strcat(logout, id);
                ta_login = true;
                strcat(logged_id, id);
                }
            else {
                write(new_socket_fd, "Wrong user information", BUFFER_SIZE);
                }
        }

        else if ((strcmp(msg, "Login" ) == 0) && (st_login || ta_login)){
            write(new_socket_fd, "Wrong user information", BUFFER_SIZE);
        }

        else if (strcmp(msg, "Logout") == 0) {
            if (!st_login & !ta_login) {
                write(new_socket_fd, "Not logged in", BUFFER_SIZE);
                continue;
            }

            else if (st_login | ta_login) {
                st_login = false;
                ta_login = false;
                write(new_socket_fd, logout, BUFFER_SIZE);
                strcpy(logout, "Goodbye ");
            }
        }

        else if (strcmp(msg, "Exit") == 0){
            if (st_login | ta_login) {
                st_login = false;
                ta_login = false;
                write(new_socket_fd, logout, BUFFER_SIZE);
                strcpy(logout, "Goodbye ");
            }
            ext_flag = true;
            close(new_socket_fd);
            free(pthread_arg);
            signal_handler(SIGKILL);
        }

        /* Actions after login*/
        else if(strcmp(msg, "ReadGrade") == 0){
            if (!st_login & !ta_login) {
                write(new_socket_fd, "Not logged in", BUFFER_SIZE);
                continue;
            }
            if ((strlen(buffer) < 10) & st_login){
                const char *grade;
                grade = read_grade(logged_id);
                write(new_socket_fd, grade, BUFFER_SIZE);
                continue;
            }
            else if (ta_login){
                char *id_read;
                const char *grade;
                char grade_answer[BUFFER_SIZE] = {0};
                if (strlen(buffer) < strlen("ReadGrade ")) {
                    write(new_socket_fd, "Missing argument", BUFFER_SIZE);
                    continue;}
                token = strtok_r(NULL, delim, &tok_buffer);
                id_read = token;
                grade = read_grade(id_read);
                if (grade == NULL) {
                    write(new_socket_fd, "Invalid id", BUFFER_SIZE);
                    continue;}
                strcat(grade_answer, id_read);
                strcat(grade_answer, ": ");
                strcat(grade_answer, grade);
                write(new_socket_fd, grade_answer, BUFFER_SIZE);
            }
            else {
                write(new_socket_fd, ANT, BUFFER_SIZE);
            }
        }

        else if(strcmp(msg, "UpdateGrade") == 0){
            if (!st_login & !ta_login) {
                write(new_socket_fd, "Not logged in", BUFFER_SIZE);
                continue;
            }
            else if (st_login){
                write(new_socket_fd, ANT, BUFFER_SIZE);
                continue;
            }
            else if (strlen(buffer) < strlen(EG)) {
                write(new_socket_fd, "Missing argument", BUFFER_SIZE);
                continue;
            }

            else if (ta_login) {
                if (strlen(buffer) < strlen(EG)) {
                    write(new_socket_fd, "Wrong input", BUFFER_SIZE);
                    continue;}
                const char *grade;
                const char *id_update;
                token = strtok_r(NULL, delim, &tok_buffer);
                id_update = token;
                token = strtok_r(NULL, delim, &tok_buffer);
                grade = token;
                write_grade(id_update, grade);
                write(new_socket_fd, "skip", BUFFER_SIZE);
            }
        }

        else if(strcmp(msg, "ListGrades") == 0){
            if (!st_login & !ta_login) {
                write(new_socket_fd, "Not logged in", BUFFER_SIZE);
                continue;}

            else if (st_login){
                write(new_socket_fd, ANT, BUFFER_SIZE);
                continue;
            }

            else if (ta_login){
                FILE *fp;
                char *line;
                char buffer1[BUFFER_SIZE] = {0};
                char newline[BUFFER_SIZE] = {0};
                char grade_answer[BUFFER_SIZE] = {0};
                char *token1, *tok1_buffer;
                char delim1[] = ":";
                char *id_read;
                const char *grade1;

                fp = fopen("students.txt", "r");

                if (fp == NULL) {
                    printf("Error opening file!\n");
                    exit(1);
                }

                write(new_socket_fd, "batch", sizeof("batch"));
                while (fgets(newline, sizeof(newline), fp) != NULL) {
                    token1 = strtok_r(newline, delim1, &tok1_buffer);
                    id_read = token1;
                    grade1 = read_grade(id_read);
                    strcpy(grade_answer, id_read);
                    strcat(grade_answer, ": ");
                    strcat(grade_answer, grade1);
                    read(new_socket_fd, buffer1, sizeof(buffer1));
                    write(new_socket_fd, grade_answer, sizeof(newline));
                }
                fclose(fp);
                read(new_socket_fd, buffer1, sizeof(buffer));
                write(new_socket_fd, "end", BUFFER_SIZE);
                continue;
              }
            }
        else {
            write(new_socket_fd, "Wrong input", BUFFER_SIZE);
        }
    }

    close(new_socket_fd);
    free(pthread_arg);
    if (ext_flag){
        pthread_exit(NULL);
        signal_handler(SIGKILL);
    }
    return NULL;
}


int user_login(const char *id, const char *pw) {
    bool check_student   = search_id_pass(ST, id, pw);
    bool check_assistant = search_id_pass(TA, id, pw);
    if (!check_student & !check_assistant){
        return 0;
    } else if (check_student){
        return 1;
    } else if (check_assistant){
        return 2;
    } else
        return -1;
}


bool search_id_pass(char filename[15], const char *id, const char *pw){
    FILE *fp;
    char line[BUFFER_SIZE];
    char *tok_buf;
    char *pw_buffer, *id_buffer;
    char delim[] = ": \r\n";
    if (!check_input(id, pw)){
        return false;
    }

    if ((fp = fopen(filename, "r")) == NULL) {
        perror("Error opening file");
        return false;}

    while(fgets(line, sizeof(line), fp) != NULL) {
        char *token = strtok_r(line, delim, &tok_buf);
        id_buffer = token;
        token = strtok_r(NULL, delim, &tok_buf);
        pw_buffer = token;
        if (strcmp(id_buffer, id) == 0 && strcmp(pw_buffer, pw) == 0) {
            fclose(fp);
            return true;
        }
        else continue;
    }
    fclose(fp);
    return false;
}



void append(FILE *inf, FILE *outf, const char *suffix) {
    char text[BUFFER_SIZE] = {0};
    char out_text[BUFFER_SIZE] = {0};
    // loop over characters in inf
    while (fgets(text, BUFFER_SIZE, inf) != NULL) {
        // append suffix to text if last character is '\n'
        if (text[strlen(text) - 1] == '\n') {
            text[strlen(text) - 1] = '\0';
            strcat(text, suffix);
            strcat(text, "\n");
            strcpy(out_text, text);
        } else{
            text[strlen(text)] = '\0';
            strcat(text, suffix);
            strcpy(out_text, text);
        }
        fputs(out_text, outf);
    }}


void init_grades() {
    FILE *in = fopen(ST, "r");
    char temp[15] = "remove.tmp";
    FILE *out = fopen(temp, "w");
    if ((in == NULL) | (out == NULL)) {
        printf("Error opening file!\n");
        exit(1);
    }
    append(in, out, " 0");
    fclose(in);
    fclose(out);
    remove(ST);
    rename(temp, ST);
}



void signal_handler(int signal_number) {
    if (signal_number == SIGINT) {
        printf("\nCaught SIGINT\n");
        exit(1);
    }
    if (signal_number == SIGPIPE) {
        printf("\nCaught SIGPIPE\n");
        exit(1);
    }

    if (signal_number == SIGSEGV) {
        printf("\nCaught SIGSEGV\n");
        exit(1);
    }

    if (signal_number == SIGTERM) {
        printf("\nCaught SIGTERM\n");
        exit(1);
    }

    if (signal_number == SIGKILL) {
        printf("\nCaught SIGUSR1\n");
        exit(1);
    }

    exit(0);
}


char *read_grade(char *id){
    FILE *fp;
    char line[BUFFER_SIZE] = {0};
    char *token2;
    char *tok_buf;
    char delim2[] = ": \r\n";
    fp = fopen(ST, "r");
    if(fp == NULL){
        printf("Error opening file\n");
        return NULL;
    }

    while(fgets(line, BUFFER_SIZE, fp) != NULL){
        token2 = strtok_r(line, delim2, &tok_buf);
        if(strcmp(token2, id) == 0){
            token2 = strtok_r(NULL, delim2, &tok_buf); // get password, skip
            token2 = strtok_r(NULL, delim2, &tok_buf); // get grade
            fclose(fp);
            return token2;
        }
    }
    fclose(fp);
    return NULL;
}

bool check_input(const char *id, const char *pw){
    if ((id == NULL) | (pw == NULL)){
        return false;
    }
    if ((strlen(id) < 7) || (strlen(pw) < 1)){
        return false;
    }
    return true;
}

int write_grade(const char *id, const char *grade) {
    FILE *fp, *temp;
    char line[BUFFER_SIZE] = {0};
    char parser[BUFFER_SIZE] = {0};
    char *token;
    char *tok_buf;
    char *pw_buffer, *id_buffer;
    char delim[] = ": \r\n";
    char new_line[BUFFER_SIZE] = {0};
    bool found = false;
    fp = fopen(ST, "r");
    temp = fopen("remove.tmp", "w");
    if (fp == NULL || temp == NULL) {
        printf("Error opening file\n");
        return -1;
    }
    while (fgets(line, BUFFER_SIZE, fp) != NULL) {
        strcpy(parser, line);
        token = strtok_r(parser, delim, &tok_buf);
        id_buffer = token;
        if (strcmp(id_buffer, id) == 0) {
            token = strtok_r(NULL, delim, &tok_buf); // get password, skip
            pw_buffer = token;
            strcat(new_line, id_buffer);
            strcat(new_line, ":");
            strcat(new_line, pw_buffer);
            strcat(new_line, " ");
            strcat(new_line, grade);
            if (line[strlen(line) - 1] == '\n') {
                strcat(new_line, "\n");
            }
            fputs(new_line, temp);
            found = true;
        } else {
            fputs(line, temp);
        }
    }
    fclose(fp);
    fclose(temp);

    if (!found){
        temp = fopen("remove.tmp", "a");
        fputs("\n", temp);
        char empty_line[BUFFER_SIZE];
        strcat(empty_line, id);
        strcat(empty_line, ":");
        strcat(empty_line, DEFAULT_PASSWORD); // since users are stored in "id:password grade" format
        strcat(empty_line, " ");
        strcat(empty_line, grade);
        fputs(empty_line, temp);
        fclose(temp);
    }
    remove(ST);
    rename("remove.tmp", ST);
    return 0;
}


