        .text
        .align 5
        .globl SharedStub
	.ent SharedStub
SharedStub:
	.frame  $30, 96, $26, 0
        .mask 0x4000000,-96
        ldgp $29,0($27)
SharedStubXv:
        subq $30,96,$30
        stq $26,0($30)
	.prologue 1
        stt $f17,16($30)
        stt $f18,24($30)
        stt $f19,32($30)
        stt $f20,40($30)
        stt $f21,48($30)
        stq $17,56($30)
        stq $18,64($30)
        stq $19,72($30)
        stq $20,80($30)
        stq $21,88($30)
        bis $1,$1,$17
        addq $30,16,$18
        bsr $26,PrepareAndDispatch
        ldq $26,0($30)
        addq $30,96,$30
        ret $31,($26),1
        .end SharedStub

/*
   The C preprocessor doesn't handle # and ## if run with -std or -std0.

   If -std  then __STDC__ is 0 [default] (K&R behavior)
   If -std0 then __STDC__ is undefined (K&R behavior)
   If -std1 then __STDC__ is 1 (ANSI preprocessor)
*/

#if __STDC__
#define STUB_ENTRY(n) ;\
        .text ;\
        .align 5 ;\
        .globl Stub##n##__14nsXPTCStubBase ;\
        .globl Stub##n##__14nsXPTCStubBaseXv ;\
        .ent Stub##n##__14nsXPTCStubBase ;\
Stub##n##__14nsXPTCStubBase: ;\
        .frame $30,0,$26,0 ;\
        ldgp $29,0($27) ;\
        .prologue 1 ;\
Stub##n##__14nsXPTCStubBaseXv: ;\
        lda $1,n ;\
        br $31,SharedStubXv ;\
        .end Stub##n##__14nsXPTCStubBase \

#else
#define STUB_ENTRY(n) ;\
        .text ;\
        .align 5 ;\
        .globl Stub/**/n/**/__14nsXPTCStubBase ;\
        .globl Stub/**/n/**/__14nsXPTCStubBaseXv ;\
        .ent Stub/**/n/**/__14nsXPTCStubBase ;\
Stub/**/n/**/__14nsXPTCStubBase: ;\
        .frame $30,0,$26,0 ;\
        ldgp $29,0($27) ;\
        .prologue 1 ;\
Stub/**/n/**/__14nsXPTCStubBaseXv: ;\
        lda $1,n ;\
        br $31,SharedStubXv ;\
        .end Stub/**/n/**/__14nsXPTCStubBase \

#endif

#define SENTINEL_ENTRY(n) \

#include "xptcstubsdef.inc"
