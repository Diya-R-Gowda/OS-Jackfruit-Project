#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fcntl.h>

#define MAX_CONTAINERS 10
#define SOCKET_PATH "/tmp/mini_runtime.sock"
#define STACK_SIZE (1024 * 1024)

typedef struct {
    char id[32];
    pid_t pid;
    char state[32];
} container_t;

container_t containers[MAX_CONTAINERS];
int container_count = 0;

/* ================= CHILD FUNCTION ================= */
int child_func(void *arg) {

    char *id = (char *)arg;

    char log_path[100];
    sprintf(log_path, "logs/%s.log", id);

    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);

    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    sethostname(id, strlen(id));

    chroot("./rootfs-alpha");
    chdir("/");

    mkdir("/proc", 0555);
    mount("proc", "/proc", "proc", 0, NULL);

    execl("/bin/sh", "/bin/sh", NULL);

    return 0;
}

/* ================= START CONTAINER ================= */
void start_container(char *id) {

    char *stack = malloc(STACK_SIZE);
    char *stack_top = stack + STACK_SIZE;

    pid_t pid = clone(
        child_func,
        stack_top,
        CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD,
        id
    );

    strcpy(containers[container_count].id, id);
    containers[container_count].pid = pid;
    strcpy(containers[container_count].state, "running");

    container_count++;

    printf("Started container %s with PID %d\n", id, pid);
}

/* ================= PRINT CONTAINERS ================= */
void print_ps() {
    printf("ID\tPID\tSTATE\n");
    for (int i = 0; i < container_count; i++) {
        printf("%s\t%d\t%s\n",
               containers[i].id,
               containers[i].pid,
               containers[i].state);
    }
}

/* ================= STOP CONTAINER ================= */
void stop_container(char *id) {
    for (int i = 0; i < container_count; i++) {
        if (strcmp(containers[i].id, id) == 0) {
            kill(containers[i].pid, SIGTERM);
            strcpy(containers[i].state, "stopped");
            printf("Stopped container %s\n", id);
            return;
        }
    }
    printf("Container not found\n");
}

/* ================= SHOW LOGS ================= */
void show_logs(char *id) {

    char path[100];
    sprintf(path, "logs/%s.log", id);

    FILE *f = fopen(path, "r");

    if (!f) {
        printf("No logs found\n");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }

    fclose(f);
}

/* ================= SUPERVISOR ================= */
int run_supervisor() {

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    unlink(SOCKET_PATH);
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("Supervisor running...\n");

    while (1) {

        int client = accept(server_fd, NULL, NULL);

        char buffer[256] = {0};
        read(client, buffer, sizeof(buffer));

        printf("Received: %s\n", buffer);

        /* ===== COMMAND PARSING ===== */

        if (strncmp(buffer, "start", 5) == 0) {
            char id[32];
            sscanf(buffer, "start %s", id);
            start_container(id);
        }

        else if (strcmp(buffer, "ps") == 0) {
            print_ps();
        }

        else if (strncmp(buffer, "stop", 4) == 0) {
            char id[32];
            sscanf(buffer, "stop %s", id);
            stop_container(id);
        }

        else if (strncmp(buffer, "logs", 4) == 0) {
            char id[32];
            sscanf(buffer, "logs %s", id);
            show_logs(id);
        }

        close(client);
    }

    return 0;
}

/* ================= CLIENT ================= */
int send_command(char *cmd) {

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    write(sock, cmd, strlen(cmd));
    close(sock);

    return 0;
}

/* ================= MAIN ================= */
int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage:\n");
        printf("./engine supervisor\n");
        printf("./engine start <id>\n");
        printf("./engine ps\n");
        printf("./engine stop <id>\n");
        printf("./engine logs <id>\n");
        return 1;
    }

    if (strcmp(argv[1], "supervisor") == 0) {
        return run_supervisor();
    }

    char command[256] = {0};

    if (strcmp(argv[1], "start") == 0) {
        sprintf(command, "start %s", argv[2]);
    }
    else if (strcmp(argv[1], "ps") == 0) {
        strcpy(command, "ps");
    }
    else if (strcmp(argv[1], "stop") == 0) {
        sprintf(command, "stop %s", argv[2]);
    }
    else if (strcmp(argv[1], "logs") == 0) {
        sprintf(command, "logs %s", argv[2]);
    }

    return send_command(command);
}

