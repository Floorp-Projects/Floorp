
	.section	".text",#alloc,#execinstr
	.file	"mpv_sparc.c"

	.section	".data",#alloc,#write
	.align	8
mask_cnst:
	.word	-2147483648
	.word	-2147483648
	.type	mask_cnst,#object
	.size	mask_cnst,8

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! CONSTANT POOL
!
                       .L_const_seg_900000106:
/* 000000	   0 */		.word	1127219200,0
/* 0x0008	     */		.word	1105199103,-4194304
/* 0x0010	   0 */		.align	4
!
! SUBROUTINE mul_add
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global mul_add
                       mul_add:
/* 000000	     */		sethi	%hi(0x1800),%g1
/* 0x0004	     */		sethi	%hi(mask_cnst),%g2
/* 0x0008	     */		xor	%g1,-984,%g1
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
!   31		      ! *  $Id: mpv_sparcv8.s,v 1.1 2000/12/13 01:19:55 nelsonb%netscape.com Exp $
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

/* 0x001c	 156 */		sethi	%hi(.L_const_seg_900000106),%g3
/* 0x0020	 149 */		add	%g5,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000154-.)),%g5
/* 0x0024	     */		or	%g0,%i4,%o1
/* 0x0028	     */		st	%o1,[%fp+84]
/* 0x002c	     */		add	%g5,%o7,%o3
/* 0x0030	 156 */		add	%g3,%lo(.L_const_seg_900000106),%g3
/* 0x0034	 149 */		ld	[%o3+%g2],%g2
/* 0x0038	     */		or	%g0,%i3,%o2
/* 0x003c	 150 */		sethi	%hi(0x80000),%g4
/* 0x0040	 156 */		ld	[%o3+%g3],%o0
/* 0x0044	 150 */		cmp	%o1,%g4
/* 0x0048	 149 */		or	%g0,%o2,%o3
/* 0x004c	     */		ldd	[%g2],%f0
/* 0x0050	 150 */		bcc,pn	%icc,.L77000048
/* 0x0054	     */		cmp	%o2,8
/* 0x0058	 152 */		bne,pn	%icc,.L77000037
/* 0x005c	     */		ld	[%fp+84],%f7
/* 0x0060	     */		ldd	[%i2],%f4
/* 0x0064	 153 */		ldd	[%o0],%f8
/* 0x0068	     */		fxnor	%f0,%f4,%f4
/* 0x006c	     */		ldd	[%i2+8],%f10
/* 0x0070	 156 */		ldd	[%o0+8],%f14
/* 0x0074	     */		fitod	%f4,%f12
/* 0x0078	     */		ldd	[%i2+16],%f16
/* 0x007c	     */		fitod	%f5,%f4
/* 0x0080	     */		ldd	[%i2+24],%f18
/* 0x0084	     */		fxnor	%f0,%f10,%f10

!  157		      !      ADD_S64_U32_8(0);

/* 0x0088	 157 */		ld	[%i1],%g2
/* 0x008c	     */		ld	[%i1+4],%g3
/* 0x0090	     */		fxnor	%f0,%f16,%f16
/* 0x0094	 156 */		fsubd	%f14,%f4,%f4
/* 0x0098	 157 */		ld	[%i1+8],%g4
/* 0x009c	     */		ld	[%i1+16],%o0
/* 0x00a0	 156 */		fitod	%f16,%f20
/* 0x00a4	 157 */		ld	[%i1+12],%g5
/* 0x00a8	     */		ld	[%i1+20],%o1
/* 0x00ac	     */		ld	[%i1+24],%o2
/* 0x00b0	 153 */		fmovs	%f8,%f6
/* 0x00b4	 157 */		ld	[%i1+28],%o3
/* 0x00b8	 153 */		fsubd	%f6,%f8,%f6
/* 0x00bc	 156 */		fsubd	%f14,%f12,%f8
/* 0x00c0	     */		fitod	%f10,%f12
/* 0x00c4	     */		fmuld	%f4,%f6,%f4
/* 0x00c8	     */		fitod	%f11,%f10
/* 0x00cc	     */		fmuld	%f8,%f6,%f8
/* 0x00d0	     */		fsubd	%f14,%f12,%f12
/* 0x00d4	     */		fdtox	%f4,%f4
/* 0x00d8	     */		std	%f4,[%sp+144]
/* 0x00dc	     */		fdtox	%f8,%f8
/* 0x00e0	     */		std	%f8,[%sp+152]
/* 0x00e4	     */		fmuld	%f12,%f6,%f12
/* 0x00e8	     */		fsubd	%f14,%f10,%f10
/* 0x00ec	     */		fsubd	%f14,%f20,%f4
/* 0x00f0	     */		fitod	%f17,%f8
/* 0x00f4	     */		fxnor	%f0,%f18,%f16
/* 0x00f8	 157 */		ldx	[%sp+152],%o4
/* 0x00fc	 156 */		fmuld	%f10,%f6,%f10
/* 0x0100	     */		fdtox	%f12,%f12
/* 0x0104	     */		std	%f12,[%sp+136]
/* 0x0108	     */		fmuld	%f4,%f6,%f4
/* 0x010c	     */		fitod	%f16,%f18
/* 0x0110	 157 */		add	%o4,%g2,%g2
/* 0x0114	     */		st	%g2,[%i0]
/* 0x0118	     */		ldx	[%sp+144],%o4
/* 0x011c	 156 */		fsubd	%f14,%f8,%f8
/* 0x0120	 157 */		srax	%g2,32,%o5
/* 0x0124	 156 */		fdtox	%f10,%f10
/* 0x0128	     */		std	%f10,[%sp+128]
/* 0x012c	     */		fdtox	%f4,%f4
/* 0x0130	     */		std	%f4,[%sp+120]
/* 0x0134	 157 */		add	%o4,%g3,%o4
/* 0x0138	 156 */		fitod	%f17,%f12
/* 0x013c	 157 */		ldx	[%sp+136],%g2
/* 0x0140	     */		add	%o4,%o5,%g3
/* 0x0144	 156 */		fmuld	%f8,%f6,%f8
/* 0x0148	     */		fsubd	%f14,%f18,%f10
/* 0x014c	 157 */		st	%g3,[%i0+4]
/* 0x0150	     */		srax	%g3,32,%g3
/* 0x0154	     */		add	%g2,%g4,%g4
/* 0x0158	     */		ldx	[%sp+128],%g2
/* 0x015c	 156 */		fsubd	%f14,%f12,%f4
/* 0x0160	 157 */		add	%g4,%g3,%g3
/* 0x0164	     */		ldx	[%sp+120],%g4
/* 0x0168	 156 */		fmuld	%f10,%f6,%f10
/* 0x016c	     */		fdtox	%f8,%f8
/* 0x0170	     */		std	%f8,[%sp+112]
/* 0x0174	 157 */		add	%g4,%o0,%g4
/* 0x0178	     */		add	%g2,%g5,%g2
/* 0x017c	     */		st	%g3,[%i0+8]
/* 0x0180	 156 */		fmuld	%f4,%f6,%f4
/* 0x0184	 157 */		srax	%g3,32,%o0
/* 0x0188	     */		ldx	[%sp+112],%g5
/* 0x018c	 156 */		fdtox	%f10,%f6
/* 0x0190	     */		std	%f6,[%sp+104]
/* 0x0194	 157 */		add	%g2,%o0,%g2
/* 0x0198	     */		srax	%g2,32,%g3
/* 0x019c	     */		st	%g2,[%i0+12]
/* 0x01a0	     */		add	%g5,%o1,%o1
/* 0x01a4	 156 */		fdtox	%f4,%f4
/* 0x01a8	     */		std	%f4,[%sp+96]
/* 0x01ac	 157 */		add	%g4,%g3,%g3
/* 0x01b0	     */		srax	%g3,32,%g4
/* 0x01b4	     */		st	%g3,[%i0+16]
/* 0x01b8	     */		ldx	[%sp+104],%o0
/* 0x01bc	     */		add	%o1,%g4,%g4
/* 0x01c0	     */		ldx	[%sp+96],%g5
/* 0x01c4	     */		srax	%g4,32,%g2
/* 0x01c8	     */		add	%o0,%o2,%o2
/* 0x01cc	     */		st	%g4,[%i0+20]
/* 0x01d0	     */		add	%o2,%g2,%g2
/* 0x01d4	     */		add	%g5,%o3,%g5
/* 0x01d8	     */		st	%g2,[%i0+24]
/* 0x01dc	     */		srax	%g2,32,%g3
/* 0x01e0	     */		add	%g5,%g3,%g2
/* 0x01e4	     */		st	%g2,[%i0+28]

!  159		      !      return c;

/* 0x01e8	 159 */		srax	%g2,32,%o5
/* 0x01ec	     */		ret	! Result =  %i0
/* 0x01f0	     */		restore	%g0,%o5,%o0
                       .L77000037:

