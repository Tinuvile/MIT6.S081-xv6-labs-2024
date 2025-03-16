#include "kernel/types.h"
#include "user/user.h"

void child(int p[2]);

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
        child(p);
    } else {
        // parent
        close(p[0]);
        for (int i = 2; i <= 280; i++) {
            if (write(p[1], &i, sizeof(i)) != sizeof(i)) {
                fprintf(2, "write error\n");
                exit(1);
            }
        }
        close(p[1]);
        wait(0);
        exit(0);
    }
}

void child(int p[2]) {
    // child
    close(p[1]);

    int prime;
    if (read(p[0], &prime, sizeof(prime)) != sizeof(prime)) {
        close(p[0]);
        exit(0);
    }
    printf("prime %d\n", prime);
        
    int new_p[2];
    pipe(new_p);
    
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork error\n");
        exit(1);
    } if (pid == 0) {
        close(p[0]);
        close(new_p[1]);
        child(new_p);
        exit(0);
    } else {
        close(new_p[0]);
        int num;

        while (read(p[0], &num, sizeof(num)) == sizeof(num)) {
            if (num % prime != 0) {
                if (write(new_p[1], &num, sizeof(num)) != sizeof(num)) {
                    fprintf(2, "write error\n");
                    exit(1);
                }
            }
        }
        close(new_p[1]);
        close(p[0]);
        wait(0);
    }
    exit(0);
}