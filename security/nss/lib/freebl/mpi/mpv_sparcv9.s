
	.section	".text",#alloc,#execinstr
	.file	"mpv_sparc.c"

	.section	".data",#alloc,#write
	.align	8
mask_cnst:
	.xword	-9223372034707292160
	.type	mask_cnst,#object
	.size	mask_cnst,8

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.register	%g3,#scratch
/* 000000	     */		.register	%g2,#scratch
/* 000000	   0 */		.align	8
!
! CONSTANT POOL
!
                       .L_const_seg_900000101:
/* 000000	   0 */		.word	1127219200,0
/* 0x0008	     */		.word	1105199103,-4194304
/* 0x0010	   0 */		.align	8
!
! SUBROUTINE mul_add
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global mul_add
                       mul_add:
/* 000000	     */		sethi	%hi(0x1800),%g1
/* 0x0004	     */		sethi	%hi(mask_cnst),%g2
/* 0x0008	     */		xor	%g1,-752,%g1
/* 0x000c	     */		add	%g2,%lo(mask_cnst),%g2
/* 0x0010	     */		save	%sp,%g1,%sp
                       .L900000154:
/* 0x0014	     */		call	.+8
/* 0x0018	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000154-.)),%g5
! FILE mpv_sparc.c

!    1		      !/*
!    2		      ! * The contents of this file are subject to the Mozilla Public
!    3		      ! * License Version 1.1 (the "License"); you may not use this file
!    4		      ! * except in compliance with the License. You may obtain a copy of
!    5		      ! * the License at http://www.mozilla.org/MPL/
!    6		      ! * 
!    7		      ! * Software distributed under the License is distributed on an "AS
!    8		      ! * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
!    9		      ! * implied. See the License for the specific language governing
!   10		      ! * rights and limitations under the License.
!   11		      ! * 
!   12		      ! * The Original Code is a SPARC/VIS optimized multiply and add function
!   13		      ! *
!   14		      ! * The Initial Developer of the Original Code is Sun Microsystems Inc.
!   15		      ! * Portions created by Sun Microsystems Inc. are 
!   16		      ! * Copyright (C) 1999-2000 Sun Microsystems Inc.  All Rights Reserved.
!   17		      ! * 
!   18		      ! * Contributor(s):
!   19		      ! * 
!   20		      ! * Alternatively, the contents of this file may be used under the
!   21		      ! * terms of the GNU General Public License Version 2 or later (the
!   22		      ! * "GPL"), in which case the provisions of the GPL are applicable 
!   23		      ! * instead of those above.   If you wish to allow use of your 
!   24		      ! * version of this file only under the terms of the GPL and not to
!   25		      ! * allow others to use your version of this file under the MPL,
!   26		      ! * indicate your decision by deleting the provisions above and
!   27		      ! * replace them with the notice and other provisions required by
!   28		      ! * the GPL.  If you do not delete the provisions above, a recipient
!   29		      ! * may use your version of this file under either the MPL or the
!   30		      ! * GPL.
!   31		      ! *  $Id: mpv_sparcv9.s,v 1.1 2000/12/13 01:19:55 nelsonb%netscape.com Exp $
!   32		      ! */
!   34		      !#include "vis_proto.h"
!   36		      !/***************************************************************/
!   38		      !typedef  int                t_s32;
!   39		      !typedef  unsigned int       t_u32;
!   40		      !#if defined(__sparcv9)
!   41		      !typedef  long               t_s64;
!   42		      !typedef  unsigned long      t_u64;
!   43		      !#else
!   44		      !typedef  long long          t_s64;
!   45		      !typedef  unsigned long long t_u64;
!   46		      !#endif
!   47		      !typedef  double             t_d64;
!   49		      !/***************************************************************/
!   51		      !typedef union {
!   52		      !  t_d64 d64;
!   53		      !  struct {
!   54		      !    t_s32 i0;
!   55		      !    t_s32 i1;
!   56		      !  } i32s;
!   57		      !} d64_2_i32;
!   59		      !/***************************************************************/
!   61		      !#define BUFF_SIZE  256
!   63		      !#define A_BITS  19
!   64		      !#define A_MASK  ((1 << A_BITS) - 1)
!   66		      !/***************************************************************/
!   68		      !static t_u64 mask_cnst[] = {
!   69		      !  0x8000000080000000ull
!   70		      !};
!   72		      !/***************************************************************/
!   74		      !#define DEF_VARS(N)                     \
!   75		      !  t_d64 *py = (t_d64*)y;                \
!   76		      !  t_d64 mask = *((t_d64*)mask_cnst);    \
!   77		      !  t_d64 ca = (1u << 31) - 1;            \
!   78		      !  t_d64 da = (t_d64)a;                  \
!   79		      !  t_s64 buff[N], s;                     \
!   80		      !  d64_2_i32 dy
!   82		      !/***************************************************************/
!   84		      !#define MUL_U32_S64_2(i)                                \
!   85		      !  dy.d64 = vis_fxnor(mask, py[i]);                      \
!   86		      !  buff[2*(i)  ] = (ca - (t_d64)dy.i32s.i0) * da;        \
!   87		      !  buff[2*(i)+1] = (ca - (t_d64)dy.i32s.i1) * da
!   89		      !#define MUL_U32_S64_2_D(i)              \
!   90		      !  dy.d64 = vis_fxnor(mask, py[i]);      \
!   91		      !  d0 = ca - (t_d64)dy.i32s.i0;          \
!   92		      !  d1 = ca - (t_d64)dy.i32s.i1;          \
!   93		      !  buff[4*(i)  ] = (t_s64)(d0 * da);     \
!   94		      !  buff[4*(i)+1] = (t_s64)(d0 * db);     \
!   95		      !  buff[4*(i)+2] = (t_s64)(d1 * da);     \
!   96		      !  buff[4*(i)+3] = (t_s64)(d1 * db)
!   98		      !/***************************************************************/
!  100		      !#define ADD_S64_U32(i)          \
!  101		      !  s = buff[i] + x[i] + c;       \
!  102		      !  z[i] = s;                     \
!  103		      !  c = (s >> 32)
!  105		      !#define ADD_S64_U32_D(i)                        \
!  106		      !  s = buff[2*(i)] +(((t_s64)(buff[2*(i)+1]))<<A_BITS) + x[i] + uc;   \
!  107		      !  z[i] = s;                                     \
!  108		      !  uc = ((t_u64)s >> 32)
!  110		      !/***************************************************************/
!  112		      !#define MUL_U32_S64_8(i)        \
!  113		      !  MUL_U32_S64_2(i);             \
!  114		      !  MUL_U32_S64_2(i+1);           \
!  115		      !  MUL_U32_S64_2(i+2);           \
!  116		      !  MUL_U32_S64_2(i+3)
!  118		      !#define MUL_U32_S64_D_8(i)      \
!  119		      !  MUL_U32_S64_2_D(i);           \
!  120		      !  MUL_U32_S64_2_D(i+1);         \
!  121		      !  MUL_U32_S64_2_D(i+2);         \
!  122		      !  MUL_U32_S64_2_D(i+3)
!  124		      !/***************************************************************/
!  126		      !#define ADD_S64_U32_8(i)        \
!  127		      !  ADD_S64_U32(i);               \
!  128		      !  ADD_S64_U32(i+1);             \
!  129		      !  ADD_S64_U32(i+2);             \
!  130		      !  ADD_S64_U32(i+3);             \
!  131		      !  ADD_S64_U32(i+4);             \
!  132		      !  ADD_S64_U32(i+5);             \
!  133		      !  ADD_S64_U32(i+6);             \
!  134		      !  ADD_S64_U32(i+7)
!  136		      !#define ADD_S64_U32_D_8(i)      \
!  137		      !  ADD_S64_U32_D(i);             \
!  138		      !  ADD_S64_U32_D(i+1);           \
!  139		      !  ADD_S64_U32_D(i+2);           \
!  140		      !  ADD_S64_U32_D(i+3);           \
!  141		      !  ADD_S64_U32_D(i+4);           \
!  142		      !  ADD_S64_U32_D(i+5);           \
!  143		      !  ADD_S64_U32_D(i+6);           \
!  144		      !  ADD_S64_U32_D(i+7)
!  146		      !/***************************************************************/
!  148		      !t_u32 mul_add(t_u32 *z, t_u32 *x, t_u32 *y, int n, t_u32 a)
!  149		      !{
!  150		      !  if (a < (1 << A_BITS)) {
!  152		      !    if (n == 8) {
!  153		      !      DEF_VARS(8);
!  154		      !      t_s32 c = 0;
!  156		      !      MUL_U32_S64_8(0);

/* 0x001c	 156 */		sethi	%hi(.L_const_seg_900000101),%g3
/* 0x0020	 149 */		add	%g5,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000154-.)),%g5
/* 0x0024	     */		or	%g0,%i0,%i5
/* 0x0028	     */		add	%g5,%o7,%o2
/* 0x002c	     */		or	%g0,%i4,%o1
/* 0x0030	     */		ldx	[%o2+%g2],%g2
/* 0x0034	 156 */		add	%g3,%lo(.L_const_seg_900000101),%g3
/* 0x0038	 150 */		sethi	%hi(0x80000),%g4
/* 0x003c	 149 */		or	%g0,%i2,%i0
/* 0x0040	     */		or	%g0,%i2,%i4
/* 0x0044	 156 */		ldx	[%o2+%g3],%o0
/* 0x0048	 149 */		ldd	[%g2],%f0
/* 0x004c	 150 */		cmp	%o1,%g4
/* 0x0050	 149 */		or	%g0,%i1,%o7
/* 0x0054	     */		or	%g0,%i3,%i2
/* 0x0058	     */		or	%g0,%i5,%g5
/* 0x005c	 150 */		bcc,pn	%icc,.L77000048
/* 0x0060	     */		cmp	%i3,8
/* 0x0064	 152 */		bne,pn	%icc,.L900000162
/* 0x0068	     */		cmp	%i3,16
/* 0x006c	 156 */		ldd	[%i0],%f4
/* 0x0070	 153 */		st	%o1,[%sp+2287]
/* 0x0074	     */		ldd	[%o0],%f8
/* 0x0078	     */		fxnor	%f0,%f4,%f4
/* 0x007c	 156 */		ldd	[%i0+8],%f10
/* 0x0080	     */		fitod	%f4,%f12
/* 0x0084	     */		ldd	[%o0+8],%f14
/* 0x0088	 153 */		ld	[%sp+2287],%f7
/* 0x008c	 156 */		fitod	%f5,%f4
/* 0x0090	     */		fxnor	%f0,%f10,%f10
/* 0x0094	     */		ldd	[%i0+16],%f16
/* 0x0098	     */		ldd	[%i0+24],%f18
/* 0x009c	     */		fsubd	%f14,%f4,%f4

!  157		      !      ADD_S64_U32_8(0);

/* 0x00a0	 157 */		ld	[%i1],%g2
/* 0x00a4	     */		fxnor	%f0,%f16,%f16
/* 0x00a8	     */		ld	[%i1+4],%g3
/* 0x00ac	     */		ld	[%i1+8],%g4
/* 0x00b0	 156 */		fitod	%f16,%f20
/* 0x00b4	 157 */		ld	[%i1+16],%o0
/* 0x00b8	     */		ld	[%i1+12],%g5
/* 0x00bc	     */		ld	[%i1+20],%o1
/* 0x00c0	     */		ld	[%i1+24],%o2
/* 0x00c4	 153 */		fmovs	%f8,%f6
/* 0x00c8	 157 */		ld	[%i1+28],%o3
/* 0x00cc	 153 */		fsubd	%f6,%f8,%f6
/* 0x00d0	 156 */		fsubd	%f14,%f12,%f8
/* 0x00d4	     */		fitod	%f10,%f12
/* 0x00d8	     */		fmuld	%f4,%f6,%f4
/* 0x00dc	     */		fitod	%f11,%f10
/* 0x00e0	     */		fmuld	%f8,%f6,%f8
/* 0x00e4	     */		fsubd	%f14,%f12,%f12
/* 0x00e8	     */		fdtox	%f4,%f4
/* 0x00ec	     */		std	%f4,[%sp+2271]
/* 0x00f0	     */		fdtox	%f8,%f8
/* 0x00f4	     */		std	%f8,[%sp+2279]
/* 0x00f8	     */		fmuld	%f12,%f6,%f12
/* 0x00fc	     */		fsubd	%f14,%f10,%f10
/* 0x0100	     */		fsubd	%f14,%f20,%f4
/* 0x0104	     */		fitod	%f17,%f8
/* 0x0108	     */		fxnor	%f0,%f18,%f16
/* 0x010c	     */		ldx	[%sp+2279],%o4
/* 0x0110	     */		fmuld	%f10,%f6,%f10
/* 0x0114	     */		fdtox	%f12,%f12
/* 0x0118	     */		std	%f12,[%sp+2263]
/* 0x011c	     */		fmuld	%f4,%f6,%f4
/* 0x0120	     */		fitod	%f16,%f18
/* 0x0124	 157 */		add	%o4,%g2,%g2
/* 0x0128	     */		st	%g2,[%i5]
/* 0x012c	 156 */		ldx	[%sp+2271],%o4
/* 0x0130	     */		fsubd	%f14,%f8,%f8
/* 0x0134	 157 */		srax	%g2,32,%o5
/* 0x0138	 156 */		fdtox	%f10,%f10
/* 0x013c	     */		std	%f10,[%sp+2255]
/* 0x0140	     */		fdtox	%f4,%f4
/* 0x0144	     */		std	%f4,[%sp+2247]
/* 0x0148	 157 */		add	%o4,%g3,%o4
/* 0x014c	 156 */		fitod	%f17,%f12
/* 0x0150	     */		ldx	[%sp+2263],%g2
/* 0x0154	 157 */		add	%o4,%o5,%g3
/* 0x0158	 156 */		fmuld	%f8,%f6,%f8
/* 0x015c	     */		fsubd	%f14,%f18,%f10
/* 0x0160	 157 */		st	%g3,[%i5+4]
/* 0x0164	     */		srax	%g3,32,%g3
/* 0x0168	     */		add	%g2,%g4,%g4
/* 0x016c	 156 */		ldx	[%sp+2255],%g2
/* 0x0170	     */		fsubd	%f14,%f12,%f4
/* 0x0174	 157 */		add	%g4,%g3,%g3
/* 0x0178	 156 */		ldx	[%sp+2247],%g4
/* 0x017c	     */		fmuld	%f10,%f6,%f10
/* 0x0180	     */		fdtox	%f8,%f8
/* 0x0184	     */		std	%f8,[%sp+2239]
/* 0x0188	 157 */		add	%g4,%o0,%g4
/* 0x018c	     */		add	%g2,%g5,%g2
/* 0x0190	     */		st	%g3,[%i5+8]
/* 0x0194	 156 */		fmuld	%f4,%f6,%f4
/* 0x0198	 157 */		srax	%g3,32,%o0
/* 0x019c	 156 */		ldx	[%sp+2239],%g5
/* 0x01a0	     */		fdtox	%f10,%f6
/* 0x01a4	     */		std	%f6,[%sp+2231]
/* 0x01a8	 157 */		add	%g2,%o0,%g2
/* 0x01ac	     */		srax	%g2,32,%g3
/* 0x01b0	     */		add	%g5,%o1,%o1
/* 0x01b4	     */		st	%g2,[%i5+12]
/* 0x01b8	 156 */		fdtox	%f4,%f4
/* 0x01bc	     */		std	%f4,[%sp+2223]
/* 0x01c0	 157 */		add	%g4,%g3,%g3
/* 0x01c4	     */		srax	%g3,32,%g4
/* 0x01c8	     */		st	%g3,[%i5+16]
/* 0x01cc	 156 */		ldx	[%sp+2231],%o0
/* 0x01d0	 157 */		add	%o1,%g4,%g4
/* 0x01d4	     */		srax	%g4,32,%g2
/* 0x01d8	 156 */		ldx	[%sp+2223],%g5
/* 0x01dc	 157 */		add	%o0,%o2,%o2
/* 0x01e0	     */		st	%g4,[%i5+20]
/* 0x01e4	     */		add	%o2,%g2,%g2
/* 0x01e8	     */		add	%g5,%o3,%g5
/* 0x01ec	     */		st	%g2,[%i5+24]
/* 0x01f0	     */		srax	%g2,32,%g3
/* 0x01f4	     */		add	%g5,%g3,%g2
/* 0x01f8	     */		st	%g2,[%i5+28]

!  159		      !      return c;

/* 0x01fc	 159 */		srax	%g2,32,%o0
/* 0x0200	     */		srl	%o0,0,%i0
/* 0x0204	     */		ret	! Result =  %i0
/* 0x0208	     */		restore	%g0,%g0,%g0
                       .L900000162:

!  161		      !    } else if (n == 16) {

/* 0x020c	 161 */		bne,a,pn	%icc,.L900000161
/* 0x0210	     */		st	%o1,[%sp+2223]

!  162		      !      DEF_VARS(16);
!  163		      !      t_s32 c = 0;
!  165		      !      MUL_U32_S64_8(0);

/* 0x0214	 165 */		ldd	[%i0],%f4
/* 0x0218	 162 */		st	%o1,[%sp+2351]
/* 0x021c	     */		ldd	[%o0],%f8
/* 0x0220	     */		fxnor	%f0,%f4,%f4
/* 0x0224	 165 */		ldd	[%o0+8],%f14
/* 0x0228	     */		ldd	[%i0+8],%f10
/* 0x022c	     */		fitod	%f4,%f12
/* 0x0230	 162 */		ld	[%sp+2351],%f7
/* 0x0234	 165 */		fitod	%f5,%f4
/* 0x0238	     */		ldd	[%i0+16],%f16
/* 0x023c	     */		fxnor	%f0,%f10,%f10
/* 0x0240	     */		ldd	[%i0+24],%f18

!  166		      !      MUL_U32_S64_8(4);
!  167		      !      ADD_S64_U32_8(0);

/* 0x0244	 167 */		ld	[%i1],%g2
/* 0x0248	 165 */		fsubd	%f14,%f4,%f4
/* 0x024c	 166 */		ldd	[%i0+32],%f20
/* 0x0250	     */		fxnor	%f0,%f16,%f16
/* 0x0254	 167 */		ld	[%i1+4],%g3
/* 0x0258	 166 */		ldd	[%i0+40],%f22
/* 0x025c	 165 */		fitod	%f16,%f28
/* 0x0260	 167 */		ld	[%i1+8],%g4
/* 0x0264	     */		ld	[%i1+12],%g5
/* 0x0268	     */		fxnor	%f0,%f22,%f22
/* 0x026c	     */		ld	[%i1+16],%o0
/* 0x0270	 166 */		ldd	[%i0+48],%f24
/* 0x0274	 167 */		ld	[%i1+20],%o1
/* 0x0278	     */		ld	[%i1+24],%o2
/* 0x027c	 162 */		fmovs	%f8,%f6
/* 0x0280	 166 */		ldd	[%i0+56],%f26
/* 0x0284	 167 */		ld	[%i1+28],%o3
/* 0x0288	 162 */		fsubd	%f6,%f8,%f6

!  168		      !      ADD_S64_U32_8(8);

/* 0x028c	 168 */		ld	[%i1+32],%o4
/* 0x0290	 165 */		fsubd	%f14,%f12,%f8
/* 0x0294	 168 */		ld	[%i1+36],%o7
/* 0x0298	 165 */		fitod	%f10,%f12
/* 0x029c	 168 */		ld	[%i1+40],%l1
/* 0x02a0	 165 */		fitod	%f11,%f10
/* 0x02a4	     */		fmuld	%f4,%f6,%f4
/* 0x02a8	 168 */		ld	[%i1+56],%l5
/* 0x02ac	 165 */		fmuld	%f8,%f6,%f8
/* 0x02b0	     */		fsubd	%f14,%f12,%f12
/* 0x02b4	     */		fsubd	%f14,%f10,%f10
/* 0x02b8	     */		fdtox	%f8,%f8
/* 0x02bc	     */		std	%f8,[%sp+2343]
/* 0x02c0	     */		fitod	%f17,%f8
/* 0x02c4	     */		fmuld	%f12,%f6,%f12
/* 0x02c8	     */		fdtox	%f4,%f4
/* 0x02cc	     */		std	%f4,[%sp+2335]
/* 0x02d0	     */		fmuld	%f10,%f6,%f10
/* 0x02d4	     */		fxnor	%f0,%f18,%f16
/* 0x02d8	     */		fsubd	%f14,%f28,%f4
/* 0x02dc	     */		fitod	%f16,%f18
/* 0x02e0	     */		ldx	[%sp+2343],%o5
/* 0x02e4	     */		fdtox	%f12,%f12
/* 0x02e8	     */		std	%f12,[%sp+2327]
/* 0x02ec	     */		fsubd	%f14,%f8,%f8
/* 0x02f0	     */		fmuld	%f4,%f6,%f4
/* 0x02f4	     */		fitod	%f17,%f12
/* 0x02f8	 167 */		add	%o5,%g2,%g2
/* 0x02fc	     */		st	%g2,[%i5]
/* 0x0300	 165 */		ldx	[%sp+2335],%o5
/* 0x0304	     */		fdtox	%f10,%f10
/* 0x0308	 167 */		srax	%g2,32,%l0
/* 0x030c	 165 */		std	%f10,[%sp+2319]
/* 0x0310	     */		fsubd	%f14,%f18,%f18
/* 0x0314	     */		fxnor	%f0,%f20,%f16
/* 0x0318	 167 */		add	%o5,%g3,%g3
/* 0x031c	 165 */		fmuld	%f8,%f6,%f20
/* 0x0320	     */		fdtox	%f4,%f4
/* 0x0324	     */		std	%f4,[%sp+2311]
/* 0x0328	 167 */		add	%g3,%l0,%g3
/* 0x032c	 165 */		ldx	[%sp+2327],%o5
/* 0x0330	 166 */		fitod	%f16,%f8
/* 0x0334	 167 */		srax	%g3,32,%l3
/* 0x0338	 165 */		fmuld	%f18,%f6,%f18
/* 0x033c	 167 */		st	%g3,[%i5+4]
/* 0x0340	 165 */		fsubd	%f14,%f12,%f4
/* 0x0344	     */		ldx	[%sp+2319],%l2
/* 0x0348	 166 */		fitod	%f17,%f16
/* 0x034c	 167 */		add	%o5,%g4,%g4
/* 0x0350	 168 */		ld	[%i1+44],%l0
/* 0x0354	 165 */		fdtox	%f20,%f20
/* 0x0358	 167 */		add	%g4,%l3,%g4
/* 0x035c	 165 */		std	%f20,[%sp+2303]
/* 0x0360	 166 */		fsubd	%f14,%f8,%f20
/* 0x0364	 167 */		srax	%g4,32,%l4
/* 0x0368	 165 */		fmuld	%f4,%f6,%f4
/* 0x036c	 167 */		st	%g4,[%i5+8]
/* 0x0370	 165 */		fdtox	%f18,%f18
/* 0x0374	 167 */		add	%l2,%g5,%l2
/* 0x0378	 165 */		ldx	[%sp+2311],%g5
/* 0x037c	 166 */		fitod	%f22,%f8
/* 0x0380	 167 */		add	%l2,%l4,%l2
/* 0x0384	 165 */		std	%f18,[%sp+2295]
/* 0x0388	 166 */		fsubd	%f14,%f16,%f10
/* 0x038c	     */		fmuld	%f20,%f6,%f12
/* 0x0390	 167 */		st	%l2,[%i5+12]
/* 0x0394	     */		fxnor	%f0,%f24,%f18
/* 0x0398	     */		add	%g5,%o0,%l4
/* 0x039c	 165 */		ldx	[%sp+2303],%g5
/* 0x03a0	 167 */		srax	%l2,32,%o0
/* 0x03a4	 165 */		fdtox	%f4,%f4
/* 0x03a8	     */		std	%f4,[%sp+2287]
/* 0x03ac	 166 */		fitod	%f23,%f16
/* 0x03b0	 167 */		add	%l4,%o0,%l4
/* 0x03b4	 166 */		fmuld	%f10,%f6,%f4
/* 0x03b8	 167 */		st	%l4,[%i5+16]
/* 0x03bc	     */		srax	%l4,32,%o0
/* 0x03c0	 166 */		fsubd	%f14,%f8,%f8
/* 0x03c4	 168 */		ld	[%i1+48],%o5
/* 0x03c8	 166 */		fitod	%f18,%f10
/* 0x03cc	 167 */		add	%g5,%o1,%g2
/* 0x03d0	 165 */		ldx	[%sp+2295],%g5
/* 0x03d4	 166 */		fdtox	%f12,%f12
/* 0x03d8	 167 */		add	%g2,%o0,%g2
/* 0x03dc	 166 */		std	%f12,[%sp+2279]
/* 0x03e0	 167 */		srax	%g2,32,%o0
/* 0x03e4	 166 */		fsubd	%f14,%f16,%f12
/* 0x03e8	     */		fmuld	%f8,%f6,%f8
/* 0x03ec	 167 */		st	%g2,[%i5+20]
/* 0x03f0	 166 */		fitod	%f19,%f16
/* 0x03f4	 167 */		add	%g5,%o2,%g3
/* 0x03f8	 165 */		ldx	[%sp+2287],%g5
/* 0x03fc	 166 */		fsubd	%f14,%f10,%f10
/* 0x0400	 167 */		add	%g3,%o0,%g3
/* 0x0404	 168 */		ld	[%i1+52],%l3
/* 0x0408	 167 */		srax	%g3,32,%o0
/* 0x040c	 166 */		fdtox	%f4,%f4
/* 0x0410	     */		std	%f4,[%sp+2271]
/* 0x0414	     */		fxnor	%f0,%f26,%f18
/* 0x0418	 167 */		add	%g5,%o3,%g4
/* 0x041c	 166 */		fmuld	%f12,%f6,%f4
/* 0x0420	 167 */		st	%g3,[%i5+24]
/* 0x0424	 166 */		fdtox	%f8,%f8
/* 0x0428	 167 */		add	%g4,%o0,%g4
/* 0x042c	 166 */		fmuld	%f10,%f6,%f10
/* 0x0430	     */		std	%f8,[%sp+2263]
/* 0x0434	     */		fsubd	%f14,%f16,%f8
/* 0x0438	 168 */		srax	%g4,32,%o0
/* 0x043c	 166 */		ldx	[%sp+2279],%g5
/* 0x0440	     */		fitod	%f18,%f12
/* 0x0444	     */		fitod	%f19,%f16
/* 0x0448	 167 */		st	%g4,[%i5+28]
/* 0x044c	 166 */		fdtox	%f4,%f4
/* 0x0450	 168 */		add	%g5,%o4,%l2
/* 0x0454	 166 */		std	%f4,[%sp+2255]
/* 0x0458	     */		fmuld	%f8,%f6,%f8
/* 0x045c	     */		ldx	[%sp+2271],%g5
/* 0x0460	     */		fdtox	%f10,%f10
/* 0x0464	 168 */		add	%l2,%o0,%l2
/* 0x0468	 166 */		fsubd	%f14,%f12,%f4
/* 0x046c	     */		std	%f10,[%sp+2247]
/* 0x0470	 168 */		srax	%l2,32,%o0
/* 0x0474	 166 */		fsubd	%f14,%f16,%f10
/* 0x0478	 168 */		add	%g5,%o7,%l4
/* 0x047c	 166 */		ldx	[%sp+2263],%g5
/* 0x0480	     */		fdtox	%f8,%f8
/* 0x0484	     */		std	%f8,[%sp+2239]
/* 0x0488	 168 */		add	%l4,%o0,%l4
/* 0x048c	 166 */		fmuld	%f4,%f6,%f4
/* 0x0490	 168 */		add	%g5,%l1,%l1
/* 0x0494	 166 */		ldx	[%sp+2255],%g2
/* 0x0498	     */		fmuld	%f10,%f6,%f6
/* 0x049c	 168 */		srax	%l4,32,%g5
/* 0x04a0	     */		st	%l2,[%i5+32]
/* 0x04a4	     */		add	%l1,%g5,%l1
/* 0x04a8	 166 */		ldx	[%sp+2247],%o0
/* 0x04ac	 168 */		add	%g2,%l0,%g2
/* 0x04b0	 166 */		fdtox	%f4,%f4
/* 0x04b4	     */		std	%f4,[%sp+2231]
/* 0x04b8	 168 */		srax	%l1,32,%o1
/* 0x04bc	 166 */		fdtox	%f6,%f4
/* 0x04c0	     */		std	%f4,[%sp+2223]
/* 0x04c4	 168 */		add	%g2,%o1,%g2
/* 0x04c8	 166 */		ldx	[%sp+2239],%g5
/* 0x04cc	 168 */		srax	%g2,32,%o1
/* 0x04d0	     */		add	%o0,%o5,%o0
/* 0x04d4	     */		st	%l4,[%i5+36]
/* 0x04d8	     */		add	%g5,%l3,%l3
/* 0x04dc	     */		add	%o0,%o1,%g5
/* 0x04e0	 166 */		ldx	[%sp+2223],%o1
/* 0x04e4	 168 */		srax	%g5,32,%o0
/* 0x04e8	 166 */		ldx	[%sp+2231],%o2
/* 0x04ec	 168 */		st	%l1,[%i5+40]
/* 0x04f0	     */		add	%l3,%o0,%g3
/* 0x04f4	     */		st	%g2,[%i5+44]
/* 0x04f8	     */		srax	%g3,32,%g4
/* 0x04fc	     */		add	%o2,%l5,%o2
/* 0x0500	     */		st	%g5,[%i5+48]
/* 0x0504	     */		add	%o2,%g4,%g4
/* 0x0508	     */		st	%g3,[%i5+52]
/* 0x050c	     */		srax	%g4,32,%o0
/* 0x0510	     */		ld	[%i1+60],%g2
/* 0x0514	     */		st	%g4,[%i5+56]
/* 0x0518	     */		add	%o1,%g2,%g2
/* 0x051c	     */		add	%g2,%o0,%g2
/* 0x0520	     */		st	%g2,[%i5+60]

!  170		      !      return c;

/* 0x0524	 170 */		srax	%g2,32,%o0
/* 0x0528	     */		srl	%o0,0,%i0
/* 0x052c	     */		ret	! Result =  %i0
/* 0x0530	     */		restore	%g0,%g0,%g0
                       .L900000161:

!  172		      !    } else {
!  173		      !      DEF_VARS(BUFF_SIZE);

/* 0x0534	 173 */		fmovd	%f0,%f14
/* 0x0538	     */		ldd	[%o0],%f8

!  174		      !      t_s32 i, c = 0;
!  176		      !#pragma pipeloop(0)
!  177		      !      for (i = 0; i < (n+1)/2; i ++) {

/* 0x053c	 177 */		add	%i3,1,%g2
/* 0x0540	 173 */		ld	[%sp+2223],%f7
/* 0x0544	 177 */		srl	%g2,31,%g3

!  178		      !        MUL_U32_S64_2(i);

/* 0x0548	 178 */		add	%fp,-25,%g4
/* 0x054c	 177 */		add	%g2,%g3,%g2
/* 0x0550	 174 */		or	%g0,0,%i0
/* 0x0554	 173 */		ldd	[%o0+8],%f18
/* 0x0558	     */		fmovs	%f8,%f6
/* 0x055c	 177 */		sra	%g2,1,%o5
/* 0x0560	     */		or	%g0,0,%o0
/* 0x0564	     */		cmp	%o5,0
/* 0x0568	 178 */		or	%g0,0,%g3
/* 0x056c	 173 */		fsubd	%f6,%f8,%f16
/* 0x0570	 177 */		ble,pt	%icc,.L900000160
/* 0x0574	     */		or	%g0,0,%g2
/* 0x0578	 178 */		or	%g0,1,%g2
/* 0x057c	     */		cmp	%o5,10
/* 0x0580	     */		bl,pn	%icc,.L77000041
/* 0x0584	     */		or	%g0,0,%o2
/* 0x0588	     */		ldd	[%i4+8],%f0
/* 0x058c	     */		sub	%o5,2,%o3
/* 0x0590	     */		ldd	[%i4],%f2
/* 0x0594	     */		or	%g0,7,%o0
/* 0x0598	     */		or	%g0,2,%g3
/* 0x059c	     */		fxnor	%f14,%f0,%f8
/* 0x05a0	     */		ldd	[%i4+16],%f4
/* 0x05a4	     */		or	%g0,16,%o2
/* 0x05a8	     */		fxnor	%f14,%f2,%f2
/* 0x05ac	     */		ldd	[%i4+24],%f6
/* 0x05b0	     */		or	%g0,48,%o4
/* 0x05b4	     */		fitod	%f8,%f12
/* 0x05b8	     */		or	%g0,24,%o1
/* 0x05bc	     */		or	%g0,3,%g2
/* 0x05c0	     */		fitod	%f2,%f0
/* 0x05c4	     */		fitod	%f3,%f20
/* 0x05c8	     */		ldd	[%i4+32],%f2
/* 0x05cc	     */		fitod	%f9,%f10
/* 0x05d0	     */		ldd	[%i4+40],%f8
/* 0x05d4	     */		fsubd	%f18,%f0,%f0
/* 0x05d8	     */		fsubd	%f18,%f20,%f22
/* 0x05dc	     */		fsubd	%f18,%f12,%f20
/* 0x05e0	     */		ldd	[%i4+48],%f12
/* 0x05e4	     */		fsubd	%f18,%f10,%f10
/* 0x05e8	     */		fmuld	%f0,%f16,%f0
/* 0x05ec	     */		fxnor	%f14,%f4,%f4
/* 0x05f0	     */		fmuld	%f22,%f16,%f22
/* 0x05f4	     */		fxnor	%f14,%f6,%f6
/* 0x05f8	     */		fmuld	%f20,%f16,%f20
/* 0x05fc	     */		fdtox	%f0,%f0
/* 0x0600	     */		std	%f0,[%fp-25]
/* 0x0604	     */		fmuld	%f10,%f16,%f10
/* 0x0608	     */		fdtox	%f22,%f22
/* 0x060c	     */		std	%f22,[%fp-17]
/* 0x0610	     */		fitod	%f5,%f0
/* 0x0614	     */		fdtox	%f10,%f10
/* 0x0618	     */		fdtox	%f20,%f20
/* 0x061c	     */		std	%f20,[%fp-9]
/* 0x0620	     */		fitod	%f4,%f4
/* 0x0624	     */		std	%f10,[%fp-1]
/* 0x0628	     */		fxnor	%f14,%f2,%f10
/* 0x062c	     */		fitod	%f7,%f2
/* 0x0630	     */		fsubd	%f18,%f0,%f0
/* 0x0634	     */		fsubd	%f18,%f4,%f4
/* 0x0638	     */		fxnor	%f14,%f8,%f8
                       .L900000145:
/* 0x063c	     */		fitod	%f11,%f22
/* 0x0640	     */		add	%o0,3,%o0
/* 0x0644	     */		add	%g2,6,%g2
/* 0x0648	     */		fmuld	%f0,%f16,%f0
/* 0x064c	     */		fmuld	%f4,%f16,%f24
/* 0x0650	     */		cmp	%o0,%o3
/* 0x0654	     */		add	%g3,6,%g3
/* 0x0658	     */		fsubd	%f18,%f2,%f2
/* 0x065c	     */		fitod	%f6,%f4
/* 0x0660	     */		fdtox	%f0,%f0
/* 0x0664	     */		add	%o4,8,%i1
/* 0x0668	     */		ldd	[%i4+%i1],%f20
/* 0x066c	     */		fdtox	%f24,%f6
/* 0x0670	     */		add	%o2,16,%o4
/* 0x0674	     */		fsubd	%f18,%f4,%f4
/* 0x0678	     */		std	%f6,[%o4+%g4]
/* 0x067c	     */		add	%o1,16,%o2
/* 0x0680	     */		fxnor	%f14,%f12,%f6
/* 0x0684	     */		std	%f0,[%o2+%g4]
/* 0x0688	     */		fitod	%f9,%f0
/* 0x068c	     */		fmuld	%f2,%f16,%f2
/* 0x0690	     */		fmuld	%f4,%f16,%f24
/* 0x0694	     */		fsubd	%f18,%f22,%f12
/* 0x0698	     */		fitod	%f10,%f4
/* 0x069c	     */		fdtox	%f2,%f2
/* 0x06a0	     */		add	%i1,8,%o1
/* 0x06a4	     */		ldd	[%i4+%o1],%f22
/* 0x06a8	     */		fdtox	%f24,%f10
/* 0x06ac	     */		add	%o4,16,%i3
/* 0x06b0	     */		fsubd	%f18,%f4,%f4
/* 0x06b4	     */		std	%f10,[%i3+%g4]
/* 0x06b8	     */		add	%o2,16,%i1
/* 0x06bc	     */		fxnor	%f14,%f20,%f10
/* 0x06c0	     */		std	%f2,[%i1+%g4]
/* 0x06c4	     */		fitod	%f7,%f2
/* 0x06c8	     */		fmuld	%f12,%f16,%f12
/* 0x06cc	     */		fmuld	%f4,%f16,%f24
/* 0x06d0	     */		fsubd	%f18,%f0,%f0
/* 0x06d4	     */		fitod	%f8,%f4
/* 0x06d8	     */		fdtox	%f12,%f20
/* 0x06dc	     */		add	%o1,8,%o4
/* 0x06e0	     */		ldd	[%i4+%o4],%f12
/* 0x06e4	     */		fdtox	%f24,%f8
/* 0x06e8	     */		add	%i3,16,%o2
/* 0x06ec	     */		fsubd	%f18,%f4,%f4
/* 0x06f0	     */		std	%f8,[%o2+%g4]
/* 0x06f4	     */		add	%i1,16,%o1
/* 0x06f8	     */		fxnor	%f14,%f22,%f8
/* 0x06fc	     */		bl,pt	%icc,.L900000145
/* 0x0700	     */		std	%f20,[%o1+%g4]
                       .L900000148:
/* 0x0704	     */		fitod	%f6,%f6
/* 0x0708	     */		fmuld	%f4,%f16,%f24
/* 0x070c	     */		add	%i3,32,%l4
/* 0x0710	     */		fsubd	%f18,%f2,%f2
/* 0x0714	     */		fmuld	%f0,%f16,%f22
/* 0x0718	     */		add	%i1,32,%l3
/* 0x071c	     */		fitod	%f10,%f28
/* 0x0720	     */		sra	%o0,0,%o1
/* 0x0724	     */		add	%i3,48,%l2
/* 0x0728	     */		fsubd	%f18,%f6,%f4
/* 0x072c	     */		add	%i1,48,%l1
/* 0x0730	     */		add	%i3,64,%l0
/* 0x0734	     */		fitod	%f11,%f26
/* 0x0738	     */		sllx	%o1,3,%o2
/* 0x073c	     */		add	%i1,64,%i5
/* 0x0740	     */		fitod	%f8,%f6
/* 0x0744	     */		add	%i3,80,%i3
/* 0x0748	     */		add	%i1,80,%i1
/* 0x074c	     */		fxnor	%f14,%f12,%f0
/* 0x0750	     */		fmuld	%f4,%f16,%f20
/* 0x0754	     */		add	%i3,16,%o4
/* 0x0758	     */		fitod	%f9,%f4
/* 0x075c	     */		fmuld	%f2,%f16,%f12
/* 0x0760	     */		add	%i1,16,%o3
/* 0x0764	     */		fsubd	%f18,%f28,%f10
/* 0x0768	     */		cmp	%o0,%o5
/* 0x076c	     */		add	%g2,12,%g2
/* 0x0770	     */		fitod	%f0,%f2
/* 0x0774	     */		fsubd	%f18,%f26,%f8
/* 0x0778	     */		fitod	%f1,%f0
/* 0x077c	     */		fmuld	%f10,%f16,%f10
/* 0x0780	     */		fdtox	%f24,%f24
/* 0x0784	     */		std	%f24,[%l4+%g4]
/* 0x0788	     */		add	%g3,12,%g3
/* 0x078c	     */		fsubd	%f18,%f6,%f6
/* 0x0790	     */		fmuld	%f8,%f16,%f8
/* 0x0794	     */		fdtox	%f22,%f22
/* 0x0798	     */		std	%f22,[%l3+%g4]
/* 0x079c	     */		fsubd	%f18,%f4,%f4
/* 0x07a0	     */		fdtox	%f20,%f20
/* 0x07a4	     */		std	%f20,[%l2+%g4]
/* 0x07a8	     */		fmuld	%f6,%f16,%f6
/* 0x07ac	     */		fsubd	%f18,%f2,%f2
/* 0x07b0	     */		fsubd	%f18,%f0,%f0
/* 0x07b4	     */		fmuld	%f4,%f16,%f4
/* 0x07b8	     */		fdtox	%f12,%f12
/* 0x07bc	     */		std	%f12,[%l1+%g4]
/* 0x07c0	     */		fdtox	%f10,%f10
/* 0x07c4	     */		std	%f10,[%l0+%g4]
/* 0x07c8	     */		fmuld	%f2,%f16,%f2
/* 0x07cc	     */		fdtox	%f8,%f8
/* 0x07d0	     */		std	%f8,[%i5+%g4]
/* 0x07d4	     */		fmuld	%f0,%f16,%f0
/* 0x07d8	     */		fdtox	%f6,%f6
/* 0x07dc	     */		std	%f6,[%i3+%g4]
/* 0x07e0	     */		fdtox	%f4,%f4
/* 0x07e4	     */		std	%f4,[%i1+%g4]
/* 0x07e8	     */		fdtox	%f2,%f2
/* 0x07ec	     */		std	%f2,[%o4+%g4]
/* 0x07f0	     */		fdtox	%f0,%f0
/* 0x07f4	     */		bge,pn	%icc,.L77000043
/* 0x07f8	     */		std	%f0,[%o3+%g4]
                       .L77000041:
/* 0x07fc	     */		ldd	[%i4+%o2],%f0
                       .L900000159:
/* 0x0800	     */		fxnor	%f14,%f0,%f0
/* 0x0804	     */		sra	%g3,0,%o1
/* 0x0808	     */		add	%o0,1,%o0
/* 0x080c	     */		sllx	%o1,3,%i3
/* 0x0810	     */		add	%g3,2,%g3
/* 0x0814	     */		fitod	%f0,%f2
/* 0x0818	     */		sra	%g2,0,%o1
/* 0x081c	     */		add	%g2,2,%g2
/* 0x0820	     */		fitod	%f1,%f0
/* 0x0824	     */		sllx	%o1,3,%i1
/* 0x0828	     */		cmp	%o0,%o5
/* 0x082c	     */		sra	%o0,0,%o1
/* 0x0830	     */		fsubd	%f18,%f2,%f2
/* 0x0834	     */		sllx	%o1,3,%o2
/* 0x0838	     */		fsubd	%f18,%f0,%f0
/* 0x083c	     */		fmuld	%f2,%f16,%f2
/* 0x0840	     */		fmuld	%f0,%f16,%f0
/* 0x0844	     */		fdtox	%f2,%f2
/* 0x0848	     */		std	%f2,[%i3+%g4]
/* 0x084c	     */		fdtox	%f0,%f0
/* 0x0850	     */		std	%f0,[%i1+%g4]
/* 0x0854	     */		bl,a,pt	%icc,.L900000159
/* 0x0858	     */		ldd	[%i4+%o2],%f0
                       .L77000043:

!  179		      !      }
!  181		      !#pragma pipeloop(0)
!  182		      !      for (i = 0; i < n; i ++) {

/* 0x085c	 182 */		or	%g0,0,%g2
                       .L900000160:
/* 0x0860	 182 */		cmp	%i2,0
/* 0x0864	     */		ble,a,pn	%icc,.L77000061
/* 0x0868	     */		or	%g0,%i0,%o0

!  183		      !        ADD_S64_U32(i);

/* 0x086c	 183 */		ldx	[%fp-17],%o0
/* 0x0870	 182 */		cmp	%i2,5
/* 0x0874	 183 */		sra	%i0,0,%i1
/* 0x0878	 182 */		bl,pn	%icc,.L77000045
/* 0x087c	     */		ldx	[%fp-25],%o3
/* 0x0880	     */		or	%g0,8,%o5
/* 0x0884	     */		or	%g0,16,%o4
/* 0x0888	 183 */		ld	[%o7],%i0
/* 0x088c	     */		or	%g0,3,%g2
/* 0x0890	     */		add	%o3,%i0,%o3
/* 0x0894	     */		ld	[%o7+4],%i0
/* 0x0898	     */		add	%o3,%i1,%o2
/* 0x089c	     */		st	%o2,[%g5]
/* 0x08a0	 182 */		sub	%i2,1,%o3
/* 0x08a4	 183 */		srax	%o2,32,%o1
/* 0x08a8	     */		ldx	[%fp-9],%o2
/* 0x08ac	     */		add	%o0,%i0,%o0
/* 0x08b0	     */		or	%g0,%o1,%i0
/* 0x08b4	     */		ld	[%o7+8],%o1
/* 0x08b8	     */		add	%o0,%i0,%o0
/* 0x08bc	     */		st	%o0,[%g5+4]
/* 0x08c0	     */		srax	%o0,32,%o0
                       .L900000141:
/* 0x08c4	     */		add	%o5,4,%g3
/* 0x08c8	     */		add	%o4,8,%o4
/* 0x08cc	 183 */		ldx	[%o4+%g4],%i1
/* 0x08d0	     */		sra	%o0,0,%i0
/* 0x08d4	     */		add	%o2,%o1,%o1
/* 0x08d8	     */		ld	[%o7+%g3],%o0
/* 0x08dc	     */		add	%o1,%i0,%o1
/* 0x08e0	     */		add	%g2,2,%g2
/* 0x08e4	     */		st	%o1,[%g5+%o5]
/* 0x08e8	     */		srax	%o1,32,%i0
/* 0x08ec	     */		cmp	%g2,%o3
/* 0x08f0	 182 */		add	%o5,8,%o5
/* 0x08f4	     */		add	%o4,8,%o4
/* 0x08f8	 183 */		ldx	[%o4+%g4],%o2
/* 0x08fc	     */		add	%i1,%o0,%o0
/* 0x0900	     */		ld	[%o7+%o5],%o1
/* 0x0904	     */		add	%o0,%i0,%o0
/* 0x0908	     */		st	%o0,[%g5+%g3]
/* 0x090c	     */		bl,pt	%icc,.L900000141
/* 0x0910	     */		srax	%o0,32,%o0
                       .L900000144:
/* 0x0914	 183 */		sra	%o0,0,%o3
/* 0x0918	     */		add	%o2,%o1,%o0
/* 0x091c	     */		add	%o0,%o3,%o0
/* 0x0920	     */		st	%o0,[%g5+%o5]
/* 0x0924	     */		cmp	%g2,%i2
/* 0x0928	     */		srax	%o0,32,%i0
/* 0x092c	     */		bge,a,pn	%icc,.L77000061
/* 0x0930	     */		or	%g0,%i0,%o0
                       .L77000045:
/* 0x0934	 183 */		sra	%g2,0,%o0
                       .L900000158:
/* 0x0938	 183 */		sllx	%o0,2,%o5
/* 0x093c	     */		add	%g2,1,%g2
/* 0x0940	     */		sllx	%o0,3,%o4
/* 0x0944	     */		ld	[%o7+%o5],%o2
/* 0x0948	     */		cmp	%g2,%i2
/* 0x094c	     */		ldx	[%o4+%g4],%o0
/* 0x0950	     */		sra	%i0,0,%o1
/* 0x0954	     */		add	%o0,%o2,%o0
/* 0x0958	     */		add	%o0,%o1,%o0
/* 0x095c	     */		st	%o0,[%g5+%o5]
/* 0x0960	     */		srax	%o0,32,%i0
/* 0x0964	     */		bl,pt	%icc,.L900000158
/* 0x0968	     */		sra	%g2,0,%o0
                       .L77000047:

!  184		      !      }
!  186		      !      return c;

/* 0x096c	 186 */		or	%g0,%i0,%o0
/* 0x0970	     */		srl	%o0,0,%i0
/* 0x0974	     */		ret	! Result =  %i0
/* 0x0978	     */		restore	%g0,%g0,%g0
                       .L77000048:

!  188		      !    }
!  189		      !  } else {
!  191		      !    if (n == 8) {

/* 0x097c	 191 */		bne,pn	%icc,.L77000050
/* 0x0980	     */		sethi	%hi(0xfff80000),%g2

!  192		      !      DEF_VARS(2*8);
!  193		      !      t_d64 d0, d1, db;
!  194		      !      t_u32 uc = 0;
!  196		      !      da = (t_d64)(a &  A_MASK);
!  197		      !      db = (t_d64)(a >> A_BITS);
!  199		      !      MUL_U32_S64_D_8(0);

/* 0x0984	 199 */		ldd	[%i0],%f4
/* 0x0988	 196 */		ldd	[%o0],%f6
/* 0x098c	 197 */		srl	%o1,19,%g3
/* 0x0990	 196 */		andn	%o1,%g2,%g2
/* 0x0994	 197 */		st	%g3,[%sp+2351]
/* 0x0998	     */		fxnor	%f0,%f4,%f4
/* 0x099c	 196 */		st	%g2,[%sp+2355]
/* 0x09a0	 199 */		ldd	[%i0+8],%f12
/* 0x09a4	     */		fitod	%f4,%f10
/* 0x09a8	     */		ldd	[%o0+8],%f16
/* 0x09ac	     */		fitod	%f5,%f4
/* 0x09b0	     */		ldd	[%i0+16],%f18
/* 0x09b4	     */		fxnor	%f0,%f12,%f12
/* 0x09b8	 197 */		ld	[%sp+2351],%f9
/* 0x09bc	 199 */		fsubd	%f16,%f10,%f10
/* 0x09c0	 196 */		ld	[%sp+2355],%f15
/* 0x09c4	 199 */		fitod	%f12,%f22
/* 0x09c8	     */		ldd	[%i0+24],%f20
/* 0x09cc	     */		fitod	%f13,%f12

!  200		      !      ADD_S64_U32_D_8(0);

/* 0x09d0	 200 */		ld	[%i1],%g2
/* 0x09d4	 199 */		fsubd	%f16,%f4,%f4
/* 0x09d8	 200 */		ld	[%i1+4],%g3
/* 0x09dc	 199 */		fsubd	%f16,%f22,%f22
/* 0x09e0	 200 */		ld	[%i1+8],%g4
/* 0x09e4	     */		fxnor	%f0,%f18,%f18
/* 0x09e8	     */		ld	[%i1+12],%g5
/* 0x09ec	 199 */		fsubd	%f16,%f12,%f12
/* 0x09f0	 200 */		ld	[%i1+16],%o0
/* 0x09f4	 199 */		fitod	%f18,%f26
/* 0x09f8	 200 */		ld	[%i1+20],%o1
/* 0x09fc	     */		fxnor	%f0,%f20,%f20
/* 0x0a00	     */		ld	[%i1+24],%o2
/* 0x0a04	     */		ld	[%i1+28],%o3
/* 0x0a08	 197 */		fmovs	%f6,%f8
/* 0x0a0c	 196 */		fmovs	%f6,%f14
/* 0x0a10	 197 */		fsubd	%f8,%f6,%f8
/* 0x0a14	 196 */		fsubd	%f14,%f6,%f6
/* 0x0a18	 199 */		fmuld	%f10,%f8,%f14
/* 0x0a1c	     */		fmuld	%f10,%f6,%f10
/* 0x0a20	     */		fmuld	%f4,%f8,%f24
/* 0x0a24	     */		fdtox	%f14,%f14
/* 0x0a28	     */		std	%f14,[%sp+2335]
/* 0x0a2c	     */		fmuld	%f22,%f8,%f28
/* 0x0a30	     */		fitod	%f19,%f14
/* 0x0a34	     */		fmuld	%f22,%f6,%f18
/* 0x0a38	     */		fdtox	%f10,%f10
/* 0x0a3c	     */		std	%f10,[%sp+2343]
/* 0x0a40	     */		fmuld	%f4,%f6,%f4
/* 0x0a44	     */		fmuld	%f12,%f8,%f22
/* 0x0a48	     */		fdtox	%f18,%f18
/* 0x0a4c	     */		std	%f18,[%sp+2311]
/* 0x0a50	     */		fmuld	%f12,%f6,%f10
/* 0x0a54	     */		ldx	[%sp+2335],%o4
/* 0x0a58	     */		fdtox	%f24,%f12
/* 0x0a5c	     */		std	%f12,[%sp+2319]
/* 0x0a60	     */		fsubd	%f16,%f26,%f12
/* 0x0a64	     */		ldx	[%sp+2343],%o5
/* 0x0a68	 200 */		sllx	%o4,19,%o4
/* 0x0a6c	 199 */		fdtox	%f4,%f4
/* 0x0a70	     */		std	%f4,[%sp+2327]
/* 0x0a74	     */		fitod	%f20,%f4
/* 0x0a78	     */		fdtox	%f28,%f24
/* 0x0a7c	     */		std	%f24,[%sp+2303]
/* 0x0a80	 200 */		add	%o5,%o4,%o4
/* 0x0a84	 199 */		ldx	[%sp+2319],%o7
/* 0x0a88	     */		fitod	%f21,%f18
/* 0x0a8c	 200 */		add	%o4,%g2,%o4
/* 0x0a90	 199 */		fmuld	%f12,%f8,%f24
/* 0x0a94	     */		ldx	[%sp+2327],%o5
/* 0x0a98	     */		fsubd	%f16,%f14,%f14
/* 0x0a9c	     */		fmuld	%f12,%f6,%f12
/* 0x0aa0	 200 */		st	%o4,[%i5]
/* 0x0aa4	     */		sllx	%o7,19,%o7
/* 0x0aa8	 199 */		fdtox	%f22,%f20
/* 0x0aac	     */		std	%f20,[%sp+2287]
/* 0x0ab0	     */		fsubd	%f16,%f4,%f4
/* 0x0ab4	 200 */		add	%o5,%o7,%o5
/* 0x0ab8	 199 */		ldx	[%sp+2303],%l0
/* 0x0abc	 200 */		srlx	%o4,32,%o7
/* 0x0ac0	 199 */		fdtox	%f10,%f10
/* 0x0ac4	     */		fmuld	%f14,%f8,%f20
/* 0x0ac8	     */		std	%f10,[%sp+2295]
/* 0x0acc	     */		fdtox	%f24,%f10
/* 0x0ad0	 200 */		add	%o5,%g3,%g3
/* 0x0ad4	 199 */		fmuld	%f14,%f6,%f14
/* 0x0ad8	     */		std	%f10,[%sp+2271]
/* 0x0adc	 200 */		sllx	%l0,19,%l0
/* 0x0ae0	     */		add	%g3,%o7,%g3
/* 0x0ae4	 199 */		fsubd	%f16,%f18,%f10
/* 0x0ae8	     */		ldx	[%sp+2311],%g2
/* 0x0aec	     */		fdtox	%f12,%f12
/* 0x0af0	     */		fmuld	%f4,%f8,%f16
/* 0x0af4	     */		std	%f12,[%sp+2279]
/* 0x0af8	     */		fdtox	%f20,%f12
/* 0x0afc	     */		fmuld	%f4,%f6,%f4
/* 0x0b00	     */		ldx	[%sp+2295],%l1
/* 0x0b04	 200 */		add	%g2,%l0,%g2
/* 0x0b08	 199 */		fmuld	%f10,%f8,%f8
/* 0x0b0c	     */		std	%f12,[%sp+2255]
/* 0x0b10	 200 */		srlx	%g3,32,%l0
/* 0x0b14	     */		add	%g2,%g4,%g4
/* 0x0b18	 199 */		fmuld	%f10,%f6,%f6
/* 0x0b1c	     */		ldx	[%sp+2287],%o5
/* 0x0b20	 200 */		add	%g4,%l0,%g4
/* 0x0b24	 199 */		fdtox	%f14,%f12
/* 0x0b28	     */		std	%f12,[%sp+2263]
/* 0x0b2c	     */		fdtox	%f16,%f10
/* 0x0b30	     */		std	%f10,[%sp+2239]
/* 0x0b34	 200 */		sllx	%o5,19,%o5
/* 0x0b38	 199 */		fdtox	%f4,%f4
/* 0x0b3c	     */		ldx	[%sp+2271],%o7
/* 0x0b40	 200 */		add	%l1,%o5,%o5
/* 0x0b44	 199 */		fdtox	%f6,%f6
/* 0x0b48	     */		std	%f4,[%sp+2247]
/* 0x0b4c	 200 */		add	%o5,%g5,%g5
/* 0x0b50	 199 */		fdtox	%f8,%f4
/* 0x0b54	 200 */		sllx	%o7,19,%o7
/* 0x0b58	 199 */		ldx	[%sp+2279],%g2
/* 0x0b5c	 200 */		srlx	%g4,32,%o5
/* 0x0b60	 199 */		std	%f4,[%sp+2223]
/* 0x0b64	     */		ldx	[%sp+2255],%l0
/* 0x0b68	 200 */		add	%g2,%o7,%g2
/* 0x0b6c	     */		add	%g5,%o5,%g5
/* 0x0b70	 199 */		std	%f6,[%sp+2231]
/* 0x0b74	 200 */		add	%g2,%o0,%g2
/* 0x0b78	     */		srlx	%g5,32,%o0
/* 0x0b7c	 199 */		ldx	[%sp+2263],%l1
/* 0x0b80	 200 */		sllx	%l0,19,%l0
/* 0x0b84	 199 */		ldx	[%sp+2247],%o7
/* 0x0b88	 200 */		add	%g2,%o0,%g2
/* 0x0b8c	     */		st	%g3,[%i5+4]
/* 0x0b90	     */		add	%l1,%l0,%l0
/* 0x0b94	 199 */		ldx	[%sp+2239],%o5
/* 0x0b98	 200 */		srlx	%g2,32,%o4
/* 0x0b9c	     */		add	%l0,%o1,%o1
/* 0x0ba0	 199 */		ldx	[%sp+2223],%o0
/* 0x0ba4	 200 */		add	%o1,%o4,%o1
/* 0x0ba8	     */		sllx	%o5,19,%g3
/* 0x0bac	 199 */		ldx	[%sp+2231],%l1
/* 0x0bb0	 200 */		add	%o7,%g3,%g3
/* 0x0bb4	     */		st	%g2,[%i5+16]
/* 0x0bb8	     */		add	%g3,%o2,%o2
/* 0x0bbc	     */		st	%o1,[%i5+20]
/* 0x0bc0	     */		srlx	%o1,32,%g2
/* 0x0bc4	     */		st	%g4,[%i5+8]
/* 0x0bc8	     */		sllx	%o0,19,%g3
/* 0x0bcc	     */		add	%o2,%g2,%g2
/* 0x0bd0	     */		st	%g2,[%i5+24]
/* 0x0bd4	     */		add	%l1,%g3,%g3
/* 0x0bd8	     */		st	%g5,[%i5+12]
/* 0x0bdc	     */		srlx	%g2,32,%g2
/* 0x0be0	     */		add	%g3,%o3,%g3
/* 0x0be4	     */		add	%g3,%g2,%g2
/* 0x0be8	     */		st	%g2,[%i5+28]

!  202		      !      return uc;

/* 0x0bec	 202 */		srlx	%g2,32,%o0
/* 0x0bf0	     */		srl	%o0,0,%i0
/* 0x0bf4	     */		ret	! Result =  %i0
/* 0x0bf8	     */		restore	%g0,%g0,%g0
                       .L77000050:

!  204		      !    } else if (n == 16) {

/* 0x0bfc	 204 */		cmp	%i3,16
/* 0x0c00	     */		bne,pn	%icc,.L77000052
/* 0x0c04	     */		sethi	%hi(0xfff80000),%g2

!  205		      !      DEF_VARS(2*16);
!  206		      !      t_d64 d0, d1, db;
!  207		      !      t_u32 uc = 0;
!  209		      !      da = (t_d64)(a &  A_MASK);
!  210		      !      db = (t_d64)(a >> A_BITS);
!  212		      !      MUL_U32_S64_D_8(0);

/* 0x0c08	 212 */		ldd	[%i0],%f4
/* 0x0c0c	 209 */		andn	%o1,%g2,%g2
/* 0x0c10	     */		st	%g2,[%sp+2483]
/* 0x0c14	 210 */		srl	%o1,19,%g2
/* 0x0c18	     */		fxnor	%f0,%f4,%f4
/* 0x0c1c	     */		st	%g2,[%sp+2479]
/* 0x0c20	 209 */		ldd	[%o0],%f8
/* 0x0c24	 212 */		fitod	%f4,%f10
/* 0x0c28	     */		ldd	[%o0+8],%f16
/* 0x0c2c	     */		fitod	%f5,%f4
/* 0x0c30	     */		ldd	[%i0+8],%f14
/* 0x0c34	 209 */		ld	[%sp+2483],%f13
/* 0x0c38	 210 */		ld	[%sp+2479],%f7
/* 0x0c3c	     */		fxnor	%f0,%f14,%f14
/* 0x0c40	 212 */		fsubd	%f16,%f4,%f4
/* 0x0c44	     */		fsubd	%f16,%f10,%f10
/* 0x0c48	 209 */		fmovs	%f8,%f12
/* 0x0c4c	 210 */		fmovs	%f8,%f6
/* 0x0c50	 209 */		fsubd	%f12,%f8,%f12
/* 0x0c54	 210 */		fsubd	%f6,%f8,%f6
/* 0x0c58	 212 */		fitod	%f14,%f8
/* 0x0c5c	     */		fmuld	%f4,%f12,%f20
/* 0x0c60	     */		fitod	%f15,%f14
/* 0x0c64	     */		fmuld	%f4,%f6,%f4
/* 0x0c68	     */		fmuld	%f10,%f12,%f18
/* 0x0c6c	     */		fmuld	%f10,%f6,%f10
/* 0x0c70	     */		fsubd	%f16,%f14,%f14
/* 0x0c74	     */		fdtox	%f4,%f4
/* 0x0c78	     */		std	%f4,[%sp+2447]
/* 0x0c7c	     */		fsubd	%f16,%f8,%f4
/* 0x0c80	     */		fdtox	%f10,%f10
/* 0x0c84	     */		std	%f10,[%sp+2463]
/* 0x0c88	     */		fdtox	%f20,%f10
/* 0x0c8c	     */		std	%f10,[%sp+2455]
/* 0x0c90	     */		fmuld	%f4,%f12,%f8
/* 0x0c94	     */		fdtox	%f18,%f18
/* 0x0c98	     */		std	%f18,[%sp+2471]
/* 0x0c9c	     */		fmuld	%f4,%f6,%f4
/* 0x0ca0	     */		ldx	[%sp+2447],%g5
/* 0x0ca4	     */		fmuld	%f14,%f12,%f10
/* 0x0ca8	     */		ldx	[%sp+2455],%g4
/* 0x0cac	     */		fmuld	%f14,%f6,%f14
/* 0x0cb0	     */		fdtox	%f8,%f8
/* 0x0cb4	     */		std	%f8,[%sp+2439]
/* 0x0cb8	     */		fdtox	%f4,%f4
/* 0x0cbc	     */		std	%f4,[%sp+2431]
/* 0x0cc0	     */		fdtox	%f10,%f4
/* 0x0cc4	     */		std	%f4,[%sp+2423]
/* 0x0cc8	     */		fdtox	%f14,%f4
/* 0x0ccc	     */		std	%f4,[%sp+2415]
/* 0x0cd0	     */		ldd	[%i0+24],%f14
/* 0x0cd4	     */		ldd	[%i0+16],%f4
/* 0x0cd8	     */		fxnor	%f0,%f14,%f14
/* 0x0cdc	     */		ldx	[%sp+2463],%g2
/* 0x0ce0	     */		fxnor	%f0,%f4,%f4
/* 0x0ce4	     */		ldx	[%sp+2471],%g3
/* 0x0ce8	     */		fitod	%f14,%f10
/* 0x0cec	     */		ldx	[%sp+2431],%o0

!  213		      !      MUL_U32_S64_D_8(4);
!  214		      !      ADD_S64_U32_D_8(0);

/* 0x0cf0	 214 */		sllx	%g2,19,%g2
/* 0x0cf4	 212 */		fitod	%f4,%f8
/* 0x0cf8	     */		ldx	[%sp+2415],%o3
/* 0x0cfc	 214 */		add	%g3,%g2,%g2
/* 0x0d00	 212 */		fitod	%f5,%f4
/* 0x0d04	     */		ldx	[%sp+2439],%o1
/* 0x0d08	 214 */		sllx	%g5,19,%g3
/* 0x0d0c	 212 */		fitod	%f15,%f14
/* 0x0d10	     */		ldx	[%sp+2423],%o2
/* 0x0d14	 214 */		sllx	%o3,19,%o3
/* 0x0d18	 212 */		fsubd	%f16,%f8,%f8
/* 0x0d1c	 214 */		add	%g4,%g3,%g3
/* 0x0d20	 212 */		fsubd	%f16,%f4,%f4
/* 0x0d24	 214 */		sllx	%o0,19,%g4
/* 0x0d28	     */		add	%o2,%o3,%g5
/* 0x0d2c	 212 */		fsubd	%f16,%f14,%f14
/* 0x0d30	 214 */		add	%o1,%g4,%g4
/* 0x0d34	 212 */		fmuld	%f8,%f12,%f18
/* 0x0d38	     */		fmuld	%f4,%f12,%f20
/* 0x0d3c	     */		fmuld	%f4,%f6,%f4
/* 0x0d40	     */		fmuld	%f8,%f6,%f8
/* 0x0d44	     */		fdtox	%f18,%f18
/* 0x0d48	     */		std	%f18,[%sp+2407]
/* 0x0d4c	     */		fdtox	%f4,%f4
/* 0x0d50	     */		std	%f4,[%sp+2383]
/* 0x0d54	     */		fsubd	%f16,%f10,%f4
/* 0x0d58	     */		fmuld	%f14,%f12,%f10
/* 0x0d5c	     */		ldx	[%sp+2407],%o5
/* 0x0d60	     */		fdtox	%f8,%f8
/* 0x0d64	     */		std	%f8,[%sp+2399]
/* 0x0d68	     */		fmuld	%f14,%f6,%f14
/* 0x0d6c	     */		fdtox	%f20,%f8
/* 0x0d70	     */		std	%f8,[%sp+2391]
/* 0x0d74	     */		fmuld	%f4,%f12,%f8
/* 0x0d78	     */		fmuld	%f4,%f6,%f4
/* 0x0d7c	     */		fdtox	%f8,%f8
/* 0x0d80	     */		std	%f8,[%sp+2375]
/* 0x0d84	     */		fdtox	%f4,%f4
/* 0x0d88	     */		std	%f4,[%sp+2367]
/* 0x0d8c	     */		fdtox	%f10,%f4
/* 0x0d90	     */		std	%f4,[%sp+2359]
/* 0x0d94	     */		fdtox	%f14,%f4
/* 0x0d98	     */		std	%f4,[%sp+2351]
/* 0x0d9c	 213 */		ldd	[%i0+40],%f14
/* 0x0da0	     */		ldd	[%i0+32],%f4
/* 0x0da4	     */		fxnor	%f0,%f14,%f14
/* 0x0da8	     */		ldd	[%i0+48],%f10
/* 0x0dac	     */		fxnor	%f0,%f4,%f4
/* 0x0db0	 212 */		ldx	[%sp+2399],%o4
/* 0x0db4	     */		ldx	[%sp+2391],%o7
/* 0x0db8	     */		fxnor	%f0,%f10,%f10
/* 0x0dbc	 213 */		fitod	%f4,%f8
/* 0x0dc0	 212 */		ldx	[%sp+2367],%l1
/* 0x0dc4	 214 */		sllx	%o4,19,%o0
/* 0x0dc8	 213 */		fitod	%f5,%f4
/* 0x0dcc	 212 */		ldx	[%sp+2375],%l2
/* 0x0dd0	 214 */		add	%o5,%o0,%o0
/* 0x0dd4	 212 */		ldx	[%sp+2359],%l3
/* 0x0dd8	 213 */		fsubd	%f16,%f8,%f8
/* 0x0ddc	 212 */		ldx	[%sp+2383],%l0
/* 0x0de0	 213 */		fsubd	%f16,%f4,%f4
/* 0x0de4	 212 */		ldx	[%sp+2351],%l4
/* 0x0de8	 214 */		sllx	%l0,19,%l0
/* 0x0dec	 213 */		fmuld	%f8,%f12,%f18
/* 0x0df0	 214 */		sllx	%l4,19,%l4
/* 0x0df4	 213 */		fmuld	%f4,%f12,%f20
/* 0x0df8	     */		fmuld	%f4,%f6,%f4
/* 0x0dfc	     */		fmuld	%f8,%f6,%f8
/* 0x0e00	     */		fdtox	%f18,%f18
/* 0x0e04	     */		std	%f18,[%sp+2343]
/* 0x0e08	     */		fdtox	%f4,%f4
/* 0x0e0c	     */		std	%f4,[%sp+2319]
/* 0x0e10	     */		fitod	%f14,%f4
/* 0x0e14	     */		ldx	[%sp+2343],%i2
/* 0x0e18	     */		fdtox	%f8,%f8
/* 0x0e1c	     */		std	%f8,[%sp+2335]
/* 0x0e20	     */		fdtox	%f20,%f8
/* 0x0e24	     */		std	%f8,[%sp+2327]
/* 0x0e28	     */		fsubd	%f16,%f4,%f4
/* 0x0e2c	     */		fitod	%f15,%f8
/* 0x0e30	     */		fitod	%f10,%f14
/* 0x0e34	     */		fmuld	%f4,%f12,%f18
/* 0x0e38	     */		fitod	%f11,%f10
/* 0x0e3c	     */		ldx	[%sp+2335],%l5
/* 0x0e40	     */		fmuld	%f4,%f6,%f4
/* 0x0e44	     */		fsubd	%f16,%f8,%f8
/* 0x0e48	     */		ldx	[%sp+2327],%i3
/* 0x0e4c	     */		fsubd	%f16,%f14,%f14
/* 0x0e50	     */		ldx	[%sp+2319],%i4
/* 0x0e54	     */		fsubd	%f16,%f10,%f10
/* 0x0e58	     */		fdtox	%f4,%f4
/* 0x0e5c	     */		std	%f4,[%sp+2303]
/* 0x0e60	     */		fmuld	%f8,%f12,%f20
/* 0x0e64	     */		fmuld	%f14,%f12,%f4
/* 0x0e68	     */		fdtox	%f18,%f18
/* 0x0e6c	     */		std	%f18,[%sp+2311]
/* 0x0e70	     */		fmuld	%f8,%f6,%f8
/* 0x0e74	     */		fdtox	%f20,%f18
/* 0x0e78	     */		std	%f18,[%sp+2295]
/* 0x0e7c	     */		fmuld	%f14,%f6,%f14
/* 0x0e80	     */		fdtox	%f4,%f4
/* 0x0e84	     */		std	%f4,[%sp+2279]
/* 0x0e88	     */		fdtox	%f8,%f8
/* 0x0e8c	     */		std	%f8,[%sp+2287]
/* 0x0e90	     */		ldd	[%i0+56],%f4
/* 0x0e94	     */		fmuld	%f10,%f12,%f8
/* 0x0e98	     */		fdtox	%f14,%f14
/* 0x0e9c	     */		std	%f14,[%sp+2271]
/* 0x0ea0	     */		fmuld	%f10,%f6,%f10
/* 0x0ea4	     */		fxnor	%f0,%f4,%f4
/* 0x0ea8	     */		fdtox	%f8,%f8
/* 0x0eac	     */		std	%f8,[%sp+2263]
/* 0x0eb0	     */		fitod	%f4,%f8
/* 0x0eb4	     */		fdtox	%f10,%f10
/* 0x0eb8	     */		std	%f10,[%sp+2255]
/* 0x0ebc	     */		fitod	%f5,%f4
/* 0x0ec0	     */		fsubd	%f16,%f8,%f14
/* 0x0ec4	     */		fsubd	%f16,%f4,%f4
/* 0x0ec8	     */		fmuld	%f14,%f12,%f8
/* 0x0ecc	     */		fmuld	%f14,%f6,%f10
/* 0x0ed0	     */		fmuld	%f4,%f6,%f6
/* 0x0ed4	     */		fdtox	%f8,%f8
/* 0x0ed8	     */		std	%f8,[%sp+2247]
/* 0x0edc	     */		fmuld	%f4,%f12,%f4
/* 0x0ee0	     */		fdtox	%f10,%f8
/* 0x0ee4	     */		std	%f8,[%sp+2239]
/* 0x0ee8	 214 */		ld	[%i1],%o1
/* 0x0eec	 213 */		fdtox	%f6,%f6
/* 0x0ef0	 214 */		ld	[%i1+4],%o2
/* 0x0ef4	 213 */		fdtox	%f4,%f4
/* 0x0ef8	     */		std	%f6,[%sp+2223]
/* 0x0efc	 214 */		add	%g2,%o1,%o1
/* 0x0f00	     */		st	%o1,[%i5]
/* 0x0f04	     */		add	%g3,%o2,%g2
/* 0x0f08	     */		ld	[%i1+8],%g3
/* 0x0f0c	     */		srlx	%o1,32,%o2
/* 0x0f10	 213 */		std	%f4,[%sp+2231]
/* 0x0f14	 214 */		add	%g2,%o2,%g2
/* 0x0f18	     */		srlx	%g2,32,%o2
/* 0x0f1c	     */		st	%g2,[%i5+4]
/* 0x0f20	     */		add	%g4,%g3,%g3
/* 0x0f24	     */		ld	[%i1+12],%g4
/* 0x0f28	     */		add	%g3,%o2,%g3
/* 0x0f2c	     */		srlx	%g3,32,%o1
/* 0x0f30	     */		st	%g3,[%i5+8]

!  215		      !      ADD_S64_U32_D_8(8);

/* 0x0f34	 215 */		ldx	[%sp+2271],%o2
/* 0x0f38	 214 */		add	%g5,%g4,%g4
/* 0x0f3c	     */		ld	[%i1+16],%g5
/* 0x0f40	     */		add	%g4,%o1,%g4
/* 0x0f44	     */		srlx	%g4,32,%g3
/* 0x0f48	     */		st	%g4,[%i5+12]
/* 0x0f4c	     */		sllx	%l1,19,%g4
/* 0x0f50	 215 */		ldx	[%sp+2303],%o1
/* 0x0f54	 214 */		add	%o0,%g5,%g2
/* 0x0f58	     */		ld	[%i1+20],%g5
/* 0x0f5c	     */		add	%g2,%g3,%g2
/* 0x0f60	     */		add	%o7,%l0,%g3
/* 0x0f64	 215 */		sllx	%o1,19,%o1
/* 0x0f68	 214 */		st	%g2,[%i5+16]
/* 0x0f6c	     */		add	%l2,%g4,%g4
/* 0x0f70	     */		srlx	%g2,32,%g2
/* 0x0f74	     */		ld	[%i1+28],%o0
/* 0x0f78	     */		add	%g3,%g5,%g3
/* 0x0f7c	     */		ld	[%i1+24],%g5
/* 0x0f80	     */		add	%g3,%g2,%g2
/* 0x0f84	     */		st	%g2,[%i5+20]
/* 0x0f88	     */		srlx	%g2,32,%g2
/* 0x0f8c	     */		add	%g4,%g5,%g3
/* 0x0f90	 215 */		ld	[%i1+32],%g4
/* 0x0f94	     */		sllx	%l5,19,%g5
/* 0x0f98	 214 */		add	%g3,%g2,%g2
/* 0x0f9c	     */		st	%g2,[%i5+24]
/* 0x0fa0	     */		srlx	%g2,32,%g2
/* 0x0fa4	     */		add	%l3,%l4,%g3
/* 0x0fa8	     */		add	%g3,%o0,%g3
/* 0x0fac	 215 */		ld	[%i1+36],%o0
/* 0x0fb0	     */		add	%i2,%g5,%g5
/* 0x0fb4	 214 */		add	%g3,%g2,%g2
/* 0x0fb8	     */		st	%g2,[%i5+28]
/* 0x0fbc	 215 */		add	%g5,%g4,%g3
/* 0x0fc0	     */		srlx	%g2,32,%g2
/* 0x0fc4	     */		ldx	[%sp+2311],%g4
/* 0x0fc8	     */		sllx	%i4,19,%g5
/* 0x0fcc	     */		add	%g3,%g2,%g2
/* 0x0fd0	     */		st	%g2,[%i5+32]
/* 0x0fd4	     */		srlx	%g2,32,%g2
/* 0x0fd8	     */		add	%i3,%g5,%g3
/* 0x0fdc	     */		ld	[%i1+40],%g5
/* 0x0fe0	     */		add	%g3,%o0,%g3
/* 0x0fe4	     */		ldx	[%sp+2287],%o0
/* 0x0fe8	     */		add	%g4,%o1,%g4
/* 0x0fec	     */		ldx	[%sp+2295],%o1
/* 0x0ff0	     */		add	%g3,%g2,%g2
/* 0x0ff4	     */		add	%g4,%g5,%g3
/* 0x0ff8	     */		sllx	%o0,19,%o0
/* 0x0ffc	     */		ld	[%i1+44],%g4
/* 0x1000	     */		srlx	%g2,32,%g5
/* 0x1004	     */		st	%g2,[%i5+36]
/* 0x1008	     */		add	%o1,%o0,%g2
/* 0x100c	     */		ld	[%i1+48],%o1
/* 0x1010	     */		sllx	%o2,19,%o0
/* 0x1014	     */		add	%g3,%g5,%g3
/* 0x1018	     */		ldx	[%sp+2279],%g5
/* 0x101c	     */		add	%g2,%g4,%g4
/* 0x1020	     */		srlx	%g3,32,%g2
/* 0x1024	     */		st	%g3,[%i5+40]
/* 0x1028	     */		ldx	[%sp+2255],%g3
/* 0x102c	     */		add	%g4,%g2,%g4
/* 0x1030	     */		add	%g5,%o0,%g2
/* 0x1034	     */		ldx	[%sp+2263],%g5
/* 0x1038	     */		add	%g2,%o1,%g2
/* 0x103c	     */		st	%g4,[%i5+44]
/* 0x1040	     */		srlx	%g4,32,%g4
/* 0x1044	     */		sllx	%g3,19,%g3
/* 0x1048	     */		ld	[%i1+52],%o1
/* 0x104c	     */		add	%g2,%g4,%g4
/* 0x1050	     */		ldx	[%sp+2239],%o0
/* 0x1054	     */		add	%g5,%g3,%g2
/* 0x1058	     */		ldx	[%sp+2247],%g3
/* 0x105c	     */		add	%g2,%o1,%g2
/* 0x1060	     */		st	%g4,[%i5+48]
/* 0x1064	     */		srlx	%g4,32,%g4
/* 0x1068	     */		sllx	%o0,19,%o0
/* 0x106c	     */		ld	[%i1+56],%o1
/* 0x1070	     */		add	%g2,%g4,%g4
/* 0x1074	     */		ldx	[%sp+2223],%g5
/* 0x1078	     */		add	%g3,%o0,%g2
/* 0x107c	     */		ldx	[%sp+2231],%g3
/* 0x1080	     */		add	%g2,%o1,%g2
/* 0x1084	     */		st	%g4,[%i5+52]
/* 0x1088	     */		srlx	%g4,32,%g4
/* 0x108c	     */		sllx	%g5,19,%g5
/* 0x1090	     */		ld	[%i1+60],%o0
/* 0x1094	     */		add	%g2,%g4,%g4
/* 0x1098	     */		add	%g3,%g5,%g2
/* 0x109c	     */		st	%g4,[%i5+56]
/* 0x10a0	     */		srlx	%g4,32,%g3
/* 0x10a4	     */		add	%g2,%o0,%g2
/* 0x10a8	     */		add	%g2,%g3,%g2
/* 0x10ac	     */		st	%g2,[%i5+60]

!  217		      !      return uc;

/* 0x10b0	 217 */		srlx	%g2,32,%o0
/* 0x10b4	     */		srl	%o0,0,%i0
/* 0x10b8	     */		ret	! Result =  %i0
/* 0x10bc	     */		restore	%g0,%g0,%g0
                       .L77000052:

!  219		      !    } else {
!  220		      !      DEF_VARS(2*BUFF_SIZE);
!  221		      !      t_d64 d0, d1, db;
!  222		      !      t_u32 i, uc = 0;
!  224		      !      da = (t_d64)(a &  A_MASK);

/* 0x10c0	 224 */		andn	%o1,%g2,%g2
/* 0x10c4	     */		st	%g2,[%sp+2227]

!  225		      !      db = (t_d64)(a >> A_BITS);
!  227		      !#pragma pipeloop(0)
!  228		      !      for (i = 0; i < (n+1)/2; i ++) {

/* 0x10c8	 228 */		add	%i3,1,%g3
/* 0x10cc	 220 */		fmovd	%f0,%f12
/* 0x10d0	 225 */		srl	%o1,19,%g2
/* 0x10d4	     */		st	%g2,[%sp+2223]

!  229		      !        MUL_U32_S64_2_D(i);

/* 0x10d8	 229 */		sethi	%hi(0x1000),%g1
/* 0x10dc	 224 */		ldd	[%o0],%f6
/* 0x10e0	 228 */		srl	%g3,31,%g2
/* 0x10e4	 229 */		xor	%g1,-305,%g1
/* 0x10e8	 228 */		add	%g3,%g2,%g2
/* 0x10ec	 229 */		add	%g1,%fp,%g4
/* 0x10f0	 220 */		ldd	[%o0+8],%f16
/* 0x10f4	 224 */		fmovs	%f6,%f8
/* 0x10f8	 228 */		sra	%g2,1,%g2
/* 0x10fc	 222 */		or	%g0,0,%i0
/* 0x1100	 224 */		ld	[%sp+2227],%f9
/* 0x1104	 225 */		fmovs	%f6,%f10
/* 0x1108	 228 */		cmp	%g2,0
/* 0x110c	 225 */		ld	[%sp+2223],%f11
/* 0x1110	 229 */		or	%g0,1,%o2
/* 0x1114	     */		or	%g0,2,%o3
/* 0x1118	 224 */		fsubd	%f8,%f6,%f14
/* 0x111c	 229 */		or	%g0,3,%o4
/* 0x1120	 228 */		or	%g0,0,%i1
/* 0x1124	 225 */		fsubd	%f10,%f6,%f8
/* 0x1128	 228 */		bleu,pt	%icc,.L900000157
/* 0x112c	     */		cmp	%i2,0
/* 0x1130	 229 */		or	%g0,0,%g3
/* 0x1134	     */		cmp	%g2,6
/* 0x1138	     */		bl,pn	%icc,.L77000054
/* 0x113c	     */		or	%g0,3,%o1
/* 0x1140	     */		ldd	[%i4],%f0
/* 0x1144	     */		sethi	%hi(0x1000),%g1
/* 0x1148	     */		ldd	[%i4+8],%f2
/* 0x114c	     */		xor	%g1,-305,%g1
/* 0x1150	     */		sub	%g2,2,%o5
/* 0x1154	     */		fxnor	%f12,%f0,%f0
/* 0x1158	     */		add	%g1,%fp,%g1
/* 0x115c	     */		or	%g0,4,%o0
/* 0x1160	     */		fxnor	%f12,%f2,%f2
/* 0x1164	     */		or	%g0,5,%o2
/* 0x1168	     */		or	%g0,6,%o3
/* 0x116c	     */		fitod	%f0,%f4
/* 0x1170	     */		or	%g0,7,%o4
/* 0x1174	     */		fitod	%f1,%f0
/* 0x1178	     */		fsubd	%f16,%f4,%f6
/* 0x117c	     */		ldd	[%i4+16],%f4
/* 0x1180	     */		fsubd	%f16,%f0,%f10
/* 0x1184	     */		fitod	%f3,%f0
/* 0x1188	     */		fmuld	%f6,%f14,%f20
/* 0x118c	     */		fitod	%f2,%f2
/* 0x1190	     */		fmuld	%f6,%f8,%f18
/* 0x1194	     */		fxnor	%f12,%f4,%f4
/* 0x1198	     */		fmuld	%f10,%f14,%f6
/* 0x119c	     */		fsubd	%f16,%f0,%f0
/* 0x11a0	     */		fdtox	%f20,%f20
/* 0x11a4	     */		std	%f20,[%g1]
/* 0x11a8	     */		fmuld	%f10,%f8,%f10
/* 0x11ac	     */		fdtox	%f18,%f18
/* 0x11b0	     */		std	%f18,[%g1+8]
/* 0x11b4	     */		fdtox	%f6,%f6
/* 0x11b8	     */		std	%f6,[%g1+16]
/* 0x11bc	     */		fdtox	%f10,%f10
/* 0x11c0	     */		ldd	[%i4+24],%f6
/* 0x11c4	     */		std	%f10,[%g1+24]
                       .L900000149:
/* 0x11c8	     */		fsubd	%f16,%f2,%f20
/* 0x11cc	     */		srl	%o4,0,%i3
/* 0x11d0	     */		srl	%o3,0,%i5
/* 0x11d4	     */		srl	%o2,0,%l0
/* 0x11d8	     */		fmuld	%f0,%f14,%f18
/* 0x11dc	     */		srl	%o0,0,%l1
/* 0x11e0	     */		fxnor	%f12,%f6,%f2
/* 0x11e4	     */		add	%o1,1,%i1
/* 0x11e8	     */		fmuld	%f20,%f14,%f10
/* 0x11ec	     */		srl	%i1,0,%o1
/* 0x11f0	     */		fitod	%f5,%f6
/* 0x11f4	     */		fmuld	%f20,%f8,%f22
/* 0x11f8	     */		sllx	%o1,3,%o1
/* 0x11fc	     */		fdtox	%f18,%f18
/* 0x1200	     */		add	%o0,4,%g3
/* 0x1204	     */		ldd	[%i4+%o1],%f20
/* 0x1208	     */		fdtox	%f10,%f24
/* 0x120c	     */		sllx	%l1,3,%o0
/* 0x1210	     */		fmuld	%f0,%f8,%f10
/* 0x1214	     */		std	%f24,[%o0+%g4]
/* 0x1218	     */		sllx	%l0,3,%o0
/* 0x121c	     */		add	%o2,4,%o2
/* 0x1220	     */		fdtox	%f22,%f0
/* 0x1224	     */		std	%f0,[%o0+%g4]
/* 0x1228	     */		sllx	%i5,3,%o0
/* 0x122c	     */		add	%o3,4,%o3
/* 0x1230	     */		fitod	%f4,%f0
/* 0x1234	     */		std	%f18,[%o0+%g4]
/* 0x1238	     */		sllx	%i3,3,%o0
/* 0x123c	     */		add	%o4,4,%o4
/* 0x1240	     */		fdtox	%f10,%f4
/* 0x1244	     */		fsubd	%f16,%f6,%f10
/* 0x1248	     */		std	%f4,[%o0+%g4]
/* 0x124c	     */		fsubd	%f16,%f0,%f18
/* 0x1250	     */		srl	%o4,0,%i3
/* 0x1254	     */		srl	%o3,0,%i5
/* 0x1258	     */		srl	%o2,0,%l0
/* 0x125c	     */		fmuld	%f10,%f14,%f6
/* 0x1260	     */		srl	%g3,0,%l1
/* 0x1264	     */		fxnor	%f12,%f20,%f4
/* 0x1268	     */		add	%i1,1,%o1
/* 0x126c	     */		fmuld	%f18,%f14,%f22
/* 0x1270	     */		srl	%o1,0,%o0
/* 0x1274	     */		fitod	%f3,%f0
/* 0x1278	     */		fmuld	%f18,%f8,%f20
/* 0x127c	     */		sllx	%o0,3,%i1
/* 0x1280	     */		fdtox	%f6,%f18
/* 0x1284	     */		add	%g3,4,%o0
/* 0x1288	     */		ldd	[%i4+%i1],%f6
/* 0x128c	     */		fdtox	%f22,%f22
/* 0x1290	     */		sllx	%l1,3,%g3
/* 0x1294	     */		fmuld	%f10,%f8,%f10
/* 0x1298	     */		std	%f22,[%g3+%g4]
/* 0x129c	     */		sllx	%l0,3,%g3
/* 0x12a0	     */		add	%o2,4,%o2
/* 0x12a4	     */		fdtox	%f20,%f20
/* 0x12a8	     */		std	%f20,[%g3+%g4]
/* 0x12ac	     */		sllx	%i5,3,%g3
/* 0x12b0	     */		add	%o3,4,%o3
/* 0x12b4	     */		fitod	%f2,%f2
/* 0x12b8	     */		std	%f18,[%g3+%g4]
/* 0x12bc	     */		sllx	%i3,3,%g3
/* 0x12c0	     */		add	%o4,4,%o4
/* 0x12c4	     */		fdtox	%f10,%f10
/* 0x12c8	     */		std	%f10,[%g3+%g4]
/* 0x12cc	     */		cmp	%o1,%o5
/* 0x12d0	     */		bcs,pt	%icc,.L900000149
/* 0x12d4	     */		fsubd	%f16,%f0,%f0
                       .L900000152:
/* 0x12d8	     */		srl	%o0,0,%g3
/* 0x12dc	     */		fsubd	%f16,%f2,%f22
/* 0x12e0	     */		fmuld	%f0,%f14,%f10
/* 0x12e4	     */		sllx	%g3,3,%i3
/* 0x12e8	     */		fitod	%f4,%f18
/* 0x12ec	     */		fmuld	%f0,%f8,%f20
/* 0x12f0	     */		srl	%o2,0,%o5
/* 0x12f4	     */		fxnor	%f12,%f6,%f0
/* 0x12f8	     */		add	%o0,4,%g3
/* 0x12fc	     */		srl	%o3,0,%i1
/* 0x1300	     */		fitod	%f5,%f2
/* 0x1304	     */		fmuld	%f22,%f14,%f6
/* 0x1308	     */		sllx	%o5,3,%o5
/* 0x130c	     */		fsubd	%f16,%f18,%f18
/* 0x1310	     */		fmuld	%f22,%f8,%f22
/* 0x1314	     */		srl	%o4,0,%o0
/* 0x1318	     */		fitod	%f0,%f4
/* 0x131c	     */		add	%o2,4,%o2
/* 0x1320	     */		sllx	%i1,3,%i1
/* 0x1324	     */		fdtox	%f6,%f6
/* 0x1328	     */		std	%f6,[%i3+%g4]
/* 0x132c	     */		sllx	%o0,3,%o0
/* 0x1330	     */		fsubd	%f16,%f2,%f2
/* 0x1334	     */		fmuld	%f18,%f14,%f6
/* 0x1338	     */		fdtox	%f22,%f22
/* 0x133c	     */		std	%f22,[%o5+%g4]
/* 0x1340	     */		srl	%g3,0,%o5
/* 0x1344	     */		fmuld	%f18,%f8,%f18
/* 0x1348	     */		sllx	%o5,3,%i3
/* 0x134c	     */		fdtox	%f10,%f10
/* 0x1350	     */		std	%f10,[%i1+%g4]
/* 0x1354	     */		srl	%o2,0,%i1
/* 0x1358	     */		fitod	%f1,%f0
/* 0x135c	     */		add	%o3,4,%o3
/* 0x1360	     */		fmuld	%f2,%f14,%f10
/* 0x1364	     */		srl	%o3,0,%o5
/* 0x1368	     */		fdtox	%f20,%f20
/* 0x136c	     */		std	%f20,[%o0+%g4]
/* 0x1370	     */		fmuld	%f2,%f8,%f2
/* 0x1374	     */		sllx	%i1,3,%i1
/* 0x1378	     */		fsubd	%f16,%f4,%f4
/* 0x137c	     */		add	%o4,4,%o0
/* 0x1380	     */		srl	%o0,0,%o4
/* 0x1384	     */		fdtox	%f6,%f6
/* 0x1388	     */		std	%f6,[%i3+%g4]
/* 0x138c	     */		sllx	%o5,3,%o5
/* 0x1390	     */		fdtox	%f18,%f18
/* 0x1394	     */		std	%f18,[%i1+%g4]
/* 0x1398	     */		sllx	%o4,3,%o4
/* 0x139c	     */		fsubd	%f16,%f0,%f0
/* 0x13a0	     */		add	%g3,4,%g3
/* 0x13a4	     */		fmuld	%f4,%f14,%f6
/* 0x13a8	     */		fdtox	%f10,%f10
/* 0x13ac	     */		std	%f10,[%o5+%g4]
/* 0x13b0	     */		srl	%g3,0,%o5
/* 0x13b4	     */		fmuld	%f4,%f8,%f4
/* 0x13b8	     */		sllx	%o5,3,%o5
/* 0x13bc	     */		fdtox	%f2,%f10
/* 0x13c0	     */		std	%f10,[%o4+%g4]
/* 0x13c4	     */		fdtox	%f6,%f6
/* 0x13c8	     */		std	%f6,[%o5+%g4]
/* 0x13cc	     */		add	%o0,4,%o4
/* 0x13d0	     */		fmuld	%f0,%f14,%f2
/* 0x13d4	     */		add	%o2,4,%o2
/* 0x13d8	     */		add	%o3,4,%o3
/* 0x13dc	     */		fdtox	%f4,%f4
/* 0x13e0	     */		fmuld	%f0,%f8,%f0
/* 0x13e4	     */		srl	%o2,0,%o0
/* 0x13e8	     */		add	%o1,1,%i1
/* 0x13ec	     */		sllx	%o0,3,%o0
/* 0x13f0	     */		cmp	%i1,%g2
/* 0x13f4	     */		fdtox	%f2,%f2
/* 0x13f8	     */		srl	%o3,0,%o1
/* 0x13fc	     */		std	%f4,[%o0+%g4]
/* 0x1400	     */		add	%g3,4,%g3
/* 0x1404	     */		srl	%o4,0,%o0
/* 0x1408	     */		add	%o2,4,%o2
/* 0x140c	     */		fdtox	%f0,%f0
/* 0x1410	     */		sllx	%o1,3,%o1
/* 0x1414	     */		add	%o3,4,%o3
/* 0x1418	     */		std	%f2,[%o1+%g4]
/* 0x141c	     */		sllx	%o0,3,%o0
/* 0x1420	     */		bcc,pn	%icc,.L77000056
/* 0x1424	     */		std	%f0,[%o0+%g4]
/* 0x1428	     */		add	%o4,4,%o4
                       .L77000054:
/* 0x142c	     */		srl	%i1,0,%o0
                       .L900000156:
/* 0x1430	     */		sllx	%o0,3,%o0
/* 0x1434	     */		add	%i1,1,%i1
/* 0x1438	     */		ldd	[%i4+%o0],%f0
/* 0x143c	     */		srl	%g3,0,%o0
/* 0x1440	     */		cmp	%i1,%g2
/* 0x1444	     */		sllx	%o0,3,%i3
/* 0x1448	     */		add	%g3,4,%g3
/* 0x144c	     */		fxnor	%f12,%f0,%f0
/* 0x1450	     */		srl	%o2,0,%o0
/* 0x1454	     */		add	%o2,4,%o2
/* 0x1458	     */		sllx	%o0,3,%o5
/* 0x145c	     */		fitod	%f0,%f2
/* 0x1460	     */		srl	%o3,0,%o0
/* 0x1464	     */		add	%o3,4,%o3
/* 0x1468	     */		fitod	%f1,%f0
/* 0x146c	     */		sllx	%o0,3,%o1
/* 0x1470	     */		srl	%o4,0,%o0
/* 0x1474	     */		add	%o4,4,%o4
/* 0x1478	     */		fsubd	%f16,%f2,%f2
/* 0x147c	     */		sllx	%o0,3,%o0
/* 0x1480	     */		fsubd	%f16,%f0,%f0
/* 0x1484	     */		fmuld	%f2,%f14,%f6
/* 0x1488	     */		fmuld	%f2,%f8,%f4
/* 0x148c	     */		fmuld	%f0,%f14,%f2
/* 0x1490	     */		fdtox	%f6,%f6
/* 0x1494	     */		fmuld	%f0,%f8,%f0
/* 0x1498	     */		std	%f6,[%i3+%g4]
/* 0x149c	     */		fdtox	%f4,%f4
/* 0x14a0	     */		std	%f4,[%o5+%g4]
/* 0x14a4	     */		fdtox	%f2,%f2
/* 0x14a8	     */		std	%f2,[%o1+%g4]
/* 0x14ac	     */		fdtox	%f0,%f0
/* 0x14b0	     */		std	%f0,[%o0+%g4]
/* 0x14b4	     */		bcs,pt	%icc,.L900000156
/* 0x14b8	     */		srl	%i1,0,%o0
                       .L77000056:

!  230		      !      }
!  232		      !#pragma pipeloop(0)
!  233		      !      for (i = 0; i < n; i ++) {

/* 0x14bc	 233 */		cmp	%i2,0
                       .L900000157:
/* 0x14c0	 233 */		bleu,pt	%icc,.L77000061
/* 0x14c4	     */		or	%g0,%i0,%o0
/* 0x14c8	     */		or	%g0,%i2,%g3

!  234		      !        ADD_S64_U32_D(i);

/* 0x14cc	 234 */		or	%g0,1,%i1
/* 0x14d0	 233 */		or	%g0,0,%i3
/* 0x14d4	 234 */		or	%g0,0,%i2
/* 0x14d8	     */		cmp	%g3,4
/* 0x14dc	     */		bl,pn	%icc,.L77000058
/* 0x14e0	     */		sethi	%hi(0x1000),%g1
/* 0x14e4	     */		or	%g0,1,%o0
/* 0x14e8	     */		xor	%g1,-297,%g1
/* 0x14ec	     */		or	%g0,2,%o2
/* 0x14f0	     */		add	%g1,%fp,%g1
/* 0x14f4	     */		sub	%g3,2,%g2
/* 0x14f8	     */		ldx	[%g1],%o5
/* 0x14fc	     */		or	%g0,2,%o4
/* 0x1500	     */		ldx	[%g1-8],%o3
/* 0x1504	     */		sllx	%o5,19,%o1
/* 0x1508	     */		ld	[%o7],%o5
/* 0x150c	     */		add	%o3,%o1,%o3
/* 0x1510	     */		or	%g0,3,%o1
/* 0x1514	     */		add	%o3,%o5,%o3
/* 0x1518	     */		st	%o3,[%g5]
/* 0x151c	     */		or	%g0,1,%o5
/* 0x1520	     */		srlx	%o3,32,%o3
                       .L900000137:
/* 0x1524	     */		srl	%o1,0,%i0
/* 0x1528	     */		sllx	%i0,3,%i0
/* 0x152c	     */		sllx	%o4,3,%i1
/* 0x1530	     */		ldx	[%i0+%g4],%i0
/* 0x1534	     */		sllx	%o5,2,%o4
/* 0x1538	     */		ldx	[%i1+%g4],%i1
/* 0x153c	     */		add	%o0,1,%i3
/* 0x1540	     */		sllx	%i0,19,%o0
/* 0x1544	     */		ld	[%o7+%o4],%i0
/* 0x1548	     */		add	%o2,2,%i2
/* 0x154c	     */		srl	%o3,0,%o5
/* 0x1550	     */		add	%i1,%o0,%o0
/* 0x1554	     */		srl	%i3,0,%o3
/* 0x1558	     */		add	%o0,%i0,%o2
/* 0x155c	     */		srl	%i2,0,%o0
/* 0x1560	     */		add	%o2,%o5,%o2
/* 0x1564	     */		st	%o2,[%g5+%o4]
/* 0x1568	     */		srlx	%o2,32,%i0
/* 0x156c	     */		add	%o1,2,%i1
/* 0x1570	     */		srl	%i1,0,%o1
/* 0x1574	     */		sllx	%o1,3,%o1
/* 0x1578	     */		sllx	%o0,3,%o0
/* 0x157c	     */		ldx	[%o1+%g4],%o2
/* 0x1580	     */		sllx	%o3,2,%o3
/* 0x1584	     */		ldx	[%o0+%g4],%o1
/* 0x1588	     */		add	%i3,1,%o0
/* 0x158c	     */		sllx	%o2,19,%o5
/* 0x1590	     */		ld	[%o7+%o3],%o4
/* 0x1594	     */		add	%i2,2,%o2
/* 0x1598	     */		add	%o1,%o5,%o1
/* 0x159c	     */		srl	%o0,0,%o5
/* 0x15a0	     */		add	%o1,%o4,%o1
/* 0x15a4	     */		srl	%o2,0,%o4
/* 0x15a8	     */		add	%o1,%i0,%o1
/* 0x15ac	     */		st	%o1,[%g5+%o3]
/* 0x15b0	     */		srlx	%o1,32,%o3
/* 0x15b4	     */		add	%i1,2,%o1
/* 0x15b8	     */		cmp	%o0,%g2
/* 0x15bc	     */		bcs,pt	%icc,.L900000137
/* 0x15c0	     */		nop
                       .L900000140:
/* 0x15c4	     */		srl	%o1,0,%g2
/* 0x15c8	     */		add	%i3,2,%i3
/* 0x15cc	     */		sllx	%g2,3,%o0
/* 0x15d0	     */		cmp	%i3,%g3
/* 0x15d4	     */		ldx	[%o0+%g4],%o2
/* 0x15d8	     */		sllx	%o4,3,%o0
/* 0x15dc	     */		add	%i2,4,%i2
/* 0x15e0	     */		sllx	%o5,2,%o1
/* 0x15e4	     */		ldx	[%o0+%g4],%o0
/* 0x15e8	     */		add	%i1,4,%i1
/* 0x15ec	     */		sllx	%o2,19,%o2
/* 0x15f0	     */		ld	[%o7+%o1],%o4
/* 0x15f4	     */		add	%o0,%o2,%o0
/* 0x15f8	     */		add	%o0,%o4,%o0
/* 0x15fc	     */		add	%o0,%o3,%o0
/* 0x1600	     */		st	%o0,[%g5+%o1]
/* 0x1604	     */		bcc,pn	%icc,.L77000060
/* 0x1608	     */		srlx	%o0,32,%i0
                       .L77000058:
/* 0x160c	     */		srl	%i1,0,%o1
                       .L900000155:
/* 0x1610	     */		sllx	%o1,3,%o1
/* 0x1614	     */		add	%i1,2,%i1
/* 0x1618	     */		srl	%i2,0,%o0
/* 0x161c	     */		ldx	[%o1+%g4],%o2
/* 0x1620	     */		add	%i2,2,%i2
/* 0x1624	     */		srl	%i3,0,%o1
/* 0x1628	     */		add	%i3,1,%i3
/* 0x162c	     */		sllx	%o0,3,%o0
/* 0x1630	     */		cmp	%i3,%g3
/* 0x1634	     */		sllx	%o1,2,%o1
/* 0x1638	     */		ldx	[%o0+%g4],%o0
/* 0x163c	     */		sllx	%o2,19,%o2
/* 0x1640	     */		ld	[%o7+%o1],%o3
/* 0x1644	     */		add	%o0,%o2,%o0
/* 0x1648	     */		srl	%i0,0,%o2
/* 0x164c	     */		add	%o0,%o3,%o0
/* 0x1650	     */		add	%o0,%o2,%o0
/* 0x1654	     */		st	%o0,[%g5+%o1]
/* 0x1658	     */		srlx	%o0,32,%i0
/* 0x165c	     */		bcs,pt	%icc,.L900000155
/* 0x1660	     */		srl	%i1,0,%o1
                       .L77000060:

!  235		      !      }
!  237		      !      return uc;

/* 0x1664	 237 */		or	%g0,%i0,%o0
                       .L77000061:
/* 0x1668	     */		srl	%o0,0,%i0
/* 0x166c	     */		ret	! Result =  %i0
/* 0x1670	     */		restore	%g0,%g0,%g0
/* 0x1674	   0 */		.type	mul_add,2
/* 0x1674	     */		.size	mul_add,(.-mul_add)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! SUBROUTINE mul_add_inp
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global mul_add_inp
                       mul_add_inp:
/* 000000	     */		save	%sp,-176,%sp

!  238		      !    }
!  239		      !  }
!  240		      !}
!  242		      !/***************************************************************/
!  244		      !t_u32 mul_add_inp(t_u32 *x, t_u32 *y, int n, t_u32 a)
!  245		      !{
!  246		      !  return mul_add(x, x, y, n, a);

/* 0x0004	 246 */		sra	%i2,0,%o3
/* 0x0008	 245 */		or	%g0,%i1,%o2
/* 0x000c	     */		or	%g0,%i0,%o0
/* 0x0010	 246 */		or	%g0,%i0,%o1
/* 0x0014	     */		call	mul_add	! params =  %o0 %o1 %o2 %o3 %o4	! Result =  %o0
/* 0x0018	     */		srl	%i3,0,%o4
/* 0x001c	     */		srl	%o0,0,%i0
/* 0x0020	     */		ret	! Result =  %i0
/* 0x0024	     */		restore	%g0,%g0,%g0
/* 0x0028	   0 */		.type	mul_add_inp,2
/* 0x0028	     */		.size	mul_add_inp,(.-mul_add_inp)

! Begin Disassembling Stabs
	.xstabs	".stab.index","Xa ; O ; P ; V=3.1 ; R=WorkShop Compilers 5.0 98/12/15 C 5.0",60,0,0,0	! (/usr/tmp/acompAAAPWaq0M:1)
	.xstabs	".stab.index","/h/interzone/d3/nelsonb/nss_tip/mozilla/security/nss/lib/freebl/mpi32; /tools/ns/workshop-5.0/bin/../SC5.0/bin/cc -fast -xO5 -xrestrict=%%all -xchip=ultra -xarch=v9a -KPIC -mt -S vis_64.il  mpv_sparc.c -W0,-xp",52,0,0,0	! (/usr/tmp/acompAAAPWaq0M:2)
! End Disassembling Stabs

! Begin Disassembling Ident
	.ident	"cg: WorkShop Compilers 5.0 99/08/12 Compiler Common 5.0 Patch 107357-05"	! (NO SOURCE LINE)
	.ident	"@(#)vis_proto.h\t1.3\t97/03/30 SMI"	! (/usr/tmp/acompAAAPWaq0M:4)
	.ident	"acomp: WorkShop Compilers 5.0 98/12/15 C 5.0"	! (/usr/tmp/acompAAAPWaq0M:11)
! End Disassembling Ident
