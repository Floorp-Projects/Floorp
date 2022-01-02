; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef __LP64__
        .LEVEL   2.0W
#else
;       .LEVEL   1.1
;       .ALLOW   2.0N
        .LEVEL   2.0
#endif
        .SPACE   $TEXT$,SORT=8
        .SUBSPA  $CODE$,QUAD=0,ALIGN=4,ACCESS=0x2c,CODE_ONLY,SORT=24

; ***************************************************************
;
;                 maxpy_[little/big]
;
; ***************************************************************

; There is no default -- you must specify one or the other.
#define LITTLE_WORDIAN 1

#ifdef LITTLE_WORDIAN
#define EIGHT 8
#define SIXTEEN 16
#define THIRTY_TWO 32
#define UN_EIGHT -8
#define UN_SIXTEEN -16
#define UN_TWENTY_FOUR -24
#endif

#ifdef BIG_WORDIAN
#define EIGHT -8
#define SIXTEEN -16
#define THIRTY_TWO -32
#define UN_EIGHT 8
#define UN_SIXTEEN 16
#define UN_TWENTY_FOUR 24
#endif

; This performs a multiple-precision integer version of "daxpy",
; Using the selected addressing direction.  "Little-wordian" means that
; the least significant word of a number is stored at the lowest address.
; "Big-wordian" means that the most significant word is at the lowest
; address.  Either way, the incoming address of the vector is that
; of the least significant word.  That means that, for little-wordian
; addressing, we move the address upward as we propagate carries
; from the least significant word to the most significant.  For
; big-wordian we move the address downward.

; We use the following registers:
;
;     r2   return PC, of course
;     r26 = arg1 =  length
;     r25 = arg2 =  address of scalar
;     r24 = arg3 =  multiplicand vector
;     r23 = arg4 =  result vector
;
;     fr9 = scalar loaded once only from r25

; The cycle counts shown in the bodies below are simply the result of a
; scheduling by hand.  The actual PCX-U hardware does it differently.
; The intention is that the overall speed is the same.

; The pipeline startup and shutdown code is constructed in the usual way,
; by taking the loop bodies and removing unnecessary instructions.
; We have left the comments describing cycle numbers in the code.
; These are intended for reference when comparing with the main loop,
; and have no particular relationship to actual cycle numbers.

#ifdef LITTLE_WORDIAN
maxpy_little
#else
maxpy_big
#endif
        .PROC
        .CALLINFO FRAME=120,ENTRY_GR=4
        .ENTRY
        STW,MA  %r3,128(%sp)
        STW     %r4,-124(%sp)

        ADDIB,< -1,%r26,$L0         ; If N = 0, exit immediately.
        FLDD    0(%r25),%fr9        ; fr9 = scalar

; First startup

        FLDD    0(%r24),%fr24       ; Cycle 1
        XMPYU   %fr9R,%fr24R,%fr27  ; Cycle 3
        XMPYU   %fr9R,%fr24L,%fr25  ; Cycle 4
        XMPYU   %fr9L,%fr24L,%fr26  ; Cycle 5
        CMPIB,> 3,%r26,$N_IS_SMALL  ; Pick out cases N = 1, 2, or 3
        XMPYU   %fr9L,%fr24R,%fr24  ; Cycle 6
        FLDD    EIGHT(%r24),%fr28   ; Cycle 8
        XMPYU   %fr9L,%fr28R,%fr31  ; Cycle 10
        FSTD    %fr24,-96(%sp)
        XMPYU   %fr9R,%fr28L,%fr30  ; Cycle 11
        FSTD    %fr25,-80(%sp)
        LDO     SIXTEEN(%r24),%r24  ; Cycle 12
        FSTD    %fr31,-64(%sp)
        XMPYU   %fr9R,%fr28R,%fr29  ; Cycle 13
        FSTD    %fr27,-48(%sp)

