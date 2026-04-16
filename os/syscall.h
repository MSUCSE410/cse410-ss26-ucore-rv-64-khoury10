#ifndef SYSCALL_H
#define SYSCALL_H

#include "proc.h"

int sys_task_info(uint64 va);
void syscall();

#endif // SYSCALL_H
