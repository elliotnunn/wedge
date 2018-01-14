#include "PPCInfoRecordsPriv.h"
#include "Wedge.h"

int main(void)
{
	Wedge(
		(NKConfigurationInfo *)0x68FEF000UL,
		*(NKProcessorInfo **)nkProcessorInfoPtr,
		*(NKSystemInfo **)nkSystemInfoPtr,
		*(NKDiagnosticInfo **)nkDiagnosticInfoPtr,
		(OSType)0, /* rtasFour */
		NULL, /* rtasProc */
		*(NKHWInfo **)nkHWInfoPtr,
		1 /* isDryRun */
	);

	return 1; /* the above func should call NanoKernel instead of returning */
}

void NanoKernel(NKConfigurationInfo *ci, NKProcessorInfo *pi, NKSystemInfo *si, NKDiagnosticInfo *di, OSType rtasFour, char *rtasProc, NKHWInfo *hi)
{
	exit(0);
}