; Second startup

        XMPYU   %fr9L,%fr28L,%fr28  ; Cycle 1
        FSTD    %fr30,-56(%sp)
        FLDD    0(%r24),%fr24

        FSTD    %fr26,-88(%sp)      ; Cycle 2

        XMPYU   %fr9R,%fr24R,%fr27  ; Cycle 3
        FSTD    %fr28,-104(%sp)

        XMPYU   %fr9R,%fr24L,%fr25  ; Cycle 4
        LDD     -96(%sp),%r3
        FSTD    %fr29,-72(%sp)

        XMPYU   %fr9L,%fr24L,%fr26  ; Cycle 5
        LDD     -64(%sp),%r19
        LDD     -80(%sp),%r21

        XMPYU   %fr9L,%fr24R,%fr24  ; Cycle 6
        LDD     -56(%sp),%r20
        ADD     %r21,%r3,%r3

        ADD,DC  %r20,%r19,%r19      ; Cycle 7
        LDD     -88(%sp),%r4
        SHRPD   %r3,%r0,32,%r21
        LDD     -48(%sp),%r1

        FLDD    EIGHT(%r24),%fr28   ; Cycle 8
        LDD     -104(%sp),%r31
        ADD,DC  %r0,%r0,%r20
        SHRPD   %r19,%r3,32,%r3

        LDD     -72(%sp),%r29       ; Cycle 9
        SHRPD   %r20,%r19,32,%r20
        ADD     %r21,%r1,%r1

        XMPYU   %fr9L,%fr28R,%fr31  ; Cycle 10
        ADD,DC  %r3,%r4,%r4
        FSTD    %fr24,-96(%sp)

        XMPYU   %fr9R,%fr28L,%fr30  ; Cycle 11
        ADD,DC  %r0,%r20,%r20
        LDD     0(%r23),%r3
        FSTD    %fr25,-80(%sp)

        LDO     SIXTEEN(%r24),%r24  ; Cycle 12
        FSTD    %fr31,-64(%sp)

        XMPYU   %fr9R,%fr28R,%fr29  ; Cycle 13
        ADD     %r0,%r0,%r0         ; clear the carry bit
        ADDIB,<= -4,%r26,$ENDLOOP   ; actually happens in cycle 12
        FSTD    %fr27,-48(%sp)
;        MFCTL   %cr16,%r21         ; for timing
;        STD     %r21,-112(%sp)

; Here is the loop.

$LOOP   XMPYU   %fr9L,%fr28L,%fr28  ; Cycle 1
        ADD,DC  %r29,%r4,%r4
        FSTD    %fr30,-56(%sp)
        FLDD    0(%r24),%fr24

        LDO     SIXTEEN(%r23),%r23  ; Cycle 2
        ADD,DC  %r0,%r20,%r20
        FSTD    %fr26,-88(%sp)

        XMPYU   %fr9R,%fr24R,%fr27  ; Cycle 3
        ADD     %r3,%r1,%r1
        FSTD    %fr28,-104(%sp)
        LDD     UN_EIGHT(%r23),%r21

        XMPYU   %fr9R,%fr24L,%fr25  ; Cycle 4
        ADD,DC  %r21,%r4,%r28
        FSTD    %fr29,-72(%sp)    
        LDD     -96(%sp),%r3

        XMPYU   %fr9L,%fr24L,%fr26  ; Cycle 5
        ADD,DC  %r20,%r31,%r22
        LDD     -64(%sp),%r19
        LDD     -80(%sp),%r21

        XMPYU   %fr9L,%fr24R,%fr24  ; Cycle 6
        ADD     %r21,%r3,%r3
        LDD     -56(%sp),%r20
        STD     %r1,UN_SIXTEEN(%r23)

        ADD,DC  %r20,%r19,%r19      ; Cycle 7
        SHRPD   %r3,%r0,32,%r21
        LDD     -88(%sp),%r4
        LDD     -48(%sp),%r1

        ADD,DC  %r0,%r0,%r20        ; Cycle 8
        SHRPD   %r19,%r3,32,%r3
        FLDD    EIGHT(%r24),%fr28
        LDD     -104(%sp),%r31

        SHRPD   %r20,%r19,32,%r20   ; Cycle 9
        ADD     %r21,%r1,%r1
        STD     %r28,UN_EIGHT(%r23)
        LDD     -72(%sp),%r29

        XMPYU   %fr9L,%fr28R,%fr31  ; Cycle 10
        ADD,DC  %r3,%r4,%r4
        FSTD    %fr24,-96(%sp)

        XMPYU   %fr9R,%fr28L,%fr30  ; Cycle 11
        ADD,DC  %r0,%r20,%r20
        FSTD    %fr25,-80(%sp)
        LDD     0(%r23),%r3

        LDO     SIXTEEN(%r24),%r24  ; Cycle 12
        FSTD    %fr31,-64(%sp)

        XMPYU   %fr9R,%fr28R,%fr29  ; Cycle 13
        ADD     %r22,%r1,%r1
        ADDIB,> -2,%r26,$LOOP       ; actually happens in cycle 12
        FSTD    %fr27,-48(%sp)

$ENDLOOP

; Shutdown code, first stage.

