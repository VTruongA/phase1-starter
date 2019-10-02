/*
 * Internal Phase 1 definitions. These may be used by other functions in Phase 1.
 * Version 1.0
 * DO NOT MODIFY THIS FILE.
 */

#ifndef _PHASE1_INT_H
#define _PHASE1_INT_H

#include "phase1.h"

// Phase 1a

void    P1ContextInit(void);
int     P1ContextCreate(void (*func)(void *), void *arg, int stacksize, int *cid) CHECKRETURN; 
int     P1ContextCreate(void (*func)(void *), void *arg, int stacksize, int *cid) CHECKRETURN; 
int     P1ContextSwitch(int cid) CHECKRETURN;
int     P1ContextFree(int cid) CHECKRETURN;
int     P1DisableInterrupts(void) CHECKRETURN;
void    P1EnableInterrupts(void);

// Phase 1b

void    P1ProcInit(void);
int     P1GetChildStatus(int tag, int *pid, int *status) CHECKRETURN;
int     P1SetState(int pid, P1_State state, int sid) CHECKRETURN;
void    P1Dispatch(int rotate);

// Phase 1c

void    P1SemInit(void);


#endif