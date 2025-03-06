/*Destroy a lock: int ldelete (int lockdescriptor) – 
Deletes the lock identified by the descriptor lockdescriptor. 
(see “Lock Deletion” below)*/

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>


int ldelete(int lockdescriptor) {
    STATWORD ps; 

    disable(ps);
    struct lentry *lptr;

    kprintf("start executing ldelete \n");

    if (isbadlock(lockdescriptor) || locktab[lockdescriptor].lstate==LFREE) { // need to clarify what LFREE actually means
		restore(ps);
		return(SYSERR);
	}

    int lock;
    int pid;
    lptr = &locktab[lockdescriptor];
    lptr->lstate = LDELETED;
    lptr->curr_mask = 0; 
    lptr->rqtail = 1 + (lptr->rqhead = newqueue());
	lptr->wqtail = 1 + (lptr->wqhead = newqueue());

    // remove processes from lock's waiting write queue
    if (nonempty(lptr->wqhead)) {
        while ((pid = getfirst(lptr->wqhead)) != EMPTY ) {
            {
                proctab[pid].plockret = DELETED;
                ready(pid,RESCHNO);
            }
            resched();
        }
    }
    // remove processes from lock's waiting read queue 
    if (nonempty(lptr->rqhead)) {
        while ((pid = getfirst(lptr->rqhead)) != EMPTY ) {
            {
                proctab[pid].plockret = DELETED;
                ready(pid,RESCHNO);
            }
            resched();
        }
    }

    // remove lock from processes holding the lock 
    int i; 
    for (i = 0; i < 50; i++) { 
        if (lptr->curr_mask & (1LL << i)) {  // Check if ith lock exists
            printf("%d ", i);

            // remove lock from proctab[i]->locks
            proctab[i].locks &= ~(1LL << lockdescriptor);  // Clear the lockdescriptor bit

        }
    }

    restore(ps);
    kprintf("finish executing ldelete \n");
    return(OK);
}