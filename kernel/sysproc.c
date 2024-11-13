#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

// 全局变量，存储 trace 掩码
int trace_mask = 0;

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  if (argaddr(0, &p) < 0) return -1;
  return wait(p);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}

uint64 sys_trace(void) {
  int mask;
  if(argint(0, &mask) < 0)
    return -1;
  trace_mask = mask;  // 设置全局的 trace 掩码
  return 0;
}

// 系统调用函数
uint64 sys_sysinfo(void) {
    struct sysinfo info;
    struct proc *p = myproc();
    uint64 addr;  // 用户空间地址

    if (argaddr(0, &addr) < 0)  // 获取用户传入的参数
        return -1;

    // 获取空闲内存量
    info.freemem = kfreemem();  // 需要自己实现kfreemem获取内存
    info.nproc = get_unused_procs();  // 获取UNUSED状态的进程数，需要实现该函数

    // 将结构体信息复制到用户空间
    if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
        return -1;

    return 0;
}

uint64 sys_wait_sched(void) {
  struct proc *np = myproc();
  int runnable_time, running_time, sleep_time;
  int pid;

  // 分别获取三个用户空间地址参数
  uint64 runnable_addr, running_addr, sleep_addr;
  if (argaddr(0, &runnable_addr) < 0 || argaddr(1, &running_addr) < 0 || argaddr(2, &sleep_addr) < 0) {
    return -1;
  }

  // 调用 wait_sched 获取子进程的调度信息
  pid = wait_sched(&runnable_time, &running_time, &sleep_time);

  if (pid == -1) {
    return -1;
  }

  // 将每个时间信息分别复制到用户空间
  if (copyout(np->pagetable, runnable_addr, (char *)&runnable_time, sizeof(int)) < 0 ||
      copyout(np->pagetable, running_addr, (char *)&running_time, sizeof(int)) < 0 ||
      copyout(np->pagetable, sleep_addr, (char *)&sleep_time, sizeof(int)) < 0) {
    return -1;
  }

  return pid;
}



// 设置进程优先级的系统调用
uint64 sys_set_priority(void) {
    int priority, pid;

    // 从用户空间获取参数
    if (argint(0, &priority) < 0 || argint(1, &pid) < 0) {
        return -1; // 获取参数失败
    }

    // 调用 set_priority 函数来设置进程优先级
    return set_priority(priority, pid);
}
