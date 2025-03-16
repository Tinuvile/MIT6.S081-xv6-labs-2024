#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

#define MAX_LINE 1024

int
main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "xargs: not enough arguments\n");
        exit(1);
    }

    char line[MAX_LINE];
    char *cmd_argv[MAXARG];
    char ch;
    int pos = 0;

    for (int i = 1; i < argc; i++) {
        cmd_argv[i - 1] = argv[i];
    }

    while (read(0, &ch, 1) > 0) {
        if (ch == '\n') {
            if (pos == 0) {
                continue;
            }
            line[pos] = 0;
            cmd_argv[argc - 1] = line;
            cmd_argv[argc] = 0;

            if (fork() == 0) {
                exec(cmd_argv[0], cmd_argv);
                fprintf(2, "xargs: exec %s failed\n", cmd_argv[0]);
                exit(1);
            } else {
                wait(0);
                pos = 0;
            }
        } else {
            if (pos < MAX_LINE - 1) {
                line[pos++] = ch;
            }
        }
    }
    exit(0);
}