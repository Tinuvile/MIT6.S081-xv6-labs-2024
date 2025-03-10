#include "kernel/types.h"
#include "user/user.h"

int
main()
{
    int p[2];

    if (pipe(p) < 0) {
        fprintf(2, "pipe error\n");
        exit(1);   
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork error\n");
        exit(1);
    } else if (pid == 0) {
        // child
        close(p[1]);

        int prime;
    } else {
        // parent
        close(p[0]);
        for (int i = 2; i <= 280; i++) {
            if (write(p[1], &i, sizeof(i))) {
                fprintf(2, "write error\n");
                exit(1);
            }
        }
        close(p[1]);
        wait(0);
        exit(0);
    }
}