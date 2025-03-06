#include <conf.h>
#include <kernel.h>
#include <q.h>
#include <lock.h>

extern int nextlock;

void linit()
{	
	int i;
	struct lentry *lptr;

	nextlock = NLOCKS - 1;

	for (i = 0; i < NLOCKS; i++)
	{ /* initialize locks */
		(lptr = &locktab[i])->lstate = LFREE;

		lptr->rqtail = 1 + (lptr->rqhead = newqueue());
		lptr->wqtail = 1 + (lptr->wqhead = newqueue());
	}
}