#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
if (q == NULL) return 1;
return (q->size == 0);
}




/* TODO: put a new process to queue [q] */
void enqueue(struct queue_t * q, struct pcb_t * proc){
if(q == NULL || proc == NULL) return;
if(q->size >= MAX_QUEUE_SIZE) return;

q->proc[q->size] = proc;
++q->size;
}




/* TODO: return a pcb whose prioprity is the highest
* in the queue [q] and remember to remove it from q
* */
struct pcb_t * dequeue(struct queue_t * q) {
if(empty(q)) return NULL;

int queue_size = q->size;
int idx = 0;
uint32_t temp = q->proc[0]->priority;
uint32_t current_prio;

for(int a = 1; a < queue_size; ++a){
if(q->proc[a] == NULL) continue;

//check prio highest (which value is lowest)
current_prio = q->proc[a]->priority;
if(current_prio < temp){
temp = current_prio;
idx = a;
}
}


// save
struct pcb_t* return_pcb_t_pointer = q->proc[idx];

// sort queue (overwrite idx value)
for(int a = idx + 1; a < queue_size; ++a){
q->proc[a - 1] = q->proc[a];
}
q->proc[queue_size - 1] = NULL;
--(q->size);

return return_pcb_t_pointer;
}

