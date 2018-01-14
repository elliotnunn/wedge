#include "PPCInfoRecordsPriv.h"
#include "Wedge.h"
#include <string.h>
#include <stdio.h>


#define kFlagNone					0
#define kFlagIRP					1
#define	kFlagKDP					2
#define kFlagEDP					3


#define LA_InfoRecord_Orig			0x5fffe000UL
#define LA_UniversalArea_Orig		0x64000000UL
#define LA_RiscRom_Orig				0x68000000UL
#define LA_ConfigInfo_Orig			0x68fef000UL
#define LA_KernelData_Orig			0x68ffe000UL
#define LA_EmulatorData_Orig		0x68fff000UL


#define kConfigInfoSize				4096
#define kHardwareInfoSize			192


#define kPatchInfoRecord			0x00000001UL
#define kPatchUniversalArea			0x00000010UL
#define kPatchConfigInfoPage		0x00000100UL
#define kPatchKDP					0x00001000UL
#define kPatchEDP					0x00010000UL
#define kCanPatchROM				0x80000000UL


#define kDelta						0x80000000UL


#define ROM							((char *)0x00c00000UL)
#define Em							((long *)(ROM + 0x360000UL))
#define MainCode					((unsigned short *)ROM)


struct PME
{
	unsigned long word1;			/* LogicalPageIndexInSegment(16b) || PageCountMinus1(16b) */
	unsigned long word2;			/* PhysicalPage(20b) || pageAttr(12b) */
};
typedef struct PME PME;


PME *getPageMapPtr(NKConfigurationInfo *ConfigInfo)
{
	return (PME *)((long)ConfigInfo + (long)ConfigInfo->PageMapInitOffset);
}


int pmeIsBlank(PME *pme)
{
	if(pme->word1 != 0x0000ffffUL) return 0;
	if((pme->word2 & 0x00000f00UL) != 0x00000a00UL) return 0;
	return 1;
}


int segmentOf(unsigned long LA)
{
	return LA >> 28;
}


/*
	Create a PageMapEntry (specifying a logical-to-physical mapping within a PowerPC segment)
	and insert it into a ConfigInfo struct, taking care not to corrupt the structure.

	(Intended to replicate the behaviour of the Trampoline, based on its debug output)
*/

int AddPageMapEntry(NKConfigurationInfo *ci, unsigned long LA, unsigned long count, unsigned long PA, unsigned long pageAttr, unsigned long flags)
{
	PME *pageMapBase = getPageMapPtr(ci);

	PME newEnt;
	int entryOffset;					/* offset of our new entry within the PageMap (bytes) */

	int i;


	/* Format string lifted from Trampoline (changed a tiny bit). */

	printf("AddPageMapEntry: LA = 0x%08X, count = 0x%05X, PA = 0x%08X, pageAttr = 0x%04X, flags = 0x%02X.\n", LA, count, PA, pageAttr, flags);


	/* "Design" the new entry. */

	newEnt.word1 = ((LA << 4) & 0xffff0000UL) | (count - 1);
	newEnt.word2 = (PA & 0xfffff000UL) | pageAttr;


	/* Choose an offset for the entry. */

	entryOffset = ci->SegMap32SupInit[segmentOf(LA) * 2];

	for(;;)
	{
		PME *existing;

		existing = (PME *)((long)pageMapBase + (long)entryOffset);

		if(pmeIsBlank(existing)) break;
		if((existing->word1 & 0xffff0000UL) > (newEnt.word1 & 0xffff0000UL)) break;

		entryOffset += sizeof(PME);
	}

	
	/* Shift the entries above our new entry by 8 bytes. */

	for(i=0; i<8; i++)
	{
		if(*((char *)pageMapBase + ci->PageMapInitSize + i) != 0)
		{
			printf("PageMap overflow!\n", entryOffset/sizeof(PME));
			return 100;
		}
	}

	for(i = ci->PageMapInitSize - 1; i >= entryOffset; i--)
	{
		*((char *)pageMapBase + i + 8) = *((char *)pageMapBase + i);
	}


	/* Bump the declared PageMap size by 8 bytes. (ignored by kernel?) */

	ci->PageMapInitSize += sizeof(PME);


	/* The SegMap (four copies) contains an offset into the PageMap at every second word -- update these. */

	for(i=segmentOf(LA)+1; i<16; i++)
	{
		ci->SegMap32SupInit[i * 2] += sizeof(PME);
		ci->SegMap32UsrInit[i * 2] += sizeof(PME);
		ci->SegMap32CPUInit[i * 2] += sizeof(PME);
		ci->SegMap32OvlInit[i * 2] += sizeof(PME);
	}


	/* Adjust the pointers to the special PageMap entries. */

	if(ci->PageMapIRPOffset >= entryOffset) ci->PageMapIRPOffset += sizeof(PME);
	if(ci->PageMapKDPOffset >= entryOffset) ci->PageMapKDPOffset += sizeof(PME);
	if(ci->PageMapEDPOffset >= entryOffset) ci->PageMapEDPOffset += sizeof(PME);


	/* Set the correct pointer if it is a special one */

	if(flags == kFlagIRP)
		ci->PageMapIRPOffset = entryOffset;
	else if(flags == kFlagKDP)
		ci->PageMapKDPOffset = entryOffset;
	else if(flags == kFlagEDP)
		ci->PageMapEDPOffset = entryOffset;


	/* Save our new entry in the gap we have made in the PageMap. */

	memcpy((char *)pageMapBase + entryOffset, &newEnt, sizeof(PME));

	return 0;
}


