#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PrepareDaemons();
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int, char*);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_SystemCallYield();
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_SystemCallSleep();
int OperatingSystem_WakeUpOnTime();
void OperatingSystem_CheckExecutingProcessPriorityIntegrity();
int OperatingSystem_ShouldTheSystemDie();
void OperatingSystem_ReleaseMainMemory(int);
void OperatingSystem_SystemCallExec();

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID=NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 

// Array that contains the identifiers of the READY processes
// int readyToRunQueue[PROCESSTABLEMAXSIZE];
// int numberOfReadyToRunProcesses=0;

// Short term politic modification, multilevel queues:
// 1. User processes queue
// 2. Daemons queue
int readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = {0,0};

char* queueNames[NUMBEROFQUEUES] = {"USER","DAEMONS"};

// Heap with blocked processes sort by when to wakeup
int sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses = 0;

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

// Variable containing the number of clock interrupts which have occurred
int numberOfClockInterrupts = 0;

// State data structure
char* statesNames[5] = {"NEW", "READY", "EXECUTING", "BLOCKED", "EXIT"};

// Exception type names
char* exceptionNames[4] = {"division by zero", "invalid processor mode", "invalid address", "invalid instruction"};

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(&programFile, "OperatingSystemCode");

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++)
		processTable[i].busy=0;
	
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+1);

	// int numberOfReadPartitions = 
	OperatingSystem_InitializePartitionTable();
		
	// Create all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);

	// Add to the arrival queue every programList's program, sorted by arrival time
	ComputerSystem_FillInArrivalTimeQueue();

	OperatingSystem_PrintStatus();
	
	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();

	// Simulation must finish 
	if (OperatingSystem_ShouldTheSystemDie())
		OperatingSystem_ReadyToShutdown();

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")) {
		// Show message "ERROR: Missing SIP program!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(21,SHUTDOWN);
		exit(1);
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
void OperatingSystem_PrepareDaemons(int programListDaemonsBase) {
  
	// Include a entry for SystemIdleProcess at 0 position
	programList[0]=(PROGRAMS_DATA *) malloc(sizeof(PROGRAMS_DATA));

	programList[0]->executableName="SystemIdleProcess";
	programList[0]->arrivalTime=0;
	programList[0]->type=DAEMONPROGRAM; // daemon program

	sipID=INITIALPID%PROCESSTABLEMAXSIZE; // first PID for sipID

	// Prepare aditionals daemons here
	// index for aditionals daemons program in programList
	baseDaemonsInProgramList=programListDaemonsBase;

}


// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command lineand daemons programs
int OperatingSystem_LongTermScheduler() {
  
	int PID, i,
		numberOfSuccessfullyCreatedProcesses = 0;
	
	// for (i = 0; programList[i] != NULL && i < PROGRAMSMAXNUMBER; i++) {
	while (OperatingSystem_IsThereANewProgram() == 1) {
		i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
		PID = OperatingSystem_CreateProcess(i);
		// Halla dar!!! Processen kunde inte passa!
		if (PID == NOFREEENTRY) {
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(103, ERROR, programList[i] -> executableName);
		} else if (PID == PROGRAMDOESNOTEXIST) {
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104, ERROR, programList[i] -> executableName, "it does not exist");
		} else if (PID == PROGRAMNOTVALID) {
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104, ERROR, programList[i] -> executableName, "invalid priority or size");
		} else if (PID == TOOBIGPROCESS) {
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(105, ERROR, programList[i] -> executableName);
		} else if (PID == MEMORYFULL) {
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(144, ERROR, programList[i] -> executableName);
		} else {
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type==USERPROGRAM) 
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
		}
	}

	if (numberOfSuccessfullyCreatedProcesses > 0)
		OperatingSystem_PrintStatus();

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}


// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {
  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	int wasTheProgramLoadedSuccessfully;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID = OperatingSystem_ObtainAnEntryInTheProcessTable();

	if (PID == NOFREEENTRY)
		return NOFREEENTRY;

	// Obtain the memory requirements of the program
	processSize = OperatingSystem_ObtainProgramSize(&programFile, executableProgram->executableName);	

	if (processSize == PROGRAMDOESNOTEXIST)
		return PROGRAMDOESNOTEXIST;
	if (processSize == PROGRAMNOTVALID)
		return PROGRAMNOTVALID;

	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);

	if (priority == PROGRAMNOTVALID)
		return PROGRAMNOTVALID;
	
	// Obtain enough memory space
 	loadingPhysicalAddress = OperatingSystem_ObtainMainMemory(processSize, PID, executableProgram->executableName);

	if (loadingPhysicalAddress == MEMORYFULL)
		return MEMORYFULL;

	if (loadingPhysicalAddress == TOOBIGPROCESS)
		return TOOBIGPROCESS;

	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(111,SYSPROC,PID,executableProgram->executableName,statesNames[NEW]);

	// Load program in the allocated memory
	wasTheProgramLoadedSuccessfully = OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);
	
	if (wasTheProgramLoadedSuccessfully == TOOBIGPROCESS)
		return TOOBIGPROCESS;

	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);

	OperatingSystem_ShowPartitionTable("after allocating memory");
	
	// Show message "Process [PID] created from program [executableName]\n"
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(22,INIT,PID,executableProgram->executableName);
	
	return PID;
}


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID, char* executableName) {
	OperatingSystem_ShowTime(SYSMEM);
	ComputerSystem_DebugMessage(142, SYSMEM, PID, executableName, processSize);

	int i, localSize, iteratedDifference, bestChosenDifference;
	int bestPartitionOptionIndex = -1;
	int availableAppropriatePartitions, appropriatePartitions = 0;
	for (i = 0; i < sizeof(partitionsTable) / sizeof(PARTITIONDATA); i++) {
		localSize = partitionsTable[i].size;
		if (localSize >= processSize) {
			appropriatePartitions++;
			if (partitionsTable[i].occupied == 0) {
				availableAppropriatePartitions++;
				if (bestPartitionOptionIndex == -1)
					bestPartitionOptionIndex = i;
				else {
					bestChosenDifference = partitionsTable[bestPartitionOptionIndex].size - processSize;
					iteratedDifference = localSize - processSize;
					if (iteratedDifference < bestChosenDifference)
						bestPartitionOptionIndex = i;
				}
			}
		}
	}
	if (appropriatePartitions > 0) {
		if (availableAppropriatePartitions > 0) {
			OperatingSystem_ShowPartitionTable("before allocating memory");
			partitionsTable[bestPartitionOptionIndex].occupied = 1;
			partitionsTable[bestPartitionOptionIndex].PID = PID;
			int initAddress = partitionsTable[bestPartitionOptionIndex].initAddress;
			int size = partitionsTable[bestPartitionOptionIndex].size;
			OperatingSystem_ShowTime(SYSMEM);
			ComputerSystem_DebugMessage(143, SYSMEM, bestPartitionOptionIndex, initAddress, size, PID, executableName);
			return initAddress;
		} else
			return MEMORYFULL;
	} else
		return TOOBIGPROCESS;

	// - Old implementation:
 	// if (processSize>MAINMEMORYSECTIONSIZE)
		// return TOOBIGPROCESS;
	
 	// return PID*MAINMEMORYSECTIONSIZE;
}


// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {

	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	processTable[PID].state=NEW;
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;
	processTable[PID].copyOfAccumulator = 0;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;
		processTable[PID].queueID=DAEMONSQUEUE;
	} 
	else {
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;
		processTable[PID].queueID=USERPROCESSQUEUE;
	}
	
	

}


// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {
	if (Heap_add(PID, readyToRunQueue[processTable[PID].queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processTable[PID].queueID], PROCESSTABLEMAXSIZE)>=0) {
		int previousState = processTable[PID].state;
		processTable[PID].state = READY;
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110,SYSPROC,PID,programList[processTable[PID].programListIndex] -> executableName,statesNames[previousState],statesNames[READY]);
	} 
	// OperatingSystem_PrintReadyToRunQueue();
}


// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess;

	selectedProcess=OperatingSystem_ExtractFromReadyToRun();

	if (selectedProcess == NOPROCESS)
		selectedProcess = sipID;
	
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun() {
	int selectedProcess = NOPROCESS; int i;
	for (i = 0; i < NUMBEROFQUEUES; i++) {
		selectedProcess = Heap_poll(readyToRunQueue[i], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[i]);
		if (selectedProcess != -1)
			break;
	}
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}


// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {
	int previousState = processTable[PID].state;
	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	// Change the process' state
	processTable[PID].state=EXECUTING;
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110,SYSPROC,PID, programList[processTable[PID].programListIndex] -> executableName,statesNames[previousState],statesNames[EXECUTING]);
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);
	Processor_SetAccumulator(processTable[PID].copyOfAccumulator);
	
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
void OperatingSystem_PreemptRunningProcess() {

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}


// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID) {
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-1);
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-2);

	processTable[PID].copyOfAccumulator = Processor_GetAccumulator();
	
}


// Exception management routine
void OperatingSystem_HandleException() {
  
	// Show message:
	// "[24] Process [1 - NombreProcesoPid1] has caused an exception (invalid address) and is being terminated\n"
	// "[31] Process [3 - NombreProcesoPid3] has caused an exception (invalid processor mode) and is being terminated\n"
	// "[37] Process [2 - NombreProcesoPid2] has caused an exception (division by zero) and is being terminated\n"
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName, exceptionNames[Processor_GetRegisterB()]);
	
	int previousState = processTable[executingProcessID].state;
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex] -> executableName,statesNames[previousState],statesNames[EXIT]);

	OperatingSystem_TerminateProcess();
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {
	int selectedProcess;
	processTable[executingProcessID].state = EXIT;
	OperatingSystem_ShowPartitionTable("before releasing memory");
	OperatingSystem_ReleaseMainMemory(executingProcessID);
	OperatingSystem_ShowPartitionTable("after releasing memory");

	if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	// Simulation must finish 
	if (OperatingSystem_ShouldTheSystemDie())
		OperatingSystem_ReadyToShutdown();

	// Select the next process to execute (sipID if no more user processes)
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);

	OperatingSystem_PrintStatus();
}

// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID; int previousState = processTable[executingProcessID].state;

	// Register A contains the identifier of the issued system call
	systemCallID = Processor_GetRegisterA();
	
	switch (systemCallID) {
		case SYSCALL_SLEEP:
			OperatingSystem_SystemCallSleep();
			break;
		case SYSCALL_PRINTEXECPID:
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(24,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_YIELD:
			OperatingSystem_SystemCallYield();
			break;

		case SYSCALL_END:
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(25,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex] -> executableName,statesNames[previousState],statesNames[EXIT]);
			OperatingSystem_TerminateProcess();
			break;
		case SYSCALL_EXEC:
			OperatingSystem_SystemCallExec();
			break;
		default:
			// Show message: "[79] Process [1 - NombreProcesoPid1] has made an invalid system call (777) and is being terminated"
			OperatingSystem_ShowTime(INTERRUPT);
			ComputerSystem_DebugMessage(141,INTERRUPT,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,systemCallID);
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex] -> executableName,statesNames[previousState],statesNames[EXIT]);
			OperatingSystem_TerminateProcess();
			break;
	}
}
	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();
			break;
		case CLOCKINT_BIT: // CLOCKINT_BIT=9
			OperatingSystem_HandleClockInterrupt();
			break;
	}

}

// Prints information about every ready process in priority queue
void OperatingSystem_PrintReadyToRunQueue() {
	int i, localQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE], localLimit[NUMBEROFQUEUES];
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);
	// We should copy the queue, don't wanna alter the system course.
	for (i = 0; i < NUMBEROFQUEUES; i++)
		memcpy(localQueue[i], readyToRunQueue[i], sizeof(localQueue[i]));
	memcpy(localLimit, numberOfReadyToRunProcesses, sizeof(localLimit));
	// 'readyToRunQueue' is a 'Priority Queue':
	// - The first, the most urgent.
	// - The last, the less urgent.
	// But, the most important,
	// it must be handled with its pertinent 'Heap' methods,
	// because we figured out that: It's a BINARY HEAP!!! :O
	for (i = 0; i < NUMBEROFQUEUES; i++) {
		int iteratedPID, iteratedProgramPriority; int message = 113; 
		ComputerSystem_DebugMessage(98,SHORTTERMSCHEDULE,Processor_PSW_BitState(EXECUTION_MODE_BIT)?"\t":"");
		ComputerSystem_DebugMessage(112,SHORTTERMSCHEDULE,queueNames[i]);
		while (localLimit[i] != 0) {
			iteratedPID = Heap_poll(localQueue[i], QUEUE_PRIORITY, &localLimit[i]);
			// Gonna look for this process in the general process table!
			iteratedProgramPriority = processTable[iteratedPID].priority;
			// Here you are! Are you the very first?
			if (localLimit[i] == 0) 
				message = 114;
			ComputerSystem_DebugMessage(message,SHORTTERMSCHEDULE,iteratedPID,iteratedProgramPriority);
		}
		ComputerSystem_DebugMessage(109,SHORTTERMSCHEDULE);
	}
}

