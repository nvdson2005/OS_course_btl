/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "string.h"

/* extern */ struct pcb_t* _proc_list[MAX_PROC]; // danh sách các process đang tồn tại

int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    char proc_name[100];
    uint32_t data;

    //hardcode for demo only
    uint32_t memrg = regs->a1;
    
    /* TODO: Get name of the target proc */
    //proc_name = libread..
    // Đọc chuỗi từ memory
    int i = 0;
    while (i < sizeof(proc_name) - 1) {
        if (libread(caller, memrg, i, &data) != 0 || (char)data == '\0') break;
        proc_name[i] = (char)data;
        i++;
    }
    proc_name[i] = '\0';
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    /* TODO: Traverse proclist to terminate the proc
     *       stcmp to check the process match proc_name
     */
    //caller->running_list
    //caller->mlq_ready_queu

    /* TODO Maching and terminating 
     *       all processes with given
     *        name in var proc_name
     */

    // Duyệt qua danh sách process để "kill"
    int killed = 0;
    for (int i = 0; i < MAX_PROC; ++i) {
        if (_proc_list[i] != NULL && strcmp(_proc_list[i]->path, proc_name) == 0) {
            printf("Killing process PID %d with name \"%s\"\n", _proc_list[i]->pid, _proc_list[i]->path);
            _proc_list[i]->pc = -1;  // Đặt program counter để tiến trình kết thúc ở vòng lặp tiếp theo
            killed++;
        }
    }

    if (killed == 0) {
        printf("No process matched the name \"%s\"\n", proc_name);
    } else {
        printf("Total %d processes killed.\n", killed);
    }

    return 0;
}