;        MFCTL   %cr16,%r21         ; for timing
;        STD     %r21,UN_SIXTEEN(%r23)
;        LDD     -112(%sp),%r21
;        STD     %r21,UN_EIGHT(%r23)

        XMPYU   %fr9L,%fr28L,%fr28  ; Cycle 1
        ADD,DC  %r29,%r4,%r4
        CMPIB,= 0,%r26,$ONEMORE
        FSTD    %fr30,-56(%sp)

        LDO     SIXTEEN(%r23),%r23  ; Cycle 2
        ADD,DC  %r0,%r20,%r20
        FSTD    %fr26,-88(%sp)

        ADD     %r3,%r1,%r1         ; Cycle 3
        FSTD    %fr28,-104(%sp)
        LDD     UN_EIGHT(%r23),%r21

        ADD,DC  %r21,%r4,%r28       ; Cycle 4
        FSTD    %fr29,-72(%sp)    
        STD     %r28,UN_EIGHT(%r23) ; moved up from cycle 9
        LDD     -96(%sp),%r3

        ADD,DC  %r20,%r31,%r22      ; Cycle 5
        STD     %r1,UN_SIXTEEN(%r23)
$JOIN4
        LDD     -64(%sp),%r19
        LDD     -80(%sp),%r21

        ADD     %r21,%r3,%r3        ; Cycle 6
        LDD     -56(%sp),%r20

        ADD,DC  %r20,%r19,%r19      ; Cycle 7
        SHRPD   %r3,%r0,32,%r21
        LDD     -88(%sp),%r4
        LDD     -48(%sp),%r1

        ADD,DC  %r0,%r0,%r20        ; Cycle 8
        SHRPD   %r19,%r3,32,%r3
        LDD     -104(%sp),%r31

        SHRPD   %r20,%r19,32,%r20   ; Cycle 9
        ADD     %r21,%r1,%r1
        LDD     -72(%sp),%r29

        ADD,DC  %r3,%r4,%r4         ; Cycle 10

        ADD,DC  %r0,%r20,%r20       ; Cycle 11
        LDD     0(%r23),%r3

        ADD     %r22,%r1,%r1        ; Cycle 13

; Shutdown code, second stage.

        ADD,DC  %r29,%r4,%r4        ; Cycle 1

        LDO     SIXTEEN(%r23),%r23  ; Cycle 2
        ADD,DC  %r0,%r20,%r20

        LDD     UN_EIGHT(%r23),%r21 ; Cycle 3
        ADD     %r3,%r1,%r1

        ADD,DC  %r21,%r4,%r28       ; Cycle 4

        ADD,DC  %r20,%r31,%r22      ; Cycle 5

        STD     %r1,UN_SIXTEEN(%r23); Cycle 6

        STD     %r28,UN_EIGHT(%r23) ; Cycle 9

        LDD     0(%r23),%r3         ; Cycle 11

; Shutdown code, third stage.

        LDO     SIXTEEN(%r23),%r23
        ADD     %r3,%r22,%r1
$JOIN1  ADD,DC  %r0,%r0,%r21
        CMPIB,*= 0,%r21,$L0         ; if no overflow, exit
        STD     %r1,UN_SIXTEEN(%r23)

; Final carry propagation

$FINAL1 LDO     EIGHT(%r23),%r23
        LDD     UN_SIXTEEN(%r23),%r21
        ADDI    1,%r21,%r21
        CMPIB,*= 0,%r21,$FINAL1     ; Keep looping if there is a carry.
        STD     %r21,UN_SIXTEEN(%r23)
        B       $L0
        NOP

; Here is the code that handles the difficult cases N=1, N=2, and N=3.
; We do the usual trick -- branch out of the startup code at appropriate
; points, and branch into the shutdown code.

$N_IS_SMALL
        CMPIB,= 0,%r26,$N_IS_ONE
        FSTD    %fr24,-96(%sp)      ; Cycle 10
        FLDD    EIGHT(%r24),%fr28   ; Cycle 8
        XMPYU   %fr9L,%fr28R,%fr31  ; Cycle 10
        XMPYU   %fr9R,%fr28L,%fr30  ; Cycle 11
        FSTD    %fr25,-80(%sp)
        FSTD    %fr31,-64(%sp)      ; Cycle 12
        XMPYU   %fr9R,%fr28R,%fr29  ; Cycle 13
        FSTD    %fr27,-48(%sp)
        XMPYU   %fr9L,%fr28L,%fr28  ; Cycle 1
        CMPIB,= 2,%r26,$N_IS_THREE
        FSTD    %fr30,-56(%sp)

; N = 2
        FSTD    %fr26,-88(%sp)      ; Cycle 2
        FSTD    %fr28,-104(%sp)     ; Cycle 3
        LDD     -96(%sp),%r3        ; Cycle 4
        FSTD    %fr29,-72(%sp)
        B       $JOIN4
        ADD     %r0,%r0,%r22

