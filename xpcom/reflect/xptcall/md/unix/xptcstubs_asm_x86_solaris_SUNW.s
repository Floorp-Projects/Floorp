#define STUB_ENTRY1(nn)	\
 	.globl __1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_; \
	.type __1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_, @function; \
__1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_: \
	push       %ebp; \
	movl       %esp,%ebp; \
	andl       $-16,%esp; \
	push       %ebx;	\
	call       .CG4./**/nn/**/; \
.CG4./**/nn/**/: \
	pop        %ebx;	 \
	addl       $_GLOBAL_OFFSET_TABLE_+0x1,%ebx; \
	leal	0xc(%ebp), %ecx; \
	pushl	%ecx; \
	pushl	$/**/nn/**/; \
	movl	0x8(%ebp), %ecx; \
	pushl	%ecx; \
	call	__1cSPrepareAndDispatch6FpnOnsXPTCStubBase_IpI_I_; \
	addl	$0xc , %esp; \
	pop        %ebx; \
	movl       %ebp,%esp; \
	pop        %ebp; \
	ret        ; \
	.size __1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_, . - __1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_ \

#define STUB_ENTRY2(nn)	\
 	.globl __1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_; \
	.type __1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_, @function; \
__1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_: \
	push       %ebp; \
	movl       %esp,%ebp; \
	andl       $-16,%esp; \
	push       %ebx;	\
	call       .CG4./**/nn/**/; \
.CG4./**/nn/**/: \
	pop        %ebx;	 \
	addl       $_GLOBAL_OFFSET_TABLE_+0x1,%ebx; \
	leal	0xc(%ebp), %ecx; \
	pushl	%ecx; \
	pushl	$/**/nn/**/; \
	movl	0x8(%ebp), %ecx; \
	pushl	%ecx; \
	call	__1cSPrepareAndDispatch6FpnOnsXPTCStubBase_IpI_I_; \
	addl	$0xc , %esp; \
	pop        %ebx; \
	movl       %ebp,%esp; \
	pop        %ebp; \
	ret        ; \
	.size __1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_, . - __1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_ \

#define STUB_ENTRY3(nn)	\
 	.globl __1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_; \
	.type __1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_, @function; \
__1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_: \
	push       %ebp; \
	movl       %esp,%ebp; \
	andl       $-16,%esp; \
	push       %ebx;	\
	call       .CG4./**/nn/**/; \
.CG4./**/nn/**/: \
	pop        %ebx;	 \
	addl       $_GLOBAL_OFFSET_TABLE_+0x1,%ebx; \
	leal	0xc(%ebp), %ecx; \
	pushl	%ecx; \
	pushl	$/**/nn/**/; \
	movl	0x8(%ebp), %ecx; \
	pushl	%ecx; \
	call	__1cSPrepareAndDispatch6FpnOnsXPTCStubBase_IpI_I_; \
	addl	$0xc , %esp; \
	pop        %ebx; \
	movl       %ebp,%esp; \
	pop        %ebp; \
	ret        ; \
	.size __1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_, . - __1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_ \

#define SENTINEL_ENTRY(nn)

#include "xptcstubsdef_asm.solx86"
