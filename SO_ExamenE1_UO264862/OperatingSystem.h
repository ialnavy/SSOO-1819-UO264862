#ifndef OPERATINGSYSTEM_H
#define OPERATINGSYSTEM_H

#include "ComputerSystem.h"
#include <stdio.h>


#define SUCCESS 1
#define PROGRAMDOESNOTEXIST -1
#define PROGRAMNOTVALID -2

#define MAXLINELENGTH 150

#define PROCESSTABLEMAXSIZE 4

#define INITIALPID 3

// In this version, every process occupies a 60 positions main memory chunk 
// so we can use 60 positions for OS code and the system stack
#define MAINMEMORYSECTIONSIZE (MAINMEMORYSIZE / (PROCESSTABLEMAXSIZE+1))

#define NOFREEENTRY -3
#define TOOBIGPROCESS -4
#define MEMORYFULL -5

#define NOPROCESS -1

// Short term politic modification, multilevel queues:
// 1. User processes queue
// 2. Daemons queue
#define NUMBEROFQUEUES 2
enum ReadyToRunProcessQueues { USERPROCESSQUEUE, DAEMONSQUEUE};

#define SLEEPINGQUEUE
#define MEMCONFIG "MemConfig"

// Contains the possible type of programs
enum ProgramTypes { USERPROGRAM, DAEMONPROGRAM }; 

// Enumerated type containing all the possible process states
enum ProcessStates { NEW, READY, EXECUTING, BLOCKED, EXIT};

// Enumerated type containing the list of system calls and their numeric identifiers
enum SystemCallIdentifiers { SYSCALL_END = 3, SYSCALL_YIELD = 4 , SYSCALL_PRINTEXECPID = 5, SYSCALL_SLEEP = 7};

// A PCB contains all of the information about a process that is needed by the OS
typedef struct {
	int busy;
	int initialPhysicalAddress;
	int processSize;
	int state;
	int priority;
	int copyOfPCRegister;
	unsigned int copyOfPSWRegister;
	int copyOfAccumulator;
	int programListIndex;
	int queueID;
	int whenToWakeUp;
	int arrivalTimeCPU;
	int howManyBursts;
} PCB;

// These "extern" declaration enables other source code files to gain access
// to the variable listed
extern PCB processTable[PROCESSTABLEMAXSIZE];
extern int OS_address_base;
extern int sipID;

// Functions prototypes
void OperatingSystem_Initialize();
void OperatingSystem_InterruptLogic(int);
int OperatingSystem_GetExecutingProcessID();

#endif
