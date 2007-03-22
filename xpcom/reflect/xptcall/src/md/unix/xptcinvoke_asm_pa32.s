  .LEVEL  1.1
framesz .EQU 128

; XPTC_InvokeByIndex(nsISuppots* that, PRUint32 methodIndex,
;   PRUint32 paramCount, nsXPTCVariant* params);

; g++ need to compile everything with -fvtable-thunks !

  .SPACE  $TEXT$,SORT=8
  .SUBSPA $CODE$,QUAD=0,ALIGN=4,ACCESS=0x2c,CODE_ONLY,SORT=24
XPTC_InvokeByIndex
  .PROC
  .CALLINFO CALLER,FRAME=72,ENTRY_GR=%r3,SAVE_RP,SAVE_SP,ARGS_SAVED,ALLOCA_FRAME

 ; frame marker takes 48 bytes,
 ; register spill area takes 8 bytes,
 ; local stack area takes 72 bytes result in 128 bytes total

  .ENTRY
        STW          %rp,-20(%sp)
        STW,MA       %r3,128(%sp)

        LDO     -framesz(%r30),%r28
        STW     %r28,-4(%r30)       ; save previous sp
        STW     %r19,-32(%r30)

        STW     %r26,-36-framesz(%r30)  ; save argument registers in
        STW     %r25,-40-framesz(%r30)  ; in PREVIOUS frame
        STW     %r24,-44-framesz(%r30)  ;
        STW     %r23,-48-framesz(%r30)  ;

        B,L     .+8,%r2
        ADDIL   L'invoke_count_bytes-$PIC_pcrel$1+4,%r2,%r1
        LDO     R'invoke_count_bytes-$PIC_pcrel$2+8(%r1),%r1
$PIC_pcrel$1
        LDSID   (%r1),%r31
$PIC_pcrel$2
        MTSP    %r31,%sr0
        .CALL   ARGW0=GR,ARGW1=GR,ARGW2=GR ;in=24,25,26;out=28
        BE,L    0(%sr0,%r1),%r31
        COPY    %r31,%r2

        CMPIB,>=        0,%r28, .+76
        COPY    %r30,%r3            ; copy stack ptr to saved stack ptr
        ADD     %r30,%r28,%r30      ; extend stack frame
        LDW     -4(%r3),%r28        ; move frame
        STW     %r28,-4(%r30)
        LDW     -8(%r3),%r28
        STW     %r28,-8(%r30)
        LDW     -12(%r3),%r28
        STW     %r28,-12(%r30)
        LDW     -16(%r3),%r28
        STW     %r28,-16(%r30)
        LDW     -20(%r3),%r28
        STW     %r28,-20(%r30)
        LDW     -24(%r3),%r28
        STW     %r28,-24(%r30)
        LDW     -28(%r3),%r28
        STW     %r28,-28(%r30)
        LDW     -32(%r3),%r28
        STW     %r28,-32(%r30)

        LDO     -40(%r30),%r26         ; load copy address
        LDW     -44-framesz(%r3),%r25  ; load rest of 2 arguments
        LDW     -48-framesz(%r3),%r24  ;

        LDW     -32(%r30),%r19 ; shared lib call destroys r19; reload
        B,L     .+8,%r2
        ADDIL   L'invoke_copy_to_stack-$PIC_pcrel$3+4,%r2,%r1
        LDO     R'invoke_copy_to_stack-$PIC_pcrel$4+8(%r1),%r1
$PIC_pcrel$3
        LDSID   (%r1),%r31
$PIC_pcrel$4
        MTSP    %r31,%sr0
        .CALL   ARGW0=GR,ARGW1=GR,ARGW2=GR ;in=24,25,26
        BE,L    0(%sr0,%r1),%r31
        COPY    %r31,%r2

        LDO     -48(%r30),%r20
        EXTRW,U,= %r28,31,1,%r22
        FLDD    0(%r20),%fr7  ; load double arg 1
        EXTRW,U,= %r28,30,1,%r22
        FLDW    8(%r20),%fr5L ; load float arg 1
        EXTRW,U,= %r28,29,1,%r22
        FLDW    4(%r20),%fr6L ; load float arg 2
        EXTRW,U,= %r28,28,1,%r22
        FLDW    0(%r20),%fr7L ; load float arg 3

        LDW     -36-framesz(%r3),%r26  ; load ptr to 'that'
        LDW     -40(%r30),%r25  ; load the rest of dispatch argument registers
        LDW     -44(%r30),%r24
        LDW     -48(%r30),%r23

        LDW     -36-framesz(%r3),%r20  ; load vtable addr
        LDW     -40-framesz(%r3),%r28  ; load index
        LDW     0(%r20),%r20    ; follow vtable
        LDO     16(%r20),%r20   ; offset vtable by 16 bytes (g++: 8, aCC: 16)
        SH2ADDL %r28,%r20,%r28  ; add 4*index to vtable entry
        LDW     0(%r28),%r22    ; load vtable entry

        B,L     .+8,%r2
        ADDIL   L'$$dyncall_external-$PIC_pcrel$5+4,%r2,%r1
        LDO     R'$$dyncall_external-$PIC_pcrel$6+8(%r1),%r1
$PIC_pcrel$5
        LDSID   (%r1),%r31
$PIC_pcrel$6
        MTSP    %r31,%sr0
        .CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,RTNVAL=GR
;in=22-26;out=28;
        BE,L    0(%sr0,%r1),%r31
        COPY    %r31,%r2

        LDW     -32(%r30),%r19
        COPY    %r3,%r30              ; restore saved stack ptr

        LDW          -148(%sp),%rp
        BVE             (%rp)
  .EXIT
        LDW,MB       -128(%sp),%r3

  .PROCEND  ;in=23,24,25,26;

  .ALIGN  8
  .SPACE  $TEXT$
  .SUBSPA $CODE$
  .IMPORT $$dyncall_external,MILLICODE
  .IMPORT invoke_count_bytes,CODE
  .IMPORT invoke_copy_to_stack,CODE
  .EXPORT XPTC_InvokeByIndex,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,RTNVAL=GR,LONG_RETURN
  .END

