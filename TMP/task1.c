#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <lock.h>
#include <stdio.h>


#define DEFAULT_LOCK_PRIO 20

#define assert(x,error) if(!(x)){ \
            kprintf(error);\
            return;\
            }
int mystrncmp(char* des,char* target,int n){
    int i;
    for (i=0;i<n;i++){
        if (target[i] == '.') continue;
        if (des[i] != target[i]) return 1;
    }
    return 0;
}

void reader (char *msg, int lck)
{
        int     ret;

        kprintf ("  %s: to acquire lock\n", msg);
        lock (lck, READ, DEFAULT_LOCK_PRIO);
        kprintf ("  %s: acquired lock\n", msg);
        kprintf ("  %s: to release lock\n", msg);
        releaseall (1, lck);
}

void writer (char *msg, int lck)
{
        kprintf ("  %s: to acquire lock\n", msg);
        lock (lck, WRITE, DEFAULT_LOCK_PRIO);
        kprintf ("  %s: acquired lock, sleep 10s\n", msg);
        sleep (10);
        kprintf ("  %s: to release lock\n", msg);
        releaseall (1, lck);
}

void multi_lock(char *msg, int lck1, int lck2, int pprio1, int pprio2) {
    kprintf("  %s: attempting to acquire lock %d\n", msg, lck1);
    lock(lck1, WRITE, DEFAULT_LOCK_PRIO); 
    kprintf("  %s: acquired lock %d, now sleeping 1s\n", msg, lck1);
    sleep(1);

    kprintf("  %s: now attempting to acquire lock %d\n", msg, lck2);
    lock(lck2, WRITE, DEFAULT_LOCK_PRIO); 
    kprintf("  %s: acquired lock %d, now sleeping 2s\n", msg, lck2);
    sleep(2);

    kprintf("  %s: releasing lock %d\n", msg, lck2);
    releaseall(1, lck2);
    kprintf("  %s: releasing lock %d\n", msg, lck1);
    releaseall(1, lck1);
}

void test()
{
    int     lck1, lck2;
    int     rd1;
    int     wr1, wr2;

    kprintf("\nAddressing priority inversion\n");
    lck1 = lcreate();
    lck2 = lcreate();
    assert (lck1 != SYSERR, "Test failed");
    assert (lck2 != SYSERR, "Test failed");

    wr1 = create(multi_lock, 2000, 10, "writers", 3, "writers", lck1, lck2);
    wr2 = create(writer, 2000, 20, "writer", 2, "writer", lck2);
    rd1 = create(reader, 2000, 30, "reader", 2, "reader", lck1);

    kprintf("-start multi_lock, then sleep 1s. lock granted to write (prio 20)\n");
    resume(wr1);
    sleep (1);

    kprintf("-start wr2, then sleep 1s. \n");
    resume(wr2);
    sleep (1);

    kprintf("-start reader, then sleep 1s. reader B(prio 30) blocked on the lock\n");
    resume(rd1);
    sleep(1);
    assert (getprio(wr1) == 30, "Test failed");

    kprintf("-kill wr1, then sleep 1s\n");
    kill (wr1);
    sleep (1);
    kprintf("getprio(wr2): %d\n", getprio(wr2));
    assert (getprio(wr2) == 20, "Test failed");

    sleep (8);
    kprintf ("Test OK\n");
}

int task1(){
    test();
    shudown();
}