/*
	Create a blank PageMap inside a pre-existing ConfigInfo structure, with only
	those "dummy" entries that the Trampoline would create before calling
	AddPageMapEntry.

	(Intended to replicate the behaviour of the Trampoline, based on its debug output)
*/

void ErasePageMapTable(NKConfigurationInfo *ci)
{
	PME *pageMapBase;
	long *pmp;
	int seg;


	printf("ErasePageMapTable at offset 0x%x\n\n", ci->PageMapInitOffset);

	/* Zero out the existing PageMap */

	pageMapBase = getPageMapPtr(ci);
	memset(pageMapBase, 0, ci->PageMapInitSize);


	/* Count up with pmp */

	pmp = (long *)pageMapBase;

	ci->PageMapInitOffset = (long)pageMapBase - (long)ci;

	for(seg=0; seg<16; seg++)
	{
		ci->SegMap32SupInit[seg * 2] =
		ci->SegMap32UsrInit[seg * 2] =
		ci->SegMap32CPUInit[seg * 2] =
		ci->SegMap32OvlInit[seg * 2] = (long)pmp - (long)pageMapBase;

		if(seg <= 5)
		{
			*pmp++ = 0x0000ffffUL; *pmp++ = 0x00000a00UL;
			*pmp++ = 0x0000ffffUL; *pmp++ = 0x00000a00UL;
		}
		else if(seg >= 6 && seg <= 7)
		{
			*pmp++ = 0x0000ffffUL; *pmp++ = 0x00000a01UL;
			*pmp++ = 0x0000ffffUL; *pmp++ = 0x00000a01UL;
			*pmp++ = 0x0000ffffUL; *pmp++ = 0x00000a00UL;
		}
		else if(seg >= 8)
		{
			*pmp++ = 0x0000ffffUL; *pmp++ = 0x00000a01UL + (seg << 28);
			*pmp++ = 0x0000ffffUL; *pmp++ = 0x00000a00UL;
		}
	}

	ci->PageMapInitSize = (long)pmp - (long)pageMapBase;


	/* Zero out the special pointers */

	ci->PageMapIRPOffset = 0;
	ci->PageMapKDPOffset = 0;
	ci->PageMapEDPOffset = 0;
}


