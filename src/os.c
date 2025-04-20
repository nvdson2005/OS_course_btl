
#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mm.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int time_slot;
static int num_cpus;
static int done = 0;

#ifdef MM_PAGING
static int memramsz;
static int memswpsz[PAGING_MAX_MMSWP];
/////////////////////START//////////////////////
static struct mm_struct * mm_list[256]; // for mem cleanup
//////////////////////END///////////////////////

struct mmpaging_ld_args {
	/* A dispatched argument struct to compact many-fields passing to loader */
	int vmemsz;
	struct memphy_struct *mram;
	struct memphy_struct **mswp;
	struct memphy_struct *active_mswp;
	int active_mswp_id;
	struct timer_id_t  *timer_id;
};
#endif

static struct ld_args{
	char ** path;
	unsigned long * start_time;
#ifdef MLQ_SCHED
	unsigned long * prio;
#endif
} ld_processes;
int num_processes;

struct cpu_args {
	struct timer_id_t * timer_id;
	int id;
};




static void * cpu_routine(void * args) {
	struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
	int id = ((struct cpu_args*)args)->id;
	/* Check for new process in ready queue */
	int time_left = 0;
	struct pcb_t * proc = NULL;
	while (1) {
		/* Check the status of current process */
		if (proc == NULL) {
			/* No process is running, the we load new process from
		 	* ready queue */
			proc = get_proc();
			if (proc == NULL) {
                           next_slot(timer_id);
                           continue; /* First load failed. skip dummy load */
                        }
		}else if (proc->pc == proc->code->size) {
			/* The porcess has finish it job */
			printf("\tCPU %d: Processed %2d has finished\n", id ,proc->pid);
/////////////////////START//////////////////////
			free(proc->page_table);
			free(proc->code->text);
			free(proc->code);
//////////////////////END///////////////////////
			free(proc);
			proc = get_proc();
			time_left = 0;
		}else if (time_left == 0) {
			/* The process has done its job in current time slot */
			printf("\tCPU %d: Put process %2d to run queue\n",
				id, proc->pid);
			put_proc(proc);
			proc = get_proc();
		}
		
		/* Recheck process status after loading new process */
		if (proc == NULL && done) {
			/* No process to run, exit */
			printf("\tCPU %d stopped\n", id);
			break;
		}else if (proc == NULL) {
			/* There may be new processes to run in
			 * next time slots, just skip current slot */
			next_slot(timer_id);
			continue;
		}else if (time_left == 0) {
			printf("\tCPU %d: Dispatched process %2d\n",
				id, proc->pid);
			time_left = time_slot;
		}
		
		/* Run current process */
		run(proc);
		time_left--;
		next_slot(timer_id);
	}
	detach_event(timer_id);
	pthread_exit(NULL);
}




static void * ld_routine(void * args){
#ifdef MM_PAGING
	struct memphy_struct* mram = ((struct mmpaging_ld_args *)args)->mram;
	struct memphy_struct** mswp = ((struct mmpaging_ld_args *)args)->mswp;
	struct memphy_struct* active_mswp = ((struct mmpaging_ld_args *)args)->active_mswp;
	struct timer_id_t * timer_id = ((struct mmpaging_ld_args *)args)->timer_id;
#else
	struct timer_id_t * timer_id = (struct timer_id_t*)args;
#endif
	int i = 0;
	printf("ld_routine\n");

	while (i < num_processes) {
		struct pcb_t * proc = load(ld_processes.path[i]);
#ifdef MLQ_SCHED
		proc->prio = ld_processes.prio[i];
#endif
		while (current_time() < ld_processes.start_time[i]) {
			next_slot(timer_id);
		}
#ifdef MM_PAGING
		proc->mm = malloc(sizeof(struct mm_struct));
		init_mm(proc->mm, proc);
/////////////////////START//////////////////////
		mm_list[i] = proc->mm; // for mem cleanup (global var)
//////////////////////END///////////////////////
		proc->mram = mram;
		proc->mswp = mswp;
		proc->active_mswp = active_mswp;
#endif
		printf("\tLoaded a process at %s, PID: %d PRIO: %ld\n", ld_processes.path[i], proc->pid, ld_processes.prio[i]);
		add_proc(proc);
		free(ld_processes.path[i]);
		i++;
		next_slot(timer_id);
	}
	// clean up
	free(ld_processes.path);
	free(ld_processes.start_time);
	done = 1;
	detach_event(timer_id);
	pthread_exit(NULL);
}




