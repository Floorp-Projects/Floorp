/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2023, Nathan Egge
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include "src/riscv/asm.S"

function inv_txfm_add_4x4_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 4, e16, mf2, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
  addi t0, t0, 8
  vle16.v v2, (t0)
  addi t0, t0, 8
  vle16.v v3, (t0)

  jalr t0, a4

  vmv.v.x v4, zero

  vsseg4e16.v v0, (a2)
  vle16.v v0, (a2)
  vse16.v v4, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
  vse16.v v4, (t0)
  addi t0, t0, 8
  vle16.v v2, (t0)
  vse16.v v4, (t0)
  addi t0, t0, 8
  vle16.v v3, (t0)
  vse16.v v4, (t0)

  jalr t0, a5

  vssra.vi v0, v0, 4
  vssra.vi v1, v1, 4
  vssra.vi v2, v2, 4
  vssra.vi v3, v3, 4

itx_4x4_end:
  vsetvli zero, zero, e8, mf4, ta, ma
  vle8.v v4, (a0)
  add t0, a0, a1
  vle8.v v5, (t0)
  add t0, t0, a1
  vle8.v v6, (t0)
  add t0, t0, a1
  vle8.v v7, (t0)

  vwaddu.wv v0, v0, v4
  vwaddu.wv v1, v1, v5
  vwaddu.wv v2, v2, v6
  vwaddu.wv v3, v3, v7

  vsetvli zero, zero, e16, mf2, ta, ma
  vmax.vx v0, v0, zero
  vmax.vx v1, v1, zero
  vmax.vx v2, v2, zero
  vmax.vx v3, v3, zero

  vsetvli zero, zero, e8, mf4, ta, ma

  vnclipu.wi v4, v0, 0
  vnclipu.wi v5, v1, 0
  vnclipu.wi v6, v2, 0
  vnclipu.wi v7, v3, 0

  vse8.v v4, (a0)
  add a0, a0, a1
  vse8.v v5, (a0)
  add a0, a0, a1
  vse8.v v6, (a0)
  add a0, a0, a1
  vse8.v v7, (a0)

  ret
endfunc

function inv_identity_e16_x4_rvv, export=1, ext=v
  li t1, (5793-4096)*8
  vsmul.vx v4, v0, t1
  vsmul.vx v5, v1, t1
  vsmul.vx v6, v2, t1
  vsmul.vx v7, v3, t1

  vsadd.vv v0, v0, v4
  vsadd.vv v1, v1, v5
  vsadd.vv v2, v2, v6
  vsadd.vv v3, v3, v7

  jr t0
endfunc

.macro idct_4 o0, o1, o2, o3
  li t1, 2896
  li t2, 1567
  li t3, 3784

  vwmul.vx v8, \o0, t1
  vwmul.vx v10, \o0, t1
  vwmacc.vx v8, t1, \o2
  neg t1, t1
  vwmacc.vx v10, t1, \o2

  vwmul.vx v12, \o1, t3
  neg t3, t3
  vwmul.vx v14, \o1, t2
  vwmacc.vx v12, t2, \o3
  vwmacc.vx v14, t3, \o3

  li t1, 2048

  vwadd.wx v8, v8, t1
  vwadd.wx v10, v10, t1
  vwadd.wx v12, v12, t1
  vwadd.wx v14, v14, t1

  vnsra.wi v8, v8, 12
  vnsra.wi v10, v10, 12
  vnsra.wi v12, v12, 12
  vnsra.wi v14, v14, 12

  vsadd.vv \o0, v8, v12
  vsadd.vv \o1, v10, v14
  vssub.vv \o2, v10, v14
  vssub.vv \o3, v8, v12
.endm

