**注：本次Lab中第二个问题没有经过测试，不保证正确性** 

## 切换分支

首先切换到`pgtbl`分支：

```bash
tinuvile@LAPTOP-7PVP3HH3:~/xv6-labs-2024$ git fetch
tinuvile@LAPTOP-7PVP3HH3:~/xv6-labs-2024$ git checkout pgtbl
M       Makefile
M       conf/lab.mk
M       grade-lab-pgtbl
M       kernel/defs.h
M       kernel/kalloc.c
M       kernel/memlayout.h
M       kernel/proc.c
M       kernel/proc.h
M       kernel/riscv.h
M       kernel/syscall.c
M       kernel/syscall.h
M       kernel/sysproc.c
M       user/pgtbltest.c
M       user/ulib.c
M       user/user.h
M       user/usys.pl
Already on 'pgtbl'
Your branch is up to date with 'origin/pgtbl'.
tinuvile@LAPTOP-7PVP3HH3:~/xv6-labs-2024$ make clean
rm -rf *.tex *.dvi *.idx *.aux *.log *.ind *.ilg *.dSYM *.zip *.pcap \
*/*.o */*.d */*.asm */*.sym \
user/initcode user/initcode.out user/usys.S user/_* \
kernel/kernel \
mkfs/mkfs fs.img .gdbinit __pycache__ xv6.out* \
ph barrier
```

## Inspect a user-process page table ([easy](https://pdos.csail.mit.edu/6.S081/2024/labs/guidance.html))

> For every page table entry in the print_pgtbl output, explain what it logically contains and what its permission bits are. Figure 3.4 in the xv6 book might be helpful, although note that the figure might have a slightly different set of pages than process that's being inspected here. Note that xv6 doesn't place the virtual pages consecutively in physical memory. 
> 对于 print_pgtbl 输出中的每个页表项，解释其逻辑内容及权限位含义。xv6 教材中的图 3.4 可能有帮助，但需注意该图展示的页表可能与当前被检测进程存在细微差异。注意 xv6 不会将虚拟页连续映射到物理内存中。

第一个任务是解释用户进程的页表。运行`make qemu`并执行用户程序`pgtbltest`，`print_pgtbl`函数使用`pgpte`系统调用，打印`pgtbltest`进程前10页和后10页的页表项。

```bash
$ pgtbltest
print_pgtbl starting
va 0x0 pte 0x21FC885B pa 0x87F22000 perm 0x5B
va 0x1000 pte 0x21FC7C17 pa 0x87F1F000 perm 0x17
va 0x2000 pte 0x21FC7807 pa 0x87F1E000 perm 0x7
va 0x3000 pte 0x21FC74D7 pa 0x87F1D000 perm 0xD7
va 0x4000 pte 0x0 pa 0x0 perm 0x0
va 0x5000 pte 0x0 pa 0x0 perm 0x0
va 0x6000 pte 0x0 pa 0x0 perm 0x0
va 0x7000 pte 0x0 pa 0x0 perm 0x0
va 0x8000 pte 0x0 pa 0x0 perm 0x0
va 0x9000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF6000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF7000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF8000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF9000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFA000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFB000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFC000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFD000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFE000 pte 0x21FD08C7 pa 0x87F42000 perm 0xC7
va 0xFFFFF000 pte 0x2000184B pa 0x80006000 perm 0x4B
print_pgtbl: OK
ugetpid_test starting
usertrap(): unexpected scause 0xd pid=4
            sepc=0x542 stval=0x3fffffd000
```

以第一个为例，`va 0x0`对应的权限为`0x5B`，即`01011011`，分解为：`PTE_V(1)`、`PTE_R(1)`、`PTE_W(0)`、`PTE_X(1)`、`PTE_U(1)`、`PTE_G(0)`、`PTE_A(1)`、`PTE_D(0)`。

以下是针对每个虚拟地址（va）的页表项（PTE）及其权限位的逻辑内容和权限解释。逻辑内容参考教材**Figure 3.4**，权限位基于**RISC-V Sv39 PTE**标志位定义：