static void read_config(const char * path) {
	FILE * file;
	if ((file = fopen(path, "r")) == NULL) {
		printf("Cannot find configure file at %s\n", path);
		exit(1);
	}
	fscanf(file, "%d %d %d\n", &time_slot, &num_cpus, &num_processes);
	// printf("Time slot: %d, Number of CPUs: %d, Number of Processes: %d\n", time_slot, num_cpus, num_processes);
	// /* Allocate memory for process list */
	ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);
	ld_processes.start_time = (unsigned long*)malloc(sizeof(unsigned long) * num_processes);
#ifdef MM_PAGING
	int sit;
#ifdef MM_FIXED_MEMSZ
// We provide here a back compatible with legacy OS simulatiom config file. In which, it have no addition config line for Mema, keep only one line for legacy info
// [time slice] [N = Number of CPU] [M = Number of Processes to be run]

        memramsz = 0x100000;
        memswpsz[0] = 0x1000000;
	for(sit = 1; sit < PAGING_MAX_MMSWP; sit++) memswpsz[sit] = 0;
#else
/////////////////////START//////////////////////
#ifdef SCHED_TEST
	long int savePos = ftell(file);
#endif
//////////////////////END///////////////////////

	/* Read input config of memory size: MEMRAM and upto 4 MEMSWP (mem swap)
	 * Format: (size=0 result non-used memswap, must have RAM and at least 1 SWAP)
	 *        MEM_RAM_SZ MEM_SWP0_SZ MEM_SWP1_SZ MEM_SWP2_SZ MEM_SWP3_SZ
	*/
	fscanf(file, "%d\n", &memramsz);
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++) fscanf(file, "%d", &(memswpsz[sit]));
	fscanf(file, "\n"); /* Final character */
#endif
#endif
/////////////////////START//////////////////////
#ifdef SCHED_TEST 
	fseek(file, savePos, SEEK_SET);
#endif
//////////////////////END///////////////////////
#ifdef MLQ_SCHED
	ld_processes.prio = (unsigned long*)malloc(sizeof(unsigned long) * num_processes);
#endif
	int i;
	for (i = 0; i < num_processes; i++) {
		ld_processes.path[i] = (char*)malloc(sizeof(char) * 100);
		ld_processes.path[i][0] = '\0';
		strcat(ld_processes.path[i], "input/proc/");
		char proc[100];
#ifdef MLQ_SCHED
		fscanf(file, "%lu %s %lu\n", &ld_processes.start_time[i], proc, &ld_processes.prio[i]);
		// printf("%lu %s %lu\n", ld_processes.start_time[i], proc, ld_processes.prio[i]);
#else
		fscanf(file, "%lu %s\n", &ld_processes.start_time[i], proc);
#endif
		strcat(ld_processes.path[i], proc);
/////////////////////START//////////////////////
		printf("Process %d: %s\n", i, ld_processes.path[i]);
//////////////////////END///////////////////////
	}
}




