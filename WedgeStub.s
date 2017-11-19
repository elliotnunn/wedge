PA_ROM	equ		0x00c00000
Me		equ		0x310000
Middle	equ		0x320000
NK		equ		0x340000
Magic	equ		'Wdg_'



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

	MACRO
	lisori &reg, &val
		lis &reg, ((&val) >> 16) & 0xffff
		ori &reg, &reg, (&val) & 0xffff
	ENDM



	MakeFunction __start
	import .wedge


	;	Pointers in r20, scratch values in r0

	;	Need to reference MagicSentinel so that it gets put before the string table
	;	(There is probably a better way)

	b		* + 8
	dc.l	MagicSentinel{RO}


	;	Initial debug log

	lisori	r20, PA_ROM + Middle
	lisori	r0, 'WgLg'
	stw		r0, -8(r20)
	li		r0, 0
	stw		r0, -4(r20)


	;	Search for the string table (tested, works)

	bl		* + 4
	mflr	r20								; start here

@a	lwzu	r0, 4(r20)						; find the sentinel
	xoris	r0, r0, Magic >> 16
	cmplwi	r0, Magic & 0xffff
	bne		@a

@b	lwzu	r0, 4(r20)						; skip the csect padding
	cmpwi	r0, 0
	beq		@b


	;	Set RTOC and save string table ptr at -4(RTOC)

	lisori	RTOC, PA_ROM + Middle - 64
	stw		r20, -4(RTOC)


	;	Get a stack, just below the waistline

	lisori	SP, PA_ROM + Middle - 128


	;	Go

	b		.wedge



	MakeFunction NanoKernelJump

	lisori	r0, PA_ROM + NK

	mtlr	r0
	blr



	MakeFunction asm_putc

	;	Just a stub/test function for now

	lisori	r6, PA_ROM + Middle
	lwz		r5, -4(r6)
;	lbz		r4, 0(r3)
	stbx	r3, r5, r6
	addi	r5, r5, 1
	stw		r5, -4(r6)
	li		r3, 0

	blr



	;	Insert a sentinel value just before the C string table
	;	(Requires PPCLink -codeorder source -roistext on)
	csect MagicSentinel[RO]

	dc.l	Magic