$N_IS_THREE
        FLDD    SIXTEEN(%r24),%fr24
        FSTD    %fr26,-88(%sp)      ; Cycle 2
        XMPYU   %fr9R,%fr24R,%fr27  ; Cycle 3
        FSTD    %fr28,-104(%sp)
        XMPYU   %fr9R,%fr24L,%fr25  ; Cycle 4
        LDD     -96(%sp),%r3
        FSTD    %fr29,-72(%sp)
        XMPYU   %fr9L,%fr24L,%fr26  ; Cycle 5
        LDD     -64(%sp),%r19
        LDD     -80(%sp),%r21
        B       $JOIN3
        ADD     %r0,%r0,%r22

$N_IS_ONE
        FSTD    %fr25,-80(%sp)
        FSTD    %fr27,-48(%sp)
        FSTD    %fr26,-88(%sp)      ; Cycle 2
        B       $JOIN5
        ADD     %r0,%r0,%r22

; We came out of the unrolled loop with wrong parity.  Do one more
; single cycle.  This is quite tricky, because of the way the
; carry chains and SHRPD chains have been chopped up.

$ONEMORE

        FLDD    0(%r24),%fr24

        LDO     SIXTEEN(%r23),%r23  ; Cycle 2
        ADD,DC  %r0,%r20,%r20
        FSTD    %fr26,-88(%sp)

        XMPYU   %fr9R,%fr24R,%fr27  ; Cycle 3
        FSTD    %fr28,-104(%sp)
        LDD     UN_EIGHT(%r23),%r21
        ADD     %r3,%r1,%r1

        XMPYU   %fr9R,%fr24L,%fr25  ; Cycle 4
        ADD,DC  %r21,%r4,%r28
        STD     %r28,UN_EIGHT(%r23) ; moved from cycle 9
        LDD     -96(%sp),%r3
        FSTD    %fr29,-72(%sp)    

        XMPYU   %fr9L,%fr24L,%fr26  ; Cycle 5
        ADD,DC  %r20,%r31,%r22
        LDD     -64(%sp),%r19
        LDD     -80(%sp),%r21

        STD     %r1,UN_SIXTEEN(%r23); Cycle 6
$JOIN3
        XMPYU   %fr9L,%fr24R,%fr24
        LDD     -56(%sp),%r20
        ADD     %r21,%r3,%r3

        ADD,DC  %r20,%r19,%r19      ; Cycle 7
        LDD     -88(%sp),%r4
        SHRPD   %r3,%r0,32,%r21
        LDD     -48(%sp),%r1

        LDD     -104(%sp),%r31      ; Cycle 8
        ADD,DC  %r0,%r0,%r20
        SHRPD   %r19,%r3,32,%r3

        LDD     -72(%sp),%r29       ; Cycle 9
        SHRPD   %r20,%r19,32,%r20
        ADD     %r21,%r1,%r1

        ADD,DC  %r3,%r4,%r4         ; Cycle 10
        FSTD    %fr24,-96(%sp)

        ADD,DC  %r0,%r20,%r20       ; Cycle 11
        LDD     0(%r23),%r3
        FSTD    %fr25,-80(%sp)

        ADD     %r22,%r1,%r1        ; Cycle 13
        FSTD    %fr27,-48(%sp)

; Shutdown code, stage 1-1/2.

        ADD,DC  %r29,%r4,%r4        ; Cycle 1

        LDO     SIXTEEN(%r23),%r23  ; Cycle 2
        ADD,DC  %r0,%r20,%r20     
        FSTD    %fr26,-88(%sp)

        LDD     UN_EIGHT(%r23),%r21 ; Cycle 3
        ADD     %r3,%r1,%r1

        ADD,DC  %r21,%r4,%r28       ; Cycle 4
        STD     %r28,UN_EIGHT(%r23) ; moved from cycle 9

        ADD,DC  %r20,%r31,%r22      ; Cycle 5
        STD     %r1,UN_SIXTEEN(%r23)
$JOIN5
        LDD     -96(%sp),%r3        ; moved from cycle 4
        LDD     -80(%sp),%r21
        ADD     %r21,%r3,%r3        ; Cycle 6
        ADD,DC  %r0,%r0,%r19        ; Cycle 7
        LDD     -88(%sp),%r4
        SHRPD   %r3,%r0,32,%r21
        LDD     -48(%sp),%r1
        SHRPD   %r19,%r3,32,%r3     ; Cycle 8
        ADD     %r21,%r1,%r1        ; Cycle 9
        ADD,DC  %r3,%r4,%r4         ; Cycle 10
        LDD     0(%r23),%r3         ; Cycle 11
        ADD     %r22,%r1,%r1        ; Cycle 13