int main(int argc, char * argv[]) {
	/* Read config */
	if (argc != 2) {
		printf("Usage: os [path to configure file]\n");
		return 1;
	}
	char path[100];
	path[0] = '\0';
	strcat(path, "input/");
	strcat(path, argv[1]);
	read_config(path);

	pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	struct cpu_args * args = (struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
	pthread_t ld;
	
	/* Init timer */
	int i;
	for (i = 0; i < num_cpus; i++) {
		args[i].timer_id = attach_event();
		args[i].id = i;
	}
	struct timer_id_t * ld_event = attach_event();
	start_timer();

#ifdef MM_PAGING
	/* Init all MEMPHY include 1 MEMRAM and n of MEMSWP */
	int rdmflag = 1; /* By default memphy is RANDOM ACCESS MEMORY */

	struct memphy_struct mram;
	struct memphy_struct mswp[PAGING_MAX_MMSWP];
/////////////////////START//////////////////////
	for(int a = 0; a < 256; ++a) mm_list[a] = NULL;
//////////////////////END///////////////////////

	/* Create MEM RAM */
	init_memphy(&mram, memramsz, rdmflag);

	/* Create all MEM SWAP */ 
	int sit;
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++) init_memphy(&mswp[sit], memswpsz[sit], rdmflag);

	/* In Paging mode, it needs passing the system mem to each PCB through loader*/
	struct mmpaging_ld_args *mm_ld_args = malloc(sizeof(struct mmpaging_ld_args));

	mm_ld_args->timer_id = ld_event;
	mm_ld_args->mram = (struct memphy_struct *) &mram;
	mm_ld_args->mswp = (struct memphy_struct**) &mswp;
	mm_ld_args->active_mswp = (struct memphy_struct *) &mswp[0];
	mm_ld_args->active_mswp_id = 0;
#endif
	/* Init scheduler */
	init_scheduler();

	/* Run CPU and loader */
#ifdef MM_PAGING
	pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);
#else
	pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif
	for (i = 0; i < num_cpus; i++) {
		pthread_create(&cpu[i], NULL, cpu_routine, (void*)&args[i]);
	}

	/* Wait for CPU and loader finishing */
	for (i = 0; i < num_cpus; i++) {
		pthread_join(cpu[i], NULL);
	}
	pthread_join(ld, NULL);

	stop_timer();

// clean up mess
/////////////////////START//////////////////////
free(cpu);
free(args);
free(mm_ld_args);
#ifdef MM_PAGING

// cleanup mram
free(mram.storage);

struct framephy_struct* frame = mram.free_fp_list;
struct framephy_struct* frame_temp = NULL;
for(;frame != NULL;){
frame_temp = frame->fp_next;
free(frame);
frame = frame_temp;
}

// cleanup swap ram
for(int a = 0; a < PAGING_MAX_MMSWP; ++a){
free(mswp[a].storage);

frame = mswp[a].free_fp_list;
frame_temp = NULL;
for(;frame != NULL;){
frame_temp = frame->fp_next;
free(frame);
frame = frame_temp;
}

}

// the global var is mm_list, also this is clean up mm_struct of each proc
for(int a = 0; a < 256; ++a){
if(mm_list[a] == NULL) continue;


if(mm_list[a]->pgd != NULL) free(mm_list[a]->pgd);


// free mmap
struct vm_area_struct* temp_vm_area = mm_list[a]->mmap;
struct vm_area_struct* temp_vm_area_next = NULL;
for(;temp_vm_area != NULL;){

// free vm_freerg_list
struct vm_rg_struct* temp_vg_str = temp_vm_area->vm_freerg_list;
struct vm_rg_struct* temp_vg_str_next = NULL;
for(;temp_vg_str != NULL;){
temp_vg_str_next = temp_vg_str->rg_next;
free(temp_vg_str);
temp_vg_str = temp_vg_str_next;
}

// free vm_area list (mmap)
temp_vm_area_next = temp_vm_area->vm_next;
free(temp_vm_area);
temp_vm_area = temp_vm_area_next;
}

// free fifo_pgn
struct pgn_t* pgn = mm_list[a]->fifo_pgn;
struct pgn_t* pgn_next = NULL;
for(;pgn != NULL;){
pgn_next = pgn->pg_next;
free(pgn);
pgn = pgn_next;
}

// finally free the memory management
free(mm_list[a]);
}
#endif
//////////////////////END///////////////////////
	return 0;

}



