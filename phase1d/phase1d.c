#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <usloss.h>
#include <phase1.h>
#include <phase1Int.h>

static void DeviceHandler(int type, void *arg);
static void SyscallHandler(int type, void *arg);
static void IllegalInstructionHandler(int type, void *arg);

static int Sentinel(void *arg);

void 
startup(int argc, char **argv)
{
    int pid;
    P1ProcInit();
    P1LockInit();
    P1CondInit();

    // initialize device data structures
    // put DeviceHandler into interrupt vector for the devices
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = SyscallHandler;

    /* create the sentinel process */
    int rc = P1_Fork("Sentinel", Sentinel, NULL, USLOSS_MIN_STACK, 6 , &pid);
    assert(rc == P1_SUCCESS);
    // should not return
    assert(0);
    return;

} /* End of startup */

int 
P1_DeviceWait(int type, int unit, int *status) 
{
    int     result = P1_SUCCESS;
    // disable interrupts
    // check kernel mode
    // acquire device's lock 
    // while interrupt has not occurred and not aborted
    //      wait for interrupt on type:unit
    // if not aborted
    //      set *status to device's status
    // release lock
    // restore interrupts
    return result;
}


static void
DeviceHandler(int type, void *arg) 
{
    // record that interrupt occurred
    // if clock device
    //      naked signal clock every 5 ticks
    //      P1Dispatch(TRUE) every 4 ticks
    // else
    //      naked signal type:unit
}

static int
Sentinel (void *notused)
{
    int     pid;
    int     rc;

    /* start the P2_Startup process */
    rc = P1_Fork("P2_Startup", P2_Startup, NULL, 4 * USLOSS_MIN_STACK, 2, &pid);
    assert(rc == P1_SUCCESS);

    // enable interrupts
    // while sentinel has children
    //      use P1GetChildStatus to get children that have quit 
    //      wait for an interrupt via USLOSS_WaitInt
    USLOSS_Console("Sentinel quitting.\n");
    return 0;
} /* End of sentinel */

int 
P1_Join(int *pid, int *status) 
{
    int result = P1_SUCCESS;
    // disable interrupts
    // check kernel mode
    // repeat
    //     call P1GetChildStatus  
    //     if there are children but no children have quit
    //        P1SetState(P1_STATE_JOINING)
    //        P1Dispatch(FALSE)
    // until either a child quit or there are no more children
    return result;
}

static void
SyscallHandler(int type, void *arg) 
{
    USLOSS_Sysargs *sysargs = (USLOSS_Sysargs *) arg;
    USLOSS_Console("System call %d not implemented.\n", sysargs->number);
    P1_Quit(1025);
}