; Shutdown code, stage 2-1/2.

        ADD,DC  %r0,%r4,%r4         ; Cycle 1
        LDO     SIXTEEN(%r23),%r23  ; Cycle 2
        LDD     UN_EIGHT(%r23),%r21 ; Cycle 3
        ADD     %r3,%r1,%r1
        STD     %r1,UN_SIXTEEN(%r23)
        ADD,DC  %r21,%r4,%r1
        B       $JOIN1
        LDO     EIGHT(%r23),%r23

; exit

$L0
        LDW     -124(%sp),%r4
        BVE     (%r2)
        .EXIT
        LDW,MB  -128(%sp),%r3

        .PROCEND

; ***************************************************************
;
;                 add_diag_[little/big]
;
; ***************************************************************

; The arguments are as follows:
;     r2   return PC, of course
;     r26 = arg1 =  length
;     r25 = arg2 =  vector to square
;     r24 = arg3 =  result vector

#ifdef LITTLE_WORDIAN
add_diag_little
#else
add_diag_big
#endif
        .PROC
        .CALLINFO FRAME=120,ENTRY_GR=4
        .ENTRY
        STW,MA  %r3,128(%sp)
        STW     %r4,-124(%sp)

        ADDIB,< -1,%r26,$Z0         ; If N=0, exit immediately.
        NOP

; Startup code

        FLDD    0(%r25),%fr7        ; Cycle 2 (alternate body)
        XMPYU   %fr7R,%fr7R,%fr29   ; Cycle 4
        XMPYU   %fr7L,%fr7R,%fr27   ; Cycle 5
        XMPYU   %fr7L,%fr7L,%fr30
        LDO     SIXTEEN(%r25),%r25  ; Cycle 6
        FSTD    %fr29,-88(%sp)
        FSTD    %fr27,-72(%sp)      ; Cycle 7
        CMPIB,= 0,%r26,$DIAG_N_IS_ONE ; Cycle 1 (main body)
        FSTD    %fr30,-96(%sp)
        FLDD    UN_EIGHT(%r25),%fr7 ; Cycle 2
        LDD     -88(%sp),%r22       ; Cycle 3
        LDD     -72(%sp),%r31       ; Cycle 4
        XMPYU   %fr7R,%fr7R,%fr28
        XMPYU   %fr7L,%fr7R,%fr24   ; Cycle 5
        XMPYU   %fr7L,%fr7L,%fr31
        LDD     -96(%sp),%r20       ; Cycle 6
        FSTD    %fr28,-80(%sp)
        ADD     %r0,%r0,%r0         ; clear the carry bit
        ADDIB,<= -2,%r26,$ENDDIAGLOOP ; Cycle 7
        FSTD    %fr24,-64(%sp)

; Here is the loop.  It is unrolled twice, modelled after the "alternate body" and then the "main body".

$DIAGLOOP
        SHRPD   %r31,%r0,31,%r3     ; Cycle 1 (alternate body)
        LDO     SIXTEEN(%r25),%r25
        LDD     0(%r24),%r1
        FSTD    %fr31,-104(%sp)
        SHRPD   %r0,%r31,31,%r4     ; Cycle 2
        ADD,DC  %r22,%r3,%r3
        FLDD    UN_SIXTEEN(%r25),%fr7   
        ADD,DC  %r0,%r20,%r20       ; Cycle 3
        ADD     %r1,%r3,%r3
        XMPYU   %fr7R,%fr7R,%fr29   ; Cycle 4
        LDD     -80(%sp),%r21
        STD     %r3,0(%r24)
        XMPYU   %fr7L,%fr7R,%fr27   ; Cycle 5
        XMPYU   %fr7L,%fr7L,%fr30
        LDD     -64(%sp),%r29       
        LDD     EIGHT(%r24),%r1  
        ADD,DC  %r4,%r20,%r20       ; Cycle 6
        LDD     -104(%sp),%r19
        FSTD    %fr29,-88(%sp)
        ADD     %r20,%r1,%r1        ; Cycle 7
        FSTD    %fr27,-72(%sp)
        SHRPD   %r29,%r0,31,%r4     ; Cycle 1 (main body)
        LDO     THIRTY_TWO(%r24),%r24
        LDD     UN_SIXTEEN(%r24),%r28
        FSTD    %fr30,-96(%sp)
        SHRPD   %r0,%r29,31,%r3     ; Cycle 2
        ADD,DC  %r21,%r4,%r4
        FLDD    UN_EIGHT(%r25),%fr7
        STD     %r1,UN_TWENTY_FOUR(%r24)
        ADD,DC  %r0,%r19,%r19       ; Cycle 3
        ADD     %r28,%r4,%r4
        XMPYU   %fr7R,%fr7R,%fr28   ; Cycle 4
        LDD     -88(%sp),%r22
        STD     %r4,UN_SIXTEEN(%r24)
        XMPYU   %fr7L,%fr7R,%fr24   ; Cycle 5
        XMPYU   %fr7L,%fr7L,%fr31
        LDD     -72(%sp),%r31
        LDD     UN_EIGHT(%r24),%r28
        ADD,DC  %r3,%r19,%r19       ; Cycle 6
        LDD     -96(%sp),%r20
        FSTD    %fr28,-80(%sp)
        ADD     %r19,%r28,%r28      ; Cycle 7
        FSTD    %fr24,-64(%sp)
        ADDIB,> -2,%r26,$DIAGLOOP   ; Cycle 8
        STD     %r28,UN_EIGHT(%r24)

