#include "../pandos_types.h"
#include "../pandos_const.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include <umps3/umps/cp0.h>
#include <umps3/umps/arch.h>
#include "vmSupport.h"

#define 
void supGeneralExceptionHandler(){

	support_t *currSup = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
	int cause = ((currSup->sup_exceptState[GENERALEXCEPT].cause) & GETEXECCODE) >> CAUSESHIFT;

	if (cause == SYSEXCEPTION){
		currSup->sup_exceptState[GENERALEXCEPT].pc_epc += 4;
		switch(currSup->sup_exceptState[GENERALEXCEPT].reg_a0)
		{
			case GETTOD: continue;
			default:
				killProc(NULL);

		}
	}
	else killProc(NULL);

}
