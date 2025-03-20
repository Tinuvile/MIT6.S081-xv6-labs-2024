#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define PATTERN "my very very very secret pw is:   "
#define PAT_LEN 31
#define MIN_MATCH 16 
#define SECRET_LEN 8

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char *mem = sbrk(PGSIZE*32);
  mem = mem + 9 * PGSIZE;
  for (char *p = mem; p < mem + PGSIZE * 128; p++) {
    int match_count = 0;
    for (int i = 0; i < PAT_LEN; i++) {
      if (p[i] == PATTERN[i]) {
        if (++match_count >= MIN_MATCH) {
          write(2, p + 32, SECRET_LEN);
          exit(0);
        }
      } else {
        match_count = 0;
      }
    }
  }
  exit(1);
}
