; 7zAsm.asm -- ASM macros
; 2018-02-03 : Igor Pavlov : Public domain

MY_ASM_START macro
  ifdef x64
    .code
  else
    .386
    .model flat
    _TEXT$00 SEGMENT PARA PUBLIC 'CODE'
  endif
endm

MY_PROC macro name:req, numParams:req
  align 16
  proc_numParams = numParams
  ifdef x64
    proc_name equ name
  else
    proc_name equ @CatStr(@,name,@, %numParams * 4)
  endif
  proc_name PROC
endm

MY_ENDP macro
  ifdef x64
    ret
  else
    if proc_numParams LT 3
      ret
    else
      ret (proc_numParams - 2) * 4
    endif
  endif
  proc_name ENDP
endm

ifdef x64
  REG_SIZE equ 8
  REG_LOGAR_SIZE equ 3
else
  REG_SIZE equ 4
  REG_LOGAR_SIZE equ 2
endif

  x0 equ EAX
  x1 equ ECX
  x2 equ EDX
  x3 equ EBX
  x4 equ ESP
  x5 equ EBP
  x6 equ ESI
  x7 equ EDI

  x0_W equ AX
  x1_W equ CX
  x2_W equ DX
  x3_W equ BX

  x5_W equ BP
  x6_W equ SI
  x7_W equ DI

  x0_L equ AL
  x1_L equ CL
  x2_L equ DL
  x3_L equ BL

  x0_H equ AH
  x1_H equ CH
  x2_H equ DH
  x3_H equ BH

ifdef x64
  x5_L equ BPL
  x6_L equ SIL
  x7_L equ DIL

  r0 equ RAX
  r1 equ RCX
  r2 equ RDX
  r3 equ RBX
  r4 equ RSP
  r5 equ RBP
  r6 equ RSI
  r7 equ RDI
  x8 equ r8d
  x9 equ r9d
  x10 equ r10d
  x11 equ r11d
  x12 equ r12d
  x13 equ r13d
  x14 equ r14d
  x15 equ r15d
else
  r0 equ x0
  r1 equ x1
  r2 equ x2
  r3 equ x3
  r4 equ x4
  r5 equ x5
  r6 equ x6
  r7 equ x7
endif

MY_PUSH_4_REGS macro
    push    r3
    push    r5
    push    r6
    push    r7
endm

MY_POP_4_REGS macro
    pop     r7
    pop     r6
    pop     r5
    pop     r3
endm


ifdef x64

; for WIN64-x64 ABI:

REG_PARAM_0 equ r1
REG_PARAM_1 equ r2
REG_PARAM_2 equ r8
REG_PARAM_3 equ r9

MY_PUSH_PRESERVED_REGS macro
    MY_PUSH_4_REGS
    push    r12
    push    r13
    push    r14
    push    r15
endm


MY_POP_PRESERVED_REGS macro
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    MY_POP_4_REGS
endm

endif