!  161		      !    } else if (n == 16) {

/* 0x01f4	 161 */		cmp	%o2,16
/* 0x01f8	     */		bne,pn	%icc,.L77000076
/* 0x01fc	     */		ld	[%fp+84],%f7
/* 0x0200	     */		ldd	[%i2],%f4

!  162		      !      DEF_VARS(16);

/* 0x0204	 162 */		ldd	[%o0],%f8

!  163		      !      t_s32 c = 0;
!  165		      !      MUL_U32_S64_8(0);

/* 0x0208	 165 */		ldd	[%o0+8],%f14
/* 0x020c	     */		fxnor	%f0,%f4,%f4
/* 0x0210	     */		ldd	[%i2+8],%f10
/* 0x0214	     */		ldd	[%i2+16],%f16
/* 0x0218	     */		fitod	%f4,%f12
/* 0x021c	     */		ldd	[%i2+24],%f18
/* 0x0220	     */		fitod	%f5,%f4

!  166		      !      MUL_U32_S64_8(4);
!  167		      !      ADD_S64_U32_8(0);
!  168		      !      ADD_S64_U32_8(8);

/* 0x0224	 168 */		ld	[%i1+36],%o5
/* 0x0228	     */		fxnor	%f0,%f10,%f10
/* 0x022c	 167 */		ld	[%i1],%g2
/* 0x0230	     */		fxnor	%f0,%f16,%f16
/* 0x0234	     */		ldd	[%i2+32],%f20
/* 0x0238	 165 */		fitod	%f10,%f24
/* 0x023c	 168 */		stx	%o5,[%sp+96]
/* 0x0240	 165 */		fsubd	%f14,%f4,%f4
/* 0x0244	 167 */		ld	[%i1+4],%g3
/* 0x0248	 165 */		fitod	%f11,%f10
/* 0x024c	 168 */		ld	[%i1+40],%o5
/* 0x0250	 165 */		fsubd	%f14,%f24,%f24
/* 0x0254	 167 */		ld	[%i1+8],%g4
/* 0x0258	 165 */		fitod	%f16,%f28
/* 0x025c	 167 */		stx	%o5,[%sp+104]
/* 0x0260	 165 */		fsubd	%f14,%f10,%f10
/* 0x0264	     */		ldd	[%i2+40],%f22
/* 0x0268	     */		fxnor	%f0,%f20,%f20
/* 0x026c	 167 */		ld	[%i1+12],%g5
/* 0x0270	     */		ldd	[%i2+48],%f2
/* 0x0274	     */		ld	[%i1+16],%o0
/* 0x0278	     */		ld	[%i1+20],%o1
/* 0x027c	 162 */		fmovs	%f8,%f6
/* 0x0280	 168 */		ld	[%i1+32],%o4
/* 0x0284	     */		ldd	[%i2+56],%f26
/* 0x0288	 162 */		fsubd	%f6,%f8,%f6
/* 0x028c	 167 */		ld	[%i1+24],%o2
/* 0x0290	 165 */		fsubd	%f14,%f12,%f8
/* 0x0294	 168 */		stx	%o4,[%sp+112]
/* 0x0298	     */		fxnor	%f0,%f2,%f12
/* 0x029c	 167 */		ld	[%i1+28],%o3
/* 0x02a0	 165 */		fmuld	%f4,%f6,%f4
/* 0x02a4	 168 */		ld	[%i1+44],%o4
/* 0x02a8	 165 */		fmuld	%f8,%f6,%f8
/* 0x02ac	     */		fmuld	%f24,%f6,%f24
/* 0x02b0	 168 */		stx	%o4,[%sp+144]
/* 0x02b4	 165 */		fdtox	%f4,%f4
/* 0x02b8	     */		fmuld	%f10,%f6,%f10
/* 0x02bc	     */		std	%f4,[%sp+280]
/* 0x02c0	     */		fdtox	%f8,%f8
/* 0x02c4	     */		std	%f8,[%sp+288]
/* 0x02c8	     */		fitod	%f17,%f8
/* 0x02cc	 168 */		ld	[%i1+60],%o4
/* 0x02d0	     */		fxnor	%f0,%f18,%f16
/* 0x02d4	 165 */		fdtox	%f24,%f24
/* 0x02d8	     */		std	%f24,[%sp+272]
/* 0x02dc	 167 */		ldx	[%sp+288],%o7
/* 0x02e0	 165 */		fsubd	%f14,%f28,%f4
/* 0x02e4	     */		fitod	%f16,%f18
/* 0x02e8	     */		fsubd	%f14,%f8,%f8
/* 0x02ec	 167 */		add	%o7,%g2,%g2
/* 0x02f0	     */		st	%g2,[%i0]
/* 0x02f4	     */		ldx	[%sp+280],%o7
/* 0x02f8	 165 */		fitod	%f17,%f16
/* 0x02fc	 167 */		srax	%g2,32,%o5
/* 0x0300	 165 */		fmuld	%f4,%f6,%f4
/* 0x0304	     */		fsubd	%f14,%f18,%f18
/* 0x0308	     */		fdtox	%f10,%f10
/* 0x030c	     */		std	%f10,[%sp+264]
/* 0x0310	 167 */		add	%o7,%g3,%g3
/* 0x0314	 165 */		fmuld	%f8,%f6,%f24
/* 0x0318	 166 */		fitod	%f20,%f8
/* 0x031c	 167 */		add	%g3,%o5,%g3
/* 0x0320	 168 */		ld	[%i1+48],%o5
/* 0x0324	 167 */		st	%g3,[%i0+4]
/* 0x0328	 165 */		fdtox	%f4,%f4
/* 0x032c	     */		fmuld	%f18,%f6,%f10
/* 0x0330	 167 */		ldx	[%sp+272],%o7
/* 0x0334	     */		fxnor	%f0,%f22,%f18
/* 0x0338	 165 */		std	%f4,[%sp+256]
/* 0x033c	     */		fsubd	%f14,%f16,%f4
/* 0x0340	 167 */		stx	%o5,[%sp+128]
/* 0x0344	 166 */		fitod	%f21,%f16
/* 0x0348	 167 */		srax	%g3,32,%o5
/* 0x034c	 166 */		fsubd	%f14,%f8,%f8
/* 0x0350	 167 */		add	%o7,%g4,%g4
/* 0x0354	 168 */		ld	[%i1+52],%o7
/* 0x0358	 165 */		fdtox	%f24,%f20
/* 0x035c	     */		std	%f20,[%sp+248]
/* 0x0360	 167 */		add	%g4,%o5,%g4
/* 0x0364	 165 */		fmuld	%f4,%f6,%f4
/* 0x0368	 167 */		stx	%o7,[%sp+120]
/* 0x036c	 165 */		fdtox	%f10,%f10
/* 0x0370	 167 */		ldx	[%sp+264],%o7
/* 0x0374	 166 */		fitod	%f18,%f20
/* 0x0378	     */		fmuld	%f8,%f6,%f8
/* 0x037c	 165 */		std	%f10,[%sp+240]
/* 0x0380	 166 */		fsubd	%f14,%f16,%f10
/* 0x0384	 168 */		ld	[%i1+56],%o5
/* 0x0388	 166 */		fitod	%f19,%f16
/* 0x038c	 167 */		add	%o7,%g5,%g5
/* 0x0390	     */		ldx	[%sp+256],%o7
/* 0x0394	 165 */		fdtox	%f4,%f4
/* 0x0398	 167 */		stx	%o5,[%sp+136]
/* 0x039c	     */		srax	%g4,32,%o5
/* 0x03a0	 166 */		fdtox	%f8,%f8
/* 0x03a4	 165 */		std	%f4,[%sp+232]
/* 0x03a8	 166 */		fitod	%f12,%f18
/* 0x03ac	 167 */		add	%g5,%o5,%g5
/* 0x03b0	 166 */		fmuld	%f10,%f6,%f4
/* 0x03b4	 167 */		ldx	[%sp+248],%o5
/* 0x03b8	 166 */		fsubd	%f14,%f20,%f10
/* 0x03bc	 167 */		add	%o7,%o0,%o0
/* 0x03c0	 166 */		std	%f8,[%sp+224]
/* 0x03c4	 167 */		srax	%g5,32,%o7
/* 0x03c8	 166 */		fsubd	%f14,%f16,%f8
/* 0x03cc	 167 */		ldx	[%sp+240],%g2
/* 0x03d0	 166 */		fdtox	%f4,%f4
/* 0x03d4	 167 */		add	%o0,%o7,%o0
/* 0x03d8	 166 */		std	%f4,[%sp+216]
/* 0x03dc	     */		fxnor	%f0,%f26,%f16
/* 0x03e0	 167 */		add	%o5,%o1,%o1
/* 0x03e4	 166 */		fmuld	%f10,%f6,%f10
/* 0x03e8	 167 */		ldx	[%sp+232],%o7
/* 0x03ec	     */		srax	%o0,32,%o5
/* 0x03f0	 166 */		fitod	%f13,%f12
/* 0x03f4	     */		fmuld	%f8,%f6,%f4
/* 0x03f8	 167 */		st	%g4,[%i0+8]
/* 0x03fc	 166 */		fsubd	%f14,%f18,%f8
/* 0x0400	 167 */		add	%o1,%o5,%g3
/* 0x0404	 168 */		ldx	[%sp+224],%o1
/* 0x0408	 166 */		fitod	%f16,%f18
/* 0x040c	 167 */		add	%g2,%o2,%g2
/* 0x0410	     */		st	%g5,[%i0+12]
/* 0x0414	     */		srax	%g3,32,%o2
/* 0x0418	 166 */		fdtox	%f10,%f10
/* 0x041c	     */		std	%f10,[%sp+208]
/* 0x0420	     */		fsubd	%f14,%f12,%f10
/* 0x0424	 167 */		add	%g2,%o2,%o2
/* 0x0428	 166 */		fmuld	%f8,%f6,%f8
/* 0x042c	 168 */		ldx	[%sp+112],%g2
/* 0x0430	 166 */		fdtox	%f4,%f4
/* 0x0434	 167 */		add	%o7,%o3,%g4
/* 0x0438	 166 */		std	%f4,[%sp+200]
/* 0x043c	 167 */		srax	%o2,32,%o3
/* 0x0440	 166 */		fitod	%f17,%f12
/* 0x0444	 168 */		ldx	[%sp+216],%g5
/* 0x0448	 167 */		add	%g4,%o3,%o3
/* 0x044c	 168 */		add	%o1,%g2,%o1
/* 0x0450	 166 */		fsubd	%f14,%f18,%f4
/* 0x0454	 168 */		ldx	[%sp+96],%o5
/* 0x0458	     */		srax	%o3,32,%g4
/* 0x045c	 166 */		fdtox	%f8,%f8
/* 0x0460	     */		fmuld	%f10,%f6,%f10
/* 0x0464	     */		std	%f8,[%sp+192]
/* 0x0468	 168 */		add	%o1,%g4,%o1
/* 0x046c	 166 */		fsubd	%f14,%f12,%f8
/* 0x0470	 168 */		ldx	[%sp+208],%o7
/* 0x0474	     */		add	%g5,%o5,%g2
/* 0x0478	 166 */		fmuld	%f4,%f6,%f4
/* 0x047c	 168 */		ldx	[%sp+104],%g4
/* 0x0480	     */		srax	%o1,32,%g5
/* 0x0484	 166 */		fdtox	%f10,%f10
/* 0x0488	 167 */		st	%g3,[%i0+20]
/* 0x048c	 168 */		add	%g2,%g5,%g2
/* 0x0490	 166 */		fmuld	%f8,%f6,%f6
/* 0x0494	 168 */		ldx	[%sp+144],%o5
/* 0x0498	     */		srax	%g2,32,%g5
/* 0x049c	     */		add	%o7,%g4,%g4
/* 0x04a0	     */		ldx	[%sp+200],%g3
/* 0x04a4	     */		add	%g4,%g5,%g4
/* 0x04a8	 166 */		fdtox	%f4,%f4
/* 0x04ac	     */		std	%f10,[%sp+184]
/* 0x04b0	 167 */		st	%o0,[%i0+16]
/* 0x04b4	 168 */		add	%g3,%o5,%g3
/* 0x04b8	 166 */		std	%f4,[%sp+176]
/* 0x04bc	 168 */		srax	%g4,32,%o5
/* 0x04c0	 166 */		fdtox	%f6,%f4
/* 0x04c4	 168 */		ldx	[%sp+128],%o7
/* 0x04c8	     */		add	%g3,%o5,%g3
/* 0x04cc	     */		stx	%g3,[%sp+152]
/* 0x04d0	     */		srax	%g3,32,%o5
/* 0x04d4	     */		ldx	[%sp+192],%o0
/* 0x04d8	     */		ldx	[%sp+184],%g5
/* 0x04dc	     */		ldx	[%sp+120],%g3
/* 0x04e0	     */		add	%o0,%o7,%o0
/* 0x04e4	 166 */		std	%f4,[%sp+168]
/* 0x04e8	 168 */		add	%g5,%g3,%g3
/* 0x04ec	     */		add	%o0,%o5,%g5
/* 0x04f0	     */		stx	%g5,[%sp+160]
/* 0x04f4	     */		ldx	[%sp+176],%o7
/* 0x04f8	     */		srax	%g5,32,%o0
/* 0x04fc	     */		ldx	[%sp+136],%g5
/* 0x0500	     */		add	%g3,%o0,%g3
/* 0x0504	     */		ldx	[%sp+168],%o5
/* 0x0508	     */		add	%o7,%g5,%g5
/* 0x050c	 167 */		st	%o2,[%i0+24]
/* 0x0510	 168 */		srax	%g3,32,%o2
/* 0x0514	     */		st	%o1,[%i0+32]
/* 0x0518	     */		add	%g5,%o2,%g5
/* 0x051c	     */		add	%o5,%o4,%o0
/* 0x0520	     */		st	%g2,[%i0+36]
/* 0x0524	     */		srax	%g5,32,%g2
/* 0x0528	     */		ldx	[%sp+152],%o1
/* 0x052c	     */		ldx	[%sp+160],%o2
/* 0x0530	     */		st	%g4,[%i0+40]
/* 0x0534	     */		add	%o0,%g2,%g4
/* 0x0538	 167 */		st	%o3,[%i0+28]

!  170		      !      return c;

/* 0x053c	 170 */		srax	%g4,32,%o5
/* 0x0540	 168 */		st	%o1,[%i0+44]
/* 0x0544	     */		st	%o2,[%i0+48]
/* 0x0548	     */		st	%g3,[%i0+52]
/* 0x054c	     */		st	%g5,[%i0+56]
/* 0x0550	     */		st	%g4,[%i0+60]
/* 0x0554	     */		ret	! Result =  %i0
/* 0x0558	     */		restore	%g0,%o5,%o0
                       .L77000076:

!  172		      !    } else {
!  173		      !      DEF_VARS(BUFF_SIZE);

/* 0x055c	 173 */		ldd	[%o0],%f8

!  174		      !      t_s32 i, c = 0;
!  176		      !#pragma pipeloop(0)
!  177		      !      for (i = 0; i < (n+1)/2; i ++) {

/* 0x0560	 177 */		add	%o2,1,%g2
/* 0x0564	 173 */		fmovd	%f0,%f14
/* 0x0568	 177 */		srl	%g2,31,%g3
/* 0x056c	 174 */		or	%g0,0,%o5
/* 0x0570	 173 */		ldd	[%o0+8],%f18
/* 0x0574	     */		fmovs	%f8,%f6
/* 0x0578	 177 */		add	%g2,%g3,%g2
/* 0x057c	 170 */		add	%fp,-2264,%o7
/* 0x0580	 177 */		sra	%g2,1,%o1
/* 0x0584	     */		or	%g0,0,%g2
/* 0x0588	 173 */		fsubd	%f6,%f8,%f16
/* 0x058c	 177 */		cmp	%o1,0
/* 0x0590	     */		ble,pt	%icc,.L900000160
/* 0x0594	     */		cmp	%o3,0
/* 0x0598	 170 */		sub	%o1,1,%o2
/* 0x059c	 177 */		cmp	%o1,8
/* 0x05a0	 170 */		add	%fp,-2256,%o4
/* 0x05a4	 177 */		bl,pn	%icc,.L77000077
/* 0x05a8	     */		add	%i2,8,%o0
/* 0x05ac	     */		ldd	[%i2],%f2
/* 0x05b0	     */		sub	%o1,3,%o1
/* 0x05b4	     */		ldd	[%o0],%f0

!  178		      !        MUL_U32_S64_2(i);

/* 0x05b8	 178 */		add	%o0,8,%g3
/* 0x05bc	     */		or	%g0,5,%g2
/* 0x05c0	     */		fxnor	%f14,%f2,%f4
/* 0x05c4	     */		ldd	[%g3],%f2
/* 0x05c8	     */		fxnor	%f14,%f0,%f6
/* 0x05cc	     */		ldd	[%o0+16],%f10
/* 0x05d0	     */		add	%o0,16,%o0
/* 0x05d4	     */		fitod	%f5,%f0
/* 0x05d8	     */		ldd	[%o0+8],%f12
/* 0x05dc	     */		add	%o0,8,%o0
/* 0x05e0	     */		fitod	%f4,%f4
/* 0x05e4	     */		fxnor	%f14,%f2,%f8
/* 0x05e8	     */		fitod	%f7,%f2
/* 0x05ec	     */		fsubd	%f18,%f0,%f0
/* 0x05f0	     */		fsubd	%f18,%f4,%f4
/* 0x05f4	     */		fxnor	%f14,%f10,%f10
                       .L900000149:
/* 0x05f8	 178 */		fitod	%f9,%f22
/* 0x05fc	     */		add	%g2,3,%g2
/* 0x0600	     */		add	%o4,48,%o4
/* 0x0604	     */		fmuld	%f0,%f16,%f0
/* 0x0608	     */		fmuld	%f4,%f16,%f24
/* 0x060c	     */		cmp	%g2,%o1
/* 0x0610	     */		add	%o7,48,%o7
/* 0x0614	     */		fsubd	%f18,%f2,%f2
/* 0x0618	     */		fitod	%f6,%f4
/* 0x061c	     */		fdtox	%f0,%f0
/* 0x0620	     */		add	%o0,8,%i2
/* 0x0624	     */		ldd	[%o0+8],%f20
/* 0x0628	     */		fdtox	%f24,%f6
/* 0x062c	     */		fsubd	%f18,%f4,%f4
/* 0x0630	     */		std	%f6,[%o7-48]
/* 0x0634	     */		fxnor	%f14,%f12,%f6
/* 0x0638	     */		std	%f0,[%o4-48]
/* 0x063c	     */		fitod	%f11,%f0
/* 0x0640	     */		fmuld	%f2,%f16,%f2
/* 0x0644	     */		fmuld	%f4,%f16,%f24
/* 0x0648	     */		fsubd	%f18,%f22,%f12
/* 0x064c	     */		fitod	%f8,%f4
/* 0x0650	     */		fdtox	%f2,%f2
/* 0x0654	     */		ldd	[%i2+8],%f22
/* 0x0658	     */		fdtox	%f24,%f8
/* 0x065c	     */		fsubd	%f18,%f4,%f4
/* 0x0660	     */		std	%f8,[%o7-32]
/* 0x0664	     */		fxnor	%f14,%f20,%f8
/* 0x0668	     */		std	%f2,[%o4-32]
/* 0x066c	     */		fitod	%f7,%f2
/* 0x0670	     */		fmuld	%f12,%f16,%f12
/* 0x0674	     */		fmuld	%f4,%f16,%f24
/* 0x0678	     */		fsubd	%f18,%f0,%f0
/* 0x067c	     */		fitod	%f10,%f4
/* 0x0680	     */		fdtox	%f12,%f20
/* 0x0684	     */		add	%i2,16,%o0
/* 0x0688	     */		ldd	[%i2+16],%f12
/* 0x068c	     */		fdtox	%f24,%f10
/* 0x0690	     */		fsubd	%f18,%f4,%f4
/* 0x0694	     */		std	%f10,[%o7-16]
/* 0x0698	     */		fxnor	%f14,%f22,%f10
/* 0x069c	     */		ble,pt	%icc,.L900000149
/* 0x06a0	     */		std	%f20,[%o4-16]
                       .L900000152:
/* 0x06a4	 178 */		fitod	%f6,%f6
/* 0x06a8	     */		fmuld	%f4,%f16,%f24
/* 0x06ac	     */		add	%o7,80,%o7
/* 0x06b0	     */		fsubd	%f18,%f2,%f2
/* 0x06b4	     */		fmuld	%f0,%f16,%f22
/* 0x06b8	     */		add	%o4,80,%o4
/* 0x06bc	     */		fitod	%f8,%f26
/* 0x06c0	     */		cmp	%g2,%o2
/* 0x06c4	     */		add	%i2,24,%i2
/* 0x06c8	     */		fsubd	%f18,%f6,%f4
/* 0x06cc	     */		fitod	%f9,%f8
/* 0x06d0	     */		fxnor	%f14,%f12,%f0
/* 0x06d4	     */		fmuld	%f2,%f16,%f12
/* 0x06d8	     */		fitod	%f10,%f6
/* 0x06dc	     */		fmuld	%f4,%f16,%f20
/* 0x06e0	     */		fitod	%f11,%f4
/* 0x06e4	     */		fsubd	%f18,%f26,%f10
/* 0x06e8	     */		fitod	%f0,%f2
/* 0x06ec	     */		fsubd	%f18,%f8,%f8
/* 0x06f0	     */		fitod	%f1,%f0
/* 0x06f4	     */		fmuld	%f10,%f16,%f10
/* 0x06f8	     */		fdtox	%f24,%f24
/* 0x06fc	     */		std	%f24,[%o7-80]
/* 0x0700	     */		fsubd	%f18,%f6,%f6
/* 0x0704	     */		fmuld	%f8,%f16,%f8
/* 0x0708	     */		fdtox	%f22,%f22
/* 0x070c	     */		std	%f22,[%o4-80]
/* 0x0710	     */		fsubd	%f18,%f4,%f4
/* 0x0714	     */		fdtox	%f20,%f20
/* 0x0718	     */		std	%f20,[%o7-64]
/* 0x071c	     */		fmuld	%f6,%f16,%f6
/* 0x0720	     */		fsubd	%f18,%f2,%f2
/* 0x0724	     */		fsubd	%f18,%f0,%f0
/* 0x0728	     */		fmuld	%f4,%f16,%f4
/* 0x072c	     */		fdtox	%f12,%f12
/* 0x0730	     */		std	%f12,[%o4-64]
/* 0x0734	     */		fdtox	%f10,%f10
/* 0x0738	     */		std	%f10,[%o7-48]
/* 0x073c	     */		fmuld	%f2,%f16,%f2
/* 0x0740	     */		fdtox	%f8,%f8
/* 0x0744	     */		std	%f8,[%o4-48]
/* 0x0748	     */		fmuld	%f0,%f16,%f0
/* 0x074c	     */		fdtox	%f6,%f6
/* 0x0750	     */		std	%f6,[%o7-32]
/* 0x0754	     */		fdtox	%f4,%f4
/* 0x0758	     */		std	%f4,[%o4-32]
/* 0x075c	     */		fdtox	%f2,%f2
/* 0x0760	     */		std	%f2,[%o7-16]
/* 0x0764	     */		fdtox	%f0,%f0
/* 0x0768	     */		bg,pn	%icc,.L77000043
/* 0x076c	     */		std	%f0,[%o4-16]
                       .L77000077:
/* 0x0770	     */		ldd	[%i2],%f0
                       .L900000159:
/* 0x0774	     */		fxnor	%f14,%f0,%f0
/* 0x0778	 178 */		add	%g2,1,%g2
/* 0x077c	     */		add	%i2,8,%i2
/* 0x0780	     */		cmp	%g2,%o2
/* 0x0784	     */		fitod	%f0,%f2
/* 0x0788	     */		fitod	%f1,%f0
/* 0x078c	     */		fsubd	%f18,%f2,%f2
/* 0x0790	     */		fsubd	%f18,%f0,%f0
/* 0x0794	     */		fmuld	%f2,%f16,%f2
/* 0x0798	     */		fmuld	%f0,%f16,%f0
/* 0x079c	     */		fdtox	%f2,%f2
/* 0x07a0	     */		std	%f2,[%o7]
/* 0x07a4	     */		add	%o7,16,%o7
/* 0x07a8	     */		fdtox	%f0,%f0
/* 0x07ac	     */		std	%f0,[%o4]
/* 0x07b0	     */		add	%o4,16,%o4
/* 0x07b4	     */		ble,a,pt	%icc,.L900000159
/* 0x07b8	     */		ldd	[%i2],%f0
                       .L77000043:

!  179		      !      }
!  181		      !#pragma pipeloop(0)
!  182		      !      for (i = 0; i < n; i ++) {

/* 0x07bc	 182 */		cmp	%o3,0
                       .L900000160:
/* 0x07c0	 182 */		ble,pt	%icc,.L77000061
/* 0x07c4	     */		nop
/* 0x07c8	     */		or	%g0,%i1,%g2
/* 0x07cc	     */		sub	%o3,1,%o4
/* 0x07d0	     */		or	%g0,0,%g3
/* 0x07d4	     */		add	%fp,-2264,%g4
/* 0x07d8	     */		or	%g0,%i0,%o7
/* 0x07dc	 178 */		cmp	%o3,5
/* 0x07e0	     */		sub	%o3,3,%o3
/* 0x07e4	     */		bl,pn	%icc,.L77000078
/* 0x07e8	     */		ldx	[%fp-2264],%o0

!  183		      !        ADD_S64_U32(i);

/* 0x07ec	 183 */		add	%o7,4,%o7
/* 0x07f0	   0 */		add	%fp,-2248,%g4
/* 0x07f4	 183 */		ld	[%g2],%o1
/* 0x07f8	     */		or	%g0,2,%g3
/* 0x07fc	     */		ld	[%g2+4],%o2
/* 0x0800	     */		add	%g2,8,%g2
/* 0x0804	     */		add	%o0,%o1,%o0
/* 0x0808	     */		ldx	[%fp-2256],%o1
/* 0x080c	     */		st	%o0,[%o7-4]
/* 0x0810	     */		srax	%o0,32,%o0
                       .L900000145:
/* 0x0814	 183 */		ldx	[%g4],%g5
/* 0x0818	     */		sra	%o0,0,%o5
/* 0x081c	     */		add	%o1,%o2,%o0
/* 0x0820	     */		ld	[%g2],%o1
/* 0x0824	     */		add	%o0,%o5,%o0
/* 0x0828	     */		add	%g3,3,%g3
/* 0x082c	     */		st	%o0,[%o7]
/* 0x0830	     */		srax	%o0,32,%o5
/* 0x0834	     */		cmp	%g3,%o3
/* 0x0838	     */		ldx	[%g4+8],%o0
/* 0x083c	     */		add	%g5,%o1,%o1
/* 0x0840	     */		ld	[%g2+4],%o2
/* 0x0844	     */		add	%o1,%o5,%o1
/* 0x0848	     */		add	%o7,12,%o7
/* 0x084c	     */		st	%o1,[%o7-8]
/* 0x0850	     */		srax	%o1,32,%o5
/* 0x0854	     */		add	%g2,12,%g2
/* 0x0858	     */		ldx	[%g4+16],%o1
/* 0x085c	     */		add	%o0,%o2,%o0
/* 0x0860	     */		ld	[%g2-4],%o2
/* 0x0864	     */		add	%o0,%o5,%o0
/* 0x0868	     */		add	%g4,24,%g4
/* 0x086c	     */		st	%o0,[%o7-4]
/* 0x0870	     */		ble,pt	%icc,.L900000145
/* 0x0874	     */		srax	%o0,32,%o0
                       .L900000148:
/* 0x0878	 183 */		sra	%o0,0,%o3
/* 0x087c	     */		add	%o1,%o2,%o0
/* 0x0880	     */		add	%o0,%o3,%o0
/* 0x0884	     */		add	%o7,4,%o7
/* 0x0888	     */		st	%o0,[%o7-4]
/* 0x088c	     */		cmp	%g3,%o4
/* 0x0890	     */		bg,pn	%icc,.L77000061
/* 0x0894	     */		srax	%o0,32,%o5
                       .L77000078:
/* 0x0898	 183 */		ld	[%g2],%o2
                       .L900000158:
/* 0x089c	 183 */		sra	%o5,0,%o1
/* 0x08a0	     */		ldx	[%g4],%o0
/* 0x08a4	     */		add	%g3,1,%g3
/* 0x08a8	     */		add	%g2,4,%g2
/* 0x08ac	     */		add	%g4,8,%g4
/* 0x08b0	     */		add	%o0,%o2,%o0
/* 0x08b4	     */		cmp	%g3,%o4
/* 0x08b8	     */		add	%o0,%o1,%o0
/* 0x08bc	     */		st	%o0,[%o7]
/* 0x08c0	     */		add	%o7,4,%o7
/* 0x08c4	     */		srax	%o0,32,%o5
/* 0x08c8	     */		ble,a,pt	%icc,.L900000158
/* 0x08cc	     */		ld	[%g2],%o2
                       .L77000047:
/* 0x08d0	     */		ret	! Result =  %i0
/* 0x08d4	     */		restore	%g0,%o5,%o0
                       .L77000048:

!  184		      !      }
!  186		      !      return c;
!  188		      !    }
!  189		      !  } else {
!  191		      !    if (n == 8) {

/* 0x08d8	 191 */		bne,pn	%icc,.L77000050
/* 0x08dc	     */		sethi	%hi(0xfff80000),%g2
/* 0x08e0	     */		ldd	[%i2],%f4

!  192		      !      DEF_VARS(2*8);
!  193		      !      t_d64 d0, d1, db;
!  194		      !      t_u32 uc = 0;
!  196		      !      da = (t_d64)(a &  A_MASK);

/* 0x08e4	 196 */		ldd	[%o0],%f6

!  197		      !      db = (t_d64)(a >> A_BITS);

/* 0x08e8	 197 */		srl	%o1,19,%g3
/* 0x08ec	 196 */		andn	%o1,%g2,%g2
/* 0x08f0	 197 */		st	%g3,[%sp+240]
/* 0x08f4	     */		fxnor	%f0,%f4,%f4
/* 0x08f8	 196 */		st	%g2,[%sp+244]
/* 0x08fc	     */		ldd	[%i2+8],%f12

!  199		      !      MUL_U32_S64_D_8(0);

/* 0x0900	 199 */		fitod	%f4,%f10
/* 0x0904	     */		ldd	[%o0+8],%f16
/* 0x0908	     */		fitod	%f5,%f4
/* 0x090c	     */		ldd	[%i2+16],%f18
/* 0x0910	     */		fxnor	%f0,%f12,%f12
/* 0x0914	 197 */		ld	[%sp+240],%f9
/* 0x0918	 199 */		fsubd	%f16,%f10,%f10
/* 0x091c	 196 */		ld	[%sp+244],%f15
/* 0x0920	 199 */		fitod	%f12,%f22
/* 0x0924	     */		ldd	[%i2+24],%f20
/* 0x0928	     */		fsubd	%f16,%f4,%f4

!  200		      !      ADD_S64_U32_D_8(0);

/* 0x092c	 200 */		ld	[%i1+28],%o3
/* 0x0930	 199 */		fitod	%f13,%f12
/* 0x0934	 200 */		ld	[%i1],%g2
/* 0x0938	 199 */		fsubd	%f16,%f22,%f22
/* 0x093c	 200 */		stx	%o3,[%sp+96]
/* 0x0940	     */		fxnor	%f0,%f18,%f18
/* 0x0944	     */		ld	[%i1+4],%g3
/* 0x0948	 199 */		fsubd	%f16,%f12,%f12
/* 0x094c	 200 */		ld	[%i1+24],%o2
/* 0x0950	 199 */		fitod	%f18,%f26
/* 0x0954	 200 */		ld	[%i1+8],%g4
/* 0x0958	     */		fxnor	%f0,%f20,%f20
/* 0x095c	     */		stx	%o2,[%sp+104]
/* 0x0960	     */		ld	[%i1+12],%g5
/* 0x0964	     */		ld	[%i1+16],%o0
/* 0x0968	     */		ld	[%i1+20],%o1
/* 0x096c	 197 */		fmovs	%f6,%f8
/* 0x0970	 196 */		fmovs	%f6,%f14
/* 0x0974	 197 */		fsubd	%f8,%f6,%f8
/* 0x0978	 196 */		fsubd	%f14,%f6,%f6
/* 0x097c	 199 */		fmuld	%f10,%f8,%f14
/* 0x0980	     */		fmuld	%f4,%f8,%f24
/* 0x0984	     */		fmuld	%f10,%f6,%f10
/* 0x0988	     */		fdtox	%f14,%f14
/* 0x098c	     */		std	%f14,[%sp+224]
/* 0x0990	     */		fmuld	%f22,%f8,%f28
/* 0x0994	     */		fitod	%f19,%f14
/* 0x0998	     */		fmuld	%f22,%f6,%f18
/* 0x099c	     */		fdtox	%f10,%f10
/* 0x09a0	     */		std	%f10,[%sp+232]
/* 0x09a4	     */		fmuld	%f4,%f6,%f4
/* 0x09a8	     */		fmuld	%f12,%f8,%f22
/* 0x09ac	     */		fdtox	%f18,%f18
/* 0x09b0	     */		std	%f18,[%sp+200]
/* 0x09b4	     */		fmuld	%f12,%f6,%f10
/* 0x09b8	 200 */		ldx	[%sp+224],%o4
/* 0x09bc	 199 */		fdtox	%f24,%f12
/* 0x09c0	     */		std	%f12,[%sp+208]
/* 0x09c4	     */		fsubd	%f16,%f26,%f12
/* 0x09c8	 200 */		ldx	[%sp+232],%o5
/* 0x09cc	     */		sllx	%o4,19,%o4
/* 0x09d0	 199 */		fdtox	%f4,%f4
/* 0x09d4	     */		std	%f4,[%sp+216]
/* 0x09d8	     */		fitod	%f20,%f4
/* 0x09dc	     */		fdtox	%f28,%f24
/* 0x09e0	     */		std	%f24,[%sp+192]
/* 0x09e4	 200 */		add	%o5,%o4,%o4
/* 0x09e8	     */		ldx	[%sp+208],%o7
/* 0x09ec	 199 */		fsubd	%f16,%f14,%f14
/* 0x09f0	 200 */		add	%o4,%g2,%o4
/* 0x09f4	 199 */		fmuld	%f12,%f8,%f24
/* 0x09f8	 200 */		ldx	[%sp+216],%o5
/* 0x09fc	 199 */		fitod	%f21,%f18
/* 0x0a00	     */		fmuld	%f12,%f6,%f12
/* 0x0a04	 200 */		st	%o4,[%i0]
/* 0x0a08	     */		sllx	%o7,19,%o7
/* 0x0a0c	 199 */		fsubd	%f16,%f4,%f4
/* 0x0a10	 200 */		ldx	[%sp+192],%o3
/* 0x0a14	 199 */		fdtox	%f10,%f10
/* 0x0a18	 200 */		add	%o5,%o7,%o5
/* 0x0a1c	 199 */		std	%f10,[%sp+184]
/* 0x0a20	 200 */		srlx	%o4,32,%o7
/* 0x0a24	 199 */		fdtox	%f22,%f20
/* 0x0a28	     */		std	%f20,[%sp+176]
/* 0x0a2c	 200 */		sllx	%o3,19,%o3
/* 0x0a30	 199 */		fdtox	%f24,%f10
/* 0x0a34	     */		fmuld	%f14,%f8,%f20
/* 0x0a38	     */		std	%f10,[%sp+160]
/* 0x0a3c	 200 */		add	%o5,%g3,%g3
/* 0x0a40	 199 */		fsubd	%f16,%f18,%f10
/* 0x0a44	     */		fmuld	%f4,%f8,%f16
/* 0x0a48	 200 */		ldx	[%sp+200],%g2
/* 0x0a4c	     */		add	%g3,%o7,%g3
/* 0x0a50	 199 */		fdtox	%f12,%f12
/* 0x0a54	     */		fmuld	%f14,%f6,%f14
/* 0x0a58	     */		std	%f12,[%sp+168]
/* 0x0a5c	     */		fdtox	%f20,%f12
/* 0x0a60	     */		fmuld	%f4,%f6,%f4
/* 0x0a64	     */		std	%f12,[%sp+144]
/* 0x0a68	 200 */		add	%g2,%o3,%o3
/* 0x0a6c	 199 */		fmuld	%f10,%f8,%f8
/* 0x0a70	 200 */		ldx	[%sp+176],%o5
/* 0x0a74	     */		add	%o3,%g4,%g4
/* 0x0a78	 199 */		fdtox	%f14,%f12
/* 0x0a7c	     */		fmuld	%f10,%f6,%f6
/* 0x0a80	 200 */		ldx	[%sp+184],%o2
/* 0x0a84	     */		srlx	%g3,32,%o3
/* 0x0a88	 199 */		fdtox	%f16,%f10
/* 0x0a8c	     */		std	%f12,[%sp+152]
/* 0x0a90	 200 */		sllx	%o5,19,%o5
/* 0x0a94	     */		add	%g4,%o3,%g4
/* 0x0a98	     */		ldx	[%sp+168],%o7
/* 0x0a9c	     */		add	%o2,%o5,%o2
/* 0x0aa0	 199 */		fdtox	%f4,%f4
/* 0x0aa4	     */		std	%f10,[%sp+128]
/* 0x0aa8	 200 */		srlx	%g4,32,%o5
/* 0x0aac	     */		add	%o2,%g5,%o2
/* 0x0ab0	     */		ldx	[%sp+160],%g2
/* 0x0ab4	     */		add	%o2,%o5,%o2
/* 0x0ab8	 199 */		std	%f4,[%sp+136]
/* 0x0abc	     */		fdtox	%f8,%f4
/* 0x0ac0	 200 */		ldx	[%sp+152],%o5
/* 0x0ac4	     */		sllx	%g2,19,%g2
/* 0x0ac8	 199 */		std	%f4,[%sp+112]
/* 0x0acc	 200 */		add	%o7,%g2,%o3
/* 0x0ad0	 199 */		fdtox	%f6,%f4
/* 0x0ad4	 200 */		ldx	[%sp+144],%g5
/* 0x0ad8	     */		add	%o3,%o0,%o3
/* 0x0adc	 199 */		std	%f4,[%sp+120]
/* 0x0ae0	 200 */		srlx	%o2,32,%o0
/* 0x0ae4	     */		ldx	[%sp+136],%o7
/* 0x0ae8	     */		sllx	%g5,19,%g5
/* 0x0aec	     */		add	%o3,%o0,%o3
/* 0x0af0	     */		st	%g3,[%i0+4]
/* 0x0af4	     */		srlx	%o3,32,%o4
/* 0x0af8	     */		add	%o5,%g5,%g5
/* 0x0afc	     */		ldx	[%sp+128],%g2
/* 0x0b00	     */		add	%g5,%o1,%o1
/* 0x0b04	     */		ldx	[%sp+112],%o0
/* 0x0b08	     */		add	%o1,%o4,%g3
/* 0x0b0c	     */		ldx	[%sp+104],%g5
/* 0x0b10	     */		sllx	%g2,19,%g2
/* 0x0b14	     */		ldx	[%sp+120],%o5
/* 0x0b18	     */		add	%o7,%g2,%o1
/* 0x0b1c	     */		st	%g4,[%i0+8]
/* 0x0b20	     */		srlx	%g3,32,%g2
/* 0x0b24	     */		add	%o1,%g5,%o4
/* 0x0b28	     */		st	%g3,[%i0+20]
/* 0x0b2c	     */		sllx	%o0,19,%g3
/* 0x0b30	     */		add	%o4,%g2,%g2
/* 0x0b34	     */		ldx	[%sp+96],%g4
/* 0x0b38	     */		add	%o5,%g3,%g3
/* 0x0b3c	     */		st	%g2,[%i0+24]
/* 0x0b40	     */		srlx	%g2,32,%g2
/* 0x0b44	     */		add	%g3,%g4,%g3
/* 0x0b48	     */		st	%o2,[%i0+12]
/* 0x0b4c	     */		add	%g3,%g2,%g2
/* 0x0b50	     */		st	%o3,[%i0+16]
/* 0x0b54	     */		st	%g2,[%i0+28]

!  202		      !      return uc;

/* 0x0b58	 202 */		srlx	%g2,32,%o5
/* 0x0b5c	     */		ret	! Result =  %i0
/* 0x0b60	     */		restore	%g0,%o5,%o0
                       .L77000050:

!  204		      !    } else if (n == 16) {

/* 0x0b64	 204 */		cmp	%o2,16
/* 0x0b68	     */		bne,pn	%icc,.L77000073
/* 0x0b6c	     */		sethi	%hi(0xfff80000),%g2
/* 0x0b70	     */		ldd	[%i2],%f4

!  205		      !      DEF_VARS(2*16);
!  206		      !      t_d64 d0, d1, db;
!  207		      !      t_u32 uc = 0;
!  209		      !      da = (t_d64)(a &  A_MASK);

/* 0x0b74	 209 */		andn	%o1,%g2,%g2
/* 0x0b78	     */		st	%g2,[%sp+356]

!  210		      !      db = (t_d64)(a >> A_BITS);

/* 0x0b7c	 210 */		srl	%o1,19,%g2
/* 0x0b80	     */		fxnor	%f0,%f4,%f4
/* 0x0b84	 209 */		ldd	[%o0],%f8
/* 0x0b88	 210 */		st	%g2,[%sp+352]

!  212		      !      MUL_U32_S64_D_8(0);

/* 0x0b8c	 212 */		fitod	%f4,%f10
/* 0x0b90	     */		ldd	[%o0+8],%f16
/* 0x0b94	     */		ldd	[%i2+8],%f14
/* 0x0b98	     */		fitod	%f5,%f4
/* 0x0b9c	 209 */		ld	[%sp+356],%f7
/* 0x0ba0	 212 */		fsubd	%f16,%f10,%f10
/* 0x0ba4	 210 */		ld	[%sp+352],%f13
/* 0x0ba8	     */		fxnor	%f0,%f14,%f14
/* 0x0bac	 212 */		fsubd	%f16,%f4,%f4
/* 0x0bb0	 209 */		fmovs	%f8,%f6
/* 0x0bb4	 210 */		fmovs	%f8,%f12
/* 0x0bb8	 209 */		fsubd	%f6,%f8,%f6
/* 0x0bbc	 210 */		fsubd	%f12,%f8,%f8
/* 0x0bc0	 212 */		fitod	%f14,%f12
/* 0x0bc4	     */		fmuld	%f10,%f6,%f18
/* 0x0bc8	     */		fitod	%f15,%f14
/* 0x0bcc	     */		fmuld	%f10,%f8,%f10
/* 0x0bd0	     */		fsubd	%f16,%f12,%f12
/* 0x0bd4	     */		fmuld	%f4,%f6,%f20
/* 0x0bd8	     */		fmuld	%f4,%f8,%f4
/* 0x0bdc	     */		fsubd	%f16,%f14,%f14
/* 0x0be0	     */		fdtox	%f10,%f10
/* 0x0be4	     */		std	%f10,[%sp+336]
/* 0x0be8	     */		fmuld	%f12,%f6,%f10
/* 0x0bec	     */		fdtox	%f18,%f18
/* 0x0bf0	     */		std	%f18,[%sp+344]
/* 0x0bf4	     */		fmuld	%f12,%f8,%f12
/* 0x0bf8	     */		fdtox	%f4,%f4
/* 0x0bfc	     */		std	%f4,[%sp+320]
/* 0x0c00	     */		fmuld	%f14,%f6,%f4
/* 0x0c04	     */		fdtox	%f20,%f18
/* 0x0c08	     */		std	%f18,[%sp+328]
/* 0x0c0c	     */		fdtox	%f10,%f10
/* 0x0c10	     */		std	%f10,[%sp+312]
/* 0x0c14	     */		fmuld	%f14,%f8,%f14
/* 0x0c18	     */		fdtox	%f12,%f12
/* 0x0c1c	     */		std	%f12,[%sp+304]
/* 0x0c20	     */		ldd	[%i2+16],%f10
/* 0x0c24	     */		fdtox	%f4,%f4
/* 0x0c28	     */		ldd	[%i2+24],%f12
/* 0x0c2c	     */		fdtox	%f14,%f14
/* 0x0c30	     */		std	%f4,[%sp+296]
/* 0x0c34	     */		fxnor	%f0,%f10,%f10
/* 0x0c38	     */		fxnor	%f0,%f12,%f4
/* 0x0c3c	     */		std	%f14,[%sp+288]
/* 0x0c40	     */		fitod	%f10,%f12
/* 0x0c44	     */		fitod	%f11,%f10
/* 0x0c48	     */		fitod	%f4,%f14
/* 0x0c4c	     */		fsubd	%f16,%f12,%f12
/* 0x0c50	     */		fsubd	%f16,%f10,%f10
/* 0x0c54	     */		fitod	%f5,%f4
/* 0x0c58	     */		fmuld	%f12,%f6,%f18
/* 0x0c5c	     */		fsubd	%f16,%f14,%f14
/* 0x0c60	     */		fmuld	%f10,%f6,%f20
/* 0x0c64	     */		fmuld	%f12,%f8,%f12
/* 0x0c68	     */		fsubd	%f16,%f4,%f4
/* 0x0c6c	     */		fmuld	%f10,%f8,%f10
/* 0x0c70	     */		fdtox	%f18,%f18
/* 0x0c74	     */		std	%f18,[%sp+280]
/* 0x0c78	     */		fdtox	%f20,%f18
/* 0x0c7c	     */		std	%f18,[%sp+264]
/* 0x0c80	     */		fdtox	%f12,%f12
/* 0x0c84	     */		std	%f12,[%sp+272]
/* 0x0c88	     */		fmuld	%f14,%f6,%f12
/* 0x0c8c	     */		fdtox	%f10,%f10
/* 0x0c90	     */		std	%f10,[%sp+256]
/* 0x0c94	     */		fmuld	%f4,%f6,%f10
/* 0x0c98	     */		fmuld	%f4,%f8,%f4
/* 0x0c9c	     */		fdtox	%f12,%f12
/* 0x0ca0	     */		std	%f12,[%sp+248]
/* 0x0ca4	     */		fmuld	%f14,%f8,%f14
/* 0x0ca8	     */		fdtox	%f10,%f10
/* 0x0cac	     */		std	%f10,[%sp+232]
/* 0x0cb0	     */		ldd	[%i2+32],%f12
/* 0x0cb4	     */		fdtox	%f4,%f4
/* 0x0cb8	     */		std	%f4,[%sp+224]
/* 0x0cbc	     */		fdtox	%f14,%f14
/* 0x0cc0	     */		fxnor	%f0,%f12,%f12
/* 0x0cc4	     */		std	%f14,[%sp+240]
/* 0x0cc8	     */		ldd	[%i2+40],%f14

!  213		      !      MUL_U32_S64_D_8(4);

/* 0x0ccc	 213 */		fitod	%f12,%f4
/* 0x0cd0	     */		fitod	%f13,%f12
/* 0x0cd4	     */		fxnor	%f0,%f14,%f10
/* 0x0cd8	     */		fsubd	%f16,%f4,%f4
/* 0x0cdc	     */		fsubd	%f16,%f12,%f12
/* 0x0ce0	     */		fitod	%f10,%f14
/* 0x0ce4	     */		fmuld	%f4,%f6,%f18
/* 0x0ce8	     */		fitod	%f11,%f10
/* 0x0cec	     */		fmuld	%f12,%f6,%f20
/* 0x0cf0	     */		fmuld	%f4,%f8,%f4
/* 0x0cf4	     */		fsubd	%f16,%f14,%f14
/* 0x0cf8	     */		fmuld	%f12,%f8,%f12
/* 0x0cfc	     */		fsubd	%f16,%f10,%f10
/* 0x0d00	     */		fdtox	%f18,%f18
/* 0x0d04	     */		std	%f18,[%sp+216]
/* 0x0d08	     */		fdtox	%f4,%f4
/* 0x0d0c	     */		std	%f4,[%sp+208]
/* 0x0d10	     */		fmuld	%f14,%f6,%f4
/* 0x0d14	     */		fdtox	%f12,%f12
/* 0x0d18	     */		std	%f12,[%sp+192]
/* 0x0d1c	     */		fmuld	%f10,%f6,%f12
/* 0x0d20	     */		fdtox	%f20,%f18
/* 0x0d24	     */		std	%f18,[%sp+200]
/* 0x0d28	     */		fmuld	%f10,%f8,%f10
/* 0x0d2c	     */		fdtox	%f4,%f4
/* 0x0d30	     */		std	%f4,[%sp+184]
/* 0x0d34	     */		fmuld	%f14,%f8,%f14
/* 0x0d38	     */		fdtox	%f12,%f12
/* 0x0d3c	     */		std	%f12,[%sp+168]
/* 0x0d40	     */		ldd	[%i2+48],%f4
/* 0x0d44	     */		fdtox	%f10,%f10
/* 0x0d48	     */		std	%f10,[%sp+160]
/* 0x0d4c	     */		fdtox	%f14,%f14
/* 0x0d50	     */		fxnor	%f0,%f4,%f4
/* 0x0d54	     */		std	%f14,[%sp+176]
/* 0x0d58	     */		ldd	[%i2+56],%f14
/* 0x0d5c	     */		fitod	%f4,%f10
/* 0x0d60	     */		fitod	%f5,%f4
/* 0x0d64	     */		fxnor	%f0,%f14,%f12
/* 0x0d68	     */		fsubd	%f16,%f10,%f10
/* 0x0d6c	     */		fsubd	%f16,%f4,%f4
/* 0x0d70	     */		fitod	%f12,%f14
/* 0x0d74	     */		fmuld	%f10,%f6,%f18
/* 0x0d78	     */		fitod	%f13,%f12
/* 0x0d7c	     */		fmuld	%f4,%f6,%f20
/* 0x0d80	     */		fsubd	%f16,%f14,%f14
/* 0x0d84	     */		fmuld	%f10,%f8,%f10
/* 0x0d88	     */		fdtox	%f18,%f18
/* 0x0d8c	     */		std	%f18,[%sp+152]
/* 0x0d90	     */		fmuld	%f4,%f8,%f4
/* 0x0d94	     */		fdtox	%f10,%f10
/* 0x0d98	     */		std	%f10,[%sp+144]
/* 0x0d9c	     */		fdtox	%f20,%f10
/* 0x0da0	     */		std	%f10,[%sp+136]
/* 0x0da4	     */		fdtox	%f4,%f4
/* 0x0da8	     */		std	%f4,[%sp+128]
/* 0x0dac	     */		fmuld	%f14,%f6,%f10
/* 0x0db0	     */		fmuld	%f14,%f8,%f4
/* 0x0db4	     */		fdtox	%f10,%f10
/* 0x0db8	     */		std	%f10,[%sp+120]
/* 0x0dbc	     */		fdtox	%f4,%f4
/* 0x0dc0	     */		std	%f4,[%sp+112]

!  214		      !      ADD_S64_U32_D_8(0);

/* 0x0dc4	 214 */		ldx	[%sp+336],%g2
/* 0x0dc8	 213 */		fsubd	%f16,%f12,%f4
/* 0x0dcc	 214 */		ldx	[%sp+344],%g3
/* 0x0dd0	     */		ld	[%i1],%g4
/* 0x0dd4	     */		sllx	%g2,19,%g2
/* 0x0dd8	     */		ldx	[%sp+328],%g5
/* 0x0ddc	     */		add	%g3,%g2,%g2
/* 0x0de0	 213 */		fmuld	%f4,%f8,%f8
/* 0x0de4	 214 */		ldx	[%sp+320],%g3
/* 0x0de8	     */		add	%g2,%g4,%g4
/* 0x0dec	 213 */		fmuld	%f4,%f6,%f4
/* 0x0df0	 214 */		st	%g4,[%i0]
/* 0x0df4	     */		srlx	%g4,32,%g4
/* 0x0df8	     */		ld	[%i1+8],%o0
/* 0x0dfc	     */		sllx	%g3,19,%g2
/* 0x0e00	 213 */		fdtox	%f8,%f6
/* 0x0e04	 214 */		ld	[%i1+4],%g3
/* 0x0e08	     */		add	%g5,%g2,%g2
/* 0x0e0c	 213 */		fdtox	%f4,%f4
/* 0x0e10	 214 */		ldx	[%sp+312],%g5
/* 0x0e14	     */		ld	[%i1+12],%o1
/* 0x0e18	     */		add	%g2,%g3,%g2
/* 0x0e1c	     */		ldx	[%sp+304],%g3
/* 0x0e20	     */		add	%g2,%g4,%g4
/* 0x0e24	     */		st	%g4,[%i0+4]
/* 0x0e28	 213 */		std	%f6,[%sp+96]
/* 0x0e2c	 214 */		sllx	%g3,19,%g2
/* 0x0e30	     */		ldx	[%sp+296],%g3
/* 0x0e34	     */		add	%g5,%g2,%g2
/* 0x0e38	     */		ldx	[%sp+288],%g5
/* 0x0e3c	     */		add	%g2,%o0,%g2
/* 0x0e40	 213 */		std	%f4,[%sp+104]
/* 0x0e44	 214 */		srlx	%g4,32,%o0
/* 0x0e48	     */		ldx	[%sp+280],%g4
/* 0x0e4c	     */		sllx	%g5,19,%g5
/* 0x0e50	     */		add	%g2,%o0,%g2
/* 0x0e54	     */		st	%g2,[%i0+8]
/* 0x0e58	     */		srlx	%g2,32,%o0
/* 0x0e5c	     */		add	%g3,%g5,%g3
/* 0x0e60	     */		ldx	[%sp+272],%g5
/* 0x0e64	     */		add	%g3,%o1,%g3
/* 0x0e68	     */		ld	[%i1+16],%o1
/* 0x0e6c	     */		add	%g3,%o0,%g3
/* 0x0e70	     */		st	%g3,[%i0+12]
/* 0x0e74	     */		sllx	%g5,19,%g5
/* 0x0e78	     */		srlx	%g3,32,%o0
/* 0x0e7c	     */		add	%g4,%g5,%g2
/* 0x0e80	     */		ldx	[%sp+256],%g5
/* 0x0e84	     */		ldx	[%sp+264],%g4
/* 0x0e88	     */		add	%g2,%o1,%g2
/* 0x0e8c	     */		ld	[%i1+20],%o1
/* 0x0e90	     */		sllx	%g5,19,%g5
/* 0x0e94	     */		add	%g2,%o0,%g2
/* 0x0e98	     */		st	%g2,[%i0+16]
/* 0x0e9c	     */		srlx	%g2,32,%o0
/* 0x0ea0	     */		add	%g4,%g5,%g3
/* 0x0ea4	     */		ldx	[%sp+240],%g5
/* 0x0ea8	     */		add	%g3,%o1,%g3
/* 0x0eac	     */		ldx	[%sp+248],%g4
/* 0x0eb0	     */		add	%g3,%o0,%g3
/* 0x0eb4	     */		ld	[%i1+24],%o1
/* 0x0eb8	     */		sllx	%g5,19,%g5
/* 0x0ebc	     */		st	%g3,[%i0+20]
/* 0x0ec0	     */		srlx	%g3,32,%o0
/* 0x0ec4	     */		add	%g4,%g5,%g2
/* 0x0ec8	     */		ldx	[%sp+224],%g5
/* 0x0ecc	     */		add	%g2,%o1,%g2
/* 0x0ed0	     */		ldx	[%sp+232],%g4
/* 0x0ed4	     */		add	%g2,%o0,%g2
/* 0x0ed8	     */		ld	[%i1+28],%o1
/* 0x0edc	     */		sllx	%g5,19,%g5
/* 0x0ee0	     */		st	%g2,[%i0+24]
/* 0x0ee4	     */		srlx	%g2,32,%o0
/* 0x0ee8	     */		add	%g4,%g5,%g3

!  215		      !      ADD_S64_U32_D_8(8);

/* 0x0eec	 215 */		ldx	[%sp+208],%g5
/* 0x0ef0	 214 */		add	%g3,%o1,%g3
/* 0x0ef4	 215 */		ldx	[%sp+216],%g4
/* 0x0ef8	 214 */		add	%g3,%o0,%g3
/* 0x0efc	 215 */		ld	[%i1+32],%o1
/* 0x0f00	     */		sllx	%g5,19,%g5
/* 0x0f04	 214 */		st	%g3,[%i0+28]
/* 0x0f08	 215 */		srlx	%g3,32,%o0
/* 0x0f0c	     */		add	%g4,%g5,%g2
/* 0x0f10	     */		ldx	[%sp+192],%g5
/* 0x0f14	     */		add	%g2,%o1,%g2
/* 0x0f18	     */		ldx	[%sp+200],%g4
/* 0x0f1c	     */		add	%g2,%o0,%g2
/* 0x0f20	     */		ld	[%i1+36],%o1
/* 0x0f24	     */		sllx	%g5,19,%g5
/* 0x0f28	     */		st	%g2,[%i0+32]
/* 0x0f2c	     */		add	%g4,%g5,%g3
/* 0x0f30	     */		ldx	[%sp+176],%g5
/* 0x0f34	     */		srlx	%g2,32,%o0
/* 0x0f38	     */		add	%g3,%o1,%g3
/* 0x0f3c	     */		ldx	[%sp+184],%g4
/* 0x0f40	     */		add	%g3,%o0,%g3
/* 0x0f44	     */		ld	[%i1+40],%o1
/* 0x0f48	     */		sllx	%g5,19,%g5
/* 0x0f4c	     */		st	%g3,[%i0+36]
/* 0x0f50	     */		add	%g4,%g5,%g2
/* 0x0f54	     */		ldx	[%sp+160],%g5
/* 0x0f58	     */		srlx	%g3,32,%o0
/* 0x0f5c	     */		add	%g2,%o1,%g2
/* 0x0f60	     */		ldx	[%sp+168],%g4
/* 0x0f64	     */		add	%g2,%o0,%g2
/* 0x0f68	     */		sllx	%g5,19,%g5
/* 0x0f6c	     */		ld	[%i1+44],%o1
/* 0x0f70	     */		add	%g4,%g5,%g3
/* 0x0f74	     */		ldx	[%sp+144],%g5
/* 0x0f78	     */		ldx	[%sp+152],%g4
/* 0x0f7c	     */		srlx	%g2,32,%o0
/* 0x0f80	     */		add	%g3,%o1,%g3
/* 0x0f84	     */		st	%g2,[%i0+40]
/* 0x0f88	     */		sllx	%g5,19,%g5
/* 0x0f8c	     */		add	%g3,%o0,%g3
/* 0x0f90	     */		ld	[%i1+48],%o1
/* 0x0f94	     */		add	%g4,%g5,%g2
/* 0x0f98	     */		ldx	[%sp+128],%g5
/* 0x0f9c	     */		srlx	%g3,32,%o0
/* 0x0fa0	     */		ldx	[%sp+136],%g4
/* 0x0fa4	     */		add	%g2,%o1,%g2
/* 0x0fa8	     */		sllx	%g5,19,%g5
/* 0x0fac	     */		st	%g3,[%i0+44]
/* 0x0fb0	     */		add	%g2,%o0,%g2
/* 0x0fb4	     */		add	%g4,%g5,%g3
/* 0x0fb8	     */		ldx	[%sp+112],%g5
/* 0x0fbc	     */		ld	[%i1+52],%o1
/* 0x0fc0	     */		srlx	%g2,32,%o0
/* 0x0fc4	     */		ldx	[%sp+120],%g4
/* 0x0fc8	     */		sllx	%g5,19,%g5
/* 0x0fcc	     */		st	%g2,[%i0+48]
/* 0x0fd0	     */		add	%g3,%o1,%g3
/* 0x0fd4	     */		ld	[%i1+56],%o1
/* 0x0fd8	     */		add	%g4,%g5,%g2
/* 0x0fdc	     */		add	%g3,%o0,%g3
/* 0x0fe0	     */		ldx	[%sp+96],%g5
/* 0x0fe4	     */		srlx	%g3,32,%o0
/* 0x0fe8	     */		ldx	[%sp+104],%g4
/* 0x0fec	     */		add	%g2,%o1,%g2
/* 0x0ff0	     */		ld	[%i1+60],%o1
/* 0x0ff4	     */		sllx	%g5,19,%g5
/* 0x0ff8	     */		add	%g2,%o0,%g2
/* 0x0ffc	     */		st	%g3,[%i0+52]
/* 0x1000	     */		add	%g4,%g5,%g3
/* 0x1004	     */		st	%g2,[%i0+56]
/* 0x1008	     */		srlx	%g2,32,%g2
/* 0x100c	     */		add	%g3,%o1,%g3
/* 0x1010	     */		add	%g3,%g2,%g2
/* 0x1014	     */		st	%g2,[%i0+60]

!  217		      !      return uc;

/* 0x1018	 217 */		srlx	%g2,32,%o5
/* 0x101c	     */		ret	! Result =  %i0
/* 0x1020	     */		restore	%g0,%o5,%o0
                       .L77000073:

!  219		      !    } else {
!  220		      !      DEF_VARS(2*BUFF_SIZE);
!  221		      !      t_d64 d0, d1, db;
!  222		      !      t_u32 i, uc = 0;
!  224		      !      da = (t_d64)(a &  A_MASK);

/* 0x1024	 224 */		ldd	[%o0],%f6
/* 0x1028	 217 */		sethi	%hi(0x1800),%g1
/* 0x102c	 224 */		andn	%o1,%g2,%g2
/* 0x1030	     */		st	%g2,[%sp+96]

!  225		      !      db = (t_d64)(a >> A_BITS);

/* 0x1034	 225 */		srl	%o1,19,%g2
/* 0x1038	 217 */		xor	%g1,-624,%g1
/* 0x103c	     */		add	%g1,%fp,%o7
/* 0x1040	 225 */		st	%g2,[%sp+92]
/* 0x1044	 217 */		sethi	%hi(0x1800),%g1
/* 0x1048	 224 */		fmovs	%f6,%f8
/* 0x104c	 217 */		xor	%g1,-616,%g1

!  227		      !#pragma pipeloop(0)
!  228		      !      for (i = 0; i < (n+1)/2; i ++) {

/* 0x1050	 228 */		add	%o2,1,%g3
/* 0x1054	 220 */		ldd	[%o0+8],%f14
/* 0x1058	 225 */		fmovs	%f6,%f10
/* 0x105c	 228 */		srl	%g3,31,%g2
/* 0x1060	 217 */		add	%g1,%fp,%o5
/* 0x1064	     */		sethi	%hi(0x1800),%g1
/* 0x1068	 228 */		add	%g3,%g2,%g2
/* 0x106c	     */		sra	%g2,1,%o1
/* 0x1070	 224 */		ld	[%sp+96],%f9
/* 0x1074	 217 */		xor	%g1,-608,%g1
/* 0x1078	     */		add	%g1,%fp,%o4
/* 0x107c	     */		sethi	%hi(0x1800),%g1
/* 0x1080	 225 */		ld	[%sp+92],%f11
/* 0x1084	 228 */		cmp	%o1,0
/* 0x1088	 224 */		fsubd	%f8,%f6,%f4
/* 0x108c	 222 */		or	%g0,0,%o2
/* 0x1090	 217 */		or	%g0,%i2,%g3
/* 0x1094	     */		xor	%g1,-600,%g1
/* 0x1098	 225 */		fsubd	%f10,%f6,%f2
/* 0x109c	 228 */		bleu,pt	%icc,.L900000157
/* 0x10a0	     */		cmp	%o3,0
/* 0x10a4	     */		ldd	[%g3],%f6
/* 0x10a8	 217 */		sub	%o1,1,%o0
/* 0x10ac	 228 */		or	%g0,0,%g2
/* 0x10b0	 217 */		add	%g1,%fp,%o1
                       .L900000156:
/* 0x10b4	     */		fxnor	%f0,%f6,%f8

!  229		      !        MUL_U32_S64_2_D(i);

/* 0x10b8	 229 */		add	%g2,1,%g2
/* 0x10bc	     */		add	%g3,8,%g3
/* 0x10c0	     */		cmp	%g2,%o0
/* 0x10c4	     */		fitod	%f8,%f6
/* 0x10c8	     */		fitod	%f9,%f8
/* 0x10cc	     */		fsubd	%f14,%f6,%f6
/* 0x10d0	     */		fsubd	%f14,%f8,%f8
/* 0x10d4	     */		fmuld	%f6,%f4,%f10
/* 0x10d8	     */		fmuld	%f8,%f4,%f12
/* 0x10dc	     */		fmuld	%f6,%f2,%f6
/* 0x10e0	     */		fdtox	%f10,%f10
/* 0x10e4	     */		std	%f10,[%o7]
/* 0x10e8	     */		fmuld	%f8,%f2,%f8
/* 0x10ec	     */		add	%o7,32,%o7
/* 0x10f0	     */		fdtox	%f6,%f6
/* 0x10f4	     */		std	%f6,[%o5]
/* 0x10f8	     */		add	%o5,32,%o5
/* 0x10fc	     */		fdtox	%f12,%f6
/* 0x1100	     */		std	%f6,[%o4]
/* 0x1104	     */		add	%o4,32,%o4
/* 0x1108	     */		fdtox	%f8,%f6
/* 0x110c	     */		std	%f6,[%o1]
/* 0x1110	     */		add	%o1,32,%o1
/* 0x1114	     */		bleu,a,pt	%icc,.L900000156
/* 0x1118	     */		ldd	[%g3],%f6
                       .L77000056:

!  230		      !      }
!  232		      !#pragma pipeloop(0)
!  233		      !      for (i = 0; i < n; i ++) {

/* 0x111c	 233 */		cmp	%o3,0
                       .L900000157:
/* 0x1120	 233 */		sethi	%hi(0x1800),%g1
/* 0x1124	     */		sub	%o3,1,%o0
/* 0x1128	     */		bleu,pt	%icc,.L77000061
/* 0x112c	     */		or	%g0,%o2,%o5
/* 0x1130	     */		xor	%g1,-624,%g1
/* 0x1134	     */		add	%g1,%fp,%o3
/* 0x1138	     */		sethi	%hi(0x1800),%g1
/* 0x113c	     */		xor	%g1,-616,%g1
/* 0x1140	     */		or	%g0,0,%o5
/* 0x1144	     */		add	%g1,%fp,%o1
/* 0x1148	     */		or	%g0,%i1,%o7

!  234		      !        ADD_S64_U32_D(i);

/* 0x114c	 234 */		ldx	[%o1],%g2
/* 0x1150	 233 */		or	%g0,%i0,%o4
                       .L900000155:
/* 0x1154	 234 */		sllx	%g2,19,%g2
/* 0x1158	     */		ldx	[%o3],%g4
/* 0x115c	     */		add	%o5,1,%o5
/* 0x1160	     */		srl	%o2,0,%g3
/* 0x1164	     */		ld	[%o7],%g5
/* 0x1168	     */		add	%o1,16,%o1
/* 0x116c	     */		add	%g4,%g2,%g4
/* 0x1170	     */		add	%o3,16,%o3
/* 0x1174	     */		add	%g4,%g5,%g2
/* 0x1178	     */		add	%o7,4,%o7
/* 0x117c	     */		add	%g2,%g3,%g2
/* 0x1180	     */		st	%g2,[%o4]
/* 0x1184	     */		cmp	%o5,%o0
/* 0x1188	     */		srlx	%g2,32,%o2
/* 0x118c	     */		add	%o4,4,%o4
/* 0x1190	     */		bleu,a,pt	%icc,.L900000155
/* 0x1194	     */		ldx	[%o1],%g2
                       .L77000060:

!  235		      !      }
!  237		      !      return uc;

/* 0x1198	 237 */		or	%g0,%o2,%o5
                       .L77000061:
/* 0x119c	     */		ret	! Result =  %i0
/* 0x11a0	     */		restore	%g0,%o5,%o0
/* 0x11a4	   0 */		.type	mul_add,2
/* 0x11a4	     */		.size	mul_add,(.-mul_add)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	4
!
! SUBROUTINE mul_add_inp
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global mul_add_inp
                       mul_add_inp:
/* 000000	     */		or	%g0,%o2,%g1
/* 0x0004	     */		or	%g0,%o3,%o4
/* 0x0008	     */		or	%g0,%o0,%g3
/* 0x000c	     */		or	%g0,%o1,%g2

!  238		      !    }
!  239		      !  }
!  240		      !}
!  242		      !/***************************************************************/
!  244		      !t_u32 mul_add_inp(t_u32 *x, t_u32 *y, int n, t_u32 a)
!  245		      !{
!  246		      !  return mul_add(x, x, y, n, a);

/* 0x0010	 246 */		or	%g0,%g1,%o3
/* 0x0014	     */		or	%g0,%g3,%o1
/* 0x0018	     */		or	%g0,%g2,%o2
/* 0x001c	     */		or	%g0,%o7,%g1
/* 0x0020	     */		call	mul_add	! params =  %o3 %o4 %o0 %o1 %o2	! Result = 	! (tail call)
/* 0x0024	     */		or	%g0,%g1,%o7
/* 0x0028	   0 */		.type	mul_add_inp,2
/* 0x0028	     */		.size	mul_add_inp,(.-mul_add_inp)

! Begin Disassembling Stabs
	.xstabs	".stab.index","Xa ; O ; P ; V=3.1 ; R=WorkShop Compilers 5.0 98/12/15 C 5.0",60,0,0,0	! (/usr/tmp/acompAAAXjay1M:1)
	.xstabs	".stab.index","/h/interzone/d3/nelsonb/nss_tip/mozilla/security/nss/lib/freebl/mpi32; /tools/ns/workshop-5.0/bin/../SC5.0/bin/cc -fast -xO5 -xrestrict=%%all -xdepend -xchip=ultra -xarch=v8plusa -KPIC -mt -S vis_32.il  mpv_sparc.c -W0,-xp",52,0,0,0	! (/usr/tmp/acompAAAXjay1M:2)
! End Disassembling Stabs

! Begin Disassembling Ident
	.ident	"cg: WorkShop Compilers 5.0 99/08/12 Compiler Common 5.0 Patch 107357-05"	! (NO SOURCE LINE)
	.ident	"@(#)vis_proto.h\t1.3\t97/03/30 SMI"	! (/usr/tmp/acompAAAXjay1M:4)
	.ident	"acomp: WorkShop Compilers 5.0 98/12/15 C 5.0"	! (/usr/tmp/acompAAAXjay1M:12)
! End Disassembling Ident
