; MakeFunction sets up everything you need to make an assembly function 
; callable from C and debuggable with a symbolic debugger. It does the following:
; - export the function's transition vector
; - export the function name
; - create a toc entry for the function's transition vector
; - create the transition vector, which must contain
;     - the function entry point (the name of the function)
;     - the TOC anchor (the predefined variable TOC[tc0])
; - tell PPCAsm to create a function entry point symbol for symbolic debuggers
; - create a csect for the function (one csect per function lets the
;	linker do dead code stripping, resulting in smaller executables)
	
	MACRO
	MakeFunction &fnName
		EXPORT &fnName[DS]
 		EXPORT .&fnName[PR]
		
		TC &fnName[TC], &fnName[DS]
			
		CSECT &fnName[DS]
			DC.L .&fnName[PR]
 			DC.L TOC[tc0]
		
		CSECT .&fnName[PR]
		FUNCTION .&fnName[PR]	
		
	ENDM
	
linkageArea:		set 24	; constant comes from the PowerPC Runtime Architecture Document
CalleesParams:		set	32	; always leave space for GPR's 3-10
CalleesLocalVars:	set 0	; NKxprintf doesn't have any
numGPRs:			set 0	; num volitile GPR's (GPR's 13-31) used by NKxprintf
numFPRs:			set 0	; num volitile FPR's (FPR's 14-31) used by NKxprintf

spaceToSave:	set linkageArea + CalleesParams + CalleesLocalVars + 4*numGPRs + 8*numFPRs  

	; declare the C function DisplayAlert as external
	import .DisplayAlert
	
	import gHelloString		; global variable from C program
	import gGoodbyeString	; global variable from C program

	toc
		tc gHelloString[TC], gHelloString
		tc gGoodbyeString[TC], gGoodbyeString
	

; Call the MakeFunction macro, defined in MakeFunction.s to begin the function
	MakeFunction	NKxprintf		
	
	li r0, 96
	sc
	blr
	

	MakeFunction	NKLocateInfoRecord				; Lifted shamelessly from MPLibrary
	
	stw       r4,0x001C(SP)
	stw       r5,0x0020(SP)
	stw       r6,0x0024(SP)
	li        r0,107
	sc
	cmpwi     r3,0
	bnelr
	lwz       r7,0x001C(SP)
	lwz       r8,0x0020(SP)
	lwz       r9,0x0024(SP)
	cmplwi    cr5,r7,0x0000
	cmplwi    cr6,r8,0x0000
	cmplwi    cr7,r9,0x0000
	beq       cr5,$+0x0008              ; 0x00008440
	stw       r4,0x0000(r7)
	beq       cr6,$+0x0008              ; 0x00008448
	stw       r5,0x0000(r8)
	beq       cr7,$+0x0008              ; 0x00008450
	stw       r6,0x0000(r9)
	blr


	MakeFunction	NKKernelDebuggerCmd		
	
	li r0, 200
	sc
	blr