.macro iadst_4 o0, o1, o2, o3
  li t1, 1321
  li t2, 3803
  li t3, 2482

  vwmul.vx v4, v0, t1
  vwmul.vx v5, v0, t3
  neg t1, t1
  vwmacc.vx v4, t2, v2
  vwmacc.vx v5, t1, v2
  neg t2, t2
  vwmacc.vx v4, t3, v3
  vwmacc.vx v5, t2, v3

  vwsub.vv v6, v0, v2
  vwadd.wv v6, v6, v3

  li t1, 3344
  vwmul.vx v7, v1, t1

  vsetvli zero, zero, e32, m1, ta, ma

  vmul.vx v6, v6, t1

  vadd.vv v8, v4, v5
  vadd.vv v4, v4, v7
  vadd.vv v5, v5, v7
  vsub.vv v7, v8, v7

  li t1, 2048

  vadd.vx v4, v4, t1
  vadd.vx v5, v5, t1
  vadd.vx v6, v6, t1
  vadd.vx v7, v7, t1

  vsetvli zero, zero, e16, mf2, ta, ma

  vnsra.wi \o0, v4, 12
  vnsra.wi \o1, v5, 12
  vnsra.wi \o2, v6, 12
  vnsra.wi \o3, v7, 12
.endm

function inv_dct_e16_x4_rvv, export=1, ext=v
  idct_4 v0, v1, v2, v3
  jr t0
endfunc

function inv_adst_e16_x4_rvv, export=1, ext=v
  iadst_4 v0, v1, v2, v3
  jr t0
endfunc

function inv_flipadst_e16_x4_rvv, export=1, ext=v
  iadst_4 v3, v2, v1, v0
  jr t0
endfunc

.macro def_fn_4x4 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_4x4_8bpc_rvv, export=1, ext=v
.ifc \txfm1\()_\txfm2, dct_dct
  beqz a3, 1f
.endif
  la a4, inv_\txfm1\()_e16_x4_rvv
  la a5, inv_\txfm2\()_e16_x4_rvv
  j inv_txfm_add_4x4_rvv
.ifc \txfm1\()_\txfm2, dct_dct
1:
  csrw vxrm, zero
  vsetivli zero, 4, e16, mf2, ta, ma
  ld t2, (a2)
  li t1, 2896*8
  vmv.v.x v0, t2
  vsmul.vx v0, v0, t1
  sd x0, (a2)
  vsmul.vx v0, v0, t1
  vssra.vi v0, v0, 4
  vmv.v.v v1, v0
  vmv.v.v v2, v0
  vmv.v.v v3, v0
  j itx_4x4_end
.endif
endfunc
.endm

def_fn_4x4 dct, dct
def_fn_4x4 identity, identity
def_fn_4x4 dct, adst
def_fn_4x4 dct, flipadst
def_fn_4x4 dct, identity
def_fn_4x4 adst, dct
def_fn_4x4 adst, adst
def_fn_4x4 adst, flipadst
def_fn_4x4 flipadst, dct
def_fn_4x4 flipadst, adst
def_fn_4x4 flipadst, flipadst
def_fn_4x4 identity, dct
def_fn_4x4 adst, identity
def_fn_4x4 flipadst, identity
def_fn_4x4 identity, adst
def_fn_4x4 identity, flipadst

.macro def_fn_8x8_base variant
function inv_txfm_\variant\()add_8x8_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 8, e16, m1, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 16
  vle16.v v1, (t0)
  addi t0, t0, 16
  vle16.v v2, (t0)
  addi t0, t0, 16
  vle16.v v3, (t0)
  addi t0, t0, 16
  vle16.v v4, (t0)
  addi t0, t0, 16
  vle16.v v5, (t0)
  addi t0, t0, 16
  vle16.v v6, (t0)
  addi t0, t0, 16
  vle16.v v7, (t0)

.ifc \variant, identity_
  // The identity vsadd.vv and downshift vssra.vi 1 cancel out
.else
  jalr t0, a4

  vssra.vi v0, v0, 1
  vssra.vi v1, v1, 1
  vssra.vi v2, v2, 1
  vssra.vi v3, v3, 1
  vssra.vi v4, v4, 1
  vssra.vi v5, v5, 1
  vssra.vi v6, v6, 1
  vssra.vi v7, v7, 1
