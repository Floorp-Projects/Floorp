#ifdef __LP64__
        .LEVEL   2.0W
#else
        .LEVEL   1.1
#endif

	.CODE	; equivalent to the following two lines
;       .SPACE   $TEXT$,SORT=8
;       .SUBSPA  $CODE$,QUAD=0,ALIGN=4,ACCESS=0x2c,CODE_ONLY,SORT=24

ret_cr16
	.PROC
	.CALLINFO 	FRAME=0, NO_CALLS
	.EXPORT 	ret_cr16,ENTRY
	.ENTER
;	BV		%r0(%rp)
	BV		0(%rp)
	MFCTL		%cr16,%ret0
        .LEAVE
        .PROCEND
        .END