$ENDDIAGLOOP

        ADD,DC  %r0,%r22,%r22    
        CMPIB,= 0,%r26,$ONEMOREDIAG
        SHRPD   %r31,%r0,31,%r3

; Shutdown code, first stage.

        FSTD    %fr31,-104(%sp)     ; Cycle 1 (alternate body)
        LDD     0(%r24),%r28
        SHRPD   %r0,%r31,31,%r4     ; Cycle 2
        ADD     %r3,%r22,%r3
        ADD,DC  %r0,%r20,%r20       ; Cycle 3
        LDD     -80(%sp),%r21
        ADD     %r3,%r28,%r3
        LDD     -64(%sp),%r29       ; Cycle 4
        STD     %r3,0(%r24)
        LDD     EIGHT(%r24),%r1     ; Cycle 5
        LDO     SIXTEEN(%r25),%r25  ; Cycle 6
        LDD     -104(%sp),%r19
        ADD,DC  %r4,%r20,%r20
        ADD     %r20,%r1,%r1        ; Cycle 7
        ADD,DC  %r0,%r21,%r21       ; Cycle 8
        STD     %r1,EIGHT(%r24)

; Shutdown code, second stage.

        SHRPD   %r29,%r0,31,%r4     ; Cycle 1 (main body)
        LDO     THIRTY_TWO(%r24),%r24
        LDD     UN_SIXTEEN(%r24),%r1
        SHRPD   %r0,%r29,31,%r3      ; Cycle 2
        ADD     %r4,%r21,%r4
        ADD,DC  %r0,%r19,%r19       ; Cycle 3
        ADD     %r4,%r1,%r4
        STD     %r4,UN_SIXTEEN(%r24); Cycle 4
        LDD     UN_EIGHT(%r24),%r28 ; Cycle 5
        ADD,DC  %r3,%r19,%r19       ; Cycle 6       
        ADD     %r19,%r28,%r28      ; Cycle 7
        ADD,DC  %r0,%r0,%r22        ; Cycle 8
        CMPIB,*= 0,%r22,$Z0         ; if no overflow, exit
        STD     %r28,UN_EIGHT(%r24)

; Final carry propagation

$FDIAG2
        LDO     EIGHT(%r24),%r24
        LDD     UN_EIGHT(%r24),%r26
        ADDI    1,%r26,%r26
        CMPIB,*= 0,%r26,$FDIAG2     ; Keep looping if there is a carry.
        STD     %r26,UN_EIGHT(%r24)

        B   $Z0
        NOP

; Here is the code that handles the difficult case N=1.
; We do the usual trick -- branch out of the startup code at appropriate
; points, and branch into the shutdown code.

$DIAG_N_IS_ONE

        LDD     -88(%sp),%r22
        LDD     -72(%sp),%r31
        B       $JOINDIAG
        LDD     -96(%sp),%r20

; We came out of the unrolled loop with wrong parity.  Do one more
; single cycle.  This is the "alternate body".  It will, of course,
; give us opposite registers from the other case, so we need
; completely different shutdown code.

$ONEMOREDIAG
        FSTD    %fr31,-104(%sp)     ; Cycle 1 (alternate body)
        LDD     0(%r24),%r28
        FLDD    0(%r25),%fr7        ; Cycle 2
        SHRPD   %r0,%r31,31,%r4
        ADD     %r3,%r22,%r3
        ADD,DC  %r0,%r20,%r20       ; Cycle 3
        LDD     -80(%sp),%r21
        ADD     %r3,%r28,%r3
        LDD     -64(%sp),%r29       ; Cycle 4
        STD     %r3,0(%r24)
        XMPYU   %fr7R,%fr7R,%fr29
        LDD     EIGHT(%r24),%r1     ; Cycle 5
        XMPYU   %fr7L,%fr7R,%fr27
        XMPYU   %fr7L,%fr7L,%fr30
        LDD     -104(%sp),%r19      ; Cycle 6
        FSTD    %fr29,-88(%sp)
        ADD,DC  %r4,%r20,%r20
        FSTD    %fr27,-72(%sp)      ; Cycle 7
        ADD     %r20,%r1,%r1
        ADD,DC  %r0,%r21,%r21       ; Cycle 8
        STD     %r1,EIGHT(%r24)