.endif

  vsseg8e16.v v0, (a2)
  vle16.v v0, (a2)
  addi t0, a2, 16
  vle16.v v1, (t0)
  addi t0, t0, 16
  vle16.v v2, (t0)
  addi t0, t0, 16
  vle16.v v3, (t0)
  addi t0, t0, 16
  vle16.v v4, (t0)
  addi t0, t0, 16
  vle16.v v5, (t0)
  addi t0, t0, 16
  vle16.v v6, (t0)
  addi t0, t0, 16
  vle16.v v7, (t0)

  jalr t0, a5

  vssra.vi v0, v0, 4
  vssra.vi v1, v1, 4
  vssra.vi v2, v2, 4
  vssra.vi v3, v3, 4
  vssra.vi v4, v4, 4
  vssra.vi v5, v5, 4
  vssra.vi v6, v6, 4
  vssra.vi v7, v7, 4

  li t1, 64
  vsetvli zero, t1, e16, m8, ta, ma
  vmv.v.x v8, zero
  vse16.v v8, (a2)

.ifc \variant, identity_
itx_8x8_end:
.endif
  vsetivli zero, 8, e8, mf2, ta, ma
  vle8.v v8, (a0)
  add t0, a0, a1
  vle8.v v9, (t0)
  add t0, t0, a1
  vle8.v v10, (t0)
  add t0, t0, a1
  vle8.v v11, (t0)
  add t0, t0, a1
  vle8.v v12, (t0)
  add t0, t0, a1
  vle8.v v13, (t0)
  add t0, t0, a1
  vle8.v v14, (t0)
  add t0, t0, a1
  vle8.v v15, (t0)

  vwaddu.wv v0, v0, v8
  vwaddu.wv v1, v1, v9
  vwaddu.wv v2, v2, v10
  vwaddu.wv v3, v3, v11
  vwaddu.wv v4, v4, v12
  vwaddu.wv v5, v5, v13
  vwaddu.wv v6, v6, v14
  vwaddu.wv v7, v7, v15

  vsetvli zero, zero, e16, m1
  vmax.vx v0, v0, zero
  vmax.vx v1, v1, zero
  vmax.vx v2, v2, zero
  vmax.vx v3, v3, zero
  vmax.vx v4, v4, zero
  vmax.vx v5, v5, zero
  vmax.vx v6, v6, zero
  vmax.vx v7, v7, zero

  vsetvli zero, zero, e8, mf2, ta, ma

  vnclipu.wi v8, v0, 0
  vnclipu.wi v9, v1, 0
  vnclipu.wi v10, v2, 0
  vnclipu.wi v11, v3, 0
  vnclipu.wi v12, v4, 0
  vnclipu.wi v13, v5, 0
  vnclipu.wi v14, v6, 0
  vnclipu.wi v15, v7, 0

  vse8.v v8, (a0)
  add a0, a0, a1
  vse8.v v9, (a0)
  add a0, a0, a1
  vse8.v v10, (a0)
  add a0, a0, a1
  vse8.v v11, (a0)
  add a0, a0, a1
  vse8.v v12, (a0)
  add a0, a0, a1
  vse8.v v13, (a0)
  add a0, a0, a1
  vse8.v v14, (a0)
  add a0, a0, a1
  vse8.v v15, (a0)

  ret
endfunc
.endm

def_fn_8x8_base
def_fn_8x8_base identity_

function inv_identity_e16_x8_rvv, export=1, ext=v
  vsadd.vv v0, v0, v0
  vsadd.vv v1, v1, v1
  vsadd.vv v2, v2, v2
  vsadd.vv v3, v3, v3
  vsadd.vv v4, v4, v4
  vsadd.vv v5, v5, v5
  vsadd.vv v6, v6, v6
  vsadd.vv v7, v7, v7

  jr t0
endfunc

