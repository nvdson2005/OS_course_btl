#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
  if (q == NULL)
    return 1;
  return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
  /* TODO: put a new process to queue [q] */
  //null condition
  if (q == NULL || proc == NULL)
    return;

  //if queue is full, do nothing 
  if (q->size == MAX_QUEUE_SIZE)
  {
    return;
  }

  //as priority queue, find the correct position to insert the process based on prio  
  int i = q->size - 1;
  //for debugging purpose
  // printf("Enqueuing process with prio = %d\n", proc->prio); // Debugging
  while (i >= 0 && (q->proc[i]->prio  >  proc->prio))
  {
    q->proc[i + 1] = q->proc[i];
    i--;
  }

  //insert at the correct position 
  q->proc[i + 1] = proc;
  q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
  /* TODO: return a pcb whose prioprity is the highest
   * in the queue [q] and remember to remove it from q
   * */
  // return null if queue is empty or there is no process in the queue
  if (q == NULL || q->size == 0)
    return NULL;

  //get the process with the highest priority
  struct pcb_t* maxPriorPcb = q->proc[0];
  //for debugging purpose
  // printf("Dequeuing process with prio = %d\n", maxPriorPcb->prio); // Debuggingg
  //remove the process from the queue
  for (int i = 0; i < q->size - 1; i++)
  {
    q->proc[i] = q->proc[i + 1];
  }
  //set the last element to null
  q->proc[q->size - 1] = NULL;
  //decrease the size of the queue
  q->size--;
  return maxPriorPcb;
}