; Shutdown code, first stage.

        SHRPD   %r29,%r0,31,%r4     ; Cycle 1 (main body)
        LDO     THIRTY_TWO(%r24),%r24
        FSTD    %fr30,-96(%sp)
        LDD     UN_SIXTEEN(%r24),%r1
        SHRPD   %r0,%r29,31,%r3     ; Cycle 2
        ADD     %r4,%r21,%r4
        ADD,DC  %r0,%r19,%r19       ; Cycle 3
        LDD     -88(%sp),%r22
        ADD     %r4,%r1,%r4
        LDD     -72(%sp),%r31       ; Cycle 4
        STD     %r4,UN_SIXTEEN(%r24)
        LDD     UN_EIGHT(%r24),%r28 ; Cycle 5
        LDD     -96(%sp),%r20       ; Cycle 6
        ADD,DC  %r3,%r19,%r19
        ADD     %r19,%r28,%r28      ; Cycle 7
        ADD,DC  %r0,%r22,%r22       ; Cycle 8
        STD     %r28,UN_EIGHT(%r24)

; Shutdown code, second stage.

$JOINDIAG
        SHRPD   %r31,%r0,31,%r3     ; Cycle 1 (alternate body)
        LDD     0(%r24),%r28        
        SHRPD   %r0,%r31,31,%r4     ; Cycle 2
        ADD     %r3,%r22,%r3
        ADD,DC  %r0,%r20,%r20       ; Cycle 3
        ADD     %r3,%r28,%r3
        STD     %r3,0(%r24)         ; Cycle 4
        LDD     EIGHT(%r24),%r1     ; Cycle 5
        ADD,DC  %r4,%r20,%r20
        ADD     %r20,%r1,%r1        ; Cycle 7
        ADD,DC  %r0,%r0,%r21        ; Cycle 8
        CMPIB,*= 0,%r21,$Z0         ; if no overflow, exit
        STD     %r1,EIGHT(%r24)

; Final carry propagation

$FDIAG1
        LDO     EIGHT(%r24),%r24
        LDD     EIGHT(%r24),%r26
        ADDI    1,%r26,%r26
        CMPIB,*= 0,%r26,$FDIAG1    ; Keep looping if there is a carry.
        STD     %r26,EIGHT(%r24)

$Z0
        LDW     -124(%sp),%r4
        BVE     (%r2)
        .EXIT
        LDW,MB  -128(%sp),%r3
        .PROCEND
;	.ALLOW

        .SPACE         $TEXT$
        .SUBSPA        $CODE$
#ifdef LITTLE_WORDIAN
#ifdef __GNUC__
; GNU-as (as of 2.19) does not support LONG_RETURN
        .EXPORT        maxpy_little,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
        .EXPORT        add_diag_little,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR
#else
        .EXPORT        maxpy_little,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,LONG_RETURN
        .EXPORT        add_diag_little,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,LONG_RETURN
#endif
#else
        .EXPORT        maxpy_big,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,LONG_RETURN
        .EXPORT        add_diag_big,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,LONG_RETURN
#endif
        .END