function inv_dct_e16_x8_rvv, export=1, ext=v
  idct_4 v0, v2, v4, v6

  li t1, 799
  li t2, 4017
  li t3, 3406
  li t4, 2276

  vwmul.vx v14, v1, t2
  neg t2, t2
  vwmul.vx v8, v1, t1
  vwmacc.vx v14, t1, v7
  vwmacc.vx v8, t2, v7

  vwmul.vx v12, v5, t4
  neg t4, t4
  vwmul.vx v10, v5, t3
  vwmacc.vx v12, t3, v3
  vwmacc.vx v10, t4, v3

  li t1, 2048

  vwadd.wx v8, v8, t1
  vwadd.wx v10, v10, t1
  vwadd.wx v12, v12, t1
  vwadd.wx v14, v14, t1

  vnsra.wi v8, v8, 12
  vnsra.wi v10, v10, 12
  vnsra.wi v12, v12, 12
  vnsra.wi v14, v14, 12

  vssub.vv v7, v14, v12
  vsadd.vv v14, v14, v12
  vssub.vv v1, v8, v10
  vsadd.vv v8, v8, v10

  li t2, 2896

  vwmul.vx v10, v7, t2
  vwmul.vx v12, v7, t2
  vwmacc.vx v12, t2, v1
  neg t2, t2
  vwmacc.vx v10, t2, v1

  vwadd.wx v10, v10, t1
  vwadd.wx v12, v12, t1

  vnsra.wi v10, v10, 12
  vnsra.wi v12, v12, 12

  vssub.vv v7, v0, v14
  vsadd.vv v0, v0, v14
  vssub.vv v9, v2, v12
  vsadd.vv v1, v2, v12
  vssub.vv v5, v4, v10
  vsadd.vv v2, v4, v10
  vssub.vv v4, v6, v8
  vsadd.vv v3, v6, v8
  vmv.v.v v6, v9

  jr t0
endfunc

.macro iadst_8 o0, o1, o2, o3, o4, o5, o6, o7
  li t1, 4076
  li t2, 401
  li t3, 3612
  li t4, 1931
  li t5, 2598
  li t6, 3166

  vwmul.vx v8, v7, t1
  neg t1, t1
  vwmul.vx v10, v7, t2
  vwmacc.vx v8, t2, v0
  vwmacc.vx v10, t1, v0

  vwmul.vx v12, v5, t3
  neg t3, t3
  vwmul.vx v14, v5, t4
  vwmacc.vx v12, t4, v2
  vwmacc.vx v14, t3, v2

  vwmul.vx v16, v3, t5
  neg t5, t5
  vwmul.vx v18, v3, t6
  vwmacc.vx v16, t6, v4
  vwmacc.vx v18, t5, v4

  li t1, 2048
  li t2, 1189
  li t3, 3920
  li t4, 1567
  li t5, 3784
  li t6, 2896

  vwmul.vx v20, v1, t2
  neg t2, t2
  vwmul.vx v22, v1, t3
  vwmacc.vx v20, t3, v6
  vwmacc.vx v22, t2, v6

  vwadd.wx v8, v8, t1
  vwadd.wx v10, v10, t1
  vwadd.wx v12, v12, t1
  vwadd.wx v14, v14, t1
  vwadd.wx v16, v16, t1
  vwadd.wx v18, v18, t1
  vwadd.wx v20, v20, t1
  vwadd.wx v22, v22, t1

  vnsra.wi v8, v8, 12
  vnsra.wi v10, v10, 12
  vnsra.wi v12, v12, 12
  vnsra.wi v14, v14, 12
  vnsra.wi v16, v16, 12
  vnsra.wi v18, v18, 12
  vnsra.wi v20, v20, 12
  vnsra.wi v22, v22, 12

  vssub.vv v4, v8, v16
  vsadd.vv v8, v8, v16
  vsadd.vv v1, v10, v18
  vsadd.vv v2, v12, v20
  vsadd.vv v3, v14, v22
  vssub.vv v5, v10, v18
  vssub.vv v6, v12, v20
  vssub.vv v22, v14, v22

  vsadd.vv \o0, v8, v2
  vsadd.vv \o7, v1, v3
  vssub.vv v2, v8, v2
  vssub.vv v3, v1, v3

  vwmul.vx v8, v4, t5
  vwmul.vx v10, v4, t4
  vwmul.vx v12, v22, t5
  vwmul.vx v14, v22, t4
  vwmacc.vx v8, t4, v5
  neg t4, t4
  vwmacc.vx v14, t5, v6
  neg t5, t5
  vwmacc.vx v12, t4, v6
  vwmacc.vx v10, t5, v5

  vwadd.wx v8, v8, t1
  vwadd.wx v10, v10, t1
  vwadd.wx v12, v12, t1
  vwadd.wx v14, v14, t1

  vnsra.wi v8, v8, 12
  vnsra.wi v10, v10, 12
  vnsra.wi v12, v12, 12
  vnsra.wi v14, v14, 12

  vsadd.vv \o1, v8, v12
  vsadd.vv \o6, v10, v14
  vssub.vv v8, v8, v12
  vssub.vv v9, v10, v14

  vwmul.vx v10, v2, t6
  vwmul.vx v12, v2, t6
  vwmul.vx v14, v8, t6
  vwmul.vx v16, v8, t6
  vwmacc.vx v10, t6, v3
  vwmacc.vx v14, t6, v9
  neg t6, t6
  vwmacc.vx v12, t6, v3
  vwmacc.vx v16, t6, v9

  vwadd.wx v10, v10, t1
  vwadd.wx v12, v12, t1
  vwadd.wx v14, v14, t1
  vwadd.wx v16, v16, t1

  vnsra.wi \o3, v10, 12
  vnsra.wi \o4, v12, 12
  vnsra.wi \o2, v14, 12
  vnsra.wi \o5, v16, 12

  vmv.v.x v8, zero
  vssub.vv \o1, v8, \o1
  vssub.vv \o3, v8, \o3
  vssub.vv \o5, v8, \o5
  vssub.vv \o7, v8, \o7
