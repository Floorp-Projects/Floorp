
#include <sys/regdef.h>
#include <sys/asm.h>

	.text
	.globl PrepareAndDispatch

LOCALSZ=16
FRAMESZ=(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK

A1OFF=FRAMESZ-(9*SZREG)
A2OFF=FRAMESZ-(8*SZREG)
A3OFF=FRAMESZ-(7*SZREG)
A4OFF=FRAMESZ-(6*SZREG)
A5OFF=FRAMESZ-(5*SZREG)
A6OFF=FRAMESZ-(4*SZREG)
A7OFF=FRAMESZ-(3*SZREG)
GPOFF=FRAMESZ-(2*SZREG)
RAOFF=FRAMESZ-(1*SZREG)

F13OFF=FRAMESZ-(16*SZREG)
F14OFF=FRAMESZ-(15*SZREG)
F15OFF=FRAMESZ-(14*SZREG)
F16OFF=FRAMESZ-(13*SZREG)
F17OFF=FRAMESZ-(12*SZREG)
F18OFF=FRAMESZ-(11*SZREG)
F19OFF=FRAMESZ-(10*SZREG)

#define SENTINEL_ENTRY(nn)         /* defined in cpp file, not here */

#ifdef __GNUC__
#define STUB_ENTRY(nn)    MAKE_STUB(nn, Stub/**/nn/**/__14nsXPTCStubBase)
#else
#define STUB_ENTRY(nn)    MAKE_STUB(nn, Stub/**/nn/**/__14nsXPTCStubBaseGv)
#endif

#define MAKE_STUB(nn, name)					\
NESTED(name, FRAMESZ, ra);					\
	SETUP_GP;						\
	PTR_SUBU sp, FRAMESZ;					\
	SETUP_GP64(GPOFF, name);				\
	SAVE_GP(GPOFF);						\
	li           t0, nn;					\
	b            sharedstub;				\
.end name;							\

#include "xptcstubsdef.inc"

	.globl sharedstub
	.ent   sharedstub
sharedstub:

	REG_S	a1, A1OFF(sp)
	REG_S	a2, A2OFF(sp)
	REG_S	a3, A3OFF(sp)
	REG_S	a4, A4OFF(sp)
	REG_S	a5, A5OFF(sp)
	REG_S	a6, A6OFF(sp)
	REG_S	a7, A7OFF(sp)
	REG_S	ra, RAOFF(sp)

	s.d	$f13, F13OFF(sp)
	s.d	$f14, F14OFF(sp)
	s.d	$f15, F15OFF(sp)
	s.d	$f16, F16OFF(sp)
	s.d	$f17, F17OFF(sp)
	s.d	$f18, F18OFF(sp)
	s.d	$f19, F19OFF(sp)

	# t0 is methodIndex
	move	a1, t0

	# a2 is stack address where extra function params
	# are stored that do not fit in registers
	move	a2, sp
	addi	a2, FRAMESZ

	# a3 is stack address of a1..a7
	move	a3, sp
	addi	a3, A1OFF

	# a4 is stack address of f13..f19
	move	a4, sp
	addi	a4, F13OFF

	# PrepareAndDispatch(that, methodIndex, args, gprArgs, fpArgs)
	#                     a0       a1        a2     a3       a4
	#
	jal	PrepareAndDispatch

	REG_L	ra, RAOFF(sp)
	REG_L	gp, GPOFF(sp)

	PTR_ADDU sp, FRAMESZ
	j	ra

.end sharedstub
