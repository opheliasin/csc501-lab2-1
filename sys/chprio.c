/* chprio.c - chprio */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

/*------------------------------------------------------------------------
 * chprio  --  change the scheduling priority of a process
 *------------------------------------------------------------------------
 */
SYSCALL chprio(int pid, int newprio)
{
	STATWORD ps;    
	struct	pentry	*pptr;

	disable(ps);
	if (isbadpid(pid) || newprio<=0 ||
	    (pptr = &proctab[pid])->pstate == PRFREE) {
		restore(ps);
		return(SYSERR);
	}

	/* priority inheritance */
	int waitmaxpprio = 0;
		
	kprintf("pid: %d\n", pid);
	int lock = (&proctab[pid])->lockid;
	if (lock >= 0) {
		kprintf("lock: %d\n", lock);

		int ptr = (&q[(&locktab[lock])->rqhead])->qnext;
		kprintf("head=%d, next=%d\n", (&locktab[lock])->rqhead, q[(&locktab[lock])->rqhead].qnext);
		
		// TODO: find out which process in these two queues have the highest priority  

		while (ptr != (&locktab[lock])->rqtail) {
			//kprintf("(&proctab[ptr])->pinh: %d \n, (&proctab[ptr])->pprio: %d\n ", (&proctab[ptr])->pinh, (&proctab[ptr])->pprio);
			if ((&proctab[ptr])->pinh > waitmaxpprio) {
				waitmaxpprio = (&proctab[ptr])->pinh;
			} else if ((&proctab[ptr])->pprio > waitmaxpprio) {
				waitmaxpprio = (&proctab[ptr])->pprio;
			}
			ptr = (&q[ptr])->qnext;
			// kprintf("ptr: %d\n", ptr);
		}

		ptr = (&q[(&locktab[lock])->wqhead])->qnext;
		// kprintf("ptr: %d\n", ptr);
		// kprintf("(&locktab[lock])->wqtail: %d\n", (&locktab[lock])->wqtail);

		while (ptr != (&locktab[lock])->wqtail) {
			//kprintf("(&proctab[ptr])->pinh: %d \n, (&proctab[ptr])->pprio: %d\n ", (&proctab[ptr])->pinh, (&proctab[ptr])->pprio);
			if ((&proctab[ptr])->pinh > waitmaxpprio) {
				waitmaxpprio = (&proctab[ptr])->pinh;
			} else if ((&proctab[ptr])->pprio > waitmaxpprio) {
				waitmaxpprio = (&proctab[ptr])->pprio;
			}
			ptr = (&q[ptr])->qnext;
			// kprintf("ptr: %d\n", ptr);
		}

		kprintf("waitmaxpprio: %d\n", waitmaxpprio);

		// find the processes that's holding the lock and change its priority
		int i;
		for (i = 0; i < 50; i++) {
			if ((&locktab[lock])->curr_mask & (1ULL << i)) {
				(&proctab[i])->pinh = waitmaxpprio; 
			}
		}
	} 

	pptr->pprio = newprio;
	restore(ps);
	return(newprio);
}
