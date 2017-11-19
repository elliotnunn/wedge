#pragma once

#include <MacTypes.h>



OSErr NKxprintf(char *);



enum {
	kNKSystemInfo = 2,
	kNKDiagInfo = 3,
	kNKNanoKernelInfo = 4,
	kNKProcessorInfo = 5,
	kNKHWInfo = 6,
	kNKProcessorState = 7,
};

OSErr NKLocateInfoRecord(long whichRecord, char **record, long *version, long *length);



OSErr NKKernelDebuggerCmd(char *cmd);
