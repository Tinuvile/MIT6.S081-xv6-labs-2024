#include "kernel/types.h"
#include "user/user.h"

int
main()
{
    int parent_fd[2];
    int child_fd[2];
    char buf[1];

    if (pipe(parent_fd) < 0 || pipe(child_fd) < 0) {
        fprintf(2, "pipe error\n");
        exit(1);
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork error\n");
        exit(1);
    } else if (pid == 0) {
        // child
        close(parent_fd[1]);
        close(child_fd[0]);
        
        if (read(parent_fd[0], buf, sizeof(buf)) != 1) {
            fprintf(2, "read error\n");
            exit(1);
        }

        printf("%d: received ping\n", getpid());

        if (write(child_fd[1], buf, sizeof(buf))!= 1) {
            fprintf(2, "write error\n");
            exit(1);
        }

        close(parent_fd[0]);
        close(child_fd[1]);
        exit(0);
    } else {
        // parent
        close(parent_fd[0]);
        close(child_fd[1]);

        if (write(parent_fd[1], buf, sizeof(buf))!= 1) {
            fprintf(2, "write error\n");
            exit(1);
        }

        if (read(child_fd[0], buf, sizeof(buf))!= 1) {
            fprintf(2, "read error\n");
            exit(1);
        }

        printf("%d: received pong\n", getpid());

        close(parent_fd[1]);
        close(child_fd[0]);
        exit(0);
    }
}