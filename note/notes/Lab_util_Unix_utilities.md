## sleep(easy)

> Implement a user-level sleep program for xv6, along the lines of the UNIX sleep command. Your sleep should pause for a user-specified number of ticks. A tick is a notion of time defined by the xv6 kernel, namely the time between two interrupts from the timer chip. Your solution should be in the file user/sleep.c.
> 
> 实现一个用户级的 sleep 程序，类似于 UNIX 的 sleep 命令。你的 sleep 应该暂停用户指定的滴答数。滴答是由 xv6 内核定义的时间概念，即来自定时器芯片的两个中断之间的时间。你的解决方案应该在文件 user/sleep.c 中。

通过阅读其他代码了解命令行参数的传递，`argc`为参数的个数，`argv`为具体的参数内容。

```c
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc != 2) {  // 强制两个参数：程序名sleep + 休眠时间
    fprintf(2, "Usage: sleep ticks\n");
    exit(1);
  }

  int ticks = atoi(argv[1]);  // atoi用于将字符串转换成整数
  sleep(ticks);

  exit(0);
}
```

## pingpong(easy)

> Write a user-level program that uses xv6 system calls to ''ping-pong'' a byte between two processes over a pair of pipes, one for each direction. The parent should send a byte to the child; the child should print "<pid>: received ping", where <pid> is its process ID, write the byte on the pipe to the parent, and exit; the parent should read the byte from the child, print "<pid>: received pong", and exit. Your solution should be in the file user/pingpong.c.  
> 编写一个用户级程序，使用 xv6 系统调用在一对管道之间“乒乓”传输一个字节，每个方向一个管道。父进程应向子进程发送一个字节；子进程应打印“: received ping”，其中是其进程 ID，将字节写入管道给父进程，然后退出；父进程应从子进程读取字节，打印“: received pong”，然后退出。你的解决方案应放在文件 user/pingpong.c 中。

```c
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
```

## primes(moderate/hard)

> Write a concurrent prime sieve program for xv6 using pipes and the design illustrated in the picture halfway down [this page](http://swtch.com/~rsc/thread/) and the surrounding text. This idea is due to Doug McIlroy, inventor of Unix pipes. Your solution should be in the file user/primes.c.  
> 为 xv6 编写一个并发素数筛程序，使用管道和本页中间图片及周围文字所示的设计。这个想法归功于 Unix 管道的发明者 Doug McIlroy。你的解决方案应该在文件 user/primes.c 中。

这个任务需要利用`pipe`与`fork`设置管道，第一个进程负责将数字2-280输入管道，然后对于每个质数，需要创建一个进程，该进程从左侧读取数据，并通过管道向右侧写入数据，将这个质数的倍数过滤掉。

父进程部分使用循环输入数据：

```c
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
```

对于这种多个输入，内核会管理管道的缓冲区，无需用户进行空间内存分配。另外，读取也是使用循环读取，管道中实际上是字节流，4字节的读即可。

根据这个过程，进入子进程中的第一个数始终是素数，可以使用数学归纳法证明。

子进程部分：

```c
// child
close(p[1]);  // 关闭写端

int prime;
while (read(p[0], &prime, sizeof(prime)) == sizeof(prime)) {
    printf("prime %d\n", prime);

    int new_p[2];  // 创建新管道
    pipe(new_p);

    int num;       // 过滤素数
    while(read(p[0], &num, sizeof(num)) == sizeof(num)) {
        if (num % prime != 0) {
            if (write(new_p[1], &num, sizeof(num))) {
                fprintf(2, "write error\n");
                exit(1);
            }
        }
    }
    close(new_p[1]);
}
close(p[0]);
exit(0);
```

需要把这部分抽象成函数，代码：

```c
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
        close(new_p[1]);
        child(new_p);
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
        exit(0);
    }
}
```

但是结果出现错误，后期乱码，怀疑是某个管道未正确关闭。

```powershell
tinuvile@LAPTOP-7PVP3HH3:~/xv6-labs-2024$ make qemu
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -global virtio-mmio.force-legacy=false -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ primes
prime 2
prime 3
prime 5
prime 7
prime 11
prime 13
prime 17
prime 19
prime 23
prime 29
prime 31
prime 37
$ /5;=CGIOSYaegkmq�����������������������
$QEMU: Terminated
```

DeepSeek认为可能原因有：

- 递归深度过大，目前的代码思路是每个质数生成一个子进程形成递归链。递归层级可能超过`xv6`默认的栈大小，导致栈溢出；

- 文件描述符泄露，未正确关闭管道文件描述符，可能耗尽`fd`资源；或可能某些分支遗漏关闭操作；

经过多次修改后成功的代码：

```c
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
```

错误原因应该在代码的`if (pid == 0)`下的`close(p[0])`这句，关闭原始管道读端并确保子进程中止。

## find(moderate)

> Write a simple version of the UNIX find program for xv6: find all the files in a directory tree with a specific name. Your solution should be in the file user/find.c.  
> 为 xv6 编写一个简单的 UNIX find 程序版本：在目录树中查找具有特定名称的所有文件。你的解决方案应该在文件 user/find.c 中。
