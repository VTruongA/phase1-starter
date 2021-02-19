#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <usloss.h>
#include <phase1Int.h>
#include <string.h>

#define CHECKKERNEL() \
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) USLOSS_IllegalInstruction()

#define FREE 0
#define BUSY 1		

typedef struct Node Node;
typedef struct Queue Queue;

/**
	The below are functions needed to create a Queue, enqueue, and dequeue.
	The Queue struct is a wrapper struct for Nodes in the queue, using a linkedlist
	as the underlying implementation. Each lock will possess its own queue, created at initialization.
 */
struct Node {
    int    id;
    Node *next;
};

struct Queue {
    Node *head;
    int size;
};


Queue* newQueue() {
    Queue *new = malloc(sizeof(Queue));

    new->head = NULL;
    new->size = 0;
    return new;
}

void enqueue(Queue *q, int id) {
    Node *new = malloc(sizeof(Node));
    new->id = id;
    new->next = NULL;
    Node *curr, *prev;
    prev = NULL;

    curr = q->head;
    if (curr == NULL) {
        q->head = new;
        q->size++;
        return;
    }

    while (curr != NULL) {
        prev = curr;
        curr = curr->next;
    }

    q->size++;
    prev->next = new;
}


int dequeue(Queue *q) {
    Node *node = q->head;
    Node *newHead = node->next;
    q->head = newHead;
    q->size--;
    node->next = NULL;
    return node->id;
}

int peek(Queue *q) {
    if (q->head != NULL) {
        return q->head->id;
    }
    return -1;
}


void queueString(Queue *q) {
    Node *curr;
    curr = q->head;
    USLOSS_Console("Size of queue: %d\n", q->size);
    while (curr != NULL) {
        USLOSS_Console("%d -> ", curr->id);
        curr = curr->next;
    }
    USLOSS_Console("\n");
}

typedef struct Lock
{
    char        name[P1_MAXNAME];
    int         state;
    int         inuse;
	int			lid;
	int			pid;
	Queue       *queue;
    // more fields here
} Lock;

static Lock locks[P1_MAXLOCKS];

void 
P1LockInit(void) 
{
    CHECKKERNEL();
    P1ProcInit();
    for (int i = 0; i < P1_MAXLOCKS; i++) {
        locks[i].inuse  = FALSE;
		locks[i].state = FREE;
		locks[i].lid = -1;
		locks[i].pid = -1;
		locks[i].queue = newQueue();
    }
}

int P1_LockCreate(char *name, int *lid)
{
    CHECKKERNEL();
	if (name[0] == NULL) 
		return P1_NAME_IS_NULL;
	if (strlen(name) > P1_MAXNAME)
		return P1_NAME_TOO_LONG;
	
	int rc;
	rc = P1DisableInterrupts();

	int freeLock = -1;
    // find a free Lock and initialize it
	for (int i = 0; i < P1_MAXLOCKS; i++) {
		if (strcmp(locks[i].name, name) == 0)
			return P1_DUPLICATE_NAME;
		
		if (locks[i].inuse == FALSE) {
			freeLock = i;
			break;
		}
		
	}
	if (freeLock == -1)
		return P1_TOO_MANY_LOCKS;
	
	for (int i = freeLock + 1; i < P1_MAXLOCKS; i++) {
		if (locks[i].inuse && (strcmp(locks[i].name, name) == 0)) {
			return P1_DUPLICATE_NAME;
		}
	}

	strcpy(locks[freeLock].name,name);
	locks[freeLock].inuse = TRUE;
	locks[freeLock].state = FREE;
	locks[freeLock].lid = freeLock;
	*lid = freeLock;
	
	if (rc)
		P1EnableInterrupts();
	
    return P1_SUCCESS;
}

int P1_LockFree(int lid) 
{	
    CHECKKERNEL();
    if (lid < 0 || lid > P1_MAXLOCKS) 
		return P1_INVALID_LOCK;
    if (locks[lid].state == BUSY)
        return P1_BLOCKED_PROCESSES;

	memset(locks[lid].name,0,P1_MAXNAME);
	locks[lid].inuse = FALSE;
	locks[lid].locked = FALSE;
	locks[lid].lid = -1;
 
    return P1_SUCCESS;
}