| 虚拟地址 (va)         | PTE 值 (十六进制) | 物理地址 (pa)  | 权限 (perm) | 逻辑内容                  | 权限位分解                                                                       |
|:-----------------:|:------------:|:----------:|:---------:|:---------------------:|:---------------------------------------------------------------------------:|
| **0x0**           | 0x21FC885B   | 0x87F22000 | 0x5B      | **用户代码段（Text）**       | `PTE_V \| PTE_R \| PTE_X \| PTE_U \| PTE_A`（有效、可读、可执行、用户可访问、已访问）            |
| **0x1000**        | 0x21FC7C17   | 0x87F1F000 | 0x17      | **用户数据段（Data）**       | `PTE_V \| PTE_R \| PTE_W \| PTE_A`（有效、可读、可写、已访问）                            |
| **0x2000**        | 0x21FC7807   | 0x87F1E000 | 0x07      | **用户 BSS 段**          | `PTE_V \| PTE_R \| PTE_W`（有效、可读、可写）                                         |
| **0x3000**        | 0x21FC74D7   | 0x87F1D000 | 0xD7      | **用户堆（Heap）**         | `PTE_V \| PTE_R \| PTE_W \| PTE_U \| PTE_A \| PTE_D`（有效、可读、可写、用户可访问、已访问、脏页） |
| **0x4000-0x9000** | 0x0          | 0x0        | 0x0       | **未映射区域（guard page）** | 无有效映射                                                                       |
| **0xFFFFE000**    | 0x21FD08C7   | 0x87F42000 | 0xC7      | **用户栈（Stack）**        | `PTE_V \| PTE_R \| PTE_W \| PTE_A \| PTE_D`（有效、可读、可写、已访问、脏页）                |
| **0xFFFFF000**    | 0x2000184B   | 0x80006000 | 0x4B      | **Trampoline 页面**     | `PTE_V \| PTE_R \| PTE_X \| PTE_A`（有效、可读、可执行、已访问）                           |

但是与教材中图有一些不同。`xv6`不会将虚拟页连续映射到物理内存中。

| **教材标准布局**                        | **当前测试输出布局**           | **差异分析**                                  |
| --------------------------------- | ---------------------- | ----------------------------------------- |
| **Text（代码段）**：0x0                 | **0x0**（权限0x5B）        | 一致，均为代码段起始地址。                             |
| **Data（数据段）**：紧接Text              | **0x1000**（权限0x17）     | 权限中缺少`PTE_U`，可能为内核调试映射或配置错误。              |
| **Guard Page**：未映射                | **0x4000-0x9000**（未映射） | Guard Page通常位于栈下方，此处未显式列出，可能因测试程序未触发栈扩展。  |
| **Stack（栈）**：高地址（向下增长）            | **0xFFFFE000**（权限0xC7） | 栈位于高地址，符合预期。                              |
| **Heap（堆）**：向上增长                  | **0x3000**（权限0xD7）     | 堆在低地址区域，与教材中的高地址布局相反，可能是测试程序的特例。          |
| **Trapframe**：接近顶端                | **未显示**                | 可能未触发陷阱处理或输出未覆盖该区域。                       |
| **Trampoline**：最高地址（0x3ffffff000） | **0xFFFFF000**（权限0x4B） | 映射到虚拟地址0xFFFFF000，而非标准0x3ffffff000，但权限一致。 |

## Speed up system calls ([easy](https://pdos.csail.mit.edu/6.S081/2024/labs/guidance.html))