int PatchMacOSAddressSpace(long patches, unsigned long makeMemAvail, NKConfigurationInfo *ci, NKConfigurationInfo *newci, NKHWInfo *hi, NKHWInfo *newhi)
{
	int seg;
	PME *pageMapBase;
	PME *entryp;

	unsigned long LA, newLA, count, PA, pageAttr, flags;

	int ret;
	int offset;

	unsigned long i;


	pageMapBase = getPageMapPtr(ci);

	ErasePageMapTable(newci);

	printf("PatchMacOSAddressSpace: makeMemAvail %08x\n", makeMemAvail);

	for(seg=0; seg<16; seg++)
	{
		for(offset = ci->SegMap32SupInit[seg*2];; offset += sizeof (PME))					/* Iterate over PMEs. */
		{
			entryp = (PME *)((long)pageMapBase + offset);


			/* Misc error conditions */

			if(seg < 15 && offset >= ci->SegMap32SupInit[(seg+1) * 2])
			{
				printf("Overran this segment of the PageMap!\n\n");
				return 101;
			}
			if(offset >= ci->PageMapInitSize)
			{
				printf("Overran the whole PageMap!\n\n");
				return 102;
			}

			if(pmeIsBlank(entryp)) break;


			/* Extract info from PME */

			LA = newLA = (seg << 28) | (entryp->word1 >> 4 & 0x0ffff000UL);
			count = (entryp->word1 & 0x0000ffffUL) + 1;
			PA = entryp->word2 & 0xfffff000UL;
			pageAttr = entryp->word2 & 0x00000fffUL;

			if(offset == ci->PageMapIRPOffset)
				flags = kFlagIRP;
			else if(offset == ci->PageMapKDPOffset)
				flags = kFlagKDP;
			else if(offset == ci->PageMapEDPOffset)
				flags = kFlagEDP;
			else
				flags = kFlagNone;

			printf("    nontrivial PME   LA = 0x%08X, count = 0x%05X, PA = 0x%08X, pageAttr = 0x%04X, flags = 0x%02X.\n    ", LA, count, PA, pageAttr, flags);


			/* Delete those two annoying PMEs that signal to end the MacOS area */
			/* (LA 50000000 count fffe PA 00000000 pageAttr a00) */

			if((LA << 4) == 0 && (pageAttr & 0xf00) == 0xa00)
			{
				printf("MacOS area delimiter: skipping\n\n");
				continue;
			}


			/* Move the InfoRecord page. */

			if((patches & kPatchInfoRecord) && LA == LA_InfoRecord_Orig)
			{
				printf("**IRP**\n    ");

				newLA += kDelta;

				/* IRP logical address in ConfigInfo */
				newci->LA_InfoRecord = newLA;

				/* IRP logical address is hardcoded into emulator by lis/ori, so change that. */
				if(patches & kCanPatchROM)
				{
					unsigned short hi = LA >> 16;
					unsigned short lo = LA & 0xffff;
					unsigned short *em = (unsigned short *)Em;

					for(i=0; i<0x10000; i+=2)
					{
						if(em[i+1] == hi && em[i+3] == lo)
						{
							printf("Patching Emulator lis/ori @ %05x\n    ", i*2);
							em[i+1] = newLA >> 16;
							em[i+3] = newLA & 0xffff;
							break;
						}
					}
				}
			}


			/* Move the area containing the Universal structures and the Device Tree */

			if((patches & kPatchUniversalArea) && LA == LA_UniversalArea_Orig)
			{
				printf("**Universal/DeviceTree area**\n    ");

				newLA += kDelta;

				/* Logical address pointers in HardwareInfo */
				newhi->DeviceTreeBase = hi->DeviceTreeBase - LA + newLA;
				newhi->UniversalInfoTableBase = hi->UniversalInfoTableBase - LA + newLA;
			}


			/* Move the (completely unused?) ConfigInfo area: crashes after DR emulator loads */

			if((patches & kPatchConfigInfoPage) && LA == LA_ConfigInfo_Orig)
			{
				printf("**ConfigInfo**\n    ");

				newLA += kDelta;

				for(i=0; i<0xaa550/2; i++)
				{
					if(MainCode[i] == 0x68fe && MainCode[i+1] >= 0xf000) MainCode[i] += kDelta >> 16;
				}

				/* No pointers? Makes sense I guess... */
			}


			/* Move the KDP: crashed in 68k fire */

			if((patches & kPatchKDP) && LA == LA_KernelData_Orig)
			{
				printf("**Kernel Data Page**\n    ");

				newLA += kDelta;

				for(i=0; i<0xaa550/2; i++)
				{
					if(MainCode[i] == 0x68ff && MainCode[i+1] >= 0x5000) MainCode[i] += kDelta >> 16;
				}

				/* Only one pointer that I know of */
				newci->LA_KernelData = newLA;
			}


			/* Move the EDP: crashes after DR emulator loads */

			if((patches & kPatchEDP) && LA == LA_EmulatorData_Orig)
			{
				printf("**Emulator Data Page**\n    ");

				newLA += kDelta;

				/* Only one pointer that I know of */
				newci->LA_EmulatorData = newLA;
			}


			if(newLA < makeMemAvail)
			{
				printf("makeMemAvail is too large with this PME in the way.\n\n");
				return 103;
			}


			/* If this PME was unchanged then newLA will just equal the original LA. */

			ret = AddPageMapEntry(newci, newLA, count, PA, pageAttr, flags);
			if(ret) return ret;

			printf("\n");
		}
	}


	/* Now do the 68000000 BAT */




	/* Done all meaningful PMEs... put the "MacOS area delimiter" back in */

	printf("    Reinserting the MacOS area delimiters:\n");
	for(i=0; i<2; i++)
	{
		printf("    ");
		ret = AddPageMapEntry(newci, makeMemAvail & 0xf0000000UL, makeMemAvail >> 12 & 0xffffUL, 0UL, 0xa00, 0);
		if(ret) return ret;
	}

	printf("\n");

	return 0;
}