int P1_Lock(int lid) 
{
	if (lid > P1_MAXLOCKS || lid < 0)
		return P1_INVALID_LOCK;

    int result = P1_SUCCESS;
	
    CHECKKERNEL();
	P1_ProcInfo *info;
	int rc,procSuc, proc;
	while (1) {
		proc = P1_GetPid();
		procSuc = P1_GetProcInfo(proc, info);
        if (procSuc != P1_SUCCESS)
            return procSuc;
		rc = P1DisableInterrupts();
		if (locks[lid].state == FREE) {
			locks[lid].pid   =  proc;
			locks[lid].state =  BUSY;
			break;
		}
		
		P1SetState(proc, P1_STATE_BLOCKED, info->lid, info->vid);
		enqueue(locks[lid].queue, proc);
		if (rc)
			P1EnableInterrupts();
		P1Dispatch(FALSE);
	}
	if (rc)
		P1EnableInterrupts();
	
    return result;
}

int P1_Unlock(int lid) 
{
    int result = P1_SUCCESS;
    CHECKKERNEL();
	
	if (lid > P1_MAXLOCKS || lid < 0)
		return P1_INVALID_LOCK;
	
	int rc, proc;
	proc = P1_GetPid();
	if (locks[lock].pid != pid)
		return P1_LOCK_NOT_HELD;
	
	rc = P1DisableInterrupts();
	
	locks[lock].state = FREE;
	if (locks[lock].queue->size != 0) {
		dequeue(locks[lock].queue);
		P1_ProcInfo *info;
		P1_GetProcInfo(proc,info);
		P1SetState(proc, P1_STATE_READY, info->lid, info->vid);
		P1Dispatch(FALSE);
	}
	if (rc)
		P1EnableInterrupts();

    return result;
}

int P1_LockName(int lid, char *name, int len) {
    int result = P1_SUCCESS;
	
	if (name[0] == NULL)
		return P1_NAME_IS_NULL;
	
	if (lid > P1_MAXLOCKS || lid < 0)
		return P1_INVALID_LOCK;
	
    CHECKKERNEL();
	memcpy(name, locks[lid].name, len);
    return result;
}

/*
 * Condition variable functions.
 */

typedef struct Condition
{
    int         inuse;
    int         state;
    int         waiting;
    char        name[P1_MAXNAME];
    int         lock;  // lock associated with this variable
    Queue       *queue;
    // more fields here
} Condition;

static Condition conditions[P1_MAXCONDS];

void P1CondInit(void) {
    CHECKKERNEL();
    P1LockInit();
    for (int i = 0; i < P1_MAXCONDS; i++) {
        conditions[i].inuse = FALSE;
        conditions[i].lock = -1;
        memset(conditions[i].name,0,P1_MAXNAME);
        conditions[i].state = FREE;
        conditions[i].queue = newQueue();
        conditions[i].waiting = 0;
    }
}

int P1_CondCreate(char *name, int lid, int *vid) {
    int result = P1_SUCCESS;
    CHECKKERNEL();
    
    if (name[0] == NULL) 
		return P1_NAME_IS_NULL;
	if (strlen(name) > P1_MAXNAME)
		return P1_NAME_TOO_LONG;

    int freeID = -1;
    for(int i = 0; i <P1_MAXCONDS; i++){
        if(strcmp(conditions[i].name,name) == 0)
            return P1_DUPLICATE_NAME;
    }
    for(int i = 0;i < P1_MAXCONDS; i++){
        if(conditions[i].inuse == FALSE){
            freeID = i;
            break;
        }
    }

    if (freeID == -1)
        return P1_TOO_MANY_CONDS;
    if (locks[lid].inuse == FALSE)
        return P1_INVALID_LOCK;

    strcpy(conditions[freeID].name,name);
    conditions[freeID].lock = lid;
    conditions[freeID].inuse = TRUE;
    *vid = freeID;

    return result;
}

int P1_CondFree(int vid) {
    int result = P1_SUCCESS;
    CHECKKERNEL();
    if (vid < 0 || vid > P1_MAXCONDS) 
        return P1_INVALID_COND;
    if (locks[conditions[vid].lock].state == BUSY)
        return P1_BLOCKED_PROCESSES;

    memset(conditions[vid].name,0,P1_MAXNAME);
    conditions[vid].lock = -1;
    conditions[vid].inuse = FALSE;
    conditions[vid].state = FREE;

    return result;
}