> When each process is created, map one read-only page at USYSCALL (a virtual address defined in memlayout.h). At the start of this page, store a struct usyscall (also defined in memlayout.h), and initialize it to store the PID of the current process. For this lab, ugetpid() has been provided on the userspace side and will automatically use the USYSCALL mapping. You will receive full credit for this part of the lab if the ugetpid test case passes when running pgtbltest. 
> 当每个进程创建时，在 USYSCALL（一个在 memlayout.h 中定义的虚拟地址）处映射一个只读页。在该页起始位置存储一个 struct usyscall （同样定义于 memlayout.h ），并将其初始化为当前进程的 PID。本实验中，用户空间侧已提供 ugetpid() ，并将自动使用 USYSCALL 映射。如果在运行 pgtbltest 时 ugetpid 测试用例通过，你将获得此部分实验的满分。

某些操作系统（如Linux）通过在用户空间和内核之间共享只读区域的数据来加速特定系统调用，这消除了执行这些系统调用时进行内核穿越的必要性。

进程创建时在USYSCALL处映射页需要修改`proc_pagetable`函数：

```c
// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  // map a read-only page on USYSCALL, which is shared with the kernel.
  if (mappages(pagetable, USYSCALL, PGSIZE,
              (uint64)(p->usyscall), PTE_R | PTE_U) < 0) {
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}
```

释放：

```c
// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmunmap(pagetable, USYSCALL, 1, 0);
  uvmfree(pagetable, sz);
}
```

然后在`allocproc`函数中初始化`usyscall`的结构，并将`pid`初始化为进程的`pid`：

```c
// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  // Allocate a usyscall page.
  if ((p->usyscall = (struct usyscall *)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  p->usyscall->pid = p->pid;

  return p;
}
```

然后释放`USYSCALL`页：

```c
/ free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  if (p->usyscall)
    kfree((void*)p->usyscall);
  p->usyscall = 0;
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}
```

还需要修改`proc.h`中的`struct proc`，加入：

```c
struct usyscall *usyscall; // in memlayout.h
```

这次实验的测试一直遇到错误：

```c
$ pgtbltest
print_pgtbl starting
va 0x0 pte 0x21FC7C5B pa 0x87F1F000 perm 0x5B
va 0x1000 pte 0x21FC7017 pa 0x87F1C000 perm 0x17
va 0x2000 pte 0x21FC6C07 pa 0x87F1B000 perm 0x7
va 0x3000 pte 0x21FC68D7 pa 0x87F1A000 perm 0xD7
va 0x4000 pte 0x0 pa 0x0 perm 0x0
va 0x5000 pte 0x0 pa 0x0 perm 0x0
va 0x6000 pte 0x0 pa 0x0 perm 0x0
va 0x7000 pte 0x0 pa 0x0 perm 0x0
va 0x8000 pte 0x0 pa 0x0 perm 0x0
va 0x9000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF6000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF7000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF8000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF9000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFA000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFB000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFC000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFD000 pte 0x21FD4413 pa 0x87F51000 perm 0x13
va 0xFFFFE000 pte 0x21FD00C7 pa 0x87F40000 perm 0xC7
va 0xFFFFF000 pte 0x2000184B pa 0x80006000 perm 0x4B
print_pgtbl: OK
ugetpid_test starting
usertrap(): unexpected scause 0x5 pid=4
            sepc=0x542 stval=0x3fffffd000
```

根据RISC-V手册，`scause` 的值`0x5`应该是**Load access fault**，但是`0x13`对应的`PTE_U`是设置了的，这里暂时还没找到原因，最近时间比较紧张，暂时搁置。不过注释掉这个函数后其他测试可以正常运行。

> Which other xv6 system call(s) could be made faster using this shared page? Explain how.  
> 还有哪些 xv6 系统调用能通过这个共享页面加速？请解释具体实现方式。

共享页面的特定是只读，且用户空间可以直接访问而不用进入内核，适合一些频繁调用且数据较少变化的系统调用。

## Print a page table ([easy](https://pdos.csail.mit.edu/6.S081/2024/labs/guidance.html))

