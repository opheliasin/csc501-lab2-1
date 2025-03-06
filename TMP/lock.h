#ifndef _LOCK_H_
#define _LOCK_H_

#define LFREE '\01'    /* this lock is free (but has not been allocated) */
#define LREAD '\02'    /* this lock is read access		*/
#define LWRITE '\03'   /* this lock has write access		*/
#define LDELETED '\04' /* this lock is deleted    */
#define LAVAIL '\05'   /* this lock is available but no process has acquired it */

#define READ '\01'
#define WRITE '\02'

#ifndef NLOCKS
#define NLOCKS 50 /* number of locks allowed */
#endif

struct lentry
{                     /* lock table entry*/
    char lstate;      /* the state LFREE, LREAD, LWRITE, LDELETED, or LAVAIL */
    long lcreatetime; /* time when lock was created */
    int lprio;        /* max priority among all processes waiting in the lock's wait queue */

    unsigned long long curr_mask; /* bit mask storing processes the lock's currently holding */
    // /* linked list of the ids of the processes currently waiting for the lock */
    int rqhead; /* q index of head of readers linked list */
    int rqtail; /* q index of tail of readers linked list */

    int wqhead; /* q index of head of writers linked list */
    int wqtail; /* q index of tail of writers linked list */

    // /* linked list storing processes holding the lock*/
    // int hqhead;
    // int hqtail;
};

extern struct lentry locktab[];
extern int nextlock;

#define isbadlock(l) (l < 0 || l >= NLOCKS)

void linit();
int lcreate();
int ldelete(int);
int lock(int, int, int);
int releaseall(int, long);

#endif