int P1_Wait(int vid) {
    int result = P1_SUCCESS;
    CHECKKERNEL();

    P1DisableInterrupts();
    P1_ProcInfo *info;
    int procSuc, proc = P1_GetPid();
    procSuc = P1_GetProcInfo(proc,info);
    if (procSuc != P1_SUCCESS)
            return procSuc;
    if (vid < 0 || vid > P1_MAXCONDS || conditions[vid].inuse == FALSE)
        return P1_INVALID_COND;
    if (conditions[vid].lock < 0 || conditions[vid].lock > P1_MAXLOCKS || locks[conditions[vid].lock].inuse == FALSE)
        return P1_INVALID_LOCK;
    if (info->lid != conditions[vid].lock && locks[conditions[vid].lock].state == FREE)
        return P1_LOCK_NOT_HELD;

    conditions[vid].waiting++;
    locks[conditions[vid].lock].state = FREE;
    info->state = P1_STATE_BLOCKED;
    enqueue(conditions[vid].queue,proc);
    P1Dispatch(FALSE);
    locks[conditions[vid].lock].state = BUSY;
    P1EnableInterrupts();

    return result;
}

int P1_Signal(int vid) {
    int result = P1_SUCCESS;
        CHECKKERNEL();

    P1DisableInterrupts();
    P1_ProcInfo *info;
    int procSuc, proc = P1_GetPid();
    procSuc = P1_GetProcInfo(proc,info);
    if (procSuc != P1_SUCCESS)
        return procSuc;
    if (vid < 0 || vid > P1_MAXCONDS || conditions[vid].inuse == FALSE)
        return P1_INVALID_COND;
    if (conditions[vid].lock < 0 || conditions[vid].lock > P1_MAXLOCKS || locks[conditions[vid].lock].inuse == FALSE)
        return P1_INVALID_LOCK;
    if (info->lid != conditions[vid].lock && locks[conditions[vid].lock].state == FREE)
        return P1_LOCK_NOT_HELD;
    if(conditions[vid].waiting > 0){
        int pid = dequeue(conditions[vid].queue);
        procSuc = P1_GetProcInfo(pid,info);
        if (procSuc != P1_SUCCESS)
            return procSuc;
        info->state = P1_STATE_READY;
        conditions[vid].waiting--;
        P1Dispatch(FALSE);
    }
    P1EnableInterrupts();

    return result;
}

int P1_Broadcast(int vid) {
    int result = P1_SUCCESS;
    CHECKKERNEL();
   

    P1DisableInterrupts();
    P1_ProcInfo *info;
    int procSuc, proc = P1_GetPid();
    procSuc = P1_GetProcInfo(proc,info);
    if (procSuc != P1_SUCCESS)
        return procSuc;
    if (vid < 0 || vid > P1_MAXCONDS || conditions[vid].inuse == FALSE)
        return P1_INVALID_COND;
    if (conditions[vid].lock < 0 || conditions[vid].lock > P1_MAXLOCKS || locks[conditions[vid].lock].inuse == FALSE)
        return P1_INVALID_LOCK;
    if (info->lid != conditions[vid].lock && locks[conditions[vid].lock].state == FREE)
        return P1_LOCK_NOT_HELD;
    while(conditions[vid].waiting > 0){
        int pid = dequeue(conditions[vid].queue);
        procSuc = P1_GetProcInfo(pid,info);
        if (procSuc != P1_SUCCESS)
            return procSuc;
        info->state = P1_STATE_READY;
        conditions[vid].waiting--;
        P1Dispatch(FALSE);
    }
    P1EnableInterrupts();
    return result;
}

int P1_NakedSignal(int vid) {
    int result = P1_SUCCESS;
    CHECKKERNEL();
    P1DisableInterrupts();
    if (vid < 0 || vid > P1_MAXCONDS || conditions[vid].inuse == FALSE)
        return P1_INVALID_COND;
    
    if (conditions[vid].waiting > 0){
        P1_ProcInfo *info;
        int procSuc, pid = dequeue(conditions[vid].queue);
        procSuc = P1_GetProcInfo(pid,info);
        if (procSuc != P1_SUCCESS)
            return procSuc;
        info->state = P1_STATE_READY;
        conditions[vid].waiting--;
        P1Dispatch(FALSE);
    }
    P1EnableInterrupts();
    return result;
}

int P1_CondName(int vid, char *name, int len) {
    int result = P1_SUCCESS;
    CHECKKERNEL();
    P1DisableInterrupts();
    if (vid < 0 || vid > P1_MAXCONDS || conditions[vid].inuse == FALSE)
        return P1_INVALID_COND;
    if (conditions[vid].name[0] == NULL)
        return P1_NAME_IS_NULL;
    memcpy(name,conditions[vid].name,len);
    P1EnableInterrupts();
    return result;
}
