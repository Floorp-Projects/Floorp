
    AREA    text,CODE,READONLY
    ALIGN   4

; ----

; extern "C" nsresult
; asmXPTC_InvokeByIndex( nsISupports* that,
;                        PRUint32 methodIndex,
;                        PRUint32 paramCount,
;                        nsXPTCVariant* params,
;                        PRUint32 pfn_CopyToStack,
;                        PRUint32 pfn_CountWords);  

    EXTERN writeArgs

    GLOBAL asmXPTC_InvokeByIndex
asmXPTC_InvokeByIndex
    mov     r12, sp
    stmdb   sp!, {r4 - r6, r12, lr} ; we're using registers 4, 5 and 6. Save them
    sub     sp, sp, #16
    mov     r6, r0          ; store 'that' (the target's this)
    mov     r5, r1, lsl #2  ; a vtable index = methodIndex * 4
    mov     r4, sp          ; Back up the initial stack pointer.
    ; Move the stack pointer to allow for `paramCount` 64-bit values. We'll get
    ; any unused space back as writeArgs tells us where the SP should end up.
    sub     sp, sp, r2, lsl #3  ; worst case estimate for stack space

    ; Put the arguments on the stack.
    ldr     ip, =writeArgs
    mov     r0, r4
    mov     r1, r2
    mov     r2, r3

    blx     ip              ; call writeArgs 

    ; Use the stack pointer returned by writeArgs, but skip the first three
    ; words as these belong in registers (r1-r3).
    add     sp, r0, #12
    
    
    mov     r0, r6          ; Restore 'that'.
    ldr     r1, [r0]        ; get that->vtable offset
    ldr     ip, [r1, r5]    ; get method adress from vtable

    ; The stack pointer now points to a stack which includes all arguments to
    ; be passed to the target method. The first three words should be passed in
    ; r1-r3 (with 'this' in r0). If we have fewer than three argument words, we
    ; will waste some cycles (and a couple of memory words) by loading them,
    ; but I suspect that we'll achieve a net gain by avoiding a conditional
    ; load here.
    ldr     r1, [sp, #-12]
    ldr     r2, [sp, #-8]
    ldr     r3, [sp, #-4]
    blx     ip              ; call function

    mov     sp, r4          ; Restore the original stack pointer.

    add     sp, sp, #16
    ldmia   sp!, {r4 - r6, sp, pc} ; Restore registers and return.

    END
