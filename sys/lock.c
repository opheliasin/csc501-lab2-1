/* Acquisition of a lock for read/write: int lock (int ldes1, int type, int priority) –  
This call is explained below (“Wait on locks with Priority”).
 */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

extern unsigned long ctr1000; 

int lock(int ldes1, int type, int priority) {
    // priority can be any integer 
    STATWORD ps; 
    disable(ps);
	struct lentry *lptr;
	struct pentry *pptr = &proctab[currpid];

	// kprintf("currpid %d enter lock %d\n", currpid, ldes1);

    // if the process is trying to acquire has already been deleted, then return SYSERR
	// prevent process from acquiring a lock that's been recreated:
	//	solution: check if the lock was created before or after the lock was requested by the process

    if (isbadlock(ldes1) || (lptr = &locktab[ldes1])->lstate == LDELETED || lptr->lcreatetime > pptr->pwait_time) {
        restore(ps);
		pptr->plockret = SYSERR;
        return(pptr->plockret);
    }
	
	int i;
	int blockproc;
	int prio; 
	int wmaxpprio; // TODO: check if this works

	if (lptr->lstate == LFREE || lptr->lstate == LAVAIL) { //if current state of lock is free\
		//then process can acquire it regardless of type 

		// add process to lock's queue // TODO change
		// enqueue(currpid, lptr->hqhead);
		// kprintf("currpid: %ld \n", currpid);

		// kprintf("Initial lock mask: 0x%lx%08lx\n", 
		// 	(unsigned long)(lptr->curr_mask >> 32), 
		// 	(unsigned long)(lptr->curr_mask & 0xFFFFFFFF));
		
		lptr->curr_mask |= (1ULL << currpid);
		
		// kprintf("Updated lock mask: 0x%lx%08lx\n\n", 
		// 	(unsigned long)(lptr->curr_mask >> 32), 
		// 	(unsigned long)(lptr->curr_mask & 0xFFFFFFFF));
		
    
		// add lock to the process' queue //TODO change
		// enqueue(ldes1, pptr->lqtail); 

		// kprintf("ldes1: %d\n", ldes1);

		// kprintf("Initial process mask: 0x%lx%08lx\n", 
		// 	(unsigned long)(pptr->locks >> 32), 
		// 	(unsigned long)(pptr->locks & 0xFFFFFFFF));
    
		pptr->locks |= (1LL << ldes1);  // Set bit ldes1 to 1
		
		//kprintf("Initial process mask: 0x%lx%08lx\n", 
			// (unsigned long)(pptr->locks >> 32), 
			// (unsigned long)(pptr->locks & 0xFFFFFFFF));
		
		if (type == READ) {
			lptr->lstate = LREAD; 
			//kprintf("Set lstate to LREAD\n");
		} else if (type == WRITE) {
			lptr->lstate = LWRITE; 
			//kprintf("Set lstate to LWRITE\n");
		} else {
			restore(ps);
			return(SYSERR);
		}
		
		restore(ps);
		return(OK);
	}
	else if (lptr->lstate == LREAD) { 
		if (type == READ) {
			// if head is not pointing to tail and
			// if there's a higher or equal priority writer already waiting for the lock
			if (((&q[lptr->wqhead])->qnext != lptr->wqtail) && (wmaxpprio = q[(&q[lptr->wqhead])->qnext].qkey) >= priority) {
				//kprintf("wmaxpprio = %d\n", wmaxpprio); 
				pptr->pstate = PRWAIT;
				pptr->lockid = ldes1;
				insert(currpid, lptr->rqhead, priority); 
				pptr->pwait_time = ctr1000; // set start time in wait queue 
				pptr->plockret = OK;
				//kprintf("writing priority is greater than reader's\n");
				resched(); // switch to another process 

				if (lptr->lstate == LDELETED) {  
					pptr->pwaitret = DELETED;
				}
				restore(ps);
				return pptr->plockret;
			} else {
				// add process to current lock's bitmask
				lptr->curr_mask |= (1ULL << currpid);

				// add lock to current process' bitmask
				(&proctab[currpid])->locks |= (1LL << ldes1);

				// all readers with higher priority than the highest priority writer should also be admitted 
				int prevptr = (&q[lptr->rqtail])->qprev; 

				while ((&q[prevptr])->qkey > wmaxpprio && (&q[prevptr])->qkey != MININT && (&q[prevptr])->qkey != MAXINT) { 
					// add process to lock's bitmask
					lptr->curr_mask |= (1ULL << prevptr);

					// add lock to process' bitmask
					(&proctab[prevptr])->locks |= (1LL << ldes1);

					ready(getlast(lptr->rqtail), RESCHYES);  
					prevptr = (&q[lptr->rqtail])->qprev; 
					//kprintf("nextptr: %d \n", nextptr);
				}
				
				restore(ps);
				return(OK);
			}

		} else if (type == WRITE) {
			pptr->pstate = PRWAIT;
			pptr->lockid = ldes1;

			// priority inheritance: the write process is blocking the read process from entering 
			// so we boost the priority of the read process to the write's process (if applicable)

			// do we need this for read? 
			// find the process that's holding the lock
			for (i = 0; i < 50; i++) {
				if ((&locktab[ldes1])->curr_mask & (1ULL << i)) {
					blockproc = i;
					break;
				}
			}
			
			// if the blocking process's priority is higher than the currpid's 
			if ((&proctab[blockproc])->pinh != 0 && (&proctab[blockproc])->pinh > (&proctab[currpid])->pprio) {
				(&proctab[currpid])->pinh = (&proctab[blockproc])->pinh;
			if ((&proctab[blockproc])->pprio > (&proctab[currpid])->pinh) {
					(&proctab[currpid])->pinh = (&proctab[blockproc])->pprio;
				}
			}

			// take care of transitivity 



			insert(currpid, lptr->wqhead, priority);

			pptr->pwait_time = ctr1000;
			pptr->plockret = OK;
			resched(); // switch to another process 

			if (lptr->lstate == LDELETED) {  
				pptr->pwaitret = DELETED;
			}

			restore(ps);
			return pptr->plockret;
		} else {
			restore(ps);
			return(SYSERR);
		}
	} 
	else if (lptr->lstate == LWRITE) {
		pptr->pstate = PRWAIT;
		pptr->lockid = ldes1;
		kprintf("lockid: %d \n", pptr->lockid);
		kprintf("Before insert: head=%d, next=%d\n", lptr->rqhead, q[lptr->rqhead].qnext);

		if (type == READ) {
			kprintf("insert into read waiting queue \n");
			insert(currpid, lptr->rqhead, priority);  
		} else if (type == WRITE) {
			kprintf("insert into write waiting queue \n");
			insert(currpid, lptr->wqhead, priority);
		} else {
			restore(ps);
			return(SYSERR);
		}
		
		// check to make sure the process is actually inserted:
		kprintf("After insert: head=%d, next=%d\n", lptr->rqhead, q[lptr->rqhead].qnext);


		// find the write process that's holding the lock
		for (i = 0; i < 50; i++) {
			if ((&locktab[ldes1])->curr_mask & (1ULL << i)) {
				blockproc = i;
				break;
			}
		}
		kprintf("blockproc: %d\n", blockproc);
		kprintf("blockproc's priority: %d \n", (&proctab[blockproc])->pinh);
		// if the blocking process's priority is lower than the currpid's 
		if ((&proctab[currpid])->pinh != 0) {
			prio = (&proctab[currpid])->pinh;
		} else {
			prio = (&proctab[currpid])->pprio;
		}
		kprintf("prio: %d\n", prio);

		if ((&proctab[blockproc])->pinh < prio) {
			(&proctab[blockproc])->pinh = prio;
		}
		kprintf("(&proctab[blockproc])->pinh: %d\n", (&proctab[blockproc])->pinh);
		pptr->plockret = OK;
		pptr->pwait_time = ctr1000;
		resched(); // switch to another process 

		if (lptr->lstate == LDELETED) {  
			pptr->pwaitret = DELETED;
		}
		restore(ps);
		return pptr->plockret;	
	} 
	else {
		restore(ps);
		return(SYSERR);
	}
}