; How to use "maxpy_PA20_little" and "maxpy_PA20_big"
; 
; The routine "maxpy_PA20_little" or "maxpy_PA20_big"
; performs a 64-bit x any-size multiply, and adds the
; result to an area of memory.  That is, it performs
; something like
; 
;      A B C D
;    *       Z
;   __________
;    P Q R S T
; 
; and then adds the "PQRST" vector into an area of memory,
; handling all carries.
; 
; Digression on nomenclature and endian-ness:
; 
; Each of the capital letters in the above represents a 64-bit
; quantity.  That is, you could think of the discussion as
; being in terms of radix-16-quintillion arithmetic.  The data
; type being manipulated is "unsigned long long int".  This
; requires the 64-bit extension of the HP-UX C compiler,
; available at release 10.  You need these compiler flags to
; enable these extensions:
; 
;       -Aa +e +DA2.0 +DS2.0
; 
; (The first specifies ANSI C, the second enables the
; extensions, which are beyond ANSI C, and the third and
; fourth tell the compiler to use whatever features of the
; PA2.0 architecture it wishes, in order to made the code more
; efficient.  Since the presence of the assembly code will
; make the program unable to run on anything less than PA2.0,
; you might as well gain the performance enhancements in the C
; code as well.)
; 
; Questions of "endian-ness" often come up, usually in the
; context of byte ordering in a word.  These routines have a
; similar issue, that could be called "wordian-ness".
; Independent of byte ordering (PA is always big-endian), one
; can make two choices when representing extremely large
; numbers as arrays of 64-bit doublewords in memory.
; 
; "Little-wordian" layout means that the least significant
; word of a number is stored at the lowest address.
; 
;   MSW     LSW
;    |       |
;    V       V
; 
;    A B C D E
; 
;    ^     ^ ^
;    |     | |____ address 0
;    |     |
;    |     |_______address 8
;    |
;    address 32
; 
; "Big-wordian" means that the most significant word is at the
; lowest address.
; 
;   MSW     LSW
;    |       |
;    V       V
; 
;    A B C D E
; 
;    ^     ^ ^
;    |     | |____ address 32
;    |     |
;    |     |_______address 24
;    |
;    address 0
; 
; When you compile the file, you must specify one or the other, with
; a switch "-DLITTLE_WORDIAN" or "-DBIG_WORDIAN".
; 
;     Incidentally, you assemble this file as part of your
;     project with the same C compiler as the rest of the program.
;     My "makefile" for a superprecision arithmetic package has
;     the following stuff:
; 
;     # definitions:
;     CC = cc -Aa +e -z +DA2.0 +DS2.0 +w1
;     CFLAGS = +O3
;     LDFLAGS = -L /usr/lib -Wl,-aarchive
; 
;     # general build rule for ".s" files:
;     .s.o:
;             $(CC) $(CFLAGS) -c $< -DBIG_WORDIAN
; 
;     # Now any bind step that calls for pa20.o will assemble pa20.s
; 
; End of digression, back to arithmetic:
; 
; The way we multiply two huge numbers is, of course, to multiply
; the "ABCD" vector by each of the "WXYZ" doublewords, adding
; the result vectors with increasing offsets, the way we learned
; in school, back before we all used calculators:
; 
;            A B C D
;          * W X Y Z
;         __________
;          P Q R S T
;        E F G H I
;      M N O P Q
;  + R S T U V
;    _______________
;    F I N A L S U M
; 
; So we call maxpy_PA20_big (in my case; my package is
; big-wordian) repeatedly, giving the W, X, Y, and Z arguments
; in turn as the "scalar", and giving the "ABCD" vector each
; time.  We direct it to add its result into an area of memory
; that we have cleared at the start.  We skew the exact
; location into that area with each call.
; 
; The prototype for the function is
; 
; extern void maxpy_PA20_big(
;    int length,        /* Number of doublewords in the multiplicand vector. */
;    const long long int *scalaraddr,    /* Address to fetch the scalar. */
;    const long long int *multiplicand,  /* The multiplicand vector. */
;    long long int *result);             /* Where to accumulate the result. */
; 
; (You should place a copy of this prototype in an include file
; or in your C file.)
; 
; Now, IN ALL CASES, the given address for the multiplicand or
; the result is that of the LEAST SIGNIFICANT DOUBLEWORD.
; That word is, of course, the word at which the routine
; starts processing.  "maxpy_PA20_little" then increases the
; addresses as it computes.  "maxpy_PA20_big" decreases them.
; 
; In our example above, "length" would be 4 in each case.
; "multiplicand" would be the "ABCD" vector.  Specifically,
; the address of the element "D".  "scalaraddr" would be the
; address of "W", "X", "Y", or "Z" on the four calls that we
; would make.  (The order doesn't matter, of course.)
; "result" would be the appropriate address in the result
; area.  When multiplying by "Z", that would be the least
; significant word.  When multiplying by "Y", it would be the
; next higher word (8 bytes higher if little-wordian; 8 bytes
; lower if big-wordian), and so on.  The size of the result
; area must be the the sum of the sizes of the multiplicand
; and multiplier vectors, and must be initialized to zero
; before we start.
; 
; Whenever the routine adds its partial product into the result
; vector, it follows carry chains as far as they need to go.
; 
; Here is the super-precision multiply routine that I use for
; my package.  The package is big-wordian.  I have taken out
; handling of exponents (it's a floating point package):
; 
; static void mul_PA20(
;   int size,
;   const long long int *arg1,
;   const long long int *arg2,
;   long long int *result)
; {
;    int i;
; 
;    for (i=0 ; i<2*size ; i++) result[i] = 0ULL;
; 
;    for (i=0 ; i<size ; i++) {
;       maxpy_PA20_big(size, &arg2[i], &arg1[size-1], &result[size+i]);
;    }
; }