.endm

function inv_adst_e16_x8_rvv, export=1, ext=v
  iadst_8 v0, v1, v2, v3, v4, v5, v6, v7
  jr t0
endfunc

function inv_flipadst_e16_x8_rvv, export=1, ext=v
  iadst_8 v7, v6, v5, v4, v3, v2, v1, v0
  jr t0
endfunc

.macro def_fn_8x8 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_8x8_8bpc_rvv, export=1, ext=v
.ifc \txfm1\()_\txfm2, dct_dct
  beqz a3, 1f
.endif
  la a5, inv_\txfm2\()_e16_x8_rvv
.ifc \txfm1, identity
  j inv_txfm_identity_add_8x8_rvv
.else
  la a4, inv_\txfm1\()_e16_x8_rvv
  j inv_txfm_add_8x8_rvv
.endif
.ifc \txfm1\()_\txfm2, dct_dct
1:
  csrw vxrm, zero
  vsetivli zero, 8, e16, m1, ta, ma
  ld t2, (a2)
  li t1, 2896*8
  vmv.v.x v0, t2
  vsmul.vx v0, v0, t1
  sd x0, (a2)
  vssra.vi v0, v0, 1
  vsmul.vx v0, v0, t1
  vssra.vi v0, v0, 4
  vmv.v.v v1, v0
  vmv.v.v v2, v0
  vmv.v.v v3, v0
  vmv.v.v v4, v0
  vmv.v.v v5, v0
  vmv.v.v v6, v0
  vmv.v.v v7, v0
  j itx_8x8_end
.endif
endfunc
.endm

def_fn_8x8 dct, dct
def_fn_8x8 identity, identity
def_fn_8x8 dct, adst
def_fn_8x8 dct, flipadst
def_fn_8x8 dct, identity
def_fn_8x8 adst, dct
def_fn_8x8 adst, adst
def_fn_8x8 adst, flipadst
def_fn_8x8 flipadst, dct
def_fn_8x8 flipadst, adst
def_fn_8x8 flipadst, flipadst
def_fn_8x8 identity, dct
def_fn_8x8 adst, identity
def_fn_8x8 flipadst, identity
def_fn_8x8 identity, adst
def_fn_8x8 identity, flipadst
