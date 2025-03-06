/* kill.c - kill */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

/*------------------------------------------------------------------------
 * kill  --  kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
SYSCALL kill(int pid)
{
	STATWORD ps;    
	struct	pentry	*pptr;		/* points to proc. table for pid*/
	int	dev;

	disable(ps);
	if (isbadpid(pid) || (pptr= &proctab[pid])->pstate==PRFREE) {
		restore(ps);
		return(SYSERR);
	}

	if (--numproc == 0)
		xdone();

	dev = pptr->pdevs[0];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->pdevs[1];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->ppagedev;
	if (! isbaddev(dev) )
		close(dev);
	
	send(pptr->pnxtkin, pid);

	freestk(pptr->pbase, pptr->pstklen);
	switch (pptr->pstate) {

	case PRCURR:	pptr->pstate = PRFREE;	/* suicide */
			resched();

	case PRWAIT:	semaph[pptr->psem].semcnt++;

	case PRREADY:	dequeue(pid);
			pptr->pstate = PRFREE;
			break;

	case PRSLEEP:
	case PRTRECV:	unsleep(pid);
						/* fall through	*/
	default:	pptr->pstate = PRFREE;
	}

	/* priority inheritance */
	int waitmaxpprio = 0;
	
	int lock = (&proctab[pid])->lockid;

	if (lock >= 0 && (&locktab[lock])->lstate != LFREE) {
		dequeue(pid);
	}

	if (lock >= 0) {
		int ptr = (&q[(&locktab[lock])->rqhead])->qnext;
		
		// Find out which process in these two queues have the highest priority  

		while (ptr != (&locktab[lock])->rqtail) {
			if ((&proctab[ptr])->pinh > waitmaxpprio) {
				waitmaxpprio = (&proctab[ptr])->pinh;
			} else if ((&proctab[ptr])->pprio > waitmaxpprio) {
				waitmaxpprio = (&proctab[ptr])->pprio;
			}
			ptr = (&q[ptr])->qnext;
		}

		ptr = (&q[(&locktab[lock])->wqhead])->qnext;
	
		while (ptr != (&locktab[lock])->wqtail) {
			if ((&proctab[ptr])->pinh > waitmaxpprio) {
				waitmaxpprio = (&proctab[ptr])->pinh;
			} else if ((&proctab[ptr])->pprio > waitmaxpprio) {
				waitmaxpprio = (&proctab[ptr])->pprio;
			}
			ptr = (&q[ptr])->qnext;
		}

		// find the processes that's holding the lock and change its priority
		int i;
		for (i = 0; i < 50; i++) {
			if ((&locktab[lock])->curr_mask & (1ULL << i)) {
				(&proctab[i])->pinh = waitmaxpprio; 
			}
		}
	} 
	// Reset all variables in pentry
	pptr->pstate = PRFREE;
    pptr->pprio = 0;
    pptr->pinh = 0;
    pptr->plockret = 0;
    pptr->pwait_time = 0;
	pptr->lockid = -1;  // reset lockid
	restore(ps);
	return(OK);
}