> We added a system call kpgtbl(), which calls vmprint() in vm.c. It takes a pagetable_t argument, and your job is to print that pagetable in the format described below. 
> 我们添加了一个系统调用 kpgtbl() ，它在 vm.c 中调用 vmprint() 。该系统调用接受一个 pagetable_t 参数，你的任务是以如下描述的格式打印该页表。
> 
> When you run print_kpgtbl() test, your implementation should print the following output: 
> 当你运行 print_kpgtbl() 测试时，你的实现应输出以下内容：
> 
> page table 0x0000000087f22000
>  ..0x0000000000000000: pte 0x0000000021fc7801 pa 0x0000000087f1e000
>  .. ..0x0000000000000000: pte 0x0000000021fc7401 pa 0x0000000087f1d000
>  .. .. ..0x0000000000000000: pte 0x0000000021fc7c5b pa 0x0000000087f1f000
>  .. .. ..0x0000000000001000: pte 0x0000000021fc70d7 pa 0x0000000087f1c000
>  .. .. ..0x0000000000002000: pte 0x0000000021fc6c07 pa 0x0000000087f1b000
>  .. .. ..0x0000000000003000: pte 0x0000000021fc68d7 pa 0x0000000087f1a000
>  ..0xffffffffc0000000: pte 0x0000000021fc8401 pa 0x0000000087f21000
>  .. ..0xffffffffffe00000: pte 0x0000000021fc8001 pa 0x0000000087f20000
>  .. .. ..0xffffffffffffd000: pte 0x0000000021fd4c13 pa 0x0000000087f53000
>  .. .. ..0xffffffffffffe000: pte 0x0000000021fd00c7 pa 0x0000000087f40000
>  .. .. ..0xfffffffffffff000: pte 0x000000002000184b pa 0x0000000080006000
> 
> The first line displays the argument to vmprint. After that there is a line for each PTE, including PTEs that refer to page-table pages deeper in the tree. Each PTE line is indented by a number of " .." that indicates its depth in the tree. Each PTE line shows its virtual addresss, the pte bits, and the physical address extracted from the PTE. Don't print PTEs that are not valid. In the above example, the top-level page-table page has mappings for entries 0 and 255. The next level down for entry 0 has only index 0 mapped, and the bottom-level for that index 0 has a few entries mapped. 
> 第一行显示的是 vmprint 的参数。之后每一行代表一个页表项（PTE），包括指向树形结构中更深层次页表页的 PTE。每个 PTE 行根据其在树中的深度缩进相应数量的 " .." 符号。每行 PTE 显示其虚拟地址、PTE 位以及从 PTE 中提取的物理地址。无效的 PTE 无需打印。在上例中，顶级页表页映射了条目 0 和 255。下一层级中条目 0 仅映射了索引 0，而该索引 0 的底层又映射了若干条目。

根据提示先查看`freewalk`函数：

```c
void
freewalk(pagetable_t pagetable)
{
  // 遍历页表的所有 512 个 PTE（2^9 = 512）
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];  // 获取第 i 个页表项

    // 检查 PTE 有效且不是叶子节点（没有 R/W/X 权限位）
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      uint64 child = PTE2PA(pte);  // 提取下级页表的物理地址
      freewalk((pagetable_t)child); // 递归释放下级页表
      pagetable[i] = 0;           // 清空当前 PTE
    } 
    // 如果是有效叶子节点（有物理页映射）
    else if(pte & PTE_V){
      panic("freewalk: leaf");  // 叶子映射未正确移除时触发错误
    }
  }
  kfree((void*)pagetable);  // 释放当前页表页的内存
}
```

这个函数主要教我们如何遍历所有PTE。容易想到写一个递归函数来逐层输出：

