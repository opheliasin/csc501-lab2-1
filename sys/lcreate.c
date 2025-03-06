/*Create a lock:  int lcreate (void) – Creates a lock and returns a lock descriptor that can be used in further calls to refer to this lock. This call should return SYSERR if there are no available entries in the lock table.  */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

LOCAL int newlock();

extern unsigned long ctr1000; 

/*------------------------------------------------------------------------
 * screate  --  create and initialize a lock, returning its id
 *------------------------------------------------------------------------
 */
int lcreate() {
    STATWORD ps;    
	disable(ps);
	int	lock;
	
	if ((lock=newlock())==SYSERR) {
		restore(ps);
		return(SYSERR);
	}

	kprintf("lock: %ld created\n", lock);
	restore(ps);
	return(lock);
}

/*------------------------------------------------------------------------
 * newlock  --  allocate an unused lock and return its index
 *------------------------------------------------------------------------
 */
LOCAL int newlock()
{
	int	lock;
	int	i;

	for (i = 0; i < NLOCKS; i++) {
		lock = nextlock--;
		if (nextlock < 0)
			nextlock = NLOCKS - 1;
		if (locktab[lock].lstate == LFREE) {
			locktab[lock].lstate = LAVAIL;
			locktab[lock].lcreatetime = ctr1000; // in case which the process tries to grab the same lock again reset lcreatetime
			locktab[lock].curr_mask = 0;
			locktab[lock].lprio = -1;
			return(lock);
		}
	}
	return(SYSERR);
}