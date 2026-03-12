#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "vm.h"

#define MAX_MMAP_SIZE (1024 * 1024 * 1024)

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

// modified this
uint64 sys_gettimeofday(uint64 va, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	struct proc *p = curr_proc();
    
    uint64 pa = useraddr(p->pagetable, va);
    if (pa == 0) {
        return -1; // Invalid address or not mapped
    }


	TimeVal *val = (TimeVal *)pa;

	uint64 cycle = get_cycle();
	val->sec = cycle / CPU_FREQ;
	val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}


//created thus
uint64 sys_getpid() {return curr_proc()->pid;}

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/

//modified this
int sys_task_info(uint64 va) {
	struct proc *p = curr_proc();

    uint64 pa = useraddr(p->pagetable, va);
    if (pa == 0) {return -1;}

    TaskInfo *ti = (TaskInfo *)pa;

    ti->status = p->info->status;

    // int size = sizeof(curr_proc()->info->syscall_times) / sizeof(curr_proc()->info->syscall_times[0]);
    for(int i = 0; i < MAX_SYSCALL_NUM; i++){
        ti->syscall_times[i] = p->info->syscall_times[i];
    }

    // uint64 cycle = get_cycle();
    // ti->time = (cycle % CPU_FREQ) * 1000 / CPU_FREQ;
    uint64 now = get_cycle() / (CPU_FREQ / 1000);
    ti->time = now - p->info->time; 

    return 0;
}

//made this
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd) {
    if (len == 0) return 0;
    if (len > MAX_MMAP_SIZE) return -1;
    if (start % PGSIZE != 0) return -1;
    if ((port & ~0x7) != 0) return -1; 
    if ((port & 0x7) == 0) return -1;

    struct proc *p = curr_proc();
    uint64 aligned_len = PGROUNDUP(len);
    
    for (uint64 addr = start; addr < start + aligned_len; addr += PGSIZE) {
        if (useraddr(p->pagetable, addr) != 0) {
            return -1;
        }
    }

    int perm = PTE_U;
    if (port & 1) perm |= PTE_R;
    if (port & 2) perm |= PTE_W;
    if (port & 4) perm |= PTE_X;

    for (uint64 addr = start; addr < start + aligned_len; addr += PGSIZE) {
        void *pa = kalloc();
        if (pa == 0) {return -1; }
        memset(pa, 0, PGSIZE);

        if (mappages(p->pagetable, addr, PGSIZE, (uint64)pa, perm) != 0) {
            kfree(pa);
            return -1;
        }
    }

    return 0;
}

//made this func
uint64 sys_munmap(uint64 start, uint64 len) {
    if (start % PGSIZE != 0) return -1;

    struct proc *p = curr_proc();
    uint64 aligned_len = PGROUNDUP(len);

    for (uint64 addr = start; addr < start + aligned_len; addr += PGSIZE) {
        if (useraddr(p->pagetable, addr) == 0) {
            return -1;
        }
    }

    uvmunmap(p->pagetable, start, aligned_len / PGSIZE, 1);

    return 0;
}

extern char trap_page[];

//added cases
void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	/*
    * LAB1: you may need to update syscall counter for task info here
    */
    if (id > 0 && id < MAX_SYSCALL_NUM) {
        curr_proc()->info->syscall_times[id]++;
    }
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
		break;
	case SYS_getpid:
        ret = sys_getpid();
        break;
	/*
    * LAB1: you may need to add SYS_taskinfo case here
    */
    case SYS_task_info:
        ret = sys_task_info(args[0]);
        break;
    case SYS_mmap:
        ret = sys_mmap(args[0], args[1], (int)args[2], (int)args[3], (int)args[4]);
        break;
    case SYS_munmap:
        ret = sys_munmap(args[0], args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