```c
#ifdef LAB_PGTBL
void vmprint_recursive(pagetable_t pagetable, int level, uint64 vaddr) {
  static char *indent = "  ..";

  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if (pte & PTE_V) {
      uint64 curr_vaddr = vaddr | ((uint64) i << (12 + 9 * (2 - level)));
      for (int j = 0; j < 2 - level; j++) {
        printf("%s", indent);
      }
      printf("0x%lx: pte %lx pa %lx\n", curr_vaddr, pte, PTE2PA(pte));
      if ((pte & (PTE_R|PTE_W|PTE_X)) == 0) {
        vmprint_recursive((pagetable_t)PTE2PA(pte), level-1, curr_vaddr);
      }
    }
  }
}

void
vmprint(pagetable_t pagetable) {
  // your code here
  printf("page table: %p\n", pagetable);
  vmprint_recursive(pagetable, 2, 0);
}
#endif
```

代码的核心部分是虚拟地址的计算：

```c
uint64 curr_vaddr = vaddr | ((uint64) i << (12 + 9 * (2 - level)));
```

根据前面所学，虚拟地址分为四级：

- L2索引（9位）：对应虚拟地址的位38-30（最高级页表）；

- L1索引（9位）：对应虚拟地址的位29-21（中间级页表）；

- L0索引（9位）：对应虚拟地址的位20-12（最低级页表）；

- 页内偏移（12位）：对应虚拟地址的位11-0。

根据层级`level`确定索引`i`在虚拟地址中的位置，`(12 + 9 * (2 - level))`为左移位数。然后将索引的位域合并到基础地址`vaddr`中，生成下一级页表或页的虚拟地址。

宏定义`#define PTE2PA(pte) (((pte) >> 10) << 12)`会将页表项转化为物理地址PA。先右移10位，去除PTE的低10位标志位，然后左移12位，将剩余的44位物理页号转换为56位物理地址。

测试：

```bash
print_kpgtbl starting
page table: 0x0000000087f22000
0x0: pte 21fc7801 pa 87f1e000
  ..0x0: pte 21fc7401 pa 87f1d000
  ..  ..0x0: pte 21fc7c5b pa 87f1f000
  ..  ..0x40000000: pte 21fc7017 pa 87f1c000
  ..  ..0x80000000: pte 21fc6c07 pa 87f1b000
  ..  ..0xc0000000: pte 21fc68d7 pa 87f1a000
0xff000: pte 21fc8401 pa 87f21000
  ..0x3feff000: pte 21fc8001 pa 87f20000
  ..  ..0x7f7feff000: pte 21fd4413 pa 87f51000
  ..  ..0x7fbfeff000: pte 21fd00c7 pa 87f40000
  ..  ..0x7fffeff000: pte 2000184b pa 80006000
print_kpgtbl: OK
```

然后

> For every leaf page in the vmprint output, explain what it logically contains and what its permission bits are, and how it relates to the output of the earlier print_pgtbl() exercise above. Figure 3.4 in the xv6 book might be helpful, although note that the figure might have a slightly different set of pages than the process that's being inspected here. 
> 对于 vmprint 输出中的每个叶子页，解释其逻辑上包含的内容及其权限位，并说明它与前面 print_pgtbl() 练习输出的关联。xv6 书籍中的图 3.4 可能有帮助，但需注意图中页面的设置可能与当前被检查的进程略有不同。

`print_pgtbl`展示虚拟地址范围到物理地址的映射关系，`vmprint`展示页表树形结构和权限细节，二者可结合展示地址转换是否正确。

## Use superpages ([moderate](https://pdos.csail.mit.edu/6.S081/2024/labs/guidance.html))/([hard](https://pdos.csail.mit.edu/6.S081/2024/labs/guidance.html))