// Performs a 'yield' action. If the priority of the most prioritary process in the executing process' queue equals to the executing process' priority, they swap their states.
void OperatingSystem_SystemCallYield() {
	int processQueueID = processTable[executingProcessID].queueID;
	int nextMostPrioritaryProcess = Heap_getFirst(readyToRunQueue[processQueueID], numberOfReadyToRunProcesses[processQueueID]);
	if (nextMostPrioritaryProcess != -1 && processTable[executingProcessID].priority == processTable[nextMostPrioritaryProcess].priority) {
		// Show message: "Process [PID - executingProcessID] will transfer the control of the processor to process [anotherPID – anotherProgramName]\n"
		OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
		ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex] -> executableName, nextMostPrioritaryProcess, programList[processTable[nextMostPrioritaryProcess].programListIndex] -> executableName);
		OperatingSystem_PreemptRunningProcess();
		nextMostPrioritaryProcess = Heap_poll(readyToRunQueue[processQueueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processQueueID]);
		OperatingSystem_Dispatch(nextMostPrioritaryProcess);
		OperatingSystem_PrintStatus();
	}
}

// Clock interrupt routine
void OperatingSystem_HandleClockInterrupt() {
	numberOfClockInterrupts++;
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120,INTERRUPT,numberOfClockInterrupts);
	// OperatingSystem_PrintStatus();
	int howManyWaken = OperatingSystem_WakeUpOnTime();
	int createdProcesses = OperatingSystem_LongTermScheduler();
	if ((howManyWaken + createdProcesses) > 0)
		OperatingSystem_CheckExecutingProcessPriorityIntegrity();
	// Simulation must finish 
	if (OperatingSystem_ShouldTheSystemDie())
		OperatingSystem_ReadyToShutdown();
	return;
}
// Performs a 'sleep' action. Blocks a process & gets it into the sleeping queue.
void OperatingSystem_SystemCallSleep() {
	if (executingProcessID != NOPROCESS && (Heap_add(executingProcessID, sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses, PROCESSTABLEMAXSIZE) >= 0)) {
		OperatingSystem_SaveContext(executingProcessID);

		int whenToWakeUp = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1;
		processTable[executingProcessID].whenToWakeUp = whenToWakeUp;
		int previousState = processTable[executingProcessID].state;
		processTable[executingProcessID].state = BLOCKED;
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex] -> executableName, statesNames[previousState], statesNames[BLOCKED]);

		executingProcessID = NOPROCESS;

		int followingProcess = OperatingSystem_ShortTermScheduler();
		
		OperatingSystem_Dispatch(followingProcess);

		OperatingSystem_PrintStatus();
	}
}

// Wakes up every program when its time to wake up comes. Then they go to the READY queue.
int OperatingSystem_WakeUpOnTime() {
	int howManyWaken = 0;
	int nextProcessToWakeUp = Heap_getFirst(sleepingProcessesQueue,numberOfSleepingProcesses);
	while (nextProcessToWakeUp != -1 && processTable[nextProcessToWakeUp].whenToWakeUp <= numberOfClockInterrupts) {
		nextProcessToWakeUp = Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses);
		OperatingSystem_MoveToTheREADYState(nextProcessToWakeUp);
		howManyWaken++;
		nextProcessToWakeUp = Heap_getFirst(sleepingProcessesQueue,numberOfSleepingProcesses);
	}
	if (howManyWaken > 0)
		OperatingSystem_PrintStatus();
	return howManyWaken;
}

// Checks whether the EXECUTING process has more priority than any other READY process. Otherwise, the most prioritary must be being executed.
void OperatingSystem_CheckExecutingProcessPriorityIntegrity() {
	int i;
	for (i = 0; i < NUMBEROFQUEUES; i++) {
		int nextMostPrioritaryProcess = Heap_getFirst(readyToRunQueue[i], numberOfReadyToRunProcesses[i]);

		if (nextMostPrioritaryProcess >= 0 && processTable[nextMostPrioritaryProcess].priority < processTable[executingProcessID].priority) {
			OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
			ComputerSystem_DebugMessage(121,SHORTTERMSCHEDULE,executingProcessID,programList[processTable[executingProcessID].programListIndex] -> executableName,nextMostPrioritaryProcess,programList[processTable[nextMostPrioritaryProcess].programListIndex] -> executableName);

			OperatingSystem_PreemptRunningProcess();
			nextMostPrioritaryProcess = Heap_poll(readyToRunQueue[i], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[i]);
			OperatingSystem_Dispatch(nextMostPrioritaryProcess);
			
			OperatingSystem_PrintStatus();

			break;
		}
	}
}

