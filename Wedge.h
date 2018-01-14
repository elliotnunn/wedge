/* Prototypes for calling into and out of the C wedge. */

#pragma once

#include "PPCInfoRecordsPriv.h"

void Wedge(NKConfigurationInfo *ci, NKProcessorInfo *pi, NKSystemInfo *si, NKDiagnosticInfo *di, OSType rtasFour, char *rtasProc, NKHWInfo *hi, int isDryRun);
void NanoKernel(NKConfigurationInfo *ci, NKProcessorInfo *pi, NKSystemInfo *si, NKDiagnosticInfo *di, OSType rtasFour, char *rtasProc, NKHWInfo *hi);
