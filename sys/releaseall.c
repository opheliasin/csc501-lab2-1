/* Simultaneous release of multiple locks: int releaseall (int numlocks, int ldes1,…, int ldesN) */

/* also need to wake up processes */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

/*------------------------------------------------------------------------
 * releaseall  --  signal a lock, releasing waiting process(es)
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000; 

int releaseall(int numlocks, long ldes) 
{	
	STATWORD ps;    
	disable(ps);
	// kprintf("starting releaseall \n");
	struct	lentry	*lptr;
	struct pentry *pptr;
	int return_state = SYSERR; // if lock in argument isn't held by calling process – return(SYSERR)

	unsigned long *a; /* points to list of args	*/

    a = (unsigned long *)(&ldes) + (numlocks-1); /* last argument*/

	int i;
	int lock;
	
	// if there aren't any locks stored in the process's linked list then we don't have to run more checks
	if ((&proctab[currpid])->locks == 0) {
		return(return_state);
	}

	int wptr;
	int rptr;
	int longestwait;
	int longestwaitproc;
	int wmaxpprio;
	int waitmaxpprio;

	// for each lock, check to see if lock is present in the process's queue
	for (i = 0; i < numlocks; i++)	{
		
		lock = *a--;

		// while the queue is not empty and (lock isn't in state LFREE or LDELETED)
		
		// we check to see if we can find the lock in the queue
		if ((pptr = &proctab[currpid])->locks & (1LL << lock)) {
			return_state = OK;
		} 

		// if it's SYSERR then we don't have to release the lock
		if (return_state == OK) {
			
			lptr = &locktab[lock];

			// kprintf("curr_pid: %ld \n", currpid);

			// kprintf("Initial process mask: 0x%lx%08lx\n", 
			// 	(unsigned long)(lptr->curr_mask >> 32), 
			// 	(unsigned long)(lptr->curr_mask & 0xFFFFFFFF));

			//remove process from the lock's linked list
			lptr->curr_mask &= ~(1LL << currpid);

			// kprintf("Later process mask: 0x%lx%08lx\n", 
			// 	(unsigned long)(lptr->curr_mask >> 32), 
			// 	(unsigned long)(lptr->curr_mask & 0xFFFFFFFF));

			
			// kprintf("Before lock mask: 0x%lx%08lx\n", 
			// 	(unsigned long)(pptr->locks >> 32), 
			// 	(unsigned long)(pptr->locks & 0xFFFFFFFF));
			// remove lock from process's linked list
			pptr->locks &= ~(1LL << lock);

			// kprintf("After lock mask: 0x%lx%08lx\n", 
			// 	(unsigned int)(pptr->locks >> 32), 
			// 	(unsigned int)(pptr->locks & 0xFFFFFFFF));


			if (lptr->curr_mask == 0) { // if no process is holding the lock -> change state to LAVAIL
				lptr->lstate = LAVAIL;
			}
			// Check state of lock 
			if (lptr->lstate == LAVAIL && ((wptr = (&q[lptr->wqtail])->qprev) != lptr->wqhead || (rptr = (&q[lptr->rqtail])->qprev) != lptr->rqhead )) {  
				// if no process is holding the lock and there are locks waiting
				int wpprio; // waiting priority of waiting write queue head
				int rpprio; // waiting priority of waiting read queue head

				if ((&q[wptr])->qkey != MININT) {
					wpprio = (&q[wptr])->qkey;
				} else {
					wpprio = 0;
				}

				if ((&q[rptr])->qkey != MININT) {
					rpprio = (&q[rptr])->qkey;
				} else {
					rpprio = 0;
				}

				//kprintf("wpprio:  %d, rpprio: %d\n", wpprio, rpprio);
				// check to see whether READ or WRITE has higher priority 
				// if writer priority higher than reader priority 
				if (wpprio >= rpprio) { //TODO: figure out syntax for this line
					longestwait = (&proctab[wptr])->pwait_time; //get wait time for queue 
					longestwaitproc = wptr;

					//kprintf("longestwait: %d, longestwaitproc: %d\n", longestwait, longestwaitproc);

					while (q[wptr].qkey == wpprio && wptr != MAXINT && wptr != MININT) { // check if there are multiple writers with same priority - if so, run the one longest waiting time 
						if (proctab[wptr].pwait_time > longestwait) {
							longestwait = proctab[wptr].pwait_time; 
							longestwaitproc = wptr;
						}
						wptr = (&q[wptr])->qprev;

						//kprintf("longestwait: %d, longestwaitproc: %d\n", longestwait, longestwaitproc);
					}
					// dequeue process from write queue
					dequeue(longestwaitproc);

					// add process to lock's bitmask
					lptr->curr_mask |= (1ULL << longestwaitproc);

					// add lock to process' bitmask
					(&proctab[longestwaitproc])->locks |= (1LL << lock);

					ready(longestwaitproc, RESCHYES);
				}
				else if (wpprio < rpprio) { // if reader priority higher than writer priority 
					int tempptr;
					// all readers with higher priority than wpprio will be allowed to enter
					while ((&q[rptr])->qkey > wpprio && rptr != MAXINT && rptr != MININT) { 
						//kprintf("rptr: %d, (&q[rptr])->qkey: %d \n", rptr, (&q[rptr])->qkey);
						// add process to lock's bitmask
						lptr->curr_mask |= (1ULL << rptr);

						// add lock to process' bitmask
						(&proctab[rptr])->locks |= (1LL << lock);
						
						tempptr = (&q[rptr])->qprev;
						ready(dequeue(rptr), RESCHYES);
						rptr = tempptr;
					}


				}  else { // else if wpprio = rpprio equal
					// if writer waiting time is at least 0.5 seconds longer than reader, then writer runs
					if ((&proctab[wptr])->pwait_time - (&proctab[rptr])->pwait_time >= 5000) {
						ready(getfirst(wptr), RESCHYES);
					} else {
						ready(getfirst(rptr), RESCHYES);
					}
				}				
			} 
			else if (lptr->lstate == LREAD) {
				//kprintf("lock is still in LREAD state \n");
				// If other processes are still holding the lock for reading after the current process releases it, the lock remains in READ state. The lock will only be available when the last reader releases it. 

				// get writer with highest priority 
				wmaxpprio = q[q[(&locktab[lock])->wqhead].qnext].qkey;
				int rptr = q[(&locktab[lock])->rqhead].qnext; // change variable name and rewrite pointer logic

				while (q[rptr].qkey > wmaxpprio) { 
					ready(getfirst(lptr->rqhead), RESCHYES); 
					rptr = q[rptr].qnext; 
				} 
			} 
		}
	}
	// kprintf("ending release all \n");
	
	/* for all the locks the process has not released – replace lprio with the max priority of 
	those processes that are waiting for those locks */
	/* Reset priority of the process – track max of this round  */
	//int unreleased_locks[50];
	int j = -1;
	waitmaxpprio = 0;

	for (i = 0; i < 50; i++) {
		if ((&proctab[currpid])->locks & (1ULL << i)) {
			// traverse read and write waiting queue to find the process with the highest priority 
			int ptr = (&q[(&locktab[i])->wqhead])->qnext;

			while (ptr != &q[(&locktab[i])->wqtail]) {
				if ((&proctab[ptr])->pinh > waitmaxpprio) {
					waitmaxpprio = (&proctab[ptr])->pinh;
				} else if ((&proctab[ptr])->pprio > waitmaxpprio) {
					waitmaxpprio = (&proctab[ptr])->pprio;
				}
				ptr = (&q[ptr])->qnext;
			}

			ptr = (&q[(&locktab[i])->rqhead])->qnext;

			while (ptr != &q[(&locktab[i])->rqtail]) {
				if ((&proctab[ptr])->pinh > waitmaxpprio) {
					waitmaxpprio = (&proctab[ptr])->pinh;
				} else if ((&proctab[ptr])->pprio > waitmaxpprio) {
					waitmaxpprio = (&proctab[ptr])->pprio;
				}
				ptr = (&q[ptr])->qnext;
			}
		}
	}

	(&proctab[currpid])->pinh = waitmaxpprio; 
	restore(ps);
	return(return_state);
}