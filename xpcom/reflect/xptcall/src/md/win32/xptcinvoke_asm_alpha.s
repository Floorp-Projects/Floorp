.text
.globl  invoke_count_words
.globl  invoke_copy_to_stack

#define v0 $0
#define t0 $1
#define t1 $2
#define t2 $3
#define t3 $4
#define s0 $9
#define fp $15
#define a0 $16
#define a1 $17
#define a2 $18
#define a3 $19
#define a4 $20
#define a5 $21
#define t9 $23
#define ra $26
#define gp $29
#define sp $30
#define zero $31
#define f0 $f0

#define LOCALSZ  7
#define NARG     2
#define SZREG    8
#define FRAMESZ  ((NARG+LOCALSZ)*SZREG)
RAOFF=FRAMESZ-(1*SZREG)
A0OFF=FRAMESZ-(2*SZREG)
A1OFF=FRAMESZ-(3*SZREG)
A2OFF=FRAMESZ-(4*SZREG)
A3OFF=FRAMESZ-(5*SZREG)
S0OFF=FRAMESZ-(6*SZREG)
GPOFF=FRAMESZ-(7*SZREG)

//
// Nested
// XPTC__InvokebyIndex( that, methodIndex, paramCount, params)
//                       a0       a1         a2          a3
//
 .text
 .align  4
 .globl  XPTC__InvokebyIndex
 .ent    XPTC__InvokebyIndex,0
XPTC__InvokebyIndex:
 .frame  sp, FRAMESZ, ra
 subl    sp,FRAMESZ,sp  // allocate stack space for structure
 stq ra, RAOFF(sp)
 stq a0, A0OFF(sp)
 stq a1, A1OFF(sp)
 stq a2, A2OFF(sp)
 stq a3, A3OFF(sp)
 stq s0, S0OFF(sp)
// stq gp, GPOFF(sp)  Don't think I am to save gp

 // invoke_count_words(paramCount, params)
 bis a2,zero,a0  // move a2 into a0
 bis a3,zero,a1  // move a3 into a1
 bsr ra,invoke_count_words

 // invoke_copy_to_stack
 ldq a1, A2OFF(sp)  // a1 = paramCount
 ldq a2, A3OFF(sp)  // a2 = params

 // save sp before we copy the params to the stack
 bis sp,zero,t0  // t0 = sp

 // assume full size of 8 bytes per param to be safe
 sll v0,4,v0   //v0 = 8 bytes * num params
 subl sp,v0,sp  //sp = sp - v0
 bis sp,zero,a0  //a0 = param stack address

 // create temporary stack space to write int and fp regs
 subl sp,64,sp  //sp = sp -64 // (64 = 8 regs of eight bytes)
 bis sp,zero,a3  // a3 = sp

 // save the old sp and save the arg stack
 subl sp,16,sp  //sp = sp -16
 stq t0,0(sp)
 stq a0,8(sp)
 // copy the param into the stack areas
 bsr ra,invoke_copy_to_stack

 ldq t3,8(sp)  // get previous a0
 ldq sp,0(sp)  // get orig sp back

 ldq a0,A0OFF(sp)  // a0 = that
 ldq a1,A1OFF(sp)  // a1 = methodIndex

 // calculate jmp address from method index
 ldl t1,0(a0) // t1 = *that
 sll a1,2,a1  // a1 = 4*index
 addl t1,a1,t9
 ldl t9,0(t9) // t9=*(that + 4*index)

 // get register save area from invoke_copy_to_stack
 subl t3,64,t1

 // a1..a5 and f17..f21 should now be set to what
 // invoke_copy_to_stack told us. skip a0 and f16
 // because that's the "this" pointer

 ldq a1,0(t1)
 ldq a2,8(t1)
 ldq a3,16(t1)
 ldq a4,24(t1)
 ldq a5,32(t1)

 ldt $f17,0(t1)
 ldt $f18,8(t1)
 ldt $f19,16(t1)
 ldt $f20,24(t1)
 ldt $f21,32(t1)

 // save away our stack point and create
 // the stack pointer for the function
 bis sp,zero,s0
 bis t3,zero,sp
 jsr ra,(t9)
 bis s0,zero,sp
 ldq ra,RAOFF(sp)
 ldq s0,S0OFF(sp)
 addl sp,FRAMESZ,sp
 ret

.end    XPTC__InvokebyIndex