void DebugDumpPageMap(NKConfigurationInfo *ci)
{
	PME *pageMapBase, *pme;
	int i, j;

	pageMapBase = getPageMapPtr(ci);

	printf("DebugDumpPageMap\n");

	for(i=0; i<ci->PageMapInitSize; i+=sizeof(PME))
	{
		for(j=0; j<16; j++)
		{
			if(ci->SegMap32SupInit[j*2] == i)
			{
				printf("%X ", j);
				break;
			}
		}
		if(j == 16) printf("  ");

		pme = (PME *)((long)pageMapBase + i);

		printf("%03x: %08x %08x", i, pme->word1, pme->word2);

		if(i == ci->PageMapIRPOffset)
		{
			printf(" IRP");
		}
		else if(i == ci->PageMapEDPOffset)
		{
			printf(" EDP");
		}
		else if(i == ci->PageMapKDPOffset)
		{
			printf(" KDP");
		}

		printf("\n");
	}
	printf("\n");
}


void DebugDumpBanks(NKSystemInfo *si)
{
	unsigned long *thisbank;
	int n = 0;
	thisbank = (unsigned long *)(&si->Bank0Start);

	printf("DebugDumpBanks:\n");

	while(thisbank[1] != 0)
	{
		printf("%02d: %08x - %08x (%x b)\n", n, thisbank[0], thisbank[0] + thisbank[1] - 1, thisbank[1]);
		n++;
		thisbank += 2;
	}
}


char *StealFromBank(NKSystemInfo *si, unsigned long len)
{
	/* Do not steal the first page of the first bank. Just don't */
	/* Return NULL if we fail */

	unsigned long *firstbank, *lastbank, *thisbank;
	char *retaddr;

	printf("StealFromBank: looking for 0x%x b... ", len);

	firstbank = (unsigned long *)(&si->Bank0Start);
	lastbank = (unsigned long *)(&si->Bank25Start);

	/* Find the last real bank */
	thisbank = firstbank;
	while(thisbank <= lastbank && thisbank[1]) thisbank += 2;
	lastbank = thisbank - 2;

	/* Find a bank of appropriate size */
	for(thisbank=lastbank; thisbank>=firstbank; thisbank-=2)
	{
		if(thisbank[1] >= len) break;
	}

	if(thisbank < firstbank || (thisbank == firstbank && len == firstbank[1]))
	{
		printf("No bank large enough.\n"); return NULL;
	}

	/* Now we're committed */

	thisbank[1] -= len;
	retaddr = (char *)thisbank[0] + thisbank[1];

	/* Erase this bank if we have taken all its contents */
	if(thisbank[1] == 0)
	{
		printf("using entire bank... ");
		memmove(thisbank, thisbank + 2, lastbank - thisbank);
		lastbank[0] = lastbank[1] = 0;
	}

	printf("Found at 0x%08x\n", retaddr);

	return retaddr;
}


char *StealFromBankAligned(NKSystemInfo *si, unsigned long len)
{
	unsigned long *firstbank, *lastbank, *thisbank;
	int log = 0;
	unsigned long bstart, bend, mystart, myend;

	printf("StealFromBankAligned: looking for 0x%x b... ", len);

	while((len >> log) > 1UL) log++;
	if(len != (1UL << log))
	{
		printf("Size must be a power of two.\n"); return NULL;
	}

	firstbank = (unsigned long *)(&si->Bank0Start);
	lastbank = (unsigned long *)(&si->Bank25Start);

	/* Find the last real bank */
	thisbank = firstbank;
	while(thisbank <= lastbank && thisbank[1]) thisbank += 2;
	lastbank = thisbank - 2;

	/* Find a bank of appropriate size */
	for(thisbank=lastbank; thisbank>=firstbank; thisbank-=2)
	{
		bstart = thisbank[0];
		bend = thisbank[0] + thisbank[1];

		myend = bend & ~(len - 1);
		mystart = myend - len;

		if(mystart >= bstart) break;
	}

	if(thisbank < firstbank || mystart == firstbank[0])
	{
		printf("No bank large enough.\n"); return NULL;
	}

	/* erase the bank that we're cannibalising */
	memmove(thisbank, thisbank + 2, (lastbank + 2 - thisbank) * sizeof *thisbank);
	lastbank -= 2;

	if(bstart < mystart) /* new bank to my left */
	{
		memmove(thisbank + 2, thisbank, (lastbank + 2 - thisbank) * sizeof *thisbank);
		thisbank[0] = bstart;
		thisbank[1] = mystart - bstart;
		thisbank += 2;
		lastbank += 2;
	}

	if(myend < bend) /* new bank to my right */
	{
		memmove(thisbank + 2, thisbank, (lastbank + 2 - thisbank) * sizeof *thisbank);
		thisbank[0] = myend;
		thisbank[1] = bend - myend;
		thisbank += 2;
		lastbank += 2;
	}

	printf("Found at 0x%08x\n", mystart);

	return (char *)mystart;
}


