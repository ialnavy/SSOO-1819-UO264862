#include "Clock.h"
#include "Processor.h"
// #include "ComputerSystem.h"

#define INTERVALBETWEENINTERRUPTS 5

int tics=0;

void Clock_Update() {
	tics++;
    // ComputerSystem_DebugMessage(97,CLOCK,tics);

	if (tics%INTERVALBETWEENINTERRUPTS == 0)
		Processor_RaiseInterrupt(CLOCKINT_BIT);
}


int Clock_GetTime() {

	return tics;
}
