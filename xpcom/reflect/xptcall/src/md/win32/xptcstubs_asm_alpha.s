
#define v0 $0                   // return value register
#define t0 $1                   // caller saved (temporary) registers
#define t1 $2                   //
#define t2 $3                   //
#define t3 $4                   //
#define t4 $5                   //
#define t5 $6                   //
#define t6 $7                   //
#define t7 $8                   //
#define s0 $9                   // callee saved (nonvolatile) registers
#define a0 $16                  // argument registers
#define a1 $17                  //
#define a2 $18                  //
#define a3 $19                  //
#define a4 $20                  //
#define a5 $21                  //
#define t8 $22                  // caller saved (temporary) registers
#define t9 $23                  //
#define t10 $24                 //
#define t11 $25                 //
#define ra $26                  // return address register
#define sp $30                  // stack pointer register
#define zero $31                // zero register
#define f16 $f16                // argument registers
#define f17 $f17                //
#define f18 $f18                //
#define f19 $f19                //
#define f20 $f20                //
#define f21 $f21                //

#define NARGSAVE 2
#define LOCALSZ  16
#define SZREG    8
#define FRAMESZ  (NARGSAVE+LOCALSZ)*SZREG

 .text
 .globl PrepareAndDispatch

A1OFF=FRAMESZ-(9*SZREG)
A2OFF=FRAMESZ-(8*SZREG)
A3OFF=FRAMESZ-(7*SZREG)
A4OFF=FRAMESZ-(6*SZREG)
A5OFF=FRAMESZ-(5*SZREG)
A6OFF=FRAMESZ-(4*SZREG)   //not used
A7OFF=FRAMESZ-(3*SZREG)   //not used
GPOFF=FRAMESZ-(2*SZREG)   //not used
RAOFF=FRAMESZ-(1*SZREG)

F16OFF=FRAMESZ-(16*SZREG) //not used
F17OFF=FRAMESZ-(15*SZREG)
F18OFF=FRAMESZ-(14*SZREG)
F19OFF=FRAMESZ-(13*SZREG)
F20OFF=FRAMESZ-(12*SZREG)
F21OFF=FRAMESZ-(11*SZREG)
F22OFF=FRAMESZ-(10*SZREG)   // not used

#define NESTED_ENTRY(Name, fsize, retrg) \
 .text;                          \
 .align  4;                      \
 .globl  Name;                   \
 .ent    Name, 0;                \
Name:;                                  \
 .frame  sp, fsize, retrg;

#define SENTINEL_ENTRY(nn)
#define STUB_ENTRY(nn) MAKE_PART(nn,@nsXPTCStubBase@@UAAIXZ )

#define MAKE_PART(aa, bb)      MAKE_STUB(aa, ?Stub##aa##bb )

#define MAKE_STUB(nn, name)             \
NESTED_ENTRY(name, FRAMESZ,ra);         \
 subl   sp,FRAMESZ,sp;           \
 mov    nn,t0;                   \
 jmp    sharedstub;              \
.end name;

#include "xptcstubsdef.inc"

 .globl sharedstub
 .ent   sharedstub
sharedstub:
 stq     a1,A1OFF(sp)
 stq     a2,A2OFF(sp)
 stq     a3,A3OFF(sp)
 stq     a4,A4OFF(sp)
 stq     a5,A5OFF(sp)
 stq     ra,RAOFF(sp)

 stt     f17,F17OFF(sp)
 stt     f18,F18OFF(sp)
 stt     f19,F19OFF(sp)
 stt     f20,F20OFF(sp)
 stt     f21,F21OFF(sp)

 // t0 is methodIndex
 bis     t0,zero,a1              // a1 = methodIndex

 // a2 is stack address where extra function params
 // are stored that do not fit in registers
 bis     sp,zero,a2              //a2 = sp
 addl    a2,FRAMESZ,a2           //a2+=FRAMESZ

 // a3 is stack addrss of a1..a5
 bis     sp,zero,a3              //a3 = sp
 addl    a3,A1OFF,a3             //a3+=A1OFF

 // a4 is stack address of f17..f21
 bis     sp,zero,a4              //a4 = sp
 addl    a4,F17OFF,a4            //a4+=F17OFF

 // PrepareAndDispatch(that, methodIndex, args, gprArgs, fpArgs)
 //                      a0    a1          a2    a3       a4
 bsr     PrepareAndDispatch
 ldq     ra,RAOFF(sp)
 addl    sp,FRAMESZ,sp
 ret

.end sharedstub