> The RISC-V paging hardware supports two-megabyte pages as well as ordinary 4096-byte pages. The general idea of larger pages is called superpages, and (since RISC-V supports more than one size) 2M pages are called megapages. The operating system creates a superpage by setting the PTE_V and PTE_R bits in the level-1 PTE, and setting the physical page number to point to the start of a two-megabyte region of physical memory. This physical address must be two-mega-byte aligned (i.e., a multiple of two megabytes). You can read about this in the RISC-V privileged manual by searching for megapage and superpage; in particular, the top of page 112. Use of superpages decreases the amount of physical memory used by the page table, and can decrease misses in the TLB cache. For some programs this leads to large increases in performance. 
> RISC-V 的分页硬件支持两兆字节的大页以及普通的 4096 字节页。这种较大页面的通用概念被称为超级页（superpages），而（由于 RISC-V 支持多种尺寸）2M 页被称为兆页（megapages）。操作系统通过在级别 1 的页表项（PTE）中设置 PTE_V 和 PTE_R 位，并将物理页号指向一个两兆字节物理内存区域的起始地址来创建超级页。该物理地址必须是两兆字节对齐的（即两兆字节的倍数）。您可以在 RISC-V 特权手册中通过搜索 megapage 和 superpage 来了解更多信息；特别是第 112 页的顶部。使用超级页可以减少页表占用的物理内存量，并可能降低 TLB 缓存的未命中率。对于某些程序来说，这会显著提高性能。
> 
> Your job is to modify the xv6 kernel to use superpages. In particular, if a user program calls sbrk() with a size of 2 megabytes or more, and the newly created address range includes one or more areas that are two-megabyte-aligned and at least two megabytes in size, the kernel should use a single superpage (instead of hundreds of ordinary pages). You will receive full credit for this part of the lab if the superpg_test test case passes when running pgtbltest. 
> 你的任务是修改 xv6 内核以使用超级页。具体来说，如果用户程序调用 sbrk()时请求的大小为 2MB 或更大，且新创建的地址范围包含一个或多个 2MB 对齐且至少 2MB 大小的区域，内核应使用单个超级页（而非数百个普通页）。当运行 pgtbltest 时，若 superpg_test 测试用例通过，你将获得这部分实验的满分。

根据提示先阅读`user/pgtbltest.c`中的`superpg_test`函数：

```c
void superpg_test()
{
  int pid;

  // 测试开始标记
  printf("superpg_test starting\n");
  testname = "superpg_test"; // 设置当前测试名称

  // 通过sbrk扩展进程堆空间（分配8MB内存）
  char *end = sbrk(N); // N = 8 * 1024 * 1024
  if (end == 0 || end == (char*)0xffffffffffffffff)
    err("sbrk failed"); // 内存分配失败处理

  // 将地址对齐到超级页边界（假设 SUPERPGROUNDUP 是 2MB 对齐）
  uint64 s = SUPERPGROUNDUP((uint64) end);

  // 第一阶段：父进程内存验证
  supercheck(s); // 检查超级页属性

  // 创建子进程验证继承的页表
  if((pid = fork()) < 0) {
    err("fork");
  } else if(pid == 0) { // 子进程
    supercheck(s);      // 验证相同内存区域
    exit(0);
  } else {              // 父进程
    int status;
    wait(&status);
    if (status != 0) {  // 子进程验证失败
      exit(0);
    }
  }
  printf("superpg_test: OK\n");  
}
```

`sbrk`系统调用触发`kernel/sysproc.c`中的`sys_sbrk`函数：

```c
uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);             // 从用户空间获取整数参数n
  addr = myproc()->sz;       // 获取当前进程的堆顶地址（旧break）
  if(growproc(n) < 0)        // 调用内存增长函数
    return -1;               // 内存分配失败时返回-1
  return addr;               // 成功时返回旧break地址
}
```

然后进入`growproc`函数：

```c
int growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();  // 获取当前进程

  sz = p->sz;                 // 当前进程内存大小

  if(n > 0){
    // 扩展内存：参数包含 PTE_W 表示新内存可写
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;              // 内存不足时失败
    }
  } else if(n < 0){
    // 收缩内存：计算新的边界
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }

  p->sz = sz;                 // 更新进程内存大小
  return 0;
}
```

其中`vm.c`的`uvmalloc`函数用于扩展用户进程的内存空间、分配物理内存并建立页表映射：

