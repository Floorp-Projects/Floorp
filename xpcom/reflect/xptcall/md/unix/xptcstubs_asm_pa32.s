        .LEVEL  1.1

curframesz  .EQU 128
; SharedStub has stack size of 128 bytes

lastframesz .EQU 64
; the StubN C++ function has a small stack size of 64 bytes

        .SPACE  $TEXT$,SORT=8
        .SUBSPA $CODE$,QUAD=0,ALIGN=4,ACCESS=0x2c,CODE_ONLY,SORT=24
SharedStub
        .PROC
        .CALLINFO CALLER,FRAME=80,SAVE_RP,ARGS_SAVED

  .ENTRY
        STW     %rp,-20(%sp)
        LDO     128(%sp),%sp

        STW     %r19,-32(%r30)
        STW     %r26,-36-curframesz(%r30) ; save arg0 in previous frame

        LDO     -80(%r30),%r28
        FSTD,MA %fr5,8(%r28)   ; save darg0
        FSTD,MA %fr7,8(%r28)   ; save darg1
        FSTW,MA %fr4L,4(%r28)  ; save farg0
        FSTW,MA %fr5L,4(%r28)  ; save farg1
        FSTW,MA %fr6L,4(%r28)  ; save farg2
        FSTW,MA %fr7L,4(%r28)  ; save farg3

        ; Former value of register 26 is already properly saved by StubN,
        ; but register 25-23 are not because of the arguments mismatch
        STW     %r25,-40-curframesz-lastframesz(%r30) ; save r25
        STW     %r24,-44-curframesz-lastframesz(%r30) ; save r24
        STW     %r23,-48-curframesz-lastframesz(%r30) ; save r23
        COPY    %r26,%r25                             ; method index is arg1
        LDW     -36-curframesz-lastframesz(%r30),%r26 ; self is arg0
        LDO     -40-curframesz-lastframesz(%r30),%r24 ; normal args is arg2
        LDO     -80(%r30),%r23                        ; floating args is arg3

        BL      .+8,%r2
        ADDIL   L'PrepareAndDispatch-$PIC_pcrel$0+4,%r2
        LDO     R'PrepareAndDispatch-$PIC_pcrel$1+8(%r1),%r1
$PIC_pcrel$0
        LDSID   (%r1),%r31
$PIC_pcrel$1
        MTSP    %r31,%sr0
        .CALL   ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,RTNVAL=GR
;in=23-26;out=28;
        BLE     0(%sr0,%r1)
        COPY    %r31,%r2

        LDW     -32(%r30),%r19

        LDW     -148(%sp),%rp
        BVE     (%rp)
  .EXIT
        LDO     -128(%sp),%sp


        .PROCEND        ;in=26;out=28;

  .ALIGN  8
        .SPACE  $TEXT$
        .SUBSPA $CODE$
        .IMPORT PrepareAndDispatch,CODE
        .EXPORT SharedStub,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR,LONG_RETURN
        .END