int allocate256MBInSegment(NKConfigurationInfo *ci, NKSystemInfo *si, int seg)
{
	char *myspace;
	const int log = 28; /* 256 MB */

	printf("allocate256MBInSegment %d\n\n", seg);

	printf("Pre-steal "); DebugDumpBanks(si); printf("\n");
	myspace = StealFromBankAligned(si, 1UL << log); printf("\n");

	if(myspace)
	{
		unsigned long ubat, lbat;
		printf("Post-steal "); DebugDumpBanks(si); printf("\n");

		ubat = /*BEPI*/ (unsigned long)seg << 24 | /*BL*/ ((1UL << (log - 17)) - 1) << 2 | /*Vs/Vp bits*/ 3;
		lbat = /*BRPN*/ (unsigned long)myspace | /*WIMG*/ 2 << 3 | /*PP=RW*/ 2;

		printf("New BAT: %08x %08x\n\n", ubat, lbat);
		ci->BATRangeInit[10] = ubat;
		ci->BATRangeInit[11] = lbat;

		return 0;
	}
	else
	{
		return 1;
	}
}


void Wedge(NKConfigurationInfo *ci, NKProcessorInfo *pi, NKSystemInfo *si, NKDiagnosticInfo *di, OSType rtasFour, char *rtasProc, NKHWInfo *hi, int isDryRun)
{
	char ci_tmp[kConfigInfoSize], hi_tmp[kHardwareInfoSize];
	int ret;

	if(isDryRun)
		printf("Hello from the dry-run Wedge.\n");
	else
		printf("Hello from the Wedge.\n");

	printf("      ConfigInfo (r3) @ %08x\n", ci);
	printf("   ProcessorInfo (r4) @ %08x\n", pi);
	printf("      SystemInfo (r5) @ %08x\n", si);
	printf("  DiagnosticInfo (r6) @ %08x\n", di);
	printf("            RTAS (r7) = %08x\n", rtasFour);
	printf("        RTASProc (r8) @ %08x\n", rtasProc);
	printf("    HardwareInfo (r9) @ %08x\n", hi);
	printf("\n");


	/* PatchMacOSAddressSpace */

	DebugDumpPageMap((NKConfigurationInfo *)ci);

	printf("Rearranging the MacOS address space...\n\n");
	memcpy(ci_tmp, ci, sizeof ci_tmp);
	memcpy(hi_tmp, hi, sizeof hi_tmp);

	ret = PatchMacOSAddressSpace(kPatchInfoRecord | kPatchUniversalArea | kCanPatchROM,// | kPatchConfigInfoPage | kPatchKDP | kPatchEDP,
	                             0x68000000UL,
	                             (NKConfigurationInfo *)ci, (NKConfigurationInfo *)ci_tmp,
	                             (NKHWInfo *)hi, (NKHWInfo *)hi_tmp);

	if(!ret)
	{
		printf("Copying modified ConfigInfo and HWInfo over the originals.\n\n");

		if(!isDryRun)
		{
			memcpy(ci, ci_tmp, sizeof ci_tmp);
			memcpy(hi, hi_tmp, sizeof hi_tmp);
		}

		DebugDumpPageMap((NKConfigurationInfo *)ci);
	}
	else
	{
		printf("PatchMacOSAddressSpace failed with error %d.\n", ret);
	}


	/* Insert more clever, interesting patches here. */


	printf("\nHanding over to the NanoKernel.\n");
	NanoKernel(ci, pi, si, di, rtasFour, rtasProc, hi);
}

/* End of Wedge.c */