```c
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  int sz;

  if(newsz < oldsz)          // 如果新尺寸更小，直接返回原尺寸
    return oldsz;

  oldsz = PGROUNDUP(oldsz);  // 将旧尺寸对齐到页边界
  for(a = oldsz; a < newsz; a += sz){ // 逐页扩展内存
    sz = PGSIZE;             // 每次处理一个页面
    mem = kalloc();          // 分配物理页
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz); // 分配失败时回滚
      return 0;
    }
#ifndef LAB_SYSCALL          // 非系统调用实验时清零页面
    memset(mem, 0, sz);
#endif
    // 建立页表映射，权限=用户可读+用户权限位+额外权限
    if(mappages(pagetable, a, sz, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);            // 映射失败时释放内存
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;              // 返回新的内存尺寸
}
```

总体调用路径大致如下：

```tex
用户程序调用 sbrk()
→ sys_sbrk() 
→ growproc()
→ uvmalloc()/uvmdealloc()
→ mappages()/uvmunmap()
```

我们根据提示，先修改`kalloc.c`。读一下`kalloc.c`的代码：

```c
// 全局内存管理结构
struct {
  struct spinlock lock;   // 保护空闲链表的自旋锁
  struct run *freelist;   // 空闲页链表头指针
  struct run *super_freelist;  // 新增大页空闲链表
} kmem;
```

这里需要新增大页空闲链表。

然后在`kinit`中预留几块2MB的物理内存区域，我这里预留了两个2MB大页：

```c
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);

  // Superalloc
  char *p = (char*)PGROUNDUP((uint64)end);
  for (int i = 0; i < 2; i++) {
    if ((uint64)p + 2*1024*1024 <= PHYSTOP) {
      superfree(p);
      p += 2*1024*1024;
    }
  }
}
```

修改`freerange`函数，跳过作为大页分配的区域，避免重复释放：

```c
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  if ((uint64)p % (2*1024*1024) != 0) { // 加一个条件判断
    kfree(p);
  }
}
```

并创建`superalloc()`和`superfree()`函数来管理这些2MB的物理内存块。

```c
void *
superalloc() {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.super_freelist;
  if(r)
    kmem.super_freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE*512); // fill with junk
  return (void*)r;
}

void
superfree(void *pa) {
  struct run *r;

  if (((uint64)pa % (PGSIZE*512)) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
    panic("superfree");
  }

  memset(pa, 1, PGSIZE*512);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.super_freelist;
  kmem.super_freelist = r;
  release(&kmem.lock);
}
```

然后修改列表遍历，先在`riscv.h`中添加`#define PTE_S (1L << 9)`，然后在`vm.c`中修改`walk`函数：

```c
// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      if (level == 1 && (*pte & PTE_S)) { // 检查是否是大页标记
        return pte;  // 返回大页pte
      }
      pagetable = (pagetable_t)PTE2PA(*pte);
#ifdef LAB_PGTBL
      if(PTE_LEAF(*pte)) {
        return pte;
      }
#endif
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}
```

接下来，同样根据提示，`uvmcopy`和`uvmunmap`需要处理超级页。`uvmcopy`在复制父进程页表时，如果遇到超级页需要整个复制，`uvmunmap`在解除映射时，需要识别超级页并正确释放物理内存。

```c

```

最后修改`uvmalloc`函数。检查是否有足够的连续2MB对齐的虚拟地址空间，并尝试分配超级页：

```c
// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  int sz;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);

  if ((newsz - oldsz) >= (2 << 20) && (oldsz % (2 << 20)) == 0) {
    void *mem = superalloc();
    if (mem) {
      uint64 a = PGROUNDUP(oldsz);
      if(mappages(pagetable, a, 2 << 20, (uint64)mem, 
                 PTE_R | PTE_U | xperm | PTE_V | PTE_S) == 0) {
        return a + (2 << 20);
      }
      superfree(mem);
    }
  }

  for(a = oldsz; a < newsz; a += sz){
    sz = PGSIZE;
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
#ifndef LAB_SYSCALL
    memset(mem, 0, sz);
#endif
    if(mappages(pagetable, a, sz, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}
```




























