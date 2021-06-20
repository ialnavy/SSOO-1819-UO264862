#include <stdio.h>
#include <stdlib.h>
#include "ComputerSystem.h"
#include "OperatingSystem.h"
#include "ComputerSystemBase.h"
#include "Processor.h"
#include "Messages.h"
#include "Asserts.h"

// Functions prototypes

// Array that contains basic data about all daemons
// and all user programs specified in the command line
PROGRAMS_DATA *programList[PROGRAMSMAXNUMBER];

// Programs arrival time queue, implemented within a binary heap
int arrivalTimeQueue[PROGRAMSMAXNUMBER];
int numberOfProgramsInArrivalTimeQueue = 0;

// Powers on of the Computer System.
void ComputerSystem_PowerOn(int argc, char *argv[]) {

	// Load debug messages
	int nm=0;
	nm=Messages_Load_Messages(nm,TEACHER_MESSAGES_FILE);
	nm=Messages_Load_Messages(nm,STUDENT_MESSAGES_FILE);
	printf("%d Messages Loaded\n",nm);

	// Obtain a list of programs in the command line and debus sections
	int daemonsBaseIndex = ComputerSystem_ObtainProgramList(argc, argv);

	int na=Asserts_LoadAsserts();
	if (na==-1)
		// printf("Asserts file unavailable\n");
		ComputerSystem_DebugMessage(84,POWERON);
	else
		// printf("%d Asserts Loaded\n",na);
		ComputerSystem_DebugMessage(85,POWERON,na);

	ComputerSystem_PrintProgramList();
	// Request the OS to do the initial set of tasks. The last one will be
	// the processor allocation to the process with the highest priority
	OperatingSystem_Initialize(daemonsBaseIndex);
	
	// Tell the processor to begin its instruction cycle 
	Processor_InstructionCycleLoop();
	
}

// Powers off the CS (the C program ends)
void ComputerSystem_PowerOff() {
	// Show message in red colour: "END of the simulation\n" 
	ComputerSystem_DebugMessage(Processor_PSW_BitState(EXECUTION_MODE_BIT)?5:4,SHUTDOWN,Clock_GetTime());
	ComputerSystem_DebugMessage(99,SHUTDOWN); 
	exit(0);
}

/////////////////////////////////////////////////////////
//  New functions below this line  //////////////////////

// Prints every user program in the program list
void ComputerSystem_PrintProgramList() {
	// The function starts, we have to tell the world it does.
	ComputerSystem_DebugMessage(Processor_PSW_BitState(EXECUTION_MODE_BIT)?5:4,INIT,Clock_GetTime());
	ComputerSystem_DebugMessage(101, INIT);
	// We're gonna work, shall we hit up the system for memory locations?
	unsigned int programArrivalTime, programType; char* programName;
	unsigned int i, startIndex = 0; PROGRAMS_DATA* progData;
	// Iterating the unmanageable program array...
	for (i = startIndex; i < sizeof(programList) / sizeof(PROGRAMS_DATA) && i < (startIndex - PROGRAMSMAXNUMBER); i++) {
		// Null programs are never user programs
		progData = programList[i];
		if (progData != NULL) {
			// This function iterates only user programs, let's check it out!
			programType = progData -> type;
			if (programType == USERPROGRAM) {
				// Given the function belongs to the user's domain, we can handle its name & its arrival time.
				programName = progData -> executableName;
				programArrivalTime = progData -> arrivalTime;
				// Everything should be properly stated, so we must conclude the printing & let the program be.
				ComputerSystem_DebugMessage(102, INIT, programName, programArrivalTime);
			}
		}
	}
}