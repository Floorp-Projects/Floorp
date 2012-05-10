; 
; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
	.ENTRY
	BV		%r0(%rp)
        .EXIT
	MFCTL		%cr16,%ret0
        .PROCEND
        .END
