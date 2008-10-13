; ***** BEGIN LICENSE BLOCK *****
; Version: MPL 1.1/GPL 2.0/LGPL 2.1
; 
; The contents of this file are subject to the Mozilla Public License Version
; 1.1 (the "License"); you may not use this file except in compliance with
; the License. You may obtain a copy of the License at
; http://www.mozilla.org/MPL/
; 
; Software distributed under the License is distributed on an "AS IS" basis,
; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
; for the specific language governing rights and limitations under the
; License.
; 
; The Original Code is Windows CE Reflect
; 
; The Initial Developer of the Original Code is John Wolfe
; Portions created by the Initial Developer are Copyright (C) 2005
; the Initial Developer. All Rights Reserved.
; 
; Contributor(s):
; 
; Alternatively, the contents of this file may be used under the terms of
; either the GNU General Public License Version 2 or later (the "GPL"), or
; the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
; in which case the provisions of the GPL or the LGPL are applicable instead
; of those above. If you wish to allow use of your version of this file only
; under the terms of either the GPL or the LGPL, and not to allow others to
; use your version of this file under the terms of the MPL, indicate your
; decision by deleting the provisions above and replace them with the notice
; and other provisions required by the GPL or the LGPL. If you do not delete
; the provisions above, a recipient may use your version of this file under
; the terms of any one of the MPL, the GPL or the LGPL.
; 
; ***** END LICENSE BLOCK *****


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This is to call a given method of class that is passed in
;   through the function's one argument, according to APCS
;
;   APCS  = ARM Procedure Calling Standard
;
; Compile this file by running the command:
; 
;   armasm -ARCH 4 -32 xptc_arm_wince.asm xptc_arm_wince.obj
; 
; The parameter is a my_params_struct, which looks like this:
;
;;    extern "C" {
;;    struct my_params_struct {
;;        nsISupports* that;      
;;        PRUint32 Index;         
;;        PRUint32 Count;         
;;        nsXPTCVariant* params;  
;;        PRUint32 fn_count;     
;;        PRUint32 fn_copy;      
;;        };
;;        };
;
; 
; The routine will issue calls to count the number of words
; required for argument passing and to copy the arguments to
; the stack.
;
; Since APCS passes the first 3 params in r1-r3, we need to
; load the first three words from the stack and correct the stack
; pointer (sp) in the appropriate way. This means:
;
; 1.) more than 3 arguments: load r1-r3, correct sp and remember No.
;			      of bytes left on the stack in r4
;
; 2.) <= 2 args: load r1-r3 (we won't be causing a stack overflow I hope),
;		  restore sp as if nothing had happened and set the marker r4 to zero.
;
; Afterwards sp will be restored using the value in r4 (which is not a temporary register
; and will be preserved by the function/method called according to APCS [ARM Procedure
; Calling Standard]).
;
; !!! IMPORTANT !!!
; This routine makes assumptions about the vtable layout of the c++ compiler. It's implemented
; for arm-WinCE MS Visual C++ Studio  >= v7!
;
;
; int XPTC_InvokeByIndexWorker(struct my_params *my_params);
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	INCLUDE kxarm.h


	MACRO
	MY_NESTED_ARMENTRY	$Name
FuncName    SETS    VBar:CC:"$Name":CC:VBar
PrologName  SETS    VBar:CC:"$Name":CC:"_Prolog":CC:VBar
FuncEndName SETS    VBar:CC:"$Name":CC:"_end":CC:VBar

	AREA |.pdata|,ALIGN=2,PDATA
	DCD	    $FuncName
    DCD     (($PrologName-$FuncName)/4) :OR: ((($FuncEndName-$FuncName)/4):SHL:8) :OR: 0x40000000
	AREA $AreaName,CODE,READONLY
	ALIGN	2
	GLOBAL  $FuncName
	EXPORT	$FuncName
$FuncName
	ROUT
$PrologName
	MEND


	MACRO
	MY_STUB_NESTED_ARMENTRY	$Number
;FuncName    SETS    VBar:CC:"?Stub$Number@nsXPTCStubBase@@UAAIXZ":CC:VBar
;PrologName  SETS    VBar:CC:"?Stub$Number@nsXPTCStubBase@@UAAIXZ":CC:"_Prolog":CC:VBar
;FuncEndName SETS    VBar:CC:"?Stub$Number@nsXPTCStubBase@@UAAIXZ":CC:"_end":CC:VBar
FuncName    SETS    VBar:CC:"?asmXPTCStubBase_Stub$Number@@YAIXZ":CC:VBar
PrologName  SETS    VBar:CC:"?asmXPTCStubBase_Stub$Number@@YAIXZ":CC:"_Prolog":CC:VBar
FuncEndName SETS    VBar:CC:"?asmXPTCStubBase_Stub$Number@@YAIXZ":CC:"_end":CC:VBar

	AREA |.pdata|,ALIGN=2,PDATA
	DCD	    $FuncName
    DCD     (($PrologName-$FuncName)/4) :OR: ((($FuncEndName-$FuncName)/4):SHL:8) :OR: 0x40000000
	AREA $AreaName,CODE,READONLY
	ALIGN	2
	GLOBAL  $FuncName
	EXPORT	$FuncName
$FuncName
	ROUT
$PrologName
	mov	ip, #$Number
	b	SharedStub
$FuncEndName
	ENDP
	MEND


	MACRO
	OLD_STUB_ENTRY	$Number
FuncName    SETS    VBar:CC:"?Stub$Number@nsXPTCStubBase@@UAAIXZ":CC:VBar
	AREA |.text|,CODE
	ALIGN	2
	GLOBAL  $FuncName
	EXPORT  $FuncName
$FuncName PROC
	mov	ip, #$Number
	b	SharedStub
	MEND


; USE THIS FUNCTION POINTER INSIDE THE ROUTING XPTC_InvokeByIndex
	IMPORT PrepareAndDispatch


	MY_NESTED_ARMENTRY asmXPTC_InvokeByIndex

; Function compile flags: /Ods
; Was In File c:\builds\wince\mozilla\xpcom\reflect\xptcall\src\md\win32\xptcinvokece.cpp

;|asmXPTC_InvokeByIndex| PROC
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	mov       r12, sp
;;	stmdb     sp!, {r0 - r3}
;;	stmdb     sp!, {r12, lr}
;;	sub       sp, sp, #4
;;
;;	mov       r0, #0	; return NS_OK;
;;	str       r0, [sp]
;;	ldr       r0, [sp]
;;
;;	add       sp, sp, #4
;;	ldmia     sp, {sp, pc}
;;
;;	ENTRY_END
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 144  : {

	; BKPT

	mov       r12, sp
	stmdb     sp!, {r0 - r3}
	stmdb     sp!, {r4, r5, r12, lr}
	sub       sp, sp, #0x2C

; 145  : 	PRUint32 result;
; 146  : 	struct my_params_struct my_params;
; 147  : 	my_params.that = that;

	add       r5, sp, #8    ; r5 = pointer to my_params (also THIS)

	;;ldr       r0, [sp, #0x34]
	;;str       r0, [sp, #8]
	;str       r0, [sp, #8]
	str       r0, [r5, #0x00]

; 148  : 	my_params.Index = methodIndex;   // R5 + 0x4

	;;ldr       r1, [sp, #0x38]
	;;str       r1, [sp, #0xC]
	;str       r1, [sp, #0x0C]
	str       r1, [r5, #0x04]

; 149  : 	my_params.Count = paramCount;	// R5 + 0x8

	;ldr       r0, [sp, #0x3C]
	;str       r0, [sp, #0x10]
	;str       r2, [sp, #0x10]
	str       r2, [r5, #0x08]

; 150  : 	my_params.params = params;	// R5 + 0xC

	;;ldr       r1, [sp, #0x40]
	;;str       r1, [sp, #0x14]
	;str       r3, [sp, #0x14]
	str       r3, [r5, #0x0C]

; 152  : 	my_params.fn_count = (PRUint32) &invoke_count_words;
;						// R5 + 0x10

	;;ldr       r1, [pc, #0x4C]
	;;str       r1, [sp, #0x18]
	ldr       r4, [r12, #0x4]
	;str       r4, [sp, #0x18]
	str       r4, [r5, #0x10]

; 151  : 	my_params.fn_copy = (PRUint32) &invoke_copy_to_stack;
;						// R5 + 0x14

	;;ldr       r0, [pc, #0x58]
	;;str       r0, [sp, #0x1C]
	ldr       r4, [r12]
	;str       r4, [sp, #0x1C]
	str       r4, [r5, #0x14]

                          ; Prepare to call invoke_count_words
	;ldr     r0, [sp, #0x3C]   ; r0 = paramCount
	;ldr     r1, [sp, #0x40]   ; r1 = params
	;ldr     ip, [pc, #0x4C]   ; ip = pointer to invoke_count_words
	;mov	lr, pc		  ; call it...
	;mov	pc, ip		  ;
	ldr     r0, [r5, #0x08]   ; r0 = paramCount (r5 + 0x08 = this + 0x08)
	ldr     r1, [r5, #0x0C]   ; r1 = params (r5 + 0x0C = this + 0x0C)
	ldr     ip, [r5, #0x10]   ; ip = pointer to invoke_count_words (r5 + 0x10 = this + 0x10)
	mov	lr, pc		  ; call it...
	mov	pc, ip		  ;


	mov	r4, r0, lsl #2	  ; This is the amount of bytes needed.

	sub	sp, sp, r4	  ; use stack space for the args...
	mov	r0, sp		  ; prepare a pointer an the stack

;;	ldr	r1, [sp, #0x3C]   ; =paramCount
;;	ldr	r2, [sp, #0x40]   ; =params
;;	ldr	ip, [pc, #0x4C]   ; =invoke_copy_to_stack
	ldr     r1, [r5, #0x08]   ; r1 = paramCount (r5 + 0x08 = this + 0x08)
	ldr     r2, [r5, #0x0C]   ; r2 = params (r5 + 0x0C = this + 0x0C)
	ldr     ip, [r5, #0x14]   ; ip = pointer to invoke_copy_to_stack (r5 + 0x14 = this + 0x14)
	mov	lr, pc		  ; copy args to the stack like the
	mov	pc, ip		  ; compiler would.


	ldr	r0, [r5, #0]	  ; get (self) that

	ldr	r1, [r0]	  ; get that->vtable offset
	ldr	r2, [r5, #4]	  ; = that->methodIndex
	mov	r2, r2, lsl #2	  ; a vtable_entry(x)=0 + (4 bytes * x)

	ldr     ip, [r1, r2]      ; get method adress from vtable

	cmp	r4, #12		  ; more than 3 arguments???
	ldmgtia	sp!, {r1, r2, r3} ; yes: load arguments for r1-r3
	subgt	r4, r4, #12	  ;      and correct the stack pointer
	ldmleia	sp, {r1, r2, r3}  ; no:  load r1-r3 from stack
	addle	sp, sp, r4	  ;      and restore stack pointer
	movle	r4, #0		  ;	a mark for restoring sp

	ldr	r0, [r5, #0]	  ; get (self) that

	; NOTE: At this point, all of the arguments are on the stack, from SP on up, 
	;   with the first argument at SP, the second argument at SP+4, the third 
	;   argument at SP+8, and so on...

	mov	lr, pc		  ; call mathod
	mov	pc, ip		  ;

	add	sp, sp, r4	  ; restore stack pointer
				  ; the result is in r0


	str       r0, [sp]        ; Start unwinding the stack
	str       r0, [sp, #0x20] 

; 225  : }    

	add       sp, sp, #0x2C
	ldmia     sp, {r4, r5, sp, pc}

	ENTRY_END

	ENDP  ; |asmXPTC_InvokeByIndex|




SharedStub
	stmfd	sp!, {r1, r2, r3}
	mov	r2, sp
	str	lr, [sp, #-4]!
	mov	r1, ip
	bl	PrepareAndDispatch
	ldr	pc, [sp], #16
	ENDP






;;	INCLUDE ..\..\..\public\xptcstubsdef.inc

	MY_STUB_NESTED_ARMENTRY 3
	MY_STUB_NESTED_ARMENTRY 4
	MY_STUB_NESTED_ARMENTRY 5
	MY_STUB_NESTED_ARMENTRY 6
	MY_STUB_NESTED_ARMENTRY 7
	MY_STUB_NESTED_ARMENTRY 8
	MY_STUB_NESTED_ARMENTRY 9
	MY_STUB_NESTED_ARMENTRY 10
	MY_STUB_NESTED_ARMENTRY 11
	MY_STUB_NESTED_ARMENTRY 12
	MY_STUB_NESTED_ARMENTRY 13
	MY_STUB_NESTED_ARMENTRY 14
	MY_STUB_NESTED_ARMENTRY 15
	MY_STUB_NESTED_ARMENTRY 16
	MY_STUB_NESTED_ARMENTRY 17
	MY_STUB_NESTED_ARMENTRY 18
	MY_STUB_NESTED_ARMENTRY 19
	MY_STUB_NESTED_ARMENTRY 20
	MY_STUB_NESTED_ARMENTRY 21
	MY_STUB_NESTED_ARMENTRY 22
	MY_STUB_NESTED_ARMENTRY 23
	MY_STUB_NESTED_ARMENTRY 24
	MY_STUB_NESTED_ARMENTRY 25
	MY_STUB_NESTED_ARMENTRY 26
	MY_STUB_NESTED_ARMENTRY 27
	MY_STUB_NESTED_ARMENTRY 28
	MY_STUB_NESTED_ARMENTRY 29
	MY_STUB_NESTED_ARMENTRY 30
	MY_STUB_NESTED_ARMENTRY 31
	MY_STUB_NESTED_ARMENTRY 32
	MY_STUB_NESTED_ARMENTRY 33
	MY_STUB_NESTED_ARMENTRY 34
	MY_STUB_NESTED_ARMENTRY 35
	MY_STUB_NESTED_ARMENTRY 36
	MY_STUB_NESTED_ARMENTRY 37
	MY_STUB_NESTED_ARMENTRY 38
	MY_STUB_NESTED_ARMENTRY 39
	MY_STUB_NESTED_ARMENTRY 40
	MY_STUB_NESTED_ARMENTRY 41
	MY_STUB_NESTED_ARMENTRY 42
	MY_STUB_NESTED_ARMENTRY 43
	MY_STUB_NESTED_ARMENTRY 44
	MY_STUB_NESTED_ARMENTRY 45
	MY_STUB_NESTED_ARMENTRY 46
	MY_STUB_NESTED_ARMENTRY 47
	MY_STUB_NESTED_ARMENTRY 48
	MY_STUB_NESTED_ARMENTRY 49
	MY_STUB_NESTED_ARMENTRY 50
	MY_STUB_NESTED_ARMENTRY 51
	MY_STUB_NESTED_ARMENTRY 52
	MY_STUB_NESTED_ARMENTRY 53
	MY_STUB_NESTED_ARMENTRY 54
	MY_STUB_NESTED_ARMENTRY 55
	MY_STUB_NESTED_ARMENTRY 56
	MY_STUB_NESTED_ARMENTRY 57
	MY_STUB_NESTED_ARMENTRY 58
	MY_STUB_NESTED_ARMENTRY 59
	MY_STUB_NESTED_ARMENTRY 60
	MY_STUB_NESTED_ARMENTRY 61
	MY_STUB_NESTED_ARMENTRY 62
	MY_STUB_NESTED_ARMENTRY 63
	MY_STUB_NESTED_ARMENTRY 64
	MY_STUB_NESTED_ARMENTRY 65
	MY_STUB_NESTED_ARMENTRY 66
	MY_STUB_NESTED_ARMENTRY 67
	MY_STUB_NESTED_ARMENTRY 68
	MY_STUB_NESTED_ARMENTRY 69
	MY_STUB_NESTED_ARMENTRY 70
	MY_STUB_NESTED_ARMENTRY 71
	MY_STUB_NESTED_ARMENTRY 72
	MY_STUB_NESTED_ARMENTRY 73
	MY_STUB_NESTED_ARMENTRY 74
	MY_STUB_NESTED_ARMENTRY 75
	MY_STUB_NESTED_ARMENTRY 76
	MY_STUB_NESTED_ARMENTRY 77
	MY_STUB_NESTED_ARMENTRY 78
	MY_STUB_NESTED_ARMENTRY 79
	MY_STUB_NESTED_ARMENTRY 80
	MY_STUB_NESTED_ARMENTRY 81
	MY_STUB_NESTED_ARMENTRY 82
	MY_STUB_NESTED_ARMENTRY 83
	MY_STUB_NESTED_ARMENTRY 84
	MY_STUB_NESTED_ARMENTRY 85
	MY_STUB_NESTED_ARMENTRY 86
	MY_STUB_NESTED_ARMENTRY 87
	MY_STUB_NESTED_ARMENTRY 88
	MY_STUB_NESTED_ARMENTRY 89
	MY_STUB_NESTED_ARMENTRY 90
	MY_STUB_NESTED_ARMENTRY 91
	MY_STUB_NESTED_ARMENTRY 92
	MY_STUB_NESTED_ARMENTRY 93
	MY_STUB_NESTED_ARMENTRY 94
	MY_STUB_NESTED_ARMENTRY 95
	MY_STUB_NESTED_ARMENTRY 96
	MY_STUB_NESTED_ARMENTRY 97
	MY_STUB_NESTED_ARMENTRY 98
	MY_STUB_NESTED_ARMENTRY 99
	MY_STUB_NESTED_ARMENTRY 100
	MY_STUB_NESTED_ARMENTRY 101
	MY_STUB_NESTED_ARMENTRY 102
	MY_STUB_NESTED_ARMENTRY 103
	MY_STUB_NESTED_ARMENTRY 104
	MY_STUB_NESTED_ARMENTRY 105
	MY_STUB_NESTED_ARMENTRY 106
	MY_STUB_NESTED_ARMENTRY 107
	MY_STUB_NESTED_ARMENTRY 108
	MY_STUB_NESTED_ARMENTRY 109
	MY_STUB_NESTED_ARMENTRY 110
	MY_STUB_NESTED_ARMENTRY 111
	MY_STUB_NESTED_ARMENTRY 112
	MY_STUB_NESTED_ARMENTRY 113
	MY_STUB_NESTED_ARMENTRY 114
	MY_STUB_NESTED_ARMENTRY 115
	MY_STUB_NESTED_ARMENTRY 116
	MY_STUB_NESTED_ARMENTRY 117
	MY_STUB_NESTED_ARMENTRY 118
	MY_STUB_NESTED_ARMENTRY 119
	MY_STUB_NESTED_ARMENTRY 120
	MY_STUB_NESTED_ARMENTRY 121
	MY_STUB_NESTED_ARMENTRY 122
	MY_STUB_NESTED_ARMENTRY 123
	MY_STUB_NESTED_ARMENTRY 124
	MY_STUB_NESTED_ARMENTRY 125
	MY_STUB_NESTED_ARMENTRY 126
	MY_STUB_NESTED_ARMENTRY 127
	MY_STUB_NESTED_ARMENTRY 128
	MY_STUB_NESTED_ARMENTRY 129
	MY_STUB_NESTED_ARMENTRY 130
	MY_STUB_NESTED_ARMENTRY 131
	MY_STUB_NESTED_ARMENTRY 132
	MY_STUB_NESTED_ARMENTRY 133
	MY_STUB_NESTED_ARMENTRY 134
	MY_STUB_NESTED_ARMENTRY 135
	MY_STUB_NESTED_ARMENTRY 136
	MY_STUB_NESTED_ARMENTRY 137
	MY_STUB_NESTED_ARMENTRY 138
	MY_STUB_NESTED_ARMENTRY 139
	MY_STUB_NESTED_ARMENTRY 140
	MY_STUB_NESTED_ARMENTRY 141
	MY_STUB_NESTED_ARMENTRY 142
	MY_STUB_NESTED_ARMENTRY 143
	MY_STUB_NESTED_ARMENTRY 144
	MY_STUB_NESTED_ARMENTRY 145
	MY_STUB_NESTED_ARMENTRY 146
	MY_STUB_NESTED_ARMENTRY 147
	MY_STUB_NESTED_ARMENTRY 148
	MY_STUB_NESTED_ARMENTRY 149
	MY_STUB_NESTED_ARMENTRY 150
	MY_STUB_NESTED_ARMENTRY 151
	MY_STUB_NESTED_ARMENTRY 152
	MY_STUB_NESTED_ARMENTRY 153
	MY_STUB_NESTED_ARMENTRY 154
	MY_STUB_NESTED_ARMENTRY 155
	MY_STUB_NESTED_ARMENTRY 156
	MY_STUB_NESTED_ARMENTRY 157
	MY_STUB_NESTED_ARMENTRY 158
	MY_STUB_NESTED_ARMENTRY 159
	MY_STUB_NESTED_ARMENTRY 160
	MY_STUB_NESTED_ARMENTRY 161
	MY_STUB_NESTED_ARMENTRY 162
	MY_STUB_NESTED_ARMENTRY 163
	MY_STUB_NESTED_ARMENTRY 164
	MY_STUB_NESTED_ARMENTRY 165
	MY_STUB_NESTED_ARMENTRY 166
	MY_STUB_NESTED_ARMENTRY 167
	MY_STUB_NESTED_ARMENTRY 168
	MY_STUB_NESTED_ARMENTRY 169
	MY_STUB_NESTED_ARMENTRY 170
	MY_STUB_NESTED_ARMENTRY 171
	MY_STUB_NESTED_ARMENTRY 172
	MY_STUB_NESTED_ARMENTRY 173
	MY_STUB_NESTED_ARMENTRY 174
	MY_STUB_NESTED_ARMENTRY 175
	MY_STUB_NESTED_ARMENTRY 176
	MY_STUB_NESTED_ARMENTRY 177
	MY_STUB_NESTED_ARMENTRY 178
	MY_STUB_NESTED_ARMENTRY 179
	MY_STUB_NESTED_ARMENTRY 180
	MY_STUB_NESTED_ARMENTRY 181
	MY_STUB_NESTED_ARMENTRY 182
	MY_STUB_NESTED_ARMENTRY 183
	MY_STUB_NESTED_ARMENTRY 184
	MY_STUB_NESTED_ARMENTRY 185
	MY_STUB_NESTED_ARMENTRY 186
	MY_STUB_NESTED_ARMENTRY 187
	MY_STUB_NESTED_ARMENTRY 188
	MY_STUB_NESTED_ARMENTRY 189
	MY_STUB_NESTED_ARMENTRY 190
	MY_STUB_NESTED_ARMENTRY 191
	MY_STUB_NESTED_ARMENTRY 192
	MY_STUB_NESTED_ARMENTRY 193
	MY_STUB_NESTED_ARMENTRY 194
	MY_STUB_NESTED_ARMENTRY 195
	MY_STUB_NESTED_ARMENTRY 196
	MY_STUB_NESTED_ARMENTRY 197
	MY_STUB_NESTED_ARMENTRY 198
	MY_STUB_NESTED_ARMENTRY 199
	MY_STUB_NESTED_ARMENTRY 200
	MY_STUB_NESTED_ARMENTRY 201
	MY_STUB_NESTED_ARMENTRY 202
	MY_STUB_NESTED_ARMENTRY 203
	MY_STUB_NESTED_ARMENTRY 204
	MY_STUB_NESTED_ARMENTRY 205
	MY_STUB_NESTED_ARMENTRY 206
	MY_STUB_NESTED_ARMENTRY 207
	MY_STUB_NESTED_ARMENTRY 208
	MY_STUB_NESTED_ARMENTRY 209
	MY_STUB_NESTED_ARMENTRY 210
	MY_STUB_NESTED_ARMENTRY 211
	MY_STUB_NESTED_ARMENTRY 212
	MY_STUB_NESTED_ARMENTRY 213
	MY_STUB_NESTED_ARMENTRY 214
	MY_STUB_NESTED_ARMENTRY 215
	MY_STUB_NESTED_ARMENTRY 216
	MY_STUB_NESTED_ARMENTRY 217
	MY_STUB_NESTED_ARMENTRY 218
	MY_STUB_NESTED_ARMENTRY 219
	MY_STUB_NESTED_ARMENTRY 220
	MY_STUB_NESTED_ARMENTRY 221
	MY_STUB_NESTED_ARMENTRY 222
	MY_STUB_NESTED_ARMENTRY 223
	MY_STUB_NESTED_ARMENTRY 224
	MY_STUB_NESTED_ARMENTRY 225
	MY_STUB_NESTED_ARMENTRY 226
	MY_STUB_NESTED_ARMENTRY 227
	MY_STUB_NESTED_ARMENTRY 228
	MY_STUB_NESTED_ARMENTRY 229
	MY_STUB_NESTED_ARMENTRY 230
	MY_STUB_NESTED_ARMENTRY 231
	MY_STUB_NESTED_ARMENTRY 232
	MY_STUB_NESTED_ARMENTRY 233
	MY_STUB_NESTED_ARMENTRY 234
	MY_STUB_NESTED_ARMENTRY 235
	MY_STUB_NESTED_ARMENTRY 236
	MY_STUB_NESTED_ARMENTRY 237
	MY_STUB_NESTED_ARMENTRY 238
	MY_STUB_NESTED_ARMENTRY 239
	MY_STUB_NESTED_ARMENTRY 240
	MY_STUB_NESTED_ARMENTRY 241
	MY_STUB_NESTED_ARMENTRY 242
	MY_STUB_NESTED_ARMENTRY 243
	MY_STUB_NESTED_ARMENTRY 244
	MY_STUB_NESTED_ARMENTRY 245
	MY_STUB_NESTED_ARMENTRY 246
	MY_STUB_NESTED_ARMENTRY 247
	MY_STUB_NESTED_ARMENTRY 248
	MY_STUB_NESTED_ARMENTRY 249


	END

