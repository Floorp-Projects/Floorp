#define STUB_ENTRY1(nn) \
    .globl	__1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_; \
    .hidden	__1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_; \
    .type	__1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_, @function; \
__1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_: \
    movl	$/**/nn/**/, %eax; \
    jmp	SharedStub; \
    .size	__1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_, . - __1cOnsXPTCStubBaseFStub/**/nn/**/6M_I_ \

#define STUB_ENTRY2(nn) \
    .globl	__1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_; \
    .hidden	__1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_; \
    .type	__1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_, @function; \
__1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_: \
    movl	$/**/nn/**/, %eax; \
    jmp	SharedStub; \
    .size	__1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_, . - __1cOnsXPTCStubBaseGStub/**/nn/**/6M_I_ \

#define STUB_ENTRY3(nn) \
    .globl	__1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_; \
    .hidden	__1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_; \
    .type	__1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_, @function; \
__1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_: \
    movl	$/**/nn/**/, %eax; \
    jmp	SharedStub; \
    .size	__1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_, . - __1cOnsXPTCStubBaseHStub/**/nn/**/6M_I_ \

// static nsresult SharedStub(uint32_t methodIndex)
    .type      SharedStub, @function;
    SharedStub:
    // make room for gpregs (48), fpregs (64)
    pushq      %rbp;
    movq       %rsp,%rbp;
    subq       $112,%rsp;
    // save GP registers
    movq       %rdi,-112(%rbp);
    movq       %rsi,-104(%rbp);
    movq       %rdx, -96(%rbp);
    movq       %rcx, -88(%rbp);
    movq       %r8 , -80(%rbp);
    movq       %r9 , -72(%rbp);
    leaq       -112(%rbp),%rcx;
    // save FP registers
    movsd      %xmm0,-64(%rbp);
    movsd      %xmm1,-56(%rbp);
    movsd      %xmm2,-48(%rbp);
    movsd      %xmm3,-40(%rbp);
    movsd      %xmm4,-32(%rbp);
    movsd      %xmm5,-24(%rbp);
    movsd      %xmm6,-16(%rbp);
    movsd      %xmm7, -8(%rbp);
    leaq       -64(%rbp),%r8;
    // rdi has the 'self' pointer already
    movl       %eax,%esi;
    leaq       16(%rbp),%rdx;
    call       PrepareAndDispatch@plt;
    leave;
    ret;
    .size      SharedStub, . - SharedStub

#define SENTINEL_ENTRY(nn)

#include "xptcstubsdef_asm.solx86"
