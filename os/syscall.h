#ifndef SYSCALL_H
#define SYSCALL_H

#include "proc.h"

int sys_task_info(TaskInfo *ti);
void syscall();

#endif // SYSCALL_H
