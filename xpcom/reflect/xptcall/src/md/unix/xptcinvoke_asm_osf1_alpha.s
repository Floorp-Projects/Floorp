/*
 * XPTC_PUBLIC_API(nsresult)
 * XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
 *                    PRUint32 paramCount, nsXPTCVariant* params)
 */
.text
.align 4
.globl XPTC_InvokeByIndex
.ent XPTC_InvokeByIndex
XPTC_InvokeByIndex:
.frame $15,32,$26,0
.mask 0x4008000,-32
ldgp $29,0($27)
XPTC_InvokeByIndexXv:
subq $30,32,$30
stq $26,0($30)
stq $15,8($30)
bis $30,$30,$15
.prologue 1

/*
 * Allocate enough stack space to hold the greater of 6 or "paramCount"+1
 * parameters. (+1 for "this" pointer)  Room for at least 6 parameters
 * is required for storage of those passed via registers.
 */

bis $31,5,$2      /* count = MAX(5, "paramCount") */
cmplt $2,$18,$1
cmovne $1,$18,$2
s8addq $2,16,$1   /* room for count+1 params (8 bytes each) */
bic $1,15,$1      /* stack space is rounded up to 0 % 16 */
subq $30,$1,$30

stq $16,0($30)    /* save "that" (as "this" pointer) */
stq $17,16($15)   /* save "methodIndex" */

addq $30,8,$16    /* pass stack pointer */
bis $18,$18,$17   /* pass "paramCount" */
bis $19,$19,$18   /* pass "params" */
jsr $26,invoke_copy_to_stack     /* call invoke_copy_to_stack */
ldgp $29,0($26)

/*
 * Copy the first 6 parameters to registers and remove from stack frame.
 * Both the integer and floating point registers are set for each parameter
 * except the first which is the "this" pointer.  (integer only)
 * The floating point registers are all set as doubles since the
 * invoke_copy_to_stack function should have converted the floats.
 */
ldq $16,0($30)    /* integer registers */
ldq $17,8($30)
ldq $18,16($30)
ldq $19,24($30)
ldq $20,32($30)
ldq $21,40($30)
ldt $f17,8($30)   /* floating point registers */
ldt $f18,16($30)
ldt $f19,24($30)
ldt $f20,32($30)
ldt $f21,40($30)

addq $30,48,$30   /* remove params from stack */

/*
 * Call the virtual function with the constructed stack frame.
 */
bis $16,$16,$1    /* load "this" */
ldq $2,16($15)    /* load "methodIndex" */
ldq $1,0($1)      /* load vtable */
s8addq $2,0,$2   /* vtable index = "methodIndex" * 8 + 16 */
addq $1,$2,$1
ldq $27,0($1)     /* load address of function */
jsr $26,($27),0   /* call virtual function */
ldgp $29,0($26)

bis $15,$15,$30
ldq $26,0($30)
ldq $15,8($30)
addq $30,32,$30
ret $31,($26),1
.end XPTC_InvokeByIndex