// Able to return the actual executing process ID
int OperatingSystem_GetExecutingProcessID() {
	return executingProcessID;
}

// Should the system die? Did it conclude every program it was meant to?
// If:
// - LTS hasn't any program to charge anymore
// 		AND
// - There's no process in bed
//		AND
// - There's no process waiting for its processor control
//		AND
// - Processor is unoccupied
// Then the system has concluded its reason for being 
//	& it must be obliterated.
int OperatingSystem_ShouldTheSystemDie() {
	if (OperatingSystem_IsThereANewProgram() == -1) {
		if (Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses) == NOPROCESS) {
			if (Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE], numberOfReadyToRunProcesses[USERPROCESSQUEUE]) == NOPROCESS) {
				if (OperatingSystem_GetExecutingProcessID() == NOPROCESS) {
					// Die
					return 1;
				} else if (programList[processTable[OperatingSystem_GetExecutingProcessID()].programListIndex] -> type == DAEMONPROGRAM) {
					// Our father which art in heaven, kill this demon!
					return 1;
				}
			}
		}
	}
	return 0;
}

void OperatingSystem_ReleaseMainMemory(int PID) {
	int i;
	int partitionIndex = -1;
	for (i = 0; i < sizeof(partitionsTable) / sizeof(PARTITIONDATA); i++) {
		if (partitionsTable[i].PID == PID && partitionsTable[i].occupied == 1) {
			partitionIndex = i;
			break;
		}
	}
	if (partitionIndex >= 0) {
		int initAddress = partitionsTable[partitionIndex].initAddress;
		int size = partitionsTable[partitionIndex].size;
		partitionsTable[partitionIndex].occupied = 0;
		OperatingSystem_ShowTime(SYSMEM);
		ComputerSystem_DebugMessage(145, SYSMEM, partitionIndex, initAddress, size, PID, programList[processTable[PID].programListIndex] -> executableName);
	}
}

void OperatingSystem_SystemCallExec() {
	FILE *programFile;
	int followingProgramIndex = Processor_GetAccumulator();
	char* previousProgramName = programList[processTable[executingProcessID].programListIndex] -> executableName;
	char* followingProgramName = programList[followingProgramIndex] -> executableName;
	if (followingProgramIndex < 0 || followingProgramIndex >= PROGRAMSMAXNUMBER || programList[followingProgramIndex] == NULL) {
		OperatingSystem_ShowTime(ERROR);
		ComputerSystem_DebugMessage(151,ERROR,followingProgramIndex);
	} else {
		int processSize = OperatingSystem_ObtainProgramSize(&programFile, followingProgramName);	
		if (processSize == PROGRAMDOESNOTEXIST || processSize == PROGRAMNOTVALID) {
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(152,ERROR,followingProgramIndex,executingProcessID,previousProgramName);
			OperatingSystem_TerminateProcess();
		} else {
			processTable[executingProcessID].processSize = processSize;
			int priority = OperatingSystem_ObtainPriority(programFile);
			if (priority == PROGRAMNOTVALID) {
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(152,ERROR,followingProgramIndex,executingProcessID,previousProgramName);
				OperatingSystem_TerminateProcess();
			} else {
				processTable[executingProcessID].priority = priority;
				int loadingPhysicalAddress = OperatingSystem_ObtainMainMemory(processSize,executingProcessID,followingProgramName);
				if (loadingPhysicalAddress == TOOBIGPROCESS || loadingPhysicalAddress == MEMORYFULL) {
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(152,ERROR,followingProgramIndex,executingProcessID,previousProgramName);
					OperatingSystem_TerminateProcess();
				} else {
					int wasTheProgramLoadedSuccessfully = OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);
					if (wasTheProgramLoadedSuccessfully == TOOBIGPROCESS) {
						OperatingSystem_ShowTime(ERROR);
						ComputerSystem_DebugMessage(152,ERROR,followingProgramIndex,executingProcessID,previousProgramName);
						OperatingSystem_TerminateProcess();
					} else {
						OperatingSystem_ShowTime(INTERRUPT);
						ComputerSystem_DebugMessage(153,INTERRUPT,executingProcessID,previousProgramName, executingProcessID,followingProgramName);
						processTable[executingProcessID].initialPhysicalAddress = loadingPhysicalAddress;
						processTable[executingProcessID].programListIndex = followingProgramIndex;
						OperatingSystem_RestoreContext(executingProcessID);
					}
				}
			}
		}
	}
}