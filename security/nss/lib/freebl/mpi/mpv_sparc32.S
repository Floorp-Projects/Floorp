/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is an asm version of SPARC/VIS multiply and add function
 *
 * The Initial Developer of the Original Code is Sun Microsystems Inc.
 * Portions created by Sun Microsystems Inc. are 
 * Copyright (C) 1999-2000 Sun Microsystems Inc.  All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *  $Id: mpv_sparc32.S,v 1.1 2000/09/29 23:38:02 nelsonb%netscape.com Exp $
 */

	.section	".text",#alloc,#execinstr
	.file	"mul_add.c"

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
/* 0x0010	     */		.word	-1008730112,0
/* 0x0018	   0 */		.align	4
!
! SUBROUTINE mul_add
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global mul_add
                       mul_add:
/* 000000	     */		sethi	%hi(0x2c00),%g1
/* 0x0004	     */		sethi	%hi(mask_cnst),%g2
/* 0x0008	     */		xor	%g1,-584,%g1
/* 0x000c	     */		add	%g2,%lo(mask_cnst),%g2
/* 0x0010	     */		save	%sp,%g1,%sp
                       .L900000190:
/* 0x0014	     */		call	.+8
/* 0x0018	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000190-.)),%g5
! FILE mul_add.c

!    1		      !/* Copyright (C) 1999, Sun Microsystems, Inc. */
!    3		      !#include "vis_proto.h"
!    5		      !/***************************************************************/
!    7		      !typedef  int                t_s32;
!    8		      !typedef  unsigned int       t_u32;
!    9		      !#if defined(__sparcv9)
!   10		      !typedef  long               t_s64;
!   11		      !typedef  unsigned long      t_u64;
!   12		      !#else
!   13		      !typedef  long long          t_s64;
!   14		      !typedef  unsigned long long t_u64;
!   15		      !#endif
!   16		      !typedef  double             t_d64;
!   18		      !/***************************************************************/
!   20		      !typedef union {
!   21		      !  t_d64 d64;
!   22		      !  struct {
!   23		      !    t_s32 i0;
!   24		      !    t_s32 i1;
!   25		      !  } i32s;
!   26		      !} d64_2_i32;
!   28		      !/***************************************************************/
!   30		      !#define BUFF_SIZE  256
!   32		      !#define A_BITS  21
!   33		      !#define A_MASK  ((1 << A_BITS) - 1)
!   35		      !/***************************************************************/
!   37		      !static t_u64 mask_cnst[] = {
!   38		      !  0x8000000080000000ull
!   39		      !};
!   41		      !/***************************************************************/
!   43		      !#define DEF_VARS(N)                     \
!   44		      !  t_d64 *py = (t_d64*)y;                \
!   45		      !  t_d64 mask = *((t_d64*)mask_cnst);    \
!   46		      !  t_d64 ca = (1u << 31) - 1;            \
!   47		      !  t_d64 da = (t_d64)a;                  \
!   48		      !  t_s64 buff[N], s;                     \
!   49		      !  d64_2_i32 dy
!   51		      !/***************************************************************/
!   53		      !#define MUL_U32_S64_1(i)                                        \
!   54		      !  dy.f32s.i0 = vis_fxnors(vis_read_hi(mask), ((t_f32*)y)[0]);   \
!   55		      !  buff[0] = (ca - (t_d64)dy.i32s.i0) * da
!   57		      !#define MUL_U32_S64_1_D()                                       \
!   58		      !  dy.f32s.i0 = vis_fxnors(vis_read_hi(mask), ((t_f32*)y)[0]);   \
!   59		      !  d0 = ca - (t_d64)dy.i32s.i0;                                  \
!   60		      !  buff[0] = (t_s64)(d0 * da);                                   \
!   61		      !  buff[1] = (t_s64)(d0 * db);                                   \
!   63		      !#define MUL_U32_S64_1_E(i)                                      \
!   64		      !  dy.f32s.i0 = vis_fxnors(vis_read_hi(mask), ((t_f32*)y)[0]);   \
!   65		      !  d0 = ca - (t_d64)dy.i32s.i0;                                  \
!   66		      !  buff[0] = (t_s64)(d0 * da);                                   \
!   67		      !  buff[1] = (t_s64)(d0 * db - (1ULL << 63))
!   69		      !/************ ***************************************************/
!   71		      !#define MUL_U32_S64_2(i)                                \
!   72		      !  dy.d64 = vis_fxnor(mask, py[i]);                      \
!   73		      !  buff[2*(i)  ] = (ca - (t_d64)dy.i32s.i0) * da;        \
!   74		      !  buff[2*(i)+1] = (ca - (t_d64)dy.i32s.i1) * da
!   76		      !#define MUL_U32_S64_2_D(i)              \
!   77		      !  dy.d64 = vis_fxnor(mask, py[i]);      \
!   78		      !  d0 = ca - (t_d64)dy.i32s.i0;          \
!   79		      !  d1 = ca - (t_d64)dy.i32s.i1;          \
!   80		      !  buff[4*(i)  ] = (t_s64)(d0 * da);     \
!   81		      !  buff[4*(i)+1] = (t_s64)(d0 * db);     \
!   82		      !  buff[4*(i)+2] = (t_s64)(d1 * da);     \
!   83		      !  buff[4*(i)+3] = (t_s64)(d1 * db)
!   85		      !#define MUL_U32_S64_2_E(i)                              \
!   86		      !  dy.d64 = vis_fxnor(mask, py[i]);                      \
!   87		      !  d0 = ca - (t_d64)dy.i32s.i0;                          \
!   88		      !  d1 = ca - (t_d64)dy.i32s.i1;                          \
!   89		      !  buff[4*(i)  ] = (t_s64)(d0 * da);                     \
!   90		      !  buff[4*(i)+1] = (t_s64)(d0 * db - (1ULL << 63));      \
!   91		      !  buff[4*(i)+2] = (t_s64)(d1 * da);                     \
!   92		      !  buff[4*(i)+3] = (t_s64)(d1 * db - (1ULL << 63))
!   94		      !/***************************************************************/
!   96		      !#define ADD_S64_U32(i)          \
!   97		      !  s = buff[i] + x[i] + c;       \
!   98		      !  z[i] = s;                     \
!   99		      !  c = (s >> 32)
!  101		      !#define ADD_S64_U32_D(i)                        \
!  102		      !  s = buff[2*(i)] + buff[2*(i)+1] + x[i] + c;   \
!  103		      !  z[i] = s;                                     \
!  104		      !  c = (s >> 32)
!  106		      !#define ADD_S64_U32_E(i)                                        \
!  107		      !  s =  buff[2*(i)] + buff[2*(i)+1] + x[i] + uc + (1ULL << 63);  \
!  108		      !  z[i] = s;                                                     \
!  109		      !  uc = ((t_u64)s >> 32)
!  111		      !/***************************************************************/
!  113		      !#define MUL_U32_S64_8(i)        \
!  114		      !  MUL_U32_S64_2(i);             \
!  115		      !  MUL_U32_S64_2(i+1);           \
!  116		      !  MUL_U32_S64_2(i+2);           \
!  117		      !  MUL_U32_S64_2(i+3)
!  119		      !#define MUL_U32_S64_D_8(i)      \
!  120		      !  MUL_U32_S64_2_D(i);           \
!  121		      !  MUL_U32_S64_2_D(i+1);         \
!  122		      !  MUL_U32_S64_2_D(i+2);         \
!  123		      !  MUL_U32_S64_2_D(i+3)
!  125		      !#define MUL_U32_S64_E_8(i)      \
!  126		      !  MUL_U32_S64_2_E(i);           \
!  127		      !  MUL_U32_S64_2_E(i+1);         \
!  128		      !  MUL_U32_S64_2_E(i+2);         \
!  129		      !  MUL_U32_S64_2_E(i+3)
!  131		      !/***************************************************************/
!  133		      !#define ADD_S64_U32_8(i)        \
!  134		      !  ADD_S64_U32(i);               \
!  135		      !  ADD_S64_U32(i+1);             \
!  136		      !  ADD_S64_U32(i+2);             \
!  137		      !  ADD_S64_U32(i+3);             \
!  138		      !  ADD_S64_U32(i+4);             \
!  139		      !  ADD_S64_U32(i+5);             \
!  140		      !  ADD_S64_U32(i+6);             \
!  141		      !  ADD_S64_U32(i+7)
!  143		      !#define ADD_S64_U32_D_8(i)      \
!  144		      !  ADD_S64_U32_D(i);             \
!  145		      !  ADD_S64_U32_D(i+1);           \
!  146		      !  ADD_S64_U32_D(i+2);           \
!  147		      !  ADD_S64_U32_D(i+3);           \
!  148		      !  ADD_S64_U32_D(i+4);           \
!  149		      !  ADD_S64_U32_D(i+5);           \
!  150		      !  ADD_S64_U32_D(i+6);           \
!  151		      !  ADD_S64_U32_D(i+7)
!  153		      !#define ADD_S64_U32_E_8(i)      \
!  154		      !  ADD_S64_U32_E(i);             \
!  155		      !  ADD_S64_U32_E(i+1);           \
!  156		      !  ADD_S64_U32_E(i+2);           \
!  157		      !  ADD_S64_U32_E(i+3);           \
!  158		      !  ADD_S64_U32_E(i+4);           \
!  159		      !  ADD_S64_U32_E(i+5);           \
!  160		      !  ADD_S64_U32_E(i+6);           \
!  161		      !  ADD_S64_U32_E(i+7)
!  163		      !/***************************************************************/
!  165		      !t_u32 mul_add(t_u32 *z, t_u32 *x, t_u32 *y, int n, t_u32 a)
!  166		      !{
!  167		      !  if (a < (1 << A_BITS)) {
!  169		      !    if (n == 8) {
!  170		      !      DEF_VARS(8);
!  171		      !      t_s32 c = 0;
!  173		      !      MUL_U32_S64_8(0);

/* 0x001c	 173 */		sethi	%hi(.L_const_seg_900000106),%g3
/* 0x0020	 166 */		add	%g5,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000190-.)),%g5
/* 0x0024	     */		or	%g0,%i4,%o0
/* 0x0028	     */		st	%o0,[%fp+84]
/* 0x002c	     */		add	%g5,%o7,%o1
/* 0x0030	     */		or	%g0,%i3,%o2
/* 0x0034	     */		ld	[%o1+%g2],%g2
/* 0x0038	 173 */		add	%g3,%lo(.L_const_seg_900000106),%g3
/* 0x003c	 167 */		sethi	%hi(0x200000),%g4
/* 0x0040	 173 */		ld	[%o1+%g3],%o1
/* 0x0044	 166 */		or	%g0,%i0,%i4
/* 0x0048	     */		or	%g0,%i2,%o7
/* 0x004c	     */		ldd	[%g2],%f30
/* 0x0050	     */		or	%g0,%i1,%o4
/* 0x0054	 167 */		cmp	%o0,%g4
/* 0x0058	 166 */		or	%g0,%o2,%o3
/* 0x005c	 167 */		bcc,pn	%icc,.L77000062
/* 0x0060	     */		or	%g0,%i4,%o5
/* 0x0064	 169 */		cmp	%o2,8
/* 0x0068	     */		bne,pn	%icc,.L77000051
/* 0x006c	     */		ld	[%fp+84],%f7
/* 0x0070	     */		ldd	[%o7],%f4
/* 0x0074	 170 */		ldd	[%o1],%f8
/* 0x0078	     */		fxnor	%f30,%f4,%f4
/* 0x007c	 173 */		ldd	[%o1+8],%f14
/* 0x0080	     */		ldd	[%o7+8],%f10
/* 0x0084	     */		fitod	%f4,%f12
/* 0x0088	     */		ldd	[%o7+16],%f16
/* 0x008c	     */		fitod	%f5,%f4
/* 0x0090	     */		ldd	[%o7+24],%f18
/* 0x0094	     */		fxnor	%f30,%f10,%f10

!  174		      !      ADD_S64_U32_8(0);

/* 0x0098	 174 */		ld	[%i1],%g2
/* 0x009c	     */		ld	[%i1+4],%g3
/* 0x00a0	     */		fxnor	%f30,%f16,%f16
/* 0x00a4	 173 */		fsubd	%f14,%f4,%f4
/* 0x00a8	 174 */		ld	[%i1+8],%g4
/* 0x00ac	     */		ld	[%i1+16],%o0
/* 0x00b0	 173 */		fitod	%f16,%f20
/* 0x00b4	 174 */		ld	[%i1+12],%g5
/* 0x00b8	     */		ld	[%i1+20],%o1
/* 0x00bc	     */		ld	[%i1+24],%o2
/* 0x00c0	 170 */		fmovs	%f8,%f6
/* 0x00c4	 174 */		ld	[%i1+28],%o3
/* 0x00c8	 170 */		fsubd	%f6,%f8,%f6
/* 0x00cc	 173 */		fsubd	%f14,%f12,%f8
/* 0x00d0	     */		fitod	%f10,%f12
/* 0x00d4	     */		fmuld	%f4,%f6,%f4
/* 0x00d8	     */		fitod	%f11,%f10
/* 0x00dc	     */		fmuld	%f8,%f6,%f8
/* 0x00e0	     */		fsubd	%f14,%f12,%f12
/* 0x00e4	     */		fdtox	%f4,%f4
/* 0x00e8	     */		std	%f4,[%sp+360]
/* 0x00ec	     */		fdtox	%f8,%f8
/* 0x00f0	     */		std	%f8,[%sp+368]
/* 0x00f4	     */		fmuld	%f12,%f6,%f12
/* 0x00f8	     */		fsubd	%f14,%f10,%f10
/* 0x00fc	     */		fsubd	%f14,%f20,%f4
/* 0x0100	     */		fitod	%f17,%f8
/* 0x0104	 174 */		ldx	[%sp+368],%o4
/* 0x0108	     */		fxnor	%f30,%f18,%f16
/* 0x010c	 173 */		fmuld	%f10,%f6,%f10
/* 0x0110	     */		fdtox	%f12,%f12
/* 0x0114	     */		std	%f12,[%sp+352]
/* 0x0118	     */		fmuld	%f4,%f6,%f4
/* 0x011c	     */		fitod	%f16,%f18
/* 0x0120	 174 */		add	%o4,%g2,%g2
/* 0x0124	     */		st	%g2,[%i4]
/* 0x0128	     */		ldx	[%sp+360],%o4
/* 0x012c	 173 */		fsubd	%f14,%f8,%f8
/* 0x0130	 174 */		srax	%g2,32,%o5
/* 0x0134	 173 */		fdtox	%f10,%f10
/* 0x0138	     */		std	%f10,[%sp+344]
/* 0x013c	     */		fdtox	%f4,%f4
/* 0x0140	     */		std	%f4,[%sp+336]
/* 0x0144	 174 */		add	%o4,%g3,%o4
/* 0x0148	 173 */		fitod	%f17,%f12
/* 0x014c	 174 */		ldx	[%sp+352],%g2
/* 0x0150	     */		add	%o4,%o5,%g3
/* 0x0154	 173 */		fmuld	%f8,%f6,%f8
/* 0x0158	     */		fsubd	%f14,%f18,%f10
/* 0x015c	 174 */		st	%g3,[%i4+4]
/* 0x0160	     */		srax	%g3,32,%g3
/* 0x0164	     */		add	%g2,%g4,%g4
/* 0x0168	     */		ldx	[%sp+344],%g2
/* 0x016c	 173 */		fsubd	%f14,%f12,%f4
/* 0x0170	 174 */		add	%g4,%g3,%g3
/* 0x0174	     */		ldx	[%sp+336],%g4
/* 0x0178	 173 */		fmuld	%f10,%f6,%f10
/* 0x017c	     */		fdtox	%f8,%f8
/* 0x0180	     */		std	%f8,[%sp+328]
/* 0x0184	 174 */		add	%g4,%o0,%g4
/* 0x0188	     */		add	%g2,%g5,%g2
/* 0x018c	     */		st	%g3,[%i4+8]
/* 0x0190	 173 */		fmuld	%f4,%f6,%f4
/* 0x0194	 174 */		srax	%g3,32,%o0
/* 0x0198	     */		ldx	[%sp+328],%g5
/* 0x019c	 173 */		fdtox	%f10,%f6
/* 0x01a0	     */		std	%f6,[%sp+320]
/* 0x01a4	 174 */		add	%g2,%o0,%g2
/* 0x01a8	     */		srax	%g2,32,%g3
/* 0x01ac	     */		st	%g2,[%i4+12]
/* 0x01b0	     */		add	%g5,%o1,%o1
/* 0x01b4	 173 */		fdtox	%f4,%f4
/* 0x01b8	     */		std	%f4,[%sp+312]
/* 0x01bc	 174 */		add	%g4,%g3,%g3
/* 0x01c0	     */		srax	%g3,32,%g4
/* 0x01c4	     */		st	%g3,[%i4+16]
/* 0x01c8	     */		ldx	[%sp+320],%o0
/* 0x01cc	     */		add	%o1,%g4,%g4
/* 0x01d0	     */		ldx	[%sp+312],%g5
/* 0x01d4	     */		srax	%g4,32,%g2
/* 0x01d8	     */		add	%o0,%o2,%o2
/* 0x01dc	     */		st	%g4,[%i4+20]
/* 0x01e0	     */		add	%o2,%g2,%g2
/* 0x01e4	     */		add	%g5,%o3,%g5
/* 0x01e8	     */		st	%g2,[%i4+24]
/* 0x01ec	     */		srax	%g2,32,%g3
/* 0x01f0	     */		add	%g5,%g3,%g2
/* 0x01f4	     */		st	%g2,[%i4+28]

!  176		      !      return c;

/* 0x01f8	 176 */		srax	%g2,32,%i0
/* 0x01fc	     */		ret	! Result =  %i0
/* 0x0200	     */		restore	%g0,%g0,%g0
                       .L77000051:

!  178		      !    } else if (n == 16) {

/* 0x0204	 178 */		cmp	%o2,16
/* 0x0208	     */		bne,pn	%icc,.L77000112
/* 0x020c	     */		ld	[%fp+84],%f7
/* 0x0210	     */		ldd	[%o7],%f4

!  179		      !      DEF_VARS(16);

/* 0x0214	 179 */		ldd	[%o1],%f8

!  180		      !      t_s32 c = 0;
!  182		      !      MUL_U32_S64_8(0);

/* 0x0218	 182 */		ldd	[%o1+8],%f14
/* 0x021c	     */		fxnor	%f30,%f4,%f4
/* 0x0220	     */		ldd	[%o7+8],%f10
/* 0x0224	     */		ldd	[%o7+16],%f16
/* 0x0228	     */		fitod	%f4,%f12
/* 0x022c	     */		ldd	[%o7+24],%f18
/* 0x0230	     */		fitod	%f5,%f4
/* 0x0234	     */		ldd	[%o7+32],%f20
/* 0x0238	     */		fxnor	%f30,%f10,%f10
/* 0x023c	     */		ldd	[%o7+40],%f22
/* 0x0240	     */		fxnor	%f30,%f16,%f16
/* 0x0244	     */		ldd	[%o7+48],%f0
/* 0x0248	     */		fitod	%f10,%f24
/* 0x024c	     */		ldd	[%o7+56],%f26
/* 0x0250	     */		fsubd	%f14,%f4,%f4

!  183		      !      MUL_U32_S64_8(4);
!  184		      !      ADD_S64_U32_8(0);
!  185		      !      ADD_S64_U32_8(8);

/* 0x0254	 185 */		ld	[%i1+36],%o5
/* 0x0258	 182 */		fitod	%f11,%f10
/* 0x025c	 184 */		ld	[%i1],%g2
/* 0x0260	 182 */		fsubd	%f14,%f24,%f24
/* 0x0264	 185 */		stx	%o5,[%sp+312]
/* 0x0268	 182 */		fitod	%f16,%f28
/* 0x026c	 184 */		ld	[%i1+4],%g3
/* 0x0270	 182 */		fsubd	%f14,%f10,%f10
/* 0x0274	 185 */		ld	[%i1+40],%o5
/* 0x0278	     */		fxnor	%f30,%f20,%f20
/* 0x027c	 184 */		ld	[%i1+8],%g4
/* 0x0280	     */		stx	%o5,[%sp+320]
/* 0x0284	     */		ld	[%i1+12],%g5
/* 0x0288	     */		ld	[%i1+16],%o0
/* 0x028c	 179 */		fmovs	%f8,%f6
/* 0x0290	 184 */		ld	[%i1+20],%o1
/* 0x0294	 185 */		ld	[%i1+32],%o4
/* 0x0298	 179 */		fsubd	%f6,%f8,%f6
/* 0x029c	 184 */		ld	[%i1+24],%o2
/* 0x02a0	 182 */		fsubd	%f14,%f12,%f8
/* 0x02a4	 185 */		stx	%o4,[%sp+328]
/* 0x02a8	     */		fxnor	%f30,%f0,%f12
/* 0x02ac	 184 */		ld	[%i1+28],%o3
/* 0x02b0	 182 */		fmuld	%f4,%f6,%f4
/* 0x02b4	 185 */		ld	[%i1+44],%o4
/* 0x02b8	 182 */		fmuld	%f8,%f6,%f8
/* 0x02bc	     */		fmuld	%f24,%f6,%f24
/* 0x02c0	 185 */		stx	%o4,[%sp+360]
/* 0x02c4	 182 */		fdtox	%f4,%f4
/* 0x02c8	     */		fmuld	%f10,%f6,%f10
/* 0x02cc	     */		std	%f4,[%sp+496]
/* 0x02d0	     */		fdtox	%f8,%f8
/* 0x02d4	     */		std	%f8,[%sp+504]
/* 0x02d8	     */		fitod	%f17,%f8
/* 0x02dc	 185 */		ld	[%i1+60],%o4
/* 0x02e0	     */		fxnor	%f30,%f18,%f16
/* 0x02e4	 182 */		fdtox	%f24,%f24
/* 0x02e8	     */		std	%f24,[%sp+488]
/* 0x02ec	 184 */		ldx	[%sp+504],%o7
/* 0x02f0	 182 */		fsubd	%f14,%f28,%f4
/* 0x02f4	     */		fitod	%f16,%f18
/* 0x02f8	     */		fsubd	%f14,%f8,%f8
/* 0x02fc	 184 */		add	%o7,%g2,%g2
/* 0x0300	     */		st	%g2,[%i4]
/* 0x0304	     */		ldx	[%sp+496],%o7
/* 0x0308	 182 */		fitod	%f17,%f16
/* 0x030c	 184 */		srax	%g2,32,%o5
/* 0x0310	 182 */		fmuld	%f4,%f6,%f4
/* 0x0314	     */		fsubd	%f14,%f18,%f18
/* 0x0318	     */		fdtox	%f10,%f10
/* 0x031c	     */		std	%f10,[%sp+480]
/* 0x0320	 184 */		add	%o7,%g3,%g3
/* 0x0324	 182 */		fmuld	%f8,%f6,%f24
/* 0x0328	 183 */		fitod	%f20,%f8
/* 0x032c	 184 */		add	%g3,%o5,%g3
/* 0x0330	 185 */		ld	[%i1+48],%o5
/* 0x0334	 184 */		st	%g3,[%i4+4]
/* 0x0338	 182 */		fdtox	%f4,%f4
/* 0x033c	     */		fmuld	%f18,%f6,%f10
/* 0x0340	 184 */		ldx	[%sp+488],%o7
/* 0x0344	     */		fxnor	%f30,%f22,%f18
/* 0x0348	 182 */		std	%f4,[%sp+472]
/* 0x034c	     */		fsubd	%f14,%f16,%f4
/* 0x0350	 184 */		stx	%o5,[%sp+344]
/* 0x0354	 183 */		fitod	%f21,%f16
/* 0x0358	 184 */		srax	%g3,32,%o5
/* 0x035c	 183 */		fsubd	%f14,%f8,%f8
/* 0x0360	 184 */		add	%o7,%g4,%g4
/* 0x0364	 185 */		ld	[%i1+52],%o7
/* 0x0368	 182 */		fdtox	%f24,%f20
/* 0x036c	     */		std	%f20,[%sp+464]
/* 0x0370	 184 */		add	%g4,%o5,%g4
/* 0x0374	 182 */		fmuld	%f4,%f6,%f4
/* 0x0378	 184 */		stx	%o7,[%sp+336]
/* 0x037c	 182 */		fdtox	%f10,%f10
/* 0x0380	 184 */		ldx	[%sp+480],%o7
/* 0x0384	 183 */		fitod	%f18,%f20
/* 0x0388	     */		fmuld	%f8,%f6,%f8
/* 0x038c	 182 */		std	%f10,[%sp+456]
/* 0x0390	 183 */		fsubd	%f14,%f16,%f10
/* 0x0394	 185 */		ld	[%i1+56],%o5
/* 0x0398	 183 */		fitod	%f19,%f16
/* 0x039c	 184 */		add	%o7,%g5,%g5
/* 0x03a0	     */		ldx	[%sp+472],%o7
/* 0x03a4	 182 */		fdtox	%f4,%f4
/* 0x03a8	 184 */		stx	%o5,[%sp+352]
/* 0x03ac	     */		srax	%g4,32,%o5
/* 0x03b0	 183 */		fdtox	%f8,%f8
/* 0x03b4	 182 */		std	%f4,[%sp+448]
/* 0x03b8	 183 */		fitod	%f12,%f18
/* 0x03bc	 184 */		add	%g5,%o5,%g5
/* 0x03c0	 183 */		fmuld	%f10,%f6,%f4
/* 0x03c4	 184 */		ldx	[%sp+464],%o5
/* 0x03c8	 183 */		fsubd	%f14,%f20,%f10
/* 0x03cc	 184 */		add	%o7,%o0,%o0
/* 0x03d0	 183 */		std	%f8,[%sp+440]
/* 0x03d4	 184 */		srax	%g5,32,%o7
/* 0x03d8	 183 */		fsubd	%f14,%f16,%f8
/* 0x03dc	 184 */		ldx	[%sp+456],%g2
/* 0x03e0	 183 */		fdtox	%f4,%f4
/* 0x03e4	 184 */		add	%o0,%o7,%o0
/* 0x03e8	 183 */		std	%f4,[%sp+432]
/* 0x03ec	     */		fxnor	%f30,%f26,%f16
/* 0x03f0	 184 */		add	%o5,%o1,%o1
/* 0x03f4	 183 */		fmuld	%f10,%f6,%f10
/* 0x03f8	 184 */		ldx	[%sp+448],%o7
/* 0x03fc	     */		srax	%o0,32,%o5
/* 0x0400	 183 */		fitod	%f13,%f12
/* 0x0404	     */		fmuld	%f8,%f6,%f4
/* 0x0408	 184 */		st	%g4,[%i4+8]
/* 0x040c	 183 */		fsubd	%f14,%f18,%f8
/* 0x0410	 184 */		add	%o1,%o5,%g3
/* 0x0414	 185 */		ldx	[%sp+440],%o1
/* 0x0418	 183 */		fitod	%f16,%f18
/* 0x041c	 184 */		add	%g2,%o2,%g2
/* 0x0420	     */		st	%g5,[%i4+12]
/* 0x0424	     */		srax	%g3,32,%o2
/* 0x0428	 183 */		fdtox	%f10,%f10
/* 0x042c	     */		std	%f10,[%sp+424]
/* 0x0430	     */		fsubd	%f14,%f12,%f10
/* 0x0434	 184 */		add	%g2,%o2,%o2
/* 0x0438	 183 */		fmuld	%f8,%f6,%f8
/* 0x043c	 185 */		ldx	[%sp+328],%g2
/* 0x0440	 183 */		fdtox	%f4,%f4
/* 0x0444	 184 */		add	%o7,%o3,%g4
/* 0x0448	 183 */		std	%f4,[%sp+416]
/* 0x044c	 184 */		srax	%o2,32,%o3
/* 0x0450	 183 */		fitod	%f17,%f12
/* 0x0454	 185 */		ldx	[%sp+432],%g5
/* 0x0458	 184 */		add	%g4,%o3,%o3
/* 0x045c	 185 */		add	%o1,%g2,%o1
/* 0x0460	 183 */		fsubd	%f14,%f18,%f4
/* 0x0464	 185 */		ldx	[%sp+312],%o5
/* 0x0468	     */		srax	%o3,32,%g4
/* 0x046c	 183 */		fdtox	%f8,%f8
/* 0x0470	     */		fmuld	%f10,%f6,%f10
/* 0x0474	     */		std	%f8,[%sp+408]
/* 0x0478	 185 */		add	%o1,%g4,%o1
/* 0x047c	 183 */		fsubd	%f14,%f12,%f8
/* 0x0480	 185 */		ldx	[%sp+424],%o7
/* 0x0484	     */		add	%g5,%o5,%g2
/* 0x0488	 183 */		fmuld	%f4,%f6,%f4
/* 0x048c	 185 */		ldx	[%sp+320],%g4
/* 0x0490	     */		srax	%o1,32,%g5
/* 0x0494	 183 */		fdtox	%f10,%f10
/* 0x0498	 184 */		st	%g3,[%i4+20]
/* 0x049c	 185 */		add	%g2,%g5,%g2
/* 0x04a0	 183 */		fmuld	%f8,%f6,%f6
/* 0x04a4	 185 */		ldx	[%sp+360],%o5
/* 0x04a8	     */		srax	%g2,32,%g5
/* 0x04ac	     */		add	%o7,%g4,%g4
/* 0x04b0	     */		ldx	[%sp+416],%g3
/* 0x04b4	     */		add	%g4,%g5,%g4
/* 0x04b8	 183 */		fdtox	%f4,%f4
/* 0x04bc	     */		std	%f10,[%sp+400]
/* 0x04c0	 184 */		st	%o0,[%i4+16]
/* 0x04c4	 185 */		add	%g3,%o5,%g3
/* 0x04c8	 183 */		std	%f4,[%sp+392]
/* 0x04cc	 185 */		srax	%g4,32,%o5
/* 0x04d0	 183 */		fdtox	%f6,%f4
/* 0x04d4	 185 */		ldx	[%sp+344],%o7
/* 0x04d8	     */		add	%g3,%o5,%g3
/* 0x04dc	     */		stx	%g3,[%sp+368]
/* 0x04e0	     */		srax	%g3,32,%o5
/* 0x04e4	     */		ldx	[%sp+408],%o0
/* 0x04e8	     */		ldx	[%sp+400],%g5
/* 0x04ec	     */		ldx	[%sp+336],%g3
/* 0x04f0	     */		add	%o0,%o7,%o0
/* 0x04f4	 183 */		std	%f4,[%sp+384]
/* 0x04f8	 185 */		add	%g5,%g3,%g3
/* 0x04fc	     */		add	%o0,%o5,%g5
/* 0x0500	     */		stx	%g5,[%sp+376]
/* 0x0504	     */		ldx	[%sp+392],%o7
/* 0x0508	     */		srax	%g5,32,%o0
/* 0x050c	     */		ldx	[%sp+352],%g5
/* 0x0510	     */		add	%g3,%o0,%g3
/* 0x0514	     */		ldx	[%sp+384],%o5
/* 0x0518	     */		add	%o7,%g5,%g5
/* 0x051c	 184 */		st	%o2,[%i4+24]
/* 0x0520	 185 */		srax	%g3,32,%o2
/* 0x0524	     */		st	%o1,[%i4+32]
/* 0x0528	     */		add	%g5,%o2,%g5
/* 0x052c	     */		add	%o5,%o4,%o0
/* 0x0530	     */		st	%g2,[%i4+36]
/* 0x0534	     */		srax	%g5,32,%g2
/* 0x0538	     */		ldx	[%sp+368],%o1
/* 0x053c	     */		ldx	[%sp+376],%o2
/* 0x0540	     */		st	%g4,[%i4+40]
/* 0x0544	     */		add	%o0,%g2,%g4
/* 0x0548	 184 */		st	%o3,[%i4+28]

!  187		      !      return c;

/* 0x054c	 187 */		srax	%g4,32,%i0
/* 0x0550	 185 */		st	%o1,[%i4+44]
/* 0x0554	     */		st	%o2,[%i4+48]
/* 0x0558	     */		st	%g3,[%i4+52]
/* 0x055c	     */		st	%g5,[%i4+56]
/* 0x0560	     */		st	%g4,[%i4+60]
/* 0x0564	     */		ret	! Result =  %i0
/* 0x0568	     */		restore	%g0,%g0,%g0
                       .L77000112:

!  189		      !    } else {
!  190		      !      DEF_VARS(BUFF_SIZE);

/* 0x056c	 190 */		ldd	[%o1],%f8

!  191		      !      t_s32 i, c = 0;
!  193		      !#pragma pipeloop(0)
!  194		      !      for (i = 0; i < (n+1)/2; i ++) {

/* 0x0570	 194 */		add	%o2,1,%g2
/* 0x0574	 191 */		or	%g0,0,%i0
/* 0x0578	 194 */		srl	%g2,31,%g3
/* 0x057c	 190 */		ldd	[%o1+8],%f18
/* 0x0580	 187 */		or	%g0,%o7,%g4
/* 0x0584	 190 */		fmovs	%f8,%f6
/* 0x0588	 194 */		add	%g2,%g3,%g2
/* 0x058c	 187 */		add	%fp,-2264,%g3
/* 0x0590	 194 */		sra	%g2,1,%o0
/* 0x0594	 187 */		add	%fp,-2256,%o7
/* 0x0598	 190 */		fsubd	%f6,%f8,%f16
/* 0x059c	 194 */		cmp	%o0,0
/* 0x05a0	     */		ble,pt	%icc,.L900000199
/* 0x05a4	     */		cmp	%o3,0
/* 0x05a8	 187 */		sub	%o0,1,%g2
/* 0x05ac	     */		add	%g2,1,%o1
/* 0x05b0	 194 */		or	%g0,0,%o0
/* 0x05b4	     */		cmp	%o1,8
/* 0x05b8	     */		bl,pn	%icc,.L77000113
/* 0x05bc	     */		sub	%g2,2,%o2
/* 0x05c0	     */		ldd	[%g4],%f2

!  195		      !        MUL_U32_S64_2(i);

/* 0x05c4	 195 */		or	%g0,5,%o0
/* 0x05c8	     */		ldd	[%g4+8],%f0
/* 0x05cc	     */		add	%g4,24,%o1
/* 0x05d0	     */		add	%g4,16,%g4
/* 0x05d4	     */		fxnor	%f30,%f2,%f4
/* 0x05d8	     */		ldd	[%g4],%f2
/* 0x05dc	     */		fxnor	%f30,%f0,%f6
/* 0x05e0	     */		ldd	[%g4+8],%f10
/* 0x05e4	     */		add	%g4,16,%o1
/* 0x05e8	     */		fitod	%f5,%f0
/* 0x05ec	     */		ldd	[%g4+16],%f12
/* 0x05f0	     */		fitod	%f4,%f4
/* 0x05f4	     */		fxnor	%f30,%f2,%f8
/* 0x05f8	     */		fitod	%f7,%f2
/* 0x05fc	     */		fsubd	%f18,%f0,%f0
/* 0x0600	     */		fsubd	%f18,%f4,%f4
/* 0x0604	     */		fxnor	%f30,%f10,%f10
                       .L900000177:
/* 0x0608	 195 */		fitod	%f9,%f22
/* 0x060c	     */		add	%o0,3,%o0
/* 0x0610	     */		add	%o7,48,%o7
/* 0x0614	     */		fmuld	%f0,%f16,%f0
/* 0x0618	     */		fmuld	%f4,%f16,%f24
/* 0x061c	     */		cmp	%o0,%o2
/* 0x0620	     */		add	%g3,48,%g3
/* 0x0624	     */		fsubd	%f18,%f2,%f2
/* 0x0628	     */		fitod	%f6,%f4
/* 0x062c	     */		fdtox	%f0,%f0
/* 0x0630	     */		ldd	[%o1+8],%f20
/* 0x0634	     */		fdtox	%f24,%f6
/* 0x0638	     */		fsubd	%f18,%f4,%f4
/* 0x063c	     */		std	%f6,[%g3-48]
/* 0x0640	     */		fxnor	%f30,%f12,%f6
/* 0x0644	     */		std	%f0,[%o7-48]
/* 0x0648	     */		fitod	%f11,%f0
/* 0x064c	     */		fmuld	%f2,%f16,%f2
/* 0x0650	     */		fmuld	%f4,%f16,%f24
/* 0x0654	     */		fsubd	%f18,%f22,%f12
/* 0x0658	     */		fitod	%f8,%f4
/* 0x065c	     */		fdtox	%f2,%f2
/* 0x0660	     */		add	%o1,16,%o1
/* 0x0664	     */		ldd	[%o1],%f22
/* 0x0668	     */		fdtox	%f24,%f8
/* 0x066c	     */		fsubd	%f18,%f4,%f4
/* 0x0670	     */		std	%f8,[%g3-32]
/* 0x0674	     */		fxnor	%f30,%f20,%f8
/* 0x0678	     */		std	%f2,[%o7-32]
/* 0x067c	     */		fitod	%f7,%f2
/* 0x0680	     */		fmuld	%f12,%f16,%f12
/* 0x0684	     */		fmuld	%f4,%f16,%f24
/* 0x0688	     */		fsubd	%f18,%f0,%f0
/* 0x068c	     */		fitod	%f10,%f4
/* 0x0690	     */		fdtox	%f12,%f20
/* 0x0694	     */		add	%o1,8,%o1
/* 0x0698	     */		ldd	[%o1],%f12
/* 0x069c	     */		fdtox	%f24,%f10
/* 0x06a0	     */		fsubd	%f18,%f4,%f4
/* 0x06a4	     */		std	%f10,[%g3-16]
/* 0x06a8	     */		fxnor	%f30,%f22,%f10
/* 0x06ac	     */		ble,pt	%icc,.L900000177
/* 0x06b0	     */		std	%f20,[%o7-16]
                       .L900000180:
/* 0x06b4	 195 */		fitod	%f6,%f6
/* 0x06b8	     */		fmuld	%f4,%f16,%f24
/* 0x06bc	     */		add	%g3,80,%g3
/* 0x06c0	     */		fsubd	%f18,%f2,%f2
/* 0x06c4	     */		fmuld	%f0,%f16,%f22
/* 0x06c8	     */		add	%o7,80,%o7
/* 0x06cc	     */		fitod	%f8,%f26
/* 0x06d0	     */		cmp	%o0,%g2
/* 0x06d4	     */		add	%o1,8,%g4
/* 0x06d8	     */		fsubd	%f18,%f6,%f4
/* 0x06dc	     */		fitod	%f9,%f8
/* 0x06e0	     */		fxnor	%f30,%f12,%f0
/* 0x06e4	     */		fmuld	%f2,%f16,%f12
/* 0x06e8	     */		fitod	%f10,%f6
/* 0x06ec	     */		fmuld	%f4,%f16,%f20
/* 0x06f0	     */		fitod	%f11,%f4
/* 0x06f4	     */		fsubd	%f18,%f26,%f10
/* 0x06f8	     */		fitod	%f0,%f2
/* 0x06fc	     */		fsubd	%f18,%f8,%f8
/* 0x0700	     */		fitod	%f1,%f0
/* 0x0704	     */		fmuld	%f10,%f16,%f10
/* 0x0708	     */		fdtox	%f24,%f24
/* 0x070c	     */		std	%f24,[%g3-80]
/* 0x0710	     */		fsubd	%f18,%f6,%f6
/* 0x0714	     */		fmuld	%f8,%f16,%f8
/* 0x0718	     */		fdtox	%f22,%f22
/* 0x071c	     */		std	%f22,[%o7-80]
/* 0x0720	     */		fsubd	%f18,%f4,%f4
/* 0x0724	     */		fdtox	%f20,%f20
/* 0x0728	     */		std	%f20,[%g3-64]
/* 0x072c	     */		fmuld	%f6,%f16,%f6
/* 0x0730	     */		fsubd	%f18,%f2,%f2
/* 0x0734	     */		fsubd	%f18,%f0,%f0
/* 0x0738	     */		fmuld	%f4,%f16,%f4
/* 0x073c	     */		fdtox	%f12,%f12
/* 0x0740	     */		std	%f12,[%o7-64]
/* 0x0744	     */		fdtox	%f10,%f10
/* 0x0748	     */		std	%f10,[%g3-48]
/* 0x074c	     */		fmuld	%f2,%f16,%f2
/* 0x0750	     */		fdtox	%f8,%f8
/* 0x0754	     */		std	%f8,[%o7-48]
/* 0x0758	     */		fmuld	%f0,%f16,%f0
/* 0x075c	     */		fdtox	%f6,%f6
/* 0x0760	     */		std	%f6,[%g3-32]
/* 0x0764	     */		fdtox	%f4,%f4
/* 0x0768	     */		std	%f4,[%o7-32]
/* 0x076c	     */		fdtox	%f2,%f2
/* 0x0770	     */		std	%f2,[%g3-16]
/* 0x0774	     */		fdtox	%f0,%f0
/* 0x0778	     */		bg,pn	%icc,.L77000057
/* 0x077c	     */		std	%f0,[%o7-16]
                       .L77000113:
/* 0x0780	     */		ldd	[%g4],%f0
                       .L900000198:
/* 0x0784	     */		fxnor	%f30,%f0,%f0
/* 0x0788	 195 */		add	%o0,1,%o0
/* 0x078c	     */		add	%g4,8,%g4
/* 0x0790	     */		cmp	%o0,%g2
/* 0x0794	     */		fitod	%f0,%f2
/* 0x0798	     */		fitod	%f1,%f0
/* 0x079c	     */		fsubd	%f18,%f2,%f2
/* 0x07a0	     */		fsubd	%f18,%f0,%f0
/* 0x07a4	     */		fmuld	%f2,%f16,%f2
/* 0x07a8	     */		fmuld	%f0,%f16,%f0
/* 0x07ac	     */		fdtox	%f2,%f2
/* 0x07b0	     */		std	%f2,[%g3]
/* 0x07b4	     */		add	%g3,16,%g3
/* 0x07b8	     */		fdtox	%f0,%f0
/* 0x07bc	     */		std	%f0,[%o7]
/* 0x07c0	     */		add	%o7,16,%o7
/* 0x07c4	     */		ble,a,pt	%icc,.L900000198
/* 0x07c8	     */		ldd	[%g4],%f0
                       .L77000057:

!  196		      !      }
!  198		      !#pragma pipeloop(0)
!  199		      !      for (i = 0; i < n; i ++) {

/* 0x07cc	 199 */		cmp	%o3,0
                       .L900000199:
/* 0x07d0	 199 */		ble,pt	%icc,.L77000089
/* 0x07d4	     */		nop
/* 0x07d8	     */		or	%g0,%o4,%g2
/* 0x07dc	     */		or	%g0,%o5,%o4
/* 0x07e0	     */		sub	%o3,1,%o7
/* 0x07e4	     */		or	%g0,0,%g4
/* 0x07e8	     */		add	%fp,-2264,%g3
/* 0x07ec	 195 */		cmp	%o3,5
/* 0x07f0	     */		sub	%o3,3,%o3
/* 0x07f4	     */		bl,pn	%icc,.L77000114
/* 0x07f8	     */		ldx	[%fp-2264],%o0

!  200		      !        ADD_S64_U32(i);

/* 0x07fc	 200 */		add	%o5,4,%o4
/* 0x0800	   0 */		add	%fp,-2248,%g3
/* 0x0804	 200 */		ld	[%g2],%o1
/* 0x0808	     */		sra	%i0,0,%o5
/* 0x080c	     */		or	%g0,2,%g4
/* 0x0810	     */		ld	[%g2+4],%o2
/* 0x0814	     */		add	%g2,8,%g2
/* 0x0818	     */		add	%o0,%o1,%o0
/* 0x081c	     */		ldx	[%fp-2256],%o1
/* 0x0820	     */		add	%o0,%o5,%o0
/* 0x0824	     */		st	%o0,[%o4-4]
/* 0x0828	     */		srax	%o0,32,%o0
                       .L900000173:
/* 0x082c	 200 */		ldx	[%g3],%g5
/* 0x0830	     */		sra	%o0,0,%o5
/* 0x0834	     */		add	%o1,%o2,%o0
/* 0x0838	     */		ld	[%g2],%o1
/* 0x083c	     */		add	%o0,%o5,%o0
/* 0x0840	     */		add	%g4,3,%g4
/* 0x0844	     */		st	%o0,[%o4]
/* 0x0848	     */		srax	%o0,32,%i0
/* 0x084c	     */		cmp	%g4,%o3
/* 0x0850	     */		ldx	[%g3+8],%o0
/* 0x0854	     */		sra	%i0,0,%o5
/* 0x0858	     */		add	%g5,%o1,%o1
/* 0x085c	     */		ld	[%g2+4],%o2
/* 0x0860	     */		add	%o1,%o5,%o1
/* 0x0864	     */		add	%o4,12,%o4
/* 0x0868	     */		st	%o1,[%o4-8]
/* 0x086c	     */		srax	%o1,32,%o5
/* 0x0870	     */		add	%g2,12,%g2
/* 0x0874	     */		ldx	[%g3+16],%o1
/* 0x0878	     */		add	%o0,%o2,%o0
/* 0x087c	     */		ld	[%g2-4],%o2
/* 0x0880	     */		add	%o0,%o5,%o0
/* 0x0884	     */		add	%g3,24,%g3
/* 0x0888	     */		st	%o0,[%o4-4]
/* 0x088c	     */		ble,pt	%icc,.L900000173
/* 0x0890	     */		srax	%o0,32,%o0
                       .L900000176:
/* 0x0894	 200 */		sra	%o0,0,%o3
/* 0x0898	     */		add	%o1,%o2,%o0
/* 0x089c	     */		add	%o0,%o3,%o0
/* 0x08a0	     */		add	%o4,4,%o4
/* 0x08a4	     */		st	%o0,[%o4-4]
/* 0x08a8	     */		cmp	%g4,%o7
/* 0x08ac	     */		bg,pn	%icc,.L77000089
/* 0x08b0	     */		srax	%o0,32,%i0
                       .L77000114:
/* 0x08b4	 200 */		ld	[%g2],%o2
                       .L900000197:
/* 0x08b8	 200 */		ldx	[%g3],%o0
/* 0x08bc	     */		sra	%i0,0,%o1
/* 0x08c0	     */		add	%g4,1,%g4
/* 0x08c4	     */		add	%g2,4,%g2
/* 0x08c8	     */		add	%g3,8,%g3
/* 0x08cc	     */		add	%o0,%o2,%o0
/* 0x08d0	     */		cmp	%g4,%o7
/* 0x08d4	     */		add	%o0,%o1,%o0
/* 0x08d8	     */		st	%o0,[%o4]
/* 0x08dc	     */		add	%o4,4,%o4
/* 0x08e0	     */		srax	%o0,32,%i0
/* 0x08e4	     */		ble,a,pt	%icc,.L900000197
/* 0x08e8	     */		ld	[%g2],%o2
                       .L77000061:
/* 0x08ec	     */		ret	! Result =  %i0
/* 0x08f0	     */		restore	%g0,%g0,%g0
                       .L77000062:
/* 0x08f4	     */		sethi	%hi(0x80000000),%g2

!  201		      !      }
!  203		      !      return c;
!  205		      !    }
!  206		      !  } else if (a < (1u << 31)) {
!  208		      !    if (n == 8) {
!  209		      !      DEF_VARS(2*8);
!  210		      !      t_d64 d0, d1, db;
!  211		      !      t_s32 c = 0;
!  213		      !      da = (t_d64)(a &  A_MASK);

/* 0x08f8	 213 */		ldd	[%o1],%f6
/* 0x08fc	 206 */		cmp	%o0,%g2
/* 0x0900	     */		bcc,pn	%icc,.L77000076
/* 0x0904	     */		cmp	%o2,8
/* 0x0908	 208 */		bne,pn	%icc,.L77000065
/* 0x090c	     */		sethi	%hi(0xffe00000),%g2
/* 0x0910	 213 */		fmovs	%f6,%f10
/* 0x0914	     */		ldd	[%o7],%f4

!  214		      !      db = (t_d64)(a &~ A_MASK);

/* 0x0918	 214 */		fmovs	%f6,%f12
/* 0x091c	     */		or	%g0,%g2,%g3
/* 0x0920	     */		ldd	[%o7+8],%f14
/* 0x0924	 213 */		andn	%o0,%g2,%g2
/* 0x0928	 214 */		and	%o0,%g3,%g3
/* 0x092c	 213 */		st	%g2,[%sp+460]
/* 0x0930	     */		fxnor	%f30,%f4,%f4
/* 0x0934	 214 */		st	%g3,[%sp+456]
/* 0x0938	     */		fxnor	%f30,%f14,%f14

!  216		      !      MUL_U32_S64_D_8(0);

/* 0x093c	 216 */		ldd	[%o1+8],%f16
/* 0x0940	     */		fitod	%f4,%f8
/* 0x0944	     */		ldd	[%o7+16],%f18
/* 0x0948	     */		fitod	%f5,%f4
/* 0x094c	     */		ldd	[%o7+24],%f20
/* 0x0950	 213 */		ld	[%sp+460],%f11
/* 0x0954	 216 */		fsubd	%f16,%f8,%f8
/* 0x0958	 214 */		ld	[%sp+456],%f13
/* 0x095c	 216 */		fsubd	%f16,%f4,%f4

!  217		      !      ADD_S64_U32_D_8(0);

/* 0x0960	 217 */		ld	[%i1],%g2
/* 0x0964	 213 */		fsubd	%f10,%f6,%f10
/* 0x0968	 217 */		ld	[%i1+28],%o3
/* 0x096c	 214 */		fsubd	%f12,%f6,%f6
/* 0x0970	 217 */		ld	[%i1+4],%g3
/* 0x0974	 216 */		fitod	%f14,%f12
/* 0x0978	 217 */		stx	%o3,[%sp+312]
/* 0x097c	 216 */		fitod	%f15,%f14
/* 0x0980	     */		fmuld	%f4,%f10,%f24
/* 0x0984	 217 */		ld	[%i1+8],%g4
/* 0x0988	     */		fxnor	%f30,%f18,%f18
/* 0x098c	 216 */		fmuld	%f8,%f10,%f22
/* 0x0990	 217 */		ld	[%i1+12],%g5
/* 0x0994	 216 */		fsubd	%f16,%f12,%f12
/* 0x0998	     */		fmuld	%f4,%f6,%f4
/* 0x099c	 217 */		ld	[%i1+16],%o0
/* 0x09a0	 216 */		fitod	%f18,%f26
/* 0x09a4	     */		fmuld	%f8,%f6,%f8
/* 0x09a8	 217 */		ld	[%i1+20],%o1
/* 0x09ac	 216 */		fsubd	%f16,%f14,%f14
/* 0x09b0	 217 */		ld	[%i1+24],%o2
/* 0x09b4	 216 */		fdtox	%f4,%f4
/* 0x09b8	     */		fmuld	%f12,%f10,%f28
/* 0x09bc	     */		std	%f4,[%sp+424]
/* 0x09c0	     */		fdtox	%f8,%f8
/* 0x09c4	     */		fmuld	%f12,%f6,%f12
/* 0x09c8	     */		std	%f8,[%sp+440]
/* 0x09cc	     */		fsubd	%f16,%f26,%f4
/* 0x09d0	     */		fitod	%f19,%f8
/* 0x09d4	     */		fmuld	%f14,%f10,%f18
/* 0x09d8	     */		fdtox	%f12,%f12
/* 0x09dc	     */		std	%f12,[%sp+408]
/* 0x09e0	     */		fmuld	%f14,%f6,%f14
/* 0x09e4	 217 */		ldx	[%sp+440],%o5
/* 0x09e8	 216 */		fdtox	%f22,%f22
/* 0x09ec	     */		fmuld	%f4,%f10,%f12
/* 0x09f0	     */		std	%f22,[%sp+448]
/* 0x09f4	     */		fxnor	%f30,%f20,%f20
/* 0x09f8	     */		fmuld	%f4,%f6,%f4
/* 0x09fc	     */		fdtox	%f24,%f22
/* 0x0a00	     */		std	%f22,[%sp+432]
/* 0x0a04	 217 */		ldx	[%sp+448],%o4
/* 0x0a08	 216 */		fsubd	%f16,%f8,%f8
/* 0x0a0c	     */		fitod	%f20,%f22
/* 0x0a10	     */		fdtox	%f14,%f14
/* 0x0a14	     */		std	%f14,[%sp+392]
/* 0x0a18	 217 */		add	%o4,%o5,%o4
/* 0x0a1c	     */		ldx	[%sp+432],%o5
/* 0x0a20	 216 */		fdtox	%f28,%f24
/* 0x0a24	 217 */		add	%o4,%g2,%o4
/* 0x0a28	 216 */		fmuld	%f8,%f10,%f14
/* 0x0a2c	     */		std	%f24,[%sp+416]
/* 0x0a30	 217 */		srax	%o4,32,%o7
/* 0x0a34	 216 */		fitod	%f21,%f20
/* 0x0a38	     */		fmuld	%f8,%f6,%f8
/* 0x0a3c	 217 */		stx	%o4,[%sp+320]
/* 0x0a40	 216 */		fdtox	%f4,%f4
/* 0x0a44	     */		std	%f4,[%sp+376]
/* 0x0a48	     */		fdtox	%f18,%f18
/* 0x0a4c	     */		std	%f18,[%sp+400]
/* 0x0a50	     */		fsubd	%f16,%f20,%f4
/* 0x0a54	 217 */		ldx	[%sp+424],%g2
/* 0x0a58	 216 */		fsubd	%f16,%f22,%f18
/* 0x0a5c	 217 */		ldx	[%sp+416],%o3
/* 0x0a60	 216 */		fdtox	%f12,%f12
/* 0x0a64	     */		std	%f12,[%sp+384]
/* 0x0a68	 217 */		add	%o5,%g2,%g2
/* 0x0a6c	 216 */		fdtox	%f14,%f14
/* 0x0a70	 217 */		ldx	[%sp+408],%o4
/* 0x0a74	     */		add	%g2,%g3,%g2
/* 0x0a78	 216 */		fdtox	%f8,%f8
/* 0x0a7c	     */		fmuld	%f18,%f10,%f12
/* 0x0a80	     */		std	%f14,[%sp+368]
/* 0x0a84	 217 */		add	%g2,%o7,%g2
/* 0x0a88	 216 */		fmuld	%f4,%f10,%f14
/* 0x0a8c	     */		std	%f8,[%sp+360]
/* 0x0a90	 217 */		srax	%g2,32,%o5
/* 0x0a94	     */		add	%o3,%o4,%o3
/* 0x0a98	 216 */		fmuld	%f18,%f6,%f16
/* 0x0a9c	 217 */		ldx	[%sp+392],%o4
/* 0x0aa0	     */		add	%o3,%g4,%g4
/* 0x0aa4	 216 */		fmuld	%f4,%f6,%f4
/* 0x0aa8	 217 */		ldx	[%sp+400],%g3
/* 0x0aac	     */		add	%g4,%o5,%g4
/* 0x0ab0	 216 */		fdtox	%f12,%f6
/* 0x0ab4	 217 */		ldx	[%sp+384],%o7
/* 0x0ab8	     */		ldx	[%sp+376],%o3
/* 0x0abc	     */		add	%g3,%o4,%g3
/* 0x0ac0	 216 */		fdtox	%f4,%f4
/* 0x0ac4	     */		std	%f6,[%sp+352]
/* 0x0ac8	 217 */		srax	%g4,32,%o4
/* 0x0acc	     */		add	%g3,%g5,%g3
/* 0x0ad0	     */		ldx	[%sp+368],%g5
/* 0x0ad4	     */		add	%o7,%o3,%o3
/* 0x0ad8	     */		add	%g3,%o4,%g3
/* 0x0adc	 216 */		std	%f4,[%sp+328]
/* 0x0ae0	 217 */		srax	%g3,32,%o4
/* 0x0ae4	     */		add	%o3,%o0,%o0
/* 0x0ae8	     */		ldx	[%sp+360],%o3
/* 0x0aec	     */		add	%o0,%o4,%o4
/* 0x0af0	 216 */		fdtox	%f16,%f6
/* 0x0af4	     */		std	%f6,[%sp+344]
/* 0x0af8	     */		fdtox	%f14,%f6
/* 0x0afc	     */		std	%f6,[%sp+336]
/* 0x0b00	 217 */		add	%g5,%o3,%o3
/* 0x0b04	     */		ldx	[%sp+352],%o5
/* 0x0b08	     */		srax	%o4,32,%g5
/* 0x0b0c	     */		add	%o3,%o1,%o1
/* 0x0b10	     */		st	%g2,[%i4+4]
/* 0x0b14	     */		add	%o1,%g5,%o1
/* 0x0b18	     */		ldx	[%sp+344],%o7
/* 0x0b1c	     */		ldx	[%sp+320],%g5
/* 0x0b20	     */		ldx	[%sp+336],%o3
/* 0x0b24	     */		add	%o5,%o7,%o0
/* 0x0b28	     */		ldx	[%sp+328],%o5
/* 0x0b2c	     */		add	%o0,%o2,%o2
/* 0x0b30	     */		st	%g5,[%i4]
/* 0x0b34	     */		srax	%o1,32,%g5
/* 0x0b38	     */		ldx	[%sp+312],%o7
/* 0x0b3c	     */		add	%o2,%g5,%o2
/* 0x0b40	     */		add	%o3,%o5,%o3
/* 0x0b44	     */		st	%o4,[%i4+16]
/* 0x0b48	     */		srax	%o2,32,%g2
/* 0x0b4c	     */		st	%o1,[%i4+20]
/* 0x0b50	     */		add	%o3,%o7,%o3
/* 0x0b54	     */		st	%o2,[%i4+24]
/* 0x0b58	     */		add	%o3,%g2,%g2
/* 0x0b5c	     */		st	%g4,[%i4+8]

!  219		      !      return c;

/* 0x0b60	 219 */		srax	%g2,32,%i0
/* 0x0b64	 217 */		st	%g3,[%i4+12]
/* 0x0b68	     */		st	%g2,[%i4+28]
/* 0x0b6c	     */		ret	! Result =  %i0
/* 0x0b70	     */		restore	%g0,%g0,%g0
                       .L77000065:

!  221		      !    } else if (n == 16) {

/* 0x0b74	 221 */		cmp	%o2,16
/* 0x0b78	     */		bne,pn	%icc,.L77000109
/* 0x0b7c	     */		sethi	%hi(0xffe00000),%g2
/* 0x0b80	     */		ldd	[%o7],%f4

!  222		      !      DEF_VARS(2*16);
!  223		      !      t_d64 d0, d1, db;
!  224		      !      t_s32 c = 0;
!  226		      !      da = (t_d64)(a &  A_MASK);
!  227		      !      db = (t_d64)(a &~ A_MASK);

/* 0x0b84	 227 */		or	%g0,%g2,%g3
/* 0x0b88	 226 */		andn	%o0,%g2,%g2
/* 0x0b8c	     */		st	%g2,[%sp+572]
/* 0x0b90	 227 */		and	%o0,%g3,%g3
/* 0x0b94	     */		fxnor	%f30,%f4,%f4
/* 0x0b98	 226 */		ldd	[%o1],%f8
/* 0x0b9c	 227 */		st	%g3,[%sp+568]

!  229		      !      MUL_U32_S64_D_8(0);

/* 0x0ba0	 229 */		fitod	%f4,%f10
/* 0x0ba4	     */		ldd	[%o1+8],%f16
/* 0x0ba8	     */		ldd	[%o7+8],%f14
/* 0x0bac	     */		fitod	%f5,%f4
/* 0x0bb0	 226 */		ld	[%sp+572],%f7
/* 0x0bb4	 229 */		fsubd	%f16,%f10,%f10
/* 0x0bb8	 227 */		ld	[%sp+568],%f13
/* 0x0bbc	     */		fxnor	%f30,%f14,%f14
/* 0x0bc0	 229 */		fsubd	%f16,%f4,%f4
/* 0x0bc4	 226 */		fmovs	%f8,%f6
/* 0x0bc8	 227 */		fmovs	%f8,%f12
/* 0x0bcc	 226 */		fsubd	%f6,%f8,%f6
/* 0x0bd0	 227 */		fsubd	%f12,%f8,%f8
/* 0x0bd4	 229 */		fitod	%f14,%f12
/* 0x0bd8	     */		fmuld	%f10,%f6,%f18
/* 0x0bdc	     */		fitod	%f15,%f14
/* 0x0be0	     */		fmuld	%f10,%f8,%f10
/* 0x0be4	     */		fsubd	%f16,%f12,%f12
/* 0x0be8	     */		fmuld	%f4,%f6,%f20
/* 0x0bec	     */		fmuld	%f4,%f8,%f4
/* 0x0bf0	     */		fsubd	%f16,%f14,%f14
/* 0x0bf4	     */		fdtox	%f10,%f10
/* 0x0bf8	     */		std	%f10,[%sp+552]
/* 0x0bfc	     */		fmuld	%f12,%f6,%f10
/* 0x0c00	     */		fdtox	%f18,%f18
/* 0x0c04	     */		std	%f18,[%sp+560]
/* 0x0c08	     */		fmuld	%f12,%f8,%f12
/* 0x0c0c	     */		fdtox	%f4,%f4
/* 0x0c10	     */		std	%f4,[%sp+536]
/* 0x0c14	     */		fmuld	%f14,%f6,%f4
/* 0x0c18	     */		fdtox	%f20,%f18
/* 0x0c1c	     */		std	%f18,[%sp+544]
/* 0x0c20	     */		fdtox	%f10,%f10
/* 0x0c24	     */		std	%f10,[%sp+528]
/* 0x0c28	     */		fmuld	%f14,%f8,%f14
/* 0x0c2c	     */		fdtox	%f12,%f12
/* 0x0c30	     */		std	%f12,[%sp+520]
/* 0x0c34	     */		ldd	[%o7+16],%f10
/* 0x0c38	     */		fdtox	%f4,%f4
/* 0x0c3c	     */		ldd	[%o7+24],%f12
/* 0x0c40	     */		fdtox	%f14,%f14
/* 0x0c44	     */		std	%f4,[%sp+512]
/* 0x0c48	     */		fxnor	%f30,%f10,%f10
/* 0x0c4c	     */		fxnor	%f30,%f12,%f4
/* 0x0c50	     */		std	%f14,[%sp+504]
/* 0x0c54	     */		fitod	%f10,%f12
/* 0x0c58	     */		fitod	%f11,%f10
/* 0x0c5c	     */		fitod	%f4,%f14
/* 0x0c60	     */		fsubd	%f16,%f12,%f12
/* 0x0c64	     */		fsubd	%f16,%f10,%f10
/* 0x0c68	     */		fitod	%f5,%f4
/* 0x0c6c	     */		fmuld	%f12,%f6,%f18
/* 0x0c70	     */		fsubd	%f16,%f14,%f14
/* 0x0c74	     */		fmuld	%f10,%f6,%f20
/* 0x0c78	     */		fmuld	%f12,%f8,%f12
/* 0x0c7c	     */		fsubd	%f16,%f4,%f4
/* 0x0c80	     */		fmuld	%f10,%f8,%f10
/* 0x0c84	     */		fdtox	%f18,%f18
/* 0x0c88	     */		std	%f18,[%sp+496]
/* 0x0c8c	     */		fdtox	%f20,%f18
/* 0x0c90	     */		std	%f18,[%sp+480]
/* 0x0c94	     */		fdtox	%f12,%f12
/* 0x0c98	     */		std	%f12,[%sp+488]
/* 0x0c9c	     */		fmuld	%f14,%f6,%f12
/* 0x0ca0	     */		fdtox	%f10,%f10
/* 0x0ca4	     */		std	%f10,[%sp+472]
/* 0x0ca8	     */		fmuld	%f4,%f6,%f10
/* 0x0cac	     */		fmuld	%f4,%f8,%f4
/* 0x0cb0	     */		fdtox	%f12,%f12
/* 0x0cb4	     */		std	%f12,[%sp+464]
/* 0x0cb8	     */		fmuld	%f14,%f8,%f14
/* 0x0cbc	     */		fdtox	%f10,%f10
/* 0x0cc0	     */		std	%f10,[%sp+448]
/* 0x0cc4	     */		ldd	[%o7+32],%f12
/* 0x0cc8	     */		fdtox	%f4,%f4
/* 0x0ccc	     */		std	%f4,[%sp+440]
/* 0x0cd0	     */		fdtox	%f14,%f14
/* 0x0cd4	     */		fxnor	%f30,%f12,%f12
/* 0x0cd8	     */		std	%f14,[%sp+456]
/* 0x0cdc	     */		ldd	[%o7+40],%f14

!  230		      !      MUL_U32_S64_D_8(4);

/* 0x0ce0	 230 */		fitod	%f12,%f4
/* 0x0ce4	     */		fitod	%f13,%f12
/* 0x0ce8	     */		fxnor	%f30,%f14,%f10
/* 0x0cec	     */		fsubd	%f16,%f4,%f4
/* 0x0cf0	     */		fsubd	%f16,%f12,%f12
/* 0x0cf4	     */		fitod	%f10,%f14
/* 0x0cf8	     */		fitod	%f11,%f10
/* 0x0cfc	     */		fmuld	%f4,%f6,%f18
/* 0x0d00	     */		fmuld	%f12,%f6,%f20
/* 0x0d04	     */		fsubd	%f16,%f14,%f14
/* 0x0d08	     */		fmuld	%f4,%f8,%f4
/* 0x0d0c	     */		fsubd	%f16,%f10,%f10
/* 0x0d10	     */		fmuld	%f12,%f8,%f12
/* 0x0d14	     */		fdtox	%f18,%f18
/* 0x0d18	     */		std	%f18,[%sp+432]
/* 0x0d1c	     */		fdtox	%f4,%f4
/* 0x0d20	     */		std	%f4,[%sp+424]
/* 0x0d24	     */		fdtox	%f12,%f12
/* 0x0d28	     */		std	%f12,[%sp+408]
/* 0x0d2c	     */		fmuld	%f14,%f6,%f4
/* 0x0d30	     */		fdtox	%f20,%f18
/* 0x0d34	     */		std	%f18,[%sp+416]
/* 0x0d38	     */		fmuld	%f10,%f6,%f12
/* 0x0d3c	     */		fmuld	%f10,%f8,%f10
/* 0x0d40	     */		fdtox	%f4,%f4
/* 0x0d44	     */		std	%f4,[%sp+400]
/* 0x0d48	     */		fmuld	%f14,%f8,%f14
/* 0x0d4c	     */		fdtox	%f12,%f12
/* 0x0d50	     */		std	%f12,[%sp+384]
/* 0x0d54	     */		ldd	[%o7+48],%f4
/* 0x0d58	     */		fdtox	%f10,%f10
/* 0x0d5c	     */		std	%f10,[%sp+376]
/* 0x0d60	     */		fdtox	%f14,%f14
/* 0x0d64	     */		std	%f14,[%sp+392]
/* 0x0d68	     */		fxnor	%f30,%f4,%f4
/* 0x0d6c	     */		ldd	[%o7+56],%f14
/* 0x0d70	     */		fitod	%f4,%f10
/* 0x0d74	     */		fitod	%f5,%f4
/* 0x0d78	     */		fxnor	%f30,%f14,%f12
/* 0x0d7c	     */		fsubd	%f16,%f10,%f10
/* 0x0d80	     */		fsubd	%f16,%f4,%f4
/* 0x0d84	     */		fitod	%f12,%f14
/* 0x0d88	     */		fitod	%f13,%f12
/* 0x0d8c	     */		fmuld	%f10,%f6,%f18
/* 0x0d90	     */		fmuld	%f4,%f6,%f20
/* 0x0d94	     */		fsubd	%f16,%f14,%f14
/* 0x0d98	     */		fmuld	%f10,%f8,%f10
/* 0x0d9c	     */		fdtox	%f18,%f18
/* 0x0da0	     */		std	%f18,[%sp+368]
/* 0x0da4	     */		fmuld	%f4,%f8,%f4
/* 0x0da8	     */		fdtox	%f10,%f10
/* 0x0dac	     */		std	%f10,[%sp+360]
/* 0x0db0	     */		fdtox	%f20,%f10
/* 0x0db4	     */		std	%f10,[%sp+352]
/* 0x0db8	     */		fdtox	%f4,%f4
/* 0x0dbc	     */		std	%f4,[%sp+344]
/* 0x0dc0	     */		fmuld	%f14,%f6,%f10
/* 0x0dc4	     */		fmuld	%f14,%f8,%f4
/* 0x0dc8	     */		fdtox	%f10,%f10
/* 0x0dcc	     */		std	%f10,[%sp+336]
/* 0x0dd0	     */		fdtox	%f4,%f4
/* 0x0dd4	     */		std	%f4,[%sp+328]

!  231		      !      ADD_S64_U32_D_8(0);

/* 0x0dd8	 231 */		ldx	[%sp+552],%g3
/* 0x0ddc	 230 */		fsubd	%f16,%f12,%f4
/* 0x0de0	 231 */		ldx	[%sp+560],%g2
/* 0x0de4	     */		ld	[%i1],%g4
/* 0x0de8	     */		ldx	[%sp+544],%g5
/* 0x0dec	     */		add	%g2,%g3,%g2
/* 0x0df0	 230 */		fmuld	%f4,%f6,%f6
/* 0x0df4	 231 */		ldx	[%sp+536],%g3
/* 0x0df8	     */		add	%g2,%g4,%g4
/* 0x0dfc	 230 */		fmuld	%f4,%f8,%f4
/* 0x0e00	 231 */		ld	[%i1+4],%g2
/* 0x0e04	     */		ldx	[%sp+520],%o0
/* 0x0e08	     */		add	%g5,%g3,%g3
/* 0x0e0c	 230 */		fdtox	%f6,%f6
/* 0x0e10	 231 */		ldx	[%sp+528],%g5
/* 0x0e14	     */		add	%g3,%g2,%g2
/* 0x0e18	 230 */		fdtox	%f4,%f4
/* 0x0e1c	 231 */		st	%g4,[%i4]
/* 0x0e20	     */		srax	%g4,32,%g4
/* 0x0e24	     */		ld	[%i1+8],%g3
/* 0x0e28	     */		add	%g2,%g4,%g2
/* 0x0e2c	     */		add	%g5,%o0,%g4
/* 0x0e30	     */		ldx	[%sp+512],%o0
/* 0x0e34	     */		srax	%g2,32,%g5
/* 0x0e38	     */		ldx	[%sp+504],%o1
/* 0x0e3c	     */		add	%g4,%g3,%g3
/* 0x0e40	     */		ld	[%i1+12],%g4
/* 0x0e44	     */		add	%g3,%g5,%g3
/* 0x0e48	     */		ldx	[%sp+488],%o2
/* 0x0e4c	     */		add	%o0,%o1,%g5
/* 0x0e50	     */		ldx	[%sp+496],%o1
/* 0x0e54	     */		srax	%g3,32,%o0
/* 0x0e58	     */		add	%g5,%g4,%g4
/* 0x0e5c	     */		st	%g2,[%i4+4]
/* 0x0e60	     */		add	%g4,%o0,%g4
/* 0x0e64	     */		ld	[%i1+16],%g2
/* 0x0e68	     */		srax	%g4,32,%o0
/* 0x0e6c	     */		add	%o1,%o2,%g5
/* 0x0e70	     */		ldx	[%sp+480],%o1
/* 0x0e74	     */		ldx	[%sp+472],%o2
/* 0x0e78	     */		add	%g5,%g2,%g2
/* 0x0e7c	     */		st	%g3,[%i4+8]
/* 0x0e80	     */		add	%g2,%o0,%g2
/* 0x0e84	     */		ld	[%i1+20],%g3
/* 0x0e88	     */		srax	%g2,32,%o0
/* 0x0e8c	     */		add	%o1,%o2,%g5
/* 0x0e90	     */		ldx	[%sp+464],%o1
/* 0x0e94	     */		ldx	[%sp+456],%o2
/* 0x0e98	     */		add	%g5,%g3,%g3
/* 0x0e9c	     */		st	%g4,[%i4+12]
/* 0x0ea0	     */		add	%g3,%o0,%g3
/* 0x0ea4	     */		ld	[%i1+24],%g4
/* 0x0ea8	     */		srax	%g3,32,%o0
/* 0x0eac	     */		add	%o1,%o2,%g5
/* 0x0eb0	     */		ldx	[%sp+448],%o1
/* 0x0eb4	     */		ldx	[%sp+440],%o2
/* 0x0eb8	     */		add	%g5,%g4,%g4
/* 0x0ebc	     */		st	%g2,[%i4+16]
/* 0x0ec0	     */		add	%g4,%o0,%g4
/* 0x0ec4	     */		ld	[%i1+28],%g2
/* 0x0ec8	     */		srax	%g4,32,%o0
/* 0x0ecc	     */		add	%o1,%o2,%g5

!  232		      !      ADD_S64_U32_D_8(8);

/* 0x0ed0	 232 */		ldx	[%sp+432],%o1
/* 0x0ed4	     */		ldx	[%sp+424],%o2
/* 0x0ed8	 231 */		add	%g5,%g2,%g2
/* 0x0edc	     */		st	%g3,[%i4+20]
/* 0x0ee0	     */		add	%g2,%o0,%g2
/* 0x0ee4	 232 */		ld	[%i1+32],%g3
/* 0x0ee8	     */		srax	%g2,32,%o0
/* 0x0eec	     */		add	%o1,%o2,%g5
/* 0x0ef0	     */		ldx	[%sp+416],%o1
/* 0x0ef4	     */		ldx	[%sp+408],%o2
/* 0x0ef8	     */		add	%g5,%g3,%g3
/* 0x0efc	 231 */		st	%g4,[%i4+24]
/* 0x0f00	 232 */		add	%g3,%o0,%g3
/* 0x0f04	     */		ld	[%i1+36],%g4
/* 0x0f08	     */		srax	%g3,32,%o0
/* 0x0f0c	     */		add	%o1,%o2,%g5
/* 0x0f10	     */		ldx	[%sp+400],%o1
/* 0x0f14	     */		ldx	[%sp+392],%o2
/* 0x0f18	     */		add	%g5,%g4,%g4
/* 0x0f1c	 231 */		st	%g2,[%i4+28]
/* 0x0f20	 232 */		add	%g4,%o0,%g4
/* 0x0f24	     */		ld	[%i1+40],%g2
/* 0x0f28	     */		srax	%g4,32,%o0
/* 0x0f2c	     */		add	%o1,%o2,%g5
/* 0x0f30	     */		ldx	[%sp+384],%o1
/* 0x0f34	     */		ldx	[%sp+376],%o2
/* 0x0f38	     */		add	%g5,%g2,%g2
/* 0x0f3c	     */		st	%g3,[%i4+32]
/* 0x0f40	     */		add	%g2,%o0,%g2
/* 0x0f44	     */		ld	[%i1+44],%g3
/* 0x0f48	     */		srax	%g2,32,%o0
/* 0x0f4c	     */		add	%o1,%o2,%g5
/* 0x0f50	     */		ldx	[%sp+368],%o1
/* 0x0f54	     */		ldx	[%sp+360],%o2
/* 0x0f58	     */		add	%g5,%g3,%g3
/* 0x0f5c	     */		st	%g4,[%i4+36]
/* 0x0f60	     */		add	%g3,%o0,%g3
/* 0x0f64	     */		ld	[%i1+48],%g4
/* 0x0f68	     */		srax	%g3,32,%o0
/* 0x0f6c	     */		add	%o1,%o2,%g5
/* 0x0f70	     */		ldx	[%sp+352],%o1
/* 0x0f74	     */		ldx	[%sp+344],%o2
/* 0x0f78	     */		add	%g5,%g4,%g4
/* 0x0f7c	 230 */		std	%f6,[%sp+320]
/* 0x0f80	 232 */		add	%g4,%o0,%g4
/* 0x0f84	 230 */		std	%f4,[%sp+312]
/* 0x0f88	 232 */		srax	%g4,32,%o0
/* 0x0f8c	     */		add	%o1,%o2,%g5
/* 0x0f90	     */		st	%g2,[%i4+40]
/* 0x0f94	     */		ld	[%i1+52],%g2
/* 0x0f98	     */		ldx	[%sp+336],%o1
/* 0x0f9c	     */		ldx	[%sp+328],%o2
/* 0x0fa0	     */		add	%g5,%g2,%g2
/* 0x0fa4	     */		st	%g3,[%i4+44]
/* 0x0fa8	     */		add	%g2,%o0,%g2
/* 0x0fac	     */		ld	[%i1+56],%g3
/* 0x0fb0	     */		add	%o1,%o2,%g5
/* 0x0fb4	     */		ldx	[%sp+320],%o1
/* 0x0fb8	     */		srax	%g2,32,%o0
/* 0x0fbc	     */		ldx	[%sp+312],%o2
/* 0x0fc0	     */		add	%g5,%g3,%g3
/* 0x0fc4	     */		st	%g4,[%i4+48]
/* 0x0fc8	     */		add	%g3,%o0,%g3
/* 0x0fcc	     */		ld	[%i1+60],%g4
/* 0x0fd0	     */		add	%o1,%o2,%g5
/* 0x0fd4	     */		st	%g2,[%i4+52]
/* 0x0fd8	     */		srax	%g3,32,%o0
/* 0x0fdc	     */		add	%g5,%g4,%g2
/* 0x0fe0	     */		st	%g3,[%i4+56]
/* 0x0fe4	     */		add	%g2,%o0,%g2
/* 0x0fe8	     */		st	%g2,[%i4+60]

!  234		      !      return c;

/* 0x0fec	 234 */		srax	%g2,32,%i0
/* 0x0ff0	     */		ret	! Result =  %i0
/* 0x0ff4	     */		restore	%g0,%g0,%g0
                       .L77000109:

!  236		      !    } else {
!  237		      !      DEF_VARS(2*BUFF_SIZE);
!  238		      !      t_d64 d0, d1, db;
!  239		      !      t_s32 i, c = 0;
!  241		      !      da = (t_d64)(a &  A_MASK);
!  242		      !      db = (t_d64)(a &~ A_MASK);

/* 0x0ff8	 242 */		or	%g0,%g2,%g3
/* 0x0ffc	 241 */		andn	%o0,%g2,%g2
/* 0x1000	     */		st	%g2,[%sp+316]
/* 0x1004	 242 */		and	%o0,%g3,%g2
/* 0x1008	     */		st	%g2,[%sp+312]
/* 0x100c	 239 */		or	%g0,0,%g4

!  244		      !#pragma pipeloop(0)
!  245		      !      for (i = 0; i < (n+1)/2; i ++) {

/* 0x1010	 245 */		add	%o2,1,%g2
/* 0x1014	 241 */		ldd	[%o1],%f6
/* 0x1018	 234 */		sethi	%hi(0x1800),%g1
/* 0x101c	 245 */		srl	%g2,31,%g3
/* 0x1020	 234 */		xor	%g1,-624,%g1
/* 0x1024	 237 */		ldd	[%o1+8],%f20
/* 0x1028	 245 */		add	%g2,%g3,%g2
/* 0x102c	 241 */		fmovs	%f6,%f8
/* 0x1030	 234 */		add	%g1,%fp,%g5
/* 0x1034	 242 */		fmovs	%f6,%f10
/* 0x1038	 241 */		ld	[%sp+316],%f9
/* 0x103c	 245 */		sra	%g2,1,%o0
/* 0x1040	 242 */		ld	[%sp+312],%f11
/* 0x1044	 245 */		cmp	%o0,0
/* 0x1048	 234 */		sethi	%hi(0x1800),%g1
/* 0x104c	 241 */		fsubd	%f8,%f6,%f18
/* 0x1050	 234 */		xor	%g1,-616,%g1
/* 0x1054	     */		or	%g0,%o7,%o1
/* 0x1058	 242 */		fsubd	%f10,%f6,%f16
/* 0x105c	 245 */		ble,pt	%icc,.L900000196
/* 0x1060	     */		cmp	%o3,0
/* 0x1064	 234 */		add	%g1,%fp,%g3
/* 0x1068	     */		sethi	%hi(0x1800),%g1
/* 0x106c	     */		xor	%g1,-608,%g1
/* 0x1070	     */		sub	%o0,1,%o2
/* 0x1074	     */		add	%g1,%fp,%g2
/* 0x1078	     */		sethi	%hi(0x1800),%g1
/* 0x107c	     */		xor	%g1,-600,%g1
/* 0x1080	 245 */		cmp	%o0,7
/* 0x1084	     */		or	%g0,0,%i0
/* 0x1088	 234 */		add	%g1,%fp,%o7
/* 0x108c	 245 */		bl,pn	%icc,.L77000110
/* 0x1090	     */		sub	%o0,3,%o0
/* 0x1094	     */		ldd	[%o1],%f2

!  246		      !        MUL_U32_S64_2_D(i);

/* 0x1098	 246 */		add	%i2,16,%i1
/* 0x109c	     */		ldd	[%o1+8],%f0
/* 0x10a0	     */		add	%o1,8,%o1
/* 0x10a4	     */		or	%g0,4,%i0
/* 0x10a8	     */		fxnor	%f30,%f2,%f6
/* 0x10ac	     */		ldd	[%i2+16],%f4
/* 0x10b0	     */		add	%i1,16,%o1
/* 0x10b4	     */		fxnor	%f30,%f0,%f12
/* 0x10b8	     */		ldd	[%i2+24],%f0
/* 0x10bc	     */		fitod	%f7,%f2
/* 0x10c0	     */		fitod	%f6,%f6
/* 0x10c4	     */		fxnor	%f30,%f4,%f10
/* 0x10c8	     */		fsubd	%f20,%f2,%f2
/* 0x10cc	     */		fxnor	%f30,%f0,%f8
/* 0x10d0	     */		fitod	%f13,%f4
/* 0x10d4	     */		fsubd	%f20,%f6,%f6
/* 0x10d8	     */		fmuld	%f2,%f16,%f0
                       .L900000181:
/* 0x10dc	     */		ldd	[%o1],%f24
/* 0x10e0	 246 */		add	%i0,3,%i0
/* 0x10e4	     */		add	%o7,96,%o7
/* 0x10e8	     */		fitod	%f11,%f22
/* 0x10ec	     */		fsubd	%f20,%f4,%f26
/* 0x10f0	     */		cmp	%i0,%o0
/* 0x10f4	     */		add	%g2,96,%g2
/* 0x10f8	     */		fmuld	%f6,%f18,%f28
/* 0x10fc	     */		fmuld	%f6,%f16,%f6
/* 0x1100	     */		add	%g3,96,%g3
/* 0x1104	     */		add	%g5,96,%g5
/* 0x1108	     */		fdtox	%f0,%f0
/* 0x110c	     */		fitod	%f12,%f4
/* 0x1110	     */		fmuld	%f2,%f18,%f2
/* 0x1114	     */		fdtox	%f28,%f12
/* 0x1118	     */		fdtox	%f6,%f6
/* 0x111c	     */		std	%f12,[%g5-96]
/* 0x1120	     */		std	%f6,[%g3-96]
/* 0x1124	     */		fdtox	%f2,%f2
/* 0x1128	     */		fsubd	%f20,%f4,%f6
/* 0x112c	     */		std	%f2,[%g2-96]
/* 0x1130	     */		add	%o1,8,%o1
/* 0x1134	     */		fxnor	%f30,%f24,%f12
/* 0x1138	     */		fmuld	%f26,%f16,%f4
/* 0x113c	     */		std	%f0,[%o7-96]
/* 0x1140	     */		ldd	[%o1],%f0
/* 0x1144	     */		fitod	%f9,%f2
/* 0x1148	     */		fsubd	%f20,%f22,%f28
/* 0x114c	     */		fmuld	%f6,%f18,%f24
/* 0x1150	     */		fmuld	%f6,%f16,%f22
/* 0x1154	     */		fdtox	%f4,%f4
/* 0x1158	     */		fitod	%f10,%f6
/* 0x115c	     */		fmuld	%f26,%f18,%f10
/* 0x1160	     */		fdtox	%f24,%f24
/* 0x1164	     */		fdtox	%f22,%f22
/* 0x1168	     */		std	%f24,[%g5-64]
/* 0x116c	     */		std	%f22,[%g3-64]
/* 0x1170	     */		fdtox	%f10,%f10
/* 0x1174	     */		fsubd	%f20,%f6,%f6
/* 0x1178	     */		std	%f10,[%g2-64]
/* 0x117c	     */		add	%o1,8,%o1
/* 0x1180	     */		fxnor	%f30,%f0,%f10
/* 0x1184	     */		fmuld	%f28,%f16,%f0
/* 0x1188	     */		std	%f4,[%o7-64]
/* 0x118c	     */		ldd	[%o1],%f22
/* 0x1190	     */		fitod	%f13,%f4
/* 0x1194	     */		fsubd	%f20,%f2,%f2
/* 0x1198	     */		fmuld	%f6,%f18,%f26
/* 0x119c	     */		fmuld	%f6,%f16,%f24
/* 0x11a0	     */		fdtox	%f0,%f0
/* 0x11a4	     */		fitod	%f8,%f6
/* 0x11a8	     */		fmuld	%f28,%f18,%f8
/* 0x11ac	     */		fdtox	%f26,%f26
/* 0x11b0	     */		fdtox	%f24,%f24
/* 0x11b4	     */		std	%f26,[%g5-32]
/* 0x11b8	     */		std	%f24,[%g3-32]
/* 0x11bc	     */		fdtox	%f8,%f8
/* 0x11c0	     */		fsubd	%f20,%f6,%f6
/* 0x11c4	     */		std	%f8,[%g2-32]
/* 0x11c8	     */		add	%o1,8,%o1
/* 0x11cc	     */		fxnor	%f30,%f22,%f8
/* 0x11d0	     */		std	%f0,[%o7-32]
/* 0x11d4	     */		ble,pt	%icc,.L900000181
/* 0x11d8	     */		fmuld	%f2,%f16,%f0
                       .L900000184:
/* 0x11dc	 246 */		fitod	%f12,%f28
/* 0x11e0	     */		fmuld	%f6,%f18,%f24
/* 0x11e4	     */		add	%g5,128,%g5
/* 0x11e8	     */		fitod	%f10,%f12
/* 0x11ec	     */		fmuld	%f6,%f16,%f26
/* 0x11f0	     */		add	%g3,128,%g3
/* 0x11f4	     */		fsubd	%f20,%f4,%f4
/* 0x11f8	     */		fmuld	%f2,%f18,%f22
/* 0x11fc	     */		add	%g2,128,%g2
/* 0x1200	     */		fdtox	%f24,%f6
/* 0x1204	     */		std	%f6,[%g5-128]
/* 0x1208	     */		add	%o7,128,%o7
/* 0x120c	     */		fsubd	%f20,%f28,%f2
/* 0x1210	     */		cmp	%i0,%o2
/* 0x1214	     */		fitod	%f11,%f6
/* 0x1218	     */		fmuld	%f4,%f18,%f24
/* 0x121c	     */		fdtox	%f26,%f10
/* 0x1220	     */		std	%f10,[%g3-128]
/* 0x1224	     */		fdtox	%f22,%f10
/* 0x1228	     */		std	%f10,[%g2-128]
/* 0x122c	     */		fmuld	%f2,%f18,%f26
/* 0x1230	     */		fsubd	%f20,%f12,%f10
/* 0x1234	     */		fmuld	%f2,%f16,%f2
/* 0x1238	     */		fsubd	%f20,%f6,%f22
/* 0x123c	     */		fmuld	%f4,%f16,%f12
/* 0x1240	     */		fdtox	%f0,%f0
/* 0x1244	     */		std	%f0,[%o7-128]
/* 0x1248	     */		fitod	%f8,%f4
/* 0x124c	     */		fmuld	%f10,%f18,%f6
/* 0x1250	     */		fdtox	%f26,%f0
/* 0x1254	     */		std	%f0,[%g5-96]
/* 0x1258	     */		fmuld	%f10,%f16,%f10
/* 0x125c	     */		fdtox	%f2,%f2
/* 0x1260	     */		std	%f2,[%g3-96]
/* 0x1264	     */		fitod	%f9,%f0
/* 0x1268	     */		fmuld	%f22,%f18,%f2
/* 0x126c	     */		fdtox	%f24,%f8
/* 0x1270	     */		std	%f8,[%g2-96]
/* 0x1274	     */		fsubd	%f20,%f4,%f4
/* 0x1278	     */		fmuld	%f22,%f16,%f8
/* 0x127c	     */		fdtox	%f12,%f12
/* 0x1280	     */		std	%f12,[%o7-96]
/* 0x1284	     */		fsubd	%f20,%f0,%f0
/* 0x1288	     */		fdtox	%f6,%f6
/* 0x128c	     */		std	%f6,[%g5-64]
/* 0x1290	     */		fdtox	%f10,%f10
/* 0x1294	     */		std	%f10,[%g3-64]
/* 0x1298	     */		fmuld	%f4,%f18,%f6
/* 0x129c	     */		fdtox	%f2,%f2
/* 0x12a0	     */		std	%f2,[%g2-64]
/* 0x12a4	     */		fmuld	%f4,%f16,%f4
/* 0x12a8	     */		fmuld	%f0,%f18,%f2
/* 0x12ac	     */		fdtox	%f8,%f8
/* 0x12b0	     */		std	%f8,[%o7-64]
/* 0x12b4	     */		fdtox	%f6,%f6
/* 0x12b8	     */		std	%f6,[%g5-32]
/* 0x12bc	     */		fmuld	%f0,%f16,%f0
/* 0x12c0	     */		fdtox	%f4,%f4
/* 0x12c4	     */		std	%f4,[%g3-32]
/* 0x12c8	     */		fdtox	%f2,%f2
/* 0x12cc	     */		std	%f2,[%g2-32]
/* 0x12d0	     */		fdtox	%f0,%f0
/* 0x12d4	     */		bg,pn	%icc,.L77000071
/* 0x12d8	     */		std	%f0,[%o7-32]
                       .L77000110:
/* 0x12dc	     */		ldd	[%o1],%f0
                       .L900000195:
/* 0x12e0	     */		fxnor	%f30,%f0,%f0
/* 0x12e4	 246 */		add	%i0,1,%i0
/* 0x12e8	     */		add	%o1,8,%o1
/* 0x12ec	     */		cmp	%i0,%o2
/* 0x12f0	     */		fitod	%f0,%f2
/* 0x12f4	     */		fitod	%f1,%f0
/* 0x12f8	     */		fsubd	%f20,%f2,%f2
/* 0x12fc	     */		fsubd	%f20,%f0,%f0
/* 0x1300	     */		fmuld	%f2,%f18,%f6
/* 0x1304	     */		fmuld	%f2,%f16,%f4
/* 0x1308	     */		fmuld	%f0,%f18,%f2
/* 0x130c	     */		fdtox	%f6,%f6
/* 0x1310	     */		std	%f6,[%g5]
/* 0x1314	     */		fmuld	%f0,%f16,%f0
/* 0x1318	     */		fdtox	%f4,%f4
/* 0x131c	     */		std	%f4,[%g3]
/* 0x1320	     */		add	%g3,32,%g3
/* 0x1324	     */		fdtox	%f2,%f2
/* 0x1328	     */		std	%f2,[%g2]
/* 0x132c	     */		add	%g2,32,%g2
/* 0x1330	     */		fdtox	%f0,%f0
/* 0x1334	     */		std	%f0,[%o7]
/* 0x1338	     */		add	%o7,32,%o7
/* 0x133c	     */		add	%g5,32,%g5
/* 0x1340	     */		ble,a,pt	%icc,.L900000195
/* 0x1344	     */		ldd	[%o1],%f0
                       .L77000071:

!  247		      !      }
!  249		      !#pragma pipeloop(0)
!  250		      !      for (i = 0; i < n; i ++) {

/* 0x1348	 250 */		cmp	%o3,0
                       .L900000196:
/* 0x134c	 250 */		ble,a,pt	%icc,.L77000089
/* 0x1350	     */		or	%g0,%g4,%i0
/* 0x1354	     */		sethi	%hi(0x1800),%g1
/* 0x1358	     */		xor	%g1,-624,%g1
/* 0x135c	     */		or	%g0,%o5,%g5
/* 0x1360	     */		add	%g1,%fp,%g3
/* 0x1364	     */		sethi	%hi(0x1800),%g1
/* 0x1368	     */		xor	%g1,-616,%g1
/* 0x136c	     */		or	%g0,%o4,%i0
/* 0x1370	     */		add	%g1,%fp,%o5
/* 0x1374	     */		or	%g0,0,%i1
/* 0x1378	     */		sub	%o3,1,%o7
/* 0x137c	 246 */		cmp	%o3,5
/* 0x1380	     */		bl,pn	%icc,.L77000111
/* 0x1384	     */		sethi	%hi(0x1800),%g1

!  251		      !        ADD_S64_U32_D(i);

/* 0x1388	 251 */		ld	[%i0],%o2
/* 0x138c	 246 */		sub	%o3,3,%o4
/* 0x1390	 251 */		xor	%g1,-624,%g1
/* 0x1394	     */		add	%g5,4,%g5
/* 0x1398	     */		add	%g1,%fp,%g1
/* 0x139c	     */		or	%g0,2,%i1
/* 0x13a0	     */		ldx	[%g1+8],%o1
/* 0x13a4	     */		ldx	[%g1],%o0
/* 0x13a8	     */		ldx	[%g1+16],%o3
/* 0x13ac	     */		sethi	%hi(0x1800),%g1
/* 0x13b0	     */		xor	%g1,-592,%g1
/* 0x13b4	     */		add	%o0,%o1,%o0
/* 0x13b8	     */		ld	[%i0+4],%o1
/* 0x13bc	     */		add	%g1,%fp,%g3
/* 0x13c0	     */		sethi	%hi(0x1800),%g1
/* 0x13c4	     */		xor	%g1,-600,%g1
/* 0x13c8	     */		add	%o0,%o2,%o0
/* 0x13cc	     */		add	%g1,%fp,%g1
/* 0x13d0	     */		add	%i0,8,%i0
/* 0x13d4	     */		ldx	[%g1],%o2
/* 0x13d8	     */		sethi	%hi(0x1800),%g1
/* 0x13dc	     */		st	%o0,[%g5-4]
/* 0x13e0	     */		srax	%o0,32,%o0
/* 0x13e4	     */		xor	%g1,-584,%g1
/* 0x13e8	     */		add	%g1,%fp,%o5
                       .L900000169:
/* 0x13ec	 251 */		add	%o3,%o2,%g4
/* 0x13f0	     */		ld	[%i0],%o3
/* 0x13f4	     */		add	%i1,3,%i1
/* 0x13f8	     */		ldx	[%g3],%o2
/* 0x13fc	     */		sra	%o0,0,%g2
/* 0x1400	     */		add	%g4,%o1,%o1
/* 0x1404	     */		ldx	[%o5],%o0
/* 0x1408	     */		add	%o1,%g2,%o1
/* 0x140c	     */		cmp	%i1,%o4
/* 0x1410	     */		st	%o1,[%g5]
/* 0x1414	     */		srax	%o1,32,%g4
/* 0x1418	     */		add	%g5,12,%g5
/* 0x141c	     */		add	%o2,%o0,%o1
/* 0x1420	     */		ld	[%i0+4],%o2
/* 0x1424	     */		add	%i0,12,%i0
/* 0x1428	     */		ldx	[%g3+16],%o0
/* 0x142c	     */		add	%o1,%o3,%o3
/* 0x1430	     */		ldx	[%o5+16],%o1
/* 0x1434	     */		add	%o3,%g4,%o3
/* 0x1438	     */		add	%o5,48,%o5
/* 0x143c	     */		st	%o3,[%g5-8]
/* 0x1440	     */		srax	%o3,32,%g2
/* 0x1444	     */		add	%g3,48,%g3
/* 0x1448	     */		add	%o0,%o1,%o0
/* 0x144c	     */		ld	[%i0-4],%o1
/* 0x1450	     */		ldx	[%g3-16],%o3
/* 0x1454	     */		add	%o0,%o2,%o0
/* 0x1458	     */		ldx	[%o5-16],%o2
/* 0x145c	     */		add	%o0,%g2,%o0
/* 0x1460	     */		st	%o0,[%g5-4]
/* 0x1464	     */		ble,pt	%icc,.L900000169
/* 0x1468	     */		srax	%o0,32,%o0
                       .L900000172:
/* 0x146c	 251 */		add	%o3,%o2,%o3
/* 0x1470	     */		add	%g5,4,%g5
/* 0x1474	     */		sra	%o0,0,%o2
/* 0x1478	     */		add	%o3,%o1,%o0
/* 0x147c	     */		add	%o0,%o2,%o0
/* 0x1480	     */		st	%o0,[%g5-4]
/* 0x1484	     */		cmp	%i1,%o7
/* 0x1488	     */		srax	%o0,32,%g4
/* 0x148c	     */		bg,a,pn	%icc,.L77000089
/* 0x1490	     */		or	%g0,%g4,%i0
                       .L77000111:
/* 0x1494	 251 */		ldx	[%g3],%o0
                       .L900000194:
/* 0x1498	 251 */		ldx	[%o5],%o3
/* 0x149c	     */		sra	%g4,0,%o1
/* 0x14a0	     */		add	%i1,1,%i1
/* 0x14a4	     */		ld	[%i0],%o2
/* 0x14a8	     */		add	%g3,16,%g3
/* 0x14ac	     */		add	%o5,16,%o5
/* 0x14b0	     */		add	%o0,%o3,%o0
/* 0x14b4	     */		add	%i0,4,%i0
/* 0x14b8	     */		add	%o0,%o2,%o0
/* 0x14bc	     */		cmp	%i1,%o7
/* 0x14c0	     */		add	%o0,%o1,%o0
/* 0x14c4	     */		st	%o0,[%g5]
/* 0x14c8	     */		add	%g5,4,%g5
/* 0x14cc	     */		srax	%o0,32,%g4
/* 0x14d0	     */		ble,a,pt	%icc,.L900000194
/* 0x14d4	     */		ldx	[%g3],%o0
                       .L77000075:
/* 0x14d8	     */		ret	! Result =  %i0
/* 0x14dc	     */		restore	%g0,%g4,%o0
                       .L77000076:

!  252		      !      }
!  254		      !      return c;
!  255		      !    }
!  257		      !  } else {
!  259		      !    if (n == 8) {

/* 0x14e0	 259 */		bne,pn	%icc,.L77000078
/* 0x14e4	     */		sethi	%hi(0xffe00000),%g2
/* 0x14e8	     */		ldd	[%o7],%f4

!  260		      !      DEF_VARS(2*8);
!  261		      !      t_d64 d0, d1, db;
!  262		      !      t_u32 uc = 0;
!  264		      !      da = (t_d64)(a &  A_MASK);

/* 0x14ec	 264 */		ldd	[%o1],%f6
/* 0x14f0	     */		or	%g0,%g2,%g3

!  265		      !      db = (t_d64)(a &~ A_MASK);

/* 0x14f4	 265 */		and	%o0,%g2,%g2
/* 0x14f8	     */		st	%g2,[%sp+456]
/* 0x14fc	     */		fxnor	%f30,%f4,%f4
/* 0x1500	 264 */		andn	%o0,%g3,%g2
/* 0x1504	     */		st	%g2,[%sp+460]

!  267		      !      MUL_U32_S64_E_8(0);

/* 0x1508	 267 */		ldd	[%o1+8],%f12
/* 0x150c	     */		fitod	%f4,%f10
/* 0x1510	     */		ldd	[%o7+8],%f14
/* 0x1514	     */		fitod	%f5,%f4
/* 0x1518	     */		ldd	[%o1+16],%f18
/* 0x151c	 265 */		ld	[%sp+456],%f9
/* 0x1520	 267 */		fsubd	%f12,%f10,%f10
/* 0x1524	 264 */		ld	[%sp+460],%f17
/* 0x1528	 267 */		fsubd	%f12,%f4,%f4

!  268		      !      ADD_S64_U32_E_8(0);

/* 0x152c	 268 */		sethi	%hi(0x80000000),%g3
/* 0x1530	     */		ldd	[%o7+16],%f20
/* 0x1534	     */		fxnor	%f30,%f14,%f14
/* 0x1538	     */		sllx	%g3,32,%g3
/* 0x153c	     */		ldd	[%o7+24],%f22
/* 0x1540	     */		ld	[%i1+24],%o3
/* 0x1544	 267 */		fitod	%f14,%f26
/* 0x1548	 268 */		ld	[%i1+28],%o4
/* 0x154c	 267 */		fitod	%f15,%f14
/* 0x1550	 268 */		stx	%o3,[%sp+320]
/* 0x1554	     */		fxnor	%f30,%f20,%f20
/* 0x1558	     */		stx	%o4,[%sp+312]
/* 0x155c	 267 */		fsubd	%f12,%f26,%f26
/* 0x1560	 268 */		ld	[%i1],%g2
/* 0x1564	 267 */		fsubd	%f12,%f14,%f14
/* 0x1568	 268 */		ld	[%i1+4],%g4
/* 0x156c	     */		ld	[%i1+8],%g5
/* 0x1570	     */		ld	[%i1+12],%o0
/* 0x1574	 265 */		fmovs	%f6,%f8
/* 0x1578	 268 */		ld	[%i1+16],%o1
/* 0x157c	 264 */		fmovs	%f6,%f16
/* 0x1580	 268 */		ld	[%i1+20],%o2
/* 0x1584	 265 */		fsubd	%f8,%f6,%f8
/* 0x1588	 264 */		fsubd	%f16,%f6,%f6
/* 0x158c	 267 */		fmuld	%f10,%f8,%f24
/* 0x1590	     */		fmuld	%f4,%f8,%f16
/* 0x1594	     */		fmuld	%f4,%f6,%f4
/* 0x1598	     */		faddd	%f24,%f18,%f24
/* 0x159c	     */		fmuld	%f10,%f6,%f10
/* 0x15a0	     */		faddd	%f16,%f18,%f16
/* 0x15a4	     */		fmuld	%f26,%f8,%f28
/* 0x15a8	     */		fdtox	%f4,%f4
/* 0x15ac	     */		std	%f4,[%sp+432]
/* 0x15b0	     */		fmuld	%f26,%f6,%f26
/* 0x15b4	     */		fdtox	%f24,%f24
/* 0x15b8	     */		std	%f24,[%sp+440]
/* 0x15bc	     */		fmuld	%f14,%f6,%f4
/* 0x15c0	     */		fdtox	%f10,%f10
/* 0x15c4	     */		std	%f10,[%sp+448]
/* 0x15c8	     */		fmuld	%f14,%f8,%f24
/* 0x15cc	 268 */		ldx	[%sp+432],%o4
/* 0x15d0	 267 */		fdtox	%f16,%f14
/* 0x15d4	     */		std	%f14,[%sp+424]
/* 0x15d8	     */		fitod	%f20,%f10
/* 0x15dc	 268 */		ldx	[%sp+440],%o7
/* 0x15e0	 267 */		faddd	%f28,%f18,%f14
/* 0x15e4	 268 */		ldx	[%sp+448],%o5
/* 0x15e8	 267 */		fitod	%f21,%f16
/* 0x15ec	     */		fsubd	%f12,%f10,%f10
/* 0x15f0	 268 */		ldx	[%sp+424],%o3
/* 0x15f4	 267 */		fdtox	%f14,%f14
/* 0x15f8	 268 */		add	%o5,%o7,%o5
/* 0x15fc	 267 */		std	%f14,[%sp+408]
/* 0x1600	     */		fxnor	%f30,%f22,%f20
/* 0x1604	 268 */		add	%o5,%g2,%g2
/* 0x1608	 267 */		faddd	%f24,%f18,%f14
/* 0x160c	 268 */		add	%o4,%o3,%o3
/* 0x1610	     */		sub	%g2,%g3,%g2
/* 0x1614	     */		st	%g2,[%i4]
/* 0x1618	 267 */		fdtox	%f26,%f22
/* 0x161c	 268 */		srlx	%g2,32,%o5
/* 0x1620	 267 */		fmuld	%f10,%f8,%f26
/* 0x1624	     */		std	%f22,[%sp+416]
/* 0x1628	     */		fsubd	%f12,%f16,%f16
/* 0x162c	 268 */		add	%o3,%g4,%g4
/* 0x1630	 267 */		fmuld	%f10,%f6,%f10
/* 0x1634	     */		fdtox	%f4,%f4
/* 0x1638	     */		std	%f4,[%sp+400]
/* 0x163c	 268 */		add	%g4,%o5,%g4
/* 0x1640	     */		ldx	[%sp+416],%o7
/* 0x1644	 267 */		fdtox	%f14,%f4
/* 0x1648	 268 */		sub	%g4,%g3,%g4
/* 0x164c	 267 */		std	%f4,[%sp+392]
/* 0x1650	     */		fitod	%f20,%f22
/* 0x1654	     */		fmuld	%f16,%f8,%f14
/* 0x1658	 268 */		ldx	[%sp+408],%o3
/* 0x165c	 267 */		faddd	%f26,%f18,%f4
/* 0x1660	     */		fmuld	%f16,%f6,%f16
/* 0x1664	 268 */		st	%g4,[%i4+4]
/* 0x1668	 267 */		fitod	%f21,%f20
/* 0x166c	 268 */		ldx	[%sp+392],%o5
/* 0x1670	 267 */		fsubd	%f12,%f22,%f22
/* 0x1674	 268 */		add	%o7,%o3,%o3
/* 0x1678	     */		ldx	[%sp+400],%o4
/* 0x167c	 267 */		fdtox	%f4,%f4
/* 0x1680	 268 */		add	%o3,%g5,%o3
/* 0x1684	 267 */		std	%f4,[%sp+376]
/* 0x1688	     */		fdtox	%f10,%f10
/* 0x168c	 268 */		srlx	%g4,32,%g5
/* 0x1690	 267 */		faddd	%f14,%f18,%f4
/* 0x1694	     */		std	%f10,[%sp+384]
/* 0x1698	     */		fmuld	%f22,%f8,%f10
/* 0x169c	 268 */		ldx	[%sp+312],%g4
/* 0x16a0	 267 */		fsubd	%f12,%f20,%f12
/* 0x16a4	 268 */		add	%o4,%o5,%o4
/* 0x16a8	 267 */		fmuld	%f22,%f6,%f14
/* 0x16ac	     */		fdtox	%f16,%f16
/* 0x16b0	     */		std	%f16,[%sp+368]
/* 0x16b4	 268 */		add	%o3,%g5,%o3
/* 0x16b8	 267 */		fdtox	%f4,%f4
/* 0x16bc	     */		std	%f4,[%sp+360]
/* 0x16c0	 268 */		add	%o4,%o0,%o0
/* 0x16c4	 267 */		faddd	%f10,%f18,%f4
/* 0x16c8	 268 */		ldx	[%sp+376],%o4
/* 0x16cc	     */		sub	%o3,%g3,%o3
/* 0x16d0	 267 */		fmuld	%f12,%f8,%f16
/* 0x16d4	 268 */		ldx	[%sp+384],%g5
/* 0x16d8	     */		srlx	%o3,32,%o5
/* 0x16dc	 267 */		fdtox	%f14,%f14
/* 0x16e0	     */		fmuld	%f12,%f6,%f6
/* 0x16e4	     */		std	%f14,[%sp+352]
/* 0x16e8	 268 */		add	%o0,%o5,%o0
/* 0x16ec	     */		add	%g5,%o4,%g5
/* 0x16f0	 267 */		fdtox	%f4,%f4
/* 0x16f4	     */		std	%f4,[%sp+344]
/* 0x16f8	 268 */		ldx	[%sp+368],%o7
/* 0x16fc	     */		sub	%o0,%g3,%o0
/* 0x1700	     */		add	%g5,%o1,%o1
/* 0x1704	 267 */		faddd	%f16,%f18,%f4
/* 0x1708	 268 */		ldx	[%sp+360],%o4
/* 0x170c	     */		srlx	%o0,32,%g5
/* 0x1710	 267 */		fdtox	%f6,%f6
/* 0x1714	 268 */		add	%o1,%g5,%o1
/* 0x1718	 267 */		std	%f6,[%sp+336]
/* 0x171c	 268 */		add	%o7,%o4,%o4
/* 0x1720	     */		sub	%o1,%g3,%o1
/* 0x1724	     */		ldx	[%sp+352],%o5
/* 0x1728	     */		add	%o4,%o2,%o2
/* 0x172c	 267 */		fdtox	%f4,%f4
/* 0x1730	     */		std	%f4,[%sp+328]
/* 0x1734	 268 */		ldx	[%sp+344],%o4
/* 0x1738	     */		srlx	%o1,32,%g5
/* 0x173c	     */		add	%o2,%g5,%o2
/* 0x1740	     */		st	%o3,[%i4+8]
/* 0x1744	     */		add	%o5,%o4,%g5
/* 0x1748	     */		ldx	[%sp+320],%o5
/* 0x174c	     */		sub	%o2,%g3,%o2
/* 0x1750	     */		ldx	[%sp+336],%o7
/* 0x1754	     */		srlx	%o2,32,%g2
/* 0x1758	     */		ldx	[%sp+328],%o4
/* 0x175c	     */		add	%g5,%o5,%g5
/* 0x1760	     */		add	%g5,%g2,%g2
/* 0x1764	     */		st	%o0,[%i4+12]
/* 0x1768	     */		add	%o7,%o4,%o4
/* 0x176c	     */		sub	%g2,%g3,%g2
/* 0x1770	     */		st	%o1,[%i4+16]
/* 0x1774	     */		srlx	%g2,32,%g5
/* 0x1778	     */		add	%o4,%g4,%g4
/* 0x177c	     */		st	%o2,[%i4+20]
/* 0x1780	     */		add	%g4,%g5,%g4
/* 0x1784	     */		st	%g2,[%i4+24]
/* 0x1788	     */		sub	%g4,%g3,%g3
/* 0x178c	     */		st	%g3,[%i4+28]

!  270		      !      return uc;

/* 0x1790	 270 */		srlx	%g3,32,%i0
/* 0x1794	     */		ret	! Result =  %i0
/* 0x1798	     */		restore	%g0,%g0,%g0
                       .L77000078:

!  272		      !    } else if (n == 16) {

/* 0x179c	 272 */		cmp	%o2,16
/* 0x17a0	     */		bne,pn	%icc,.L77000106
/* 0x17a4	     */		sethi	%hi(0xffe00000),%g2
/* 0x17a8	     */		ldd	[%o7],%f6

!  273		      !      DEF_VARS(2*16);
!  274		      !      t_d64 d0, d1, db;
!  275		      !      t_u32 uc = 0;
!  277		      !      da = (t_d64)(a &  A_MASK);

/* 0x17ac	 277 */		or	%g0,%g2,%g3

!  278		      !      db = (t_d64)(a &~ A_MASK);

/* 0x17b0	 278 */		and	%o0,%g2,%g2
/* 0x17b4	     */		st	%g2,[%sp+312]
/* 0x17b8	 277 */		andn	%o0,%g3,%g2
/* 0x17bc	     */		sethi	%hi(0x80000000),%g3
/* 0x17c0	     */		fxnor	%f30,%f6,%f6
/* 0x17c4	     */		ldd	[%o1],%f8
/* 0x17c8	     */		sllx	%g3,32,%o0
/* 0x17cc	     */		st	%g2,[%sp+316]

!  280		      !      MUL_U32_S64_E_8(0);

/* 0x17d0	 280 */		fitod	%f6,%f12
/* 0x17d4	     */		ldd	[%o1+8],%f4
/* 0x17d8	     */		fitod	%f7,%f6
/* 0x17dc	     */		ldd	[%o1+16],%f2

!  281		      !      MUL_U32_S64_E_8(4);
!  282		      !      ADD_S64_U32_E_8(0);

/* 0x17e0	 282 */		stx	%o0,[%sp+96]
/* 0x17e4	 278 */		ld	[%sp+312],%f11
/* 0x17e8	 280 */		fsubd	%f4,%f6,%f6
/* 0x17ec	 277 */		ld	[%sp+316],%f15
/* 0x17f0	 282 */		ldx	[%sp+96],%o3
/* 0x17f4	 278 */		fmovs	%f8,%f10
/* 0x17f8	 277 */		fmovs	%f8,%f14
/* 0x17fc	 278 */		fsubd	%f10,%f8,%f16
/* 0x1800	 280 */		fsubd	%f4,%f12,%f10
/* 0x1804	 277 */		fsubd	%f14,%f8,%f14
/* 0x1808	 280 */		fmuld	%f6,%f16,%f12
/* 0x180c	     */		fmuld	%f10,%f16,%f8
/* 0x1810	     */		fmuld	%f6,%f14,%f6
/* 0x1814	     */		faddd	%f12,%f2,%f12
/* 0x1818	     */		fmuld	%f10,%f14,%f10
/* 0x181c	     */		faddd	%f8,%f2,%f8
/* 0x1820	     */		fdtox	%f6,%f6
/* 0x1824	     */		std	%f6,[%sp+288]
/* 0x1828	     */		fdtox	%f12,%f6
/* 0x182c	     */		std	%f6,[%sp+280]
/* 0x1830	     */		fdtox	%f10,%f10
/* 0x1834	     */		std	%f10,[%sp+304]
/* 0x1838	     */		fdtox	%f8,%f8
/* 0x183c	     */		std	%f8,[%sp+296]
/* 0x1840	     */		ldd	[%o7+8],%f0
/* 0x1844	     */		fxnor	%f30,%f0,%f6
/* 0x1848	     */		ldx	[%sp+296],%g3
/* 0x184c	     */		fitod	%f6,%f8
/* 0x1850	     */		ldx	[%sp+304],%g2
/* 0x1854	     */		fitod	%f7,%f6
/* 0x1858	     */		add	%g2,%g3,%g2
/* 0x185c	     */		fsubd	%f4,%f8,%f8
/* 0x1860	     */		fsubd	%f4,%f6,%f6
/* 0x1864	     */		fmuld	%f8,%f16,%f10
/* 0x1868	     */		fmuld	%f6,%f16,%f12
/* 0x186c	     */		fmuld	%f8,%f14,%f8
/* 0x1870	     */		fmuld	%f6,%f14,%f6
/* 0x1874	     */		faddd	%f10,%f2,%f10
/* 0x1878	     */		faddd	%f12,%f2,%f12
/* 0x187c	     */		fdtox	%f8,%f8
/* 0x1880	     */		std	%f8,[%sp+272]
/* 0x1884	     */		fdtox	%f6,%f6
/* 0x1888	     */		std	%f6,[%sp+256]
/* 0x188c	     */		fdtox	%f10,%f8
/* 0x1890	     */		std	%f8,[%sp+264]
/* 0x1894	     */		fdtox	%f12,%f6
/* 0x1898	     */		std	%f6,[%sp+248]
/* 0x189c	     */		ldd	[%o7+16],%f0
/* 0x18a0	     */		fxnor	%f30,%f0,%f6
/* 0x18a4	     */		fitod	%f6,%f8
/* 0x18a8	     */		fitod	%f7,%f6
/* 0x18ac	     */		fsubd	%f4,%f8,%f8
/* 0x18b0	     */		fsubd	%f4,%f6,%f6
/* 0x18b4	     */		fmuld	%f8,%f16,%f10
/* 0x18b8	     */		fmuld	%f6,%f16,%f12
/* 0x18bc	     */		fmuld	%f8,%f14,%f8
/* 0x18c0	     */		fmuld	%f6,%f14,%f6
/* 0x18c4	     */		faddd	%f10,%f2,%f10
/* 0x18c8	     */		faddd	%f12,%f2,%f12
/* 0x18cc	     */		fdtox	%f8,%f8
/* 0x18d0	     */		std	%f8,[%sp+240]
/* 0x18d4	     */		fdtox	%f6,%f6
/* 0x18d8	     */		std	%f6,[%sp+224]
/* 0x18dc	     */		fdtox	%f10,%f8
/* 0x18e0	     */		std	%f8,[%sp+232]
/* 0x18e4	     */		fdtox	%f12,%f6
/* 0x18e8	     */		std	%f6,[%sp+216]
/* 0x18ec	     */		ldd	[%o7+24],%f0
/* 0x18f0	     */		fxnor	%f30,%f0,%f6
/* 0x18f4	     */		fitod	%f6,%f8
/* 0x18f8	     */		fitod	%f7,%f6
/* 0x18fc	     */		fsubd	%f4,%f8,%f8
/* 0x1900	     */		fsubd	%f4,%f6,%f6
/* 0x1904	     */		fmuld	%f8,%f16,%f10
/* 0x1908	     */		fmuld	%f6,%f16,%f12
/* 0x190c	     */		fmuld	%f8,%f14,%f8
/* 0x1910	     */		fmuld	%f6,%f14,%f6
/* 0x1914	     */		faddd	%f10,%f2,%f10
/* 0x1918	     */		faddd	%f12,%f2,%f12
/* 0x191c	     */		fdtox	%f8,%f8
/* 0x1920	     */		std	%f8,[%sp+208]
/* 0x1924	     */		fdtox	%f6,%f6
/* 0x1928	     */		std	%f6,[%sp+192]
/* 0x192c	     */		fdtox	%f10,%f8
/* 0x1930	     */		std	%f8,[%sp+200]
/* 0x1934	     */		fdtox	%f12,%f6
/* 0x1938	     */		std	%f6,[%sp+184]
/* 0x193c	     */		ldd	[%o7+32],%f0
/* 0x1940	     */		fxnor	%f30,%f0,%f6
/* 0x1944	 281 */		fitod	%f6,%f8
/* 0x1948	     */		fitod	%f7,%f6
/* 0x194c	     */		fsubd	%f4,%f8,%f8
/* 0x1950	     */		fsubd	%f4,%f6,%f6
/* 0x1954	     */		fmuld	%f8,%f16,%f10
/* 0x1958	     */		fmuld	%f6,%f16,%f12
/* 0x195c	     */		fmuld	%f8,%f14,%f8
/* 0x1960	     */		fmuld	%f6,%f14,%f6
/* 0x1964	     */		faddd	%f10,%f2,%f10
/* 0x1968	     */		faddd	%f12,%f2,%f12
/* 0x196c	     */		fdtox	%f8,%f8
/* 0x1970	     */		std	%f8,[%sp+176]
/* 0x1974	     */		fdtox	%f6,%f6
/* 0x1978	     */		std	%f6,[%sp+160]
/* 0x197c	     */		fdtox	%f10,%f8
/* 0x1980	     */		std	%f8,[%sp+168]
/* 0x1984	     */		fdtox	%f12,%f6
/* 0x1988	     */		std	%f6,[%sp+152]
/* 0x198c	     */		ldd	[%o7+40],%f0
/* 0x1990	     */		fxnor	%f30,%f0,%f6
/* 0x1994	     */		fitod	%f6,%f8
/* 0x1998	     */		fitod	%f7,%f6
/* 0x199c	     */		fsubd	%f4,%f8,%f8
/* 0x19a0	     */		fsubd	%f4,%f6,%f6
/* 0x19a4	     */		fmuld	%f8,%f16,%f10
/* 0x19a8	     */		fmuld	%f6,%f16,%f12
/* 0x19ac	     */		fmuld	%f8,%f14,%f8
/* 0x19b0	     */		fmuld	%f6,%f14,%f6
/* 0x19b4	     */		faddd	%f10,%f2,%f10
/* 0x19b8	     */		faddd	%f12,%f2,%f12
/* 0x19bc	     */		fdtox	%f8,%f8
/* 0x19c0	     */		std	%f8,[%sp+144]
/* 0x19c4	     */		fdtox	%f6,%f6
/* 0x19c8	     */		std	%f6,[%sp+128]
/* 0x19cc	     */		fdtox	%f10,%f8
/* 0x19d0	     */		std	%f8,[%sp+136]
/* 0x19d4	     */		fdtox	%f12,%f6
/* 0x19d8	     */		std	%f6,[%sp+120]
/* 0x19dc	     */		ldd	[%o7+48],%f0
/* 0x19e0	     */		fxnor	%f30,%f0,%f6
/* 0x19e4	     */		fitod	%f6,%f8
/* 0x19e8	     */		fitod	%f7,%f6
/* 0x19ec	     */		fsubd	%f4,%f8,%f8
/* 0x19f0	     */		fsubd	%f4,%f6,%f12
/* 0x19f4	     */		fmuld	%f8,%f16,%f6
/* 0x19f8	     */		fmuld	%f8,%f14,%f8
/* 0x19fc	     */		fmuld	%f12,%f14,%f10
/* 0x1a00	     */		faddd	%f6,%f2,%f6
/* 0x1a04	     */		fdtox	%f8,%f8
/* 0x1a08	     */		std	%f8,[%sp+112]
/* 0x1a0c	     */		fdtox	%f10,%f0
/* 0x1a10	     */		fdtox	%f6,%f6
/* 0x1a14	     */		std	%f6,[%sp+104]
/* 0x1a18	     */		ld	[%i1],%g3
/* 0x1a1c	     */		fmuld	%f12,%f16,%f6
/* 0x1a20	 282 */		ld	[%i1+4],%g5
/* 0x1a24	     */		add	%g2,%g3,%g2
/* 0x1a28	     */		ld	[%i1+8],%o4
/* 0x1a2c	     */		sub	%g2,%o0,%o0
/* 0x1a30	     */		st	%o0,[%i4]
/* 0x1a34	 281 */		faddd	%f6,%f2,%f6
/* 0x1a38	 282 */		ldx	[%sp+280],%g4
/* 0x1a3c	     */		srlx	%o0,32,%g2
/* 0x1a40	     */		ldx	[%sp+288],%g3
/* 0x1a44	     */		ld	[%i1+20],%o2
/* 0x1a48	 281 */		fdtox	%f6,%f6
/* 0x1a4c	 282 */		add	%g3,%g4,%g3
/* 0x1a50	     */		add	%g3,%g5,%g3
/* 0x1a54	     */		ld	[%i1+12],%g5
/* 0x1a58	     */		add	%g3,%g2,%g2
/* 0x1a5c	     */		ld	[%i1+16],%g3
/* 0x1a60	     */		sub	%g2,%o3,%g2
/* 0x1a64	     */		st	%g2,[%i4+4]
/* 0x1a68	     */		ldx	[%sp+264],%o1
/* 0x1a6c	     */		srlx	%g2,32,%g2
/* 0x1a70	     */		ldx	[%sp+272],%o0
/* 0x1a74	     */		add	%o0,%o1,%o0
/* 0x1a78	     */		ld	[%i1+24],%o1
/* 0x1a7c	     */		add	%o0,%o4,%g4
/* 0x1a80	     */		ld	[%i1+28],%o0
/* 0x1a84	     */		add	%g4,%g2,%g4

!  283		      !      ADD_S64_U32_E_8(8);

/* 0x1a88	 283 */		ld	[%i1+32],%g2
/* 0x1a8c	 282 */		sub	%g4,%o3,%g4
/* 0x1a90	     */		st	%g4,[%i4+8]
/* 0x1a94	     */		ldx	[%sp+248],%o4
/* 0x1a98	     */		srlx	%g4,32,%g4
/* 0x1a9c	     */		ldx	[%sp+256],%o3
/* 0x1aa0	     */		ldd	[%o7+56],%f8
/* 0x1aa4	     */		add	%o3,%o4,%o3
/* 0x1aa8	     */		ldx	[%sp+96],%o7
/* 0x1aac	     */		add	%o3,%g5,%g5
/* 0x1ab0	     */		fxnor	%f30,%f8,%f8
/* 0x1ab4	 281 */		std	%f6,[%sp+360]
/* 0x1ab8	 282 */		add	%g5,%g4,%o4
/* 0x1abc	 281 */		std	%f0,[%sp+368]
/* 0x1ac0	 282 */		sub	%o4,%o7,%o4
/* 0x1ac4	     */		st	%o4,[%i4+12]
/* 0x1ac8	 281 */		fitod	%f8,%f10
/* 0x1acc	 282 */		ldx	[%sp+232],%o7
/* 0x1ad0	     */		srlx	%o4,32,%o4
/* 0x1ad4	 281 */		fitod	%f9,%f8
/* 0x1ad8	 282 */		ldx	[%sp+240],%g5
/* 0x1adc	 281 */		fsubd	%f4,%f10,%f10
/* 0x1ae0	 283 */		ld	[%i1+44],%g4
/* 0x1ae4	 282 */		add	%g5,%o7,%g5
/* 0x1ae8	     */		ldx	[%sp+96],%o7
/* 0x1aec	 281 */		fsubd	%f4,%f8,%f6
/* 0x1af0	 282 */		add	%g5,%g3,%g3
/* 0x1af4	 283 */		ld	[%i1+48],%g5
/* 0x1af8	 282 */		add	%g3,%o4,%g3
/* 0x1afc	 281 */		fmuld	%f10,%f16,%f8
/* 0x1b00	 283 */		stx	%g4,[%sp+320]
/* 0x1b04	 282 */		sub	%g3,%o7,%g3
/* 0x1b08	     */		st	%g3,[%i4+16]
/* 0x1b0c	 281 */		fmuld	%f10,%f14,%f10
/* 0x1b10	 282 */		stx	%g5,[%sp+312]
/* 0x1b14	     */		srlx	%g3,32,%g3
/* 0x1b18	 281 */		fmuld	%f6,%f16,%f12
/* 0x1b1c	 282 */		ldx	[%sp+224],%o7
/* 0x1b20	 281 */		faddd	%f8,%f2,%f8
/* 0x1b24	     */		fmuld	%f6,%f14,%f6
/* 0x1b28	 282 */		ldx	[%sp+216],%g5
/* 0x1b2c	 281 */		fdtox	%f10,%f10
/* 0x1b30	 283 */		ldx	[%sp+368],%o4
/* 0x1b34	 282 */		add	%o7,%g5,%g5
/* 0x1b38	 283 */		ldx	[%sp+360],%g4
/* 0x1b3c	 281 */		fdtox	%f8,%f8
/* 0x1b40	 282 */		add	%g5,%o2,%o2
/* 0x1b44	 283 */		ld	[%i1+52],%g5
/* 0x1b48	 281 */		fdtox	%f6,%f6
/* 0x1b4c	 282 */		add	%o2,%g3,%g3
/* 0x1b50	     */		ldx	[%sp+96],%o2
/* 0x1b54	 283 */		add	%o4,%g4,%g4
/* 0x1b58	 281 */		std	%f10,[%sp+352]
/* 0x1b5c	 283 */		add	%g4,%g5,%o4
/* 0x1b60	 281 */		faddd	%f12,%f2,%f10
/* 0x1b64	 282 */		sub	%g3,%o2,%g3
/* 0x1b68	     */		st	%g3,[%i4+20]
/* 0x1b6c	     */		ldx	[%sp+200],%o7
/* 0x1b70	     */		srlx	%g3,32,%g3
/* 0x1b74	     */		ldx	[%sp+208],%o2
/* 0x1b78	 281 */		std	%f8,[%sp+344]
/* 0x1b7c	     */		fdtox	%f10,%f8
/* 0x1b80	 282 */		add	%o2,%o7,%o2
/* 0x1b84	 281 */		std	%f6,[%sp+336]
/* 0x1b88	 282 */		add	%o2,%o1,%g4
/* 0x1b8c	     */		ldx	[%sp+96],%o1
/* 0x1b90	     */		add	%g4,%g3,%g3
/* 0x1b94	 283 */		ldx	[%sp+352],%g5
/* 0x1b98	 282 */		sub	%g3,%o1,%g3
/* 0x1b9c	     */		st	%g3,[%i4+24]
/* 0x1ba0	 283 */		ldx	[%sp+344],%o7
/* 0x1ba4	 282 */		srlx	%g3,32,%g3
/* 0x1ba8	 281 */		std	%f8,[%sp+328]
/* 0x1bac	 282 */		ldx	[%sp+192],%o1
/* 0x1bb0	 283 */		add	%g5,%o7,%o2
/* 0x1bb4	 282 */		ldx	[%sp+184],%g4
/* 0x1bb8	 283 */		ld	[%i1+56],%g5
/* 0x1bbc	 282 */		add	%o1,%g4,%g4
/* 0x1bc0	     */		ldx	[%sp+96],%o7
/* 0x1bc4	 283 */		add	%o2,%g5,%o2
/* 0x1bc8	 282 */		add	%g4,%o0,%g4
/* 0x1bcc	 283 */		ldx	[%sp+336],%o1
/* 0x1bd0	     */		ldx	[%sp+328],%g5
/* 0x1bd4	 282 */		add	%g4,%g3,%g3
/* 0x1bd8	 283 */		ld	[%i1+60],%g4
/* 0x1bdc	 282 */		sub	%g3,%o7,%g3
/* 0x1be0	 283 */		add	%o1,%g5,%g5
/* 0x1be4	 282 */		st	%g3,[%i4+28]
/* 0x1be8	 283 */		add	%g5,%g4,%o1
/* 0x1bec	     */		ldx	[%sp+176],%o0
/* 0x1bf0	     */		ldx	[%sp+168],%g4
/* 0x1bf4	     */		srlx	%g3,32,%g3
/* 0x1bf8	     */		ld	[%i1+36],%o5
/* 0x1bfc	     */		add	%o0,%g4,%g4
/* 0x1c00	     */		ld	[%i1+40],%o3
/* 0x1c04	     */		add	%g4,%g2,%g2
/* 0x1c08	     */		add	%g2,%g3,%g2
/* 0x1c0c	     */		sub	%g2,%o7,%g2
/* 0x1c10	     */		st	%g2,[%i4+32]
/* 0x1c14	     */		ldx	[%sp+160],%g5
/* 0x1c18	     */		srlx	%g2,32,%g2
/* 0x1c1c	     */		ldx	[%sp+152],%g4
/* 0x1c20	     */		add	%g5,%g4,%g3
/* 0x1c24	     */		add	%g3,%o5,%g3
/* 0x1c28	     */		add	%g3,%g2,%g2
/* 0x1c2c	     */		sub	%g2,%o7,%g2
/* 0x1c30	     */		st	%g2,[%i4+36]
/* 0x1c34	     */		ldx	[%sp+144],%g5
/* 0x1c38	     */		srlx	%g2,32,%g2
/* 0x1c3c	     */		ldx	[%sp+136],%g4
/* 0x1c40	     */		add	%g5,%g4,%g3
/* 0x1c44	     */		ldx	[%sp+320],%g5
/* 0x1c48	     */		add	%g3,%o3,%o3
/* 0x1c4c	     */		add	%o3,%g2,%o3
/* 0x1c50	     */		sub	%o3,%o7,%o3
/* 0x1c54	     */		st	%o3,[%i4+40]
/* 0x1c58	     */		ldx	[%sp+128],%g4
/* 0x1c5c	     */		srlx	%o3,32,%o3
/* 0x1c60	     */		ldx	[%sp+120],%g3
/* 0x1c64	     */		add	%g4,%g3,%g2
/* 0x1c68	     */		add	%g2,%g5,%g2
/* 0x1c6c	     */		ldx	[%sp+312],%g5
/* 0x1c70	     */		add	%g2,%o3,%o3
/* 0x1c74	     */		sub	%o3,%o7,%o3
/* 0x1c78	     */		st	%o3,[%i4+44]
/* 0x1c7c	     */		ldx	[%sp+112],%g4
/* 0x1c80	     */		srlx	%o3,32,%o3
/* 0x1c84	     */		ldx	[%sp+104],%g3
/* 0x1c88	     */		add	%g4,%g3,%g2
/* 0x1c8c	     */		add	%g2,%g5,%g2
/* 0x1c90	     */		add	%g2,%o3,%o3
/* 0x1c94	     */		sub	%o3,%o7,%o3
/* 0x1c98	     */		st	%o3,[%i4+48]
/* 0x1c9c	     */		srlx	%o3,32,%o3
/* 0x1ca0	     */		add	%o4,%o3,%g2
/* 0x1ca4	     */		sub	%g2,%o7,%g2
/* 0x1ca8	     */		st	%g2,[%i4+52]
/* 0x1cac	     */		srlx	%g2,32,%g2
/* 0x1cb0	     */		add	%o2,%g2,%g2
/* 0x1cb4	     */		sub	%g2,%o7,%g2
/* 0x1cb8	     */		st	%g2,[%i4+56]
/* 0x1cbc	     */		srlx	%g2,32,%g2
/* 0x1cc0	     */		add	%o1,%g2,%g2
/* 0x1cc4	     */		sub	%g2,%o7,%g2
/* 0x1cc8	     */		st	%g2,[%i4+60]

!  285		      !      return uc;

/* 0x1ccc	 285 */		srlx	%g2,32,%i0
/* 0x1cd0	     */		ret	! Result =  %i0
/* 0x1cd4	     */		restore	%g0,%g0,%g0
                       .L77000106:

!  287		      !    } else {
!  288		      !      DEF_VARS(2*BUFF_SIZE);
!  289		      !      t_d64 d0, d1, db;
!  290		      !      t_u32 uc = 0;
!  291		      !      t_s32 i;
!  293		      !      da = (t_d64)(a &  A_MASK);
!  294		      !      db = (t_d64)(a &~ A_MASK);

/* 0x1cd8	 294 */		or	%g0,%g2,%g3
/* 0x1cdc	 293 */		andn	%o0,%g2,%g2
/* 0x1ce0	     */		st	%g2,[%sp+316]
/* 0x1ce4	 288 */		fmovd	%f30,%f14
/* 0x1ce8	 294 */		and	%o0,%g3,%g2
/* 0x1cec	     */		st	%g2,[%sp+312]
/* 0x1cf0	 290 */		or	%g0,0,%g4

!  296		      !#pragma pipeloop(0)
!  297		      !      for (i = 0; i < (n+1)/2; i ++) {

/* 0x1cf4	 297 */		add	%o2,1,%g2
/* 0x1cf8	 293 */		ldd	[%o1],%f6
/* 0x1cfc	 285 */		sethi	%hi(0x2c00),%g1
/* 0x1d00	 297 */		srl	%g2,31,%g3
/* 0x1d04	 285 */		xor	%g1,-8,%g1
/* 0x1d08	 288 */		ldd	[%o1+8],%f22
/* 0x1d0c	 297 */		add	%g2,%g3,%g2
/* 0x1d10	 293 */		fmovs	%f6,%f8
/* 0x1d14	 285 */		add	%g1,%fp,%g5
/* 0x1d18	 294 */		fmovs	%f6,%f10
/* 0x1d1c	 293 */		ld	[%sp+316],%f9
/* 0x1d20	 297 */		sra	%g2,1,%o0
/* 0x1d24	 294 */		ld	[%sp+312],%f11
/* 0x1d28	 297 */		cmp	%o0,0
/* 0x1d2c	 285 */		sethi	%hi(0x2800),%g1
/* 0x1d30	 293 */		fsubd	%f8,%f6,%f20
/* 0x1d34	 285 */		xor	%g1,-1024,%g1
/* 0x1d38	     */		ldd	[%o1+16],%f16
/* 0x1d3c	 294 */		fsubd	%f10,%f6,%f18
/* 0x1d40	 297 */		ble,pt	%icc,.L900000193
/* 0x1d44	     */		cmp	%o3,0
/* 0x1d48	 285 */		add	%g1,%fp,%g3
/* 0x1d4c	     */		sethi	%hi(0x2800),%g1
/* 0x1d50	     */		xor	%g1,-1016,%g1
/* 0x1d54	     */		or	%g0,%o7,%o1
/* 0x1d58	     */		add	%g1,%fp,%g2
/* 0x1d5c	     */		sethi	%hi(0x2800),%g1
/* 0x1d60	     */		xor	%g1,-1008,%g1
/* 0x1d64	     */		sub	%o0,1,%o2
/* 0x1d68	 297 */		cmp	%o0,7
/* 0x1d6c	     */		or	%g0,0,%i0
/* 0x1d70	 285 */		add	%g1,%fp,%o7
/* 0x1d74	 297 */		bl,pn	%icc,.L77000107
/* 0x1d78	     */		add	%o1,8,%o0
/* 0x1d7c	     */		ldd	[%o1],%f2

!  298		      !        MUL_U32_S64_2_E(i);

/* 0x1d80	 298 */		add	%i2,16,%i1
/* 0x1d84	 297 */		sub	%o2,2,%o1
/* 0x1d88	     */		ldd	[%o0],%f0
/* 0x1d8c	 298 */		or	%g0,4,%i0
/* 0x1d90	     */		add	%i1,8,%o0
/* 0x1d94	     */		fxnor	%f30,%f2,%f6
/* 0x1d98	     */		ldd	[%i2+16],%f4
/* 0x1d9c	     */		fxnor	%f30,%f0,%f12
/* 0x1da0	     */		ldd	[%i2+24],%f0
/* 0x1da4	     */		fitod	%f7,%f2
/* 0x1da8	     */		fitod	%f6,%f6
/* 0x1dac	     */		fxnor	%f30,%f4,%f10
/* 0x1db0	     */		fsubd	%f22,%f2,%f2
/* 0x1db4	     */		fxnor	%f30,%f0,%f8
/* 0x1db8	     */		fitod	%f13,%f4
/* 0x1dbc	     */		fsubd	%f22,%f6,%f6
/* 0x1dc0	     */		fmuld	%f2,%f20,%f0
                       .L900000185:
/* 0x1dc4	 298 */		fitod	%f11,%f26
/* 0x1dc8	     */		add	%i0,3,%i0
/* 0x1dcc	     */		add	%o7,96,%o7
/* 0x1dd0	     */		fmuld	%f2,%f18,%f32
/* 0x1dd4	     */		fsubd	%f22,%f4,%f4
/* 0x1dd8	     */		add	%o0,8,%o0
/* 0x1ddc	     */		cmp	%i0,%o1
/* 0x1de0	     */		fmuld	%f6,%f18,%f30
/* 0x1de4	     */		ldd	[%o0],%f2
/* 0x1de8	     */		add	%g2,96,%g2
/* 0x1dec	     */		add	%g3,96,%g3
/* 0x1df0	     */		fdtox	%f0,%f24
/* 0x1df4	     */		fmuld	%f6,%f20,%f28
/* 0x1df8	     */		faddd	%f32,%f16,%f0
/* 0x1dfc	     */		add	%g5,96,%g5
/* 0x1e00	     */		faddd	%f30,%f16,%f30
/* 0x1e04	     */		fitod	%f12,%f6
/* 0x1e08	     */		fdtox	%f28,%f28
/* 0x1e0c	     */		fdtox	%f30,%f12
/* 0x1e10	     */		std	%f28,[%g5-96]
/* 0x1e14	     */		std	%f12,[%g3-96]
/* 0x1e18	     */		fdtox	%f0,%f0
/* 0x1e1c	     */		fsubd	%f22,%f6,%f28
/* 0x1e20	     */		std	%f24,[%g2-96]
/* 0x1e24	     */		fxnor	%f14,%f2,%f12
/* 0x1e28	     */		fmuld	%f4,%f20,%f24
/* 0x1e2c	     */		std	%f0,[%o7-96]
/* 0x1e30	     */		fitod	%f9,%f2
/* 0x1e34	     */		fmuld	%f4,%f18,%f4
/* 0x1e38	     */		fsubd	%f22,%f26,%f0
/* 0x1e3c	     */		fmuld	%f28,%f18,%f26
/* 0x1e40	     */		add	%o0,8,%o0
/* 0x1e44	     */		ldd	[%o0],%f6
/* 0x1e48	     */		fdtox	%f24,%f24
/* 0x1e4c	     */		faddd	%f4,%f16,%f4
/* 0x1e50	     */		fmuld	%f28,%f20,%f28
/* 0x1e54	     */		faddd	%f26,%f16,%f26
/* 0x1e58	     */		fitod	%f10,%f10
/* 0x1e5c	     */		fdtox	%f28,%f28
/* 0x1e60	     */		fdtox	%f26,%f26
/* 0x1e64	     */		std	%f28,[%g5-64]
/* 0x1e68	     */		std	%f26,[%g3-64]
/* 0x1e6c	     */		fdtox	%f4,%f4
/* 0x1e70	     */		fsubd	%f22,%f10,%f28
/* 0x1e74	     */		std	%f24,[%g2-64]
/* 0x1e78	     */		fxnor	%f14,%f6,%f10
/* 0x1e7c	     */		fmuld	%f0,%f20,%f26
/* 0x1e80	     */		std	%f4,[%o7-64]
/* 0x1e84	     */		fitod	%f13,%f4
/* 0x1e88	     */		fmuld	%f0,%f18,%f0
/* 0x1e8c	     */		fsubd	%f22,%f2,%f2
/* 0x1e90	     */		fmuld	%f28,%f18,%f6
/* 0x1e94	     */		add	%o0,8,%o0
/* 0x1e98	     */		ldd	[%o0],%f24
/* 0x1e9c	     */		fdtox	%f26,%f26
/* 0x1ea0	     */		faddd	%f0,%f16,%f0
/* 0x1ea4	     */		fmuld	%f28,%f20,%f28
/* 0x1ea8	     */		faddd	%f6,%f16,%f30
/* 0x1eac	     */		fitod	%f8,%f6
/* 0x1eb0	     */		fdtox	%f28,%f28
/* 0x1eb4	     */		fdtox	%f30,%f8
/* 0x1eb8	     */		std	%f28,[%g5-32]
/* 0x1ebc	     */		std	%f8,[%g3-32]
/* 0x1ec0	     */		fdtox	%f0,%f0
/* 0x1ec4	     */		fsubd	%f22,%f6,%f6
/* 0x1ec8	     */		std	%f26,[%g2-32]
/* 0x1ecc	     */		fxnor	%f14,%f24,%f8
/* 0x1ed0	     */		std	%f0,[%o7-32]
/* 0x1ed4	     */		ble,pt	%icc,.L900000185
/* 0x1ed8	     */		fmuld	%f2,%f20,%f0
                       .L900000188:
/* 0x1edc	 298 */		fitod	%f12,%f24
/* 0x1ee0	     */		fmuld	%f6,%f18,%f26
/* 0x1ee4	     */		add	%g5,128,%g5
/* 0x1ee8	     */		fitod	%f10,%f12
/* 0x1eec	     */		fmuld	%f6,%f20,%f28
/* 0x1ef0	     */		add	%g3,128,%g3
/* 0x1ef4	     */		fsubd	%f22,%f4,%f4
/* 0x1ef8	     */		fmuld	%f2,%f18,%f2
/* 0x1efc	     */		add	%g2,128,%g2
/* 0x1f00	     */		fsubd	%f22,%f24,%f24
/* 0x1f04	     */		add	%o7,128,%o7
/* 0x1f08	     */		cmp	%i0,%o2
/* 0x1f0c	     */		faddd	%f26,%f16,%f6
/* 0x1f10	     */		add	%o0,8,%o1
/* 0x1f14	     */		fdtox	%f28,%f28
/* 0x1f18	     */		std	%f28,[%g5-128]
/* 0x1f1c	     */		fmuld	%f4,%f18,%f30
/* 0x1f20	     */		faddd	%f2,%f16,%f2
/* 0x1f24	     */		fmuld	%f24,%f18,%f26
/* 0x1f28	     */		fitod	%f11,%f10
/* 0x1f2c	     */		fmuld	%f24,%f20,%f28
/* 0x1f30	     */		fsubd	%f22,%f12,%f24
/* 0x1f34	     */		fmuld	%f4,%f20,%f4
/* 0x1f38	     */		fdtox	%f6,%f6
/* 0x1f3c	     */		std	%f6,[%g3-128]
/* 0x1f40	     */		fdtox	%f0,%f0
/* 0x1f44	     */		std	%f0,[%g2-128]
/* 0x1f48	     */		fitod	%f8,%f6
/* 0x1f4c	     */		fmuld	%f24,%f18,%f12
/* 0x1f50	     */		fdtox	%f2,%f0
/* 0x1f54	     */		std	%f0,[%o7-128]
/* 0x1f58	     */		fmuld	%f24,%f20,%f24
/* 0x1f5c	     */		fsubd	%f22,%f10,%f10
/* 0x1f60	     */		faddd	%f26,%f16,%f26
/* 0x1f64	     */		fitod	%f9,%f2
/* 0x1f68	     */		fsubd	%f22,%f6,%f6
/* 0x1f6c	     */		fmuld	%f10,%f18,%f0
/* 0x1f70	     */		fdtox	%f28,%f28
/* 0x1f74	     */		std	%f28,[%g5-96]
/* 0x1f78	     */		fmuld	%f10,%f20,%f10
/* 0x1f7c	     */		faddd	%f30,%f16,%f8
/* 0x1f80	     */		fdtox	%f26,%f26
/* 0x1f84	     */		std	%f26,[%g3-96]
/* 0x1f88	     */		fdtox	%f4,%f4
/* 0x1f8c	     */		std	%f4,[%g2-96]
/* 0x1f90	     */		fsubd	%f22,%f2,%f2
/* 0x1f94	     */		fmuld	%f6,%f18,%f4
/* 0x1f98	     */		faddd	%f12,%f16,%f12
/* 0x1f9c	     */		fmuld	%f6,%f20,%f6
/* 0x1fa0	     */		fdtox	%f8,%f8
/* 0x1fa4	     */		std	%f8,[%o7-96]
/* 0x1fa8	     */		faddd	%f0,%f16,%f8
/* 0x1fac	     */		fmuld	%f2,%f18,%f0
/* 0x1fb0	     */		fdtox	%f24,%f24
/* 0x1fb4	     */		std	%f24,[%g5-64]
/* 0x1fb8	     */		fmuld	%f2,%f20,%f2
/* 0x1fbc	     */		fdtox	%f12,%f12
/* 0x1fc0	     */		std	%f12,[%g3-64]
/* 0x1fc4	     */		fdtox	%f10,%f10
/* 0x1fc8	     */		std	%f10,[%g2-64]
/* 0x1fcc	     */		faddd	%f4,%f16,%f4
/* 0x1fd0	     */		fdtox	%f8,%f8
/* 0x1fd4	     */		std	%f8,[%o7-64]
/* 0x1fd8	     */		faddd	%f0,%f16,%f0
/* 0x1fdc	     */		fdtox	%f6,%f6
/* 0x1fe0	     */		std	%f6,[%g5-32]
/* 0x1fe4	     */		fdtox	%f4,%f4
/* 0x1fe8	     */		std	%f4,[%g3-32]
/* 0x1fec	     */		fdtox	%f2,%f2
/* 0x1ff0	     */		std	%f2,[%g2-32]
/* 0x1ff4	     */		fdtox	%f0,%f0
/* 0x1ff8	     */		bg,pn	%icc,.L77000084
/* 0x1ffc	     */		std	%f0,[%o7-32]
                       .L77000107:
/* 0x2000	     */		ldd	[%o1],%f0
                       .L900000192:
/* 0x2004	     */		fxnor	%f14,%f0,%f2
/* 0x2008	 298 */		add	%i0,1,%i0
/* 0x200c	     */		add	%o1,8,%o1
/* 0x2010	     */		cmp	%i0,%o2
/* 0x2014	     */		fitod	%f2,%f0
/* 0x2018	     */		fitod	%f3,%f2
/* 0x201c	     */		fsubd	%f22,%f0,%f0
/* 0x2020	     */		fsubd	%f22,%f2,%f2
/* 0x2024	     */		fmuld	%f0,%f18,%f4
/* 0x2028	     */		fmuld	%f0,%f20,%f6
/* 0x202c	     */		fmuld	%f2,%f18,%f0
/* 0x2030	     */		faddd	%f4,%f16,%f4
/* 0x2034	     */		fmuld	%f2,%f20,%f2
/* 0x2038	     */		fdtox	%f6,%f6
/* 0x203c	     */		std	%f6,[%g5]
/* 0x2040	     */		add	%g5,32,%g5
/* 0x2044	     */		faddd	%f0,%f16,%f0
/* 0x2048	     */		fdtox	%f4,%f4
/* 0x204c	     */		std	%f4,[%g3]
/* 0x2050	     */		add	%g3,32,%g3
/* 0x2054	     */		fdtox	%f2,%f2
/* 0x2058	     */		std	%f2,[%g2]
/* 0x205c	     */		add	%g2,32,%g2
/* 0x2060	     */		fdtox	%f0,%f0
/* 0x2064	     */		std	%f0,[%o7]
/* 0x2068	     */		add	%o7,32,%o7
/* 0x206c	     */		ble,a,pt	%icc,.L900000192
/* 0x2070	     */		ldd	[%o1],%f0
                       .L77000084:

!  299		      !      }
!  301		      !#pragma pipeloop(0)
!  302		      !      for (i = 0; i < n; i ++) {

/* 0x2074	 302 */		cmp	%o3,0
                       .L900000193:
/* 0x2078	 302 */		ble,pt	%icc,.L77000089
/* 0x207c	     */		or	%g0,%g4,%i0
/* 0x2080	     */		sethi	%hi(0x2c00),%g1
/* 0x2084	     */		xor	%g1,-8,%g1
/* 0x2088	     */		or	%g0,%o4,%i1
/* 0x208c	     */		add	%g1,%fp,%g5
/* 0x2090	     */		sethi	%hi(0x2800),%g1
/* 0x2094	     */		xor	%g1,-1024,%g1
/* 0x2098	     */		or	%g0,0,%i2
/* 0x209c	     */		add	%g1,%fp,%g3
/* 0x20a0	     */		sub	%o3,1,%o7
/* 0x20a4	     */		or	%g0,%i4,%i0
/* 0x20a8	 298 */		cmp	%o3,7
/* 0x20ac	     */		bl,pn	%icc,.L77000108
/* 0x20b0	     */		sethi	%hi(0x2c00),%g1
/* 0x20b4	     */		sub	%o3,4,%o4

!  303		      !        ADD_S64_U32_E(i);

/* 0x20b8	 303 */		ld	[%i1],%o3
/* 0x20bc	     */		xor	%g1,-8,%g1
/* 0x20c0	     */		ld	[%i1+4],%o1
/* 0x20c4	     */		sethi	%hi(0x80000000),%o5
/* 0x20c8	     */		add	%g1,%fp,%g1
/* 0x20cc	     */		add	%i4,4,%i0
/* 0x20d0	     */		ldx	[%g1+8],%o2
/* 0x20d4	     */		sllx	%o5,32,%o5
/* 0x20d8	     */		add	%i1,8,%i1
/* 0x20dc	     */		ldx	[%g1],%o0
/* 0x20e0	     */		or	%g0,3,%i2
/* 0x20e4	     */		ldx	[%g1+24],%g2
/* 0x20e8	     */		add	%o0,%o2,%o0
/* 0x20ec	     */		ldx	[%g1+16],%o2
/* 0x20f0	     */		sethi	%hi(0x2800),%g1
/* 0x20f4	     */		xor	%g1,-992,%g1
/* 0x20f8	     */		add	%o0,%o3,%o0
/* 0x20fc	     */		add	%g1,%fp,%g3
/* 0x2100	     */		sethi	%hi(0x2800),%g1
/* 0x2104	     */		xor	%g1,-1000,%g1
/* 0x2108	     */		sub	%o0,%o5,%o0
/* 0x210c	     */		add	%g1,%fp,%g1
/* 0x2110	     */		add	%o2,%g2,%o2
/* 0x2114	     */		ldx	[%g1],%o3
/* 0x2118	     */		sethi	%hi(0x2800),%g1
/* 0x211c	     */		st	%o0,[%i4]
/* 0x2120	     */		srlx	%o0,32,%o0
/* 0x2124	     */		xor	%g1,-984,%g1
/* 0x2128	     */		add	%g1,%fp,%g5
                       .L900000165:
/* 0x212c	 303 */		ldx	[%g3],%g4
/* 0x2130	     */		srl	%o0,0,%g2
/* 0x2134	     */		add	%o2,%o1,%o0
/* 0x2138	     */		ld	[%i1],%o2
/* 0x213c	     */		add	%o0,%g2,%g2
/* 0x2140	     */		add	%i2,4,%i2
/* 0x2144	     */		ldx	[%g5],%o1
/* 0x2148	     */		add	%o3,%g4,%o0
/* 0x214c	     */		sub	%g2,%o5,%o3
/* 0x2150	     */		st	%o3,[%i0]
/* 0x2154	     */		srlx	%o3,32,%o3
/* 0x2158	     */		cmp	%i2,%o4
/* 0x215c	     */		ldx	[%g3+16],%g2
/* 0x2160	     */		add	%o0,%o2,%o0
/* 0x2164	     */		ld	[%i1+4],%o2
/* 0x2168	     */		add	%o0,%o3,%o3
/* 0x216c	     */		add	%i0,16,%i0
/* 0x2170	     */		ldx	[%g5+16],%o0
/* 0x2174	     */		add	%o1,%g2,%o1
/* 0x2178	     */		sub	%o3,%o5,%o3
/* 0x217c	     */		st	%o3,[%i0-12]
/* 0x2180	     */		srlx	%o3,32,%g4
/* 0x2184	     */		add	%i1,16,%i1
/* 0x2188	     */		ldx	[%g3+32],%g2
/* 0x218c	     */		add	%o1,%o2,%o2
/* 0x2190	     */		ld	[%i1-8],%o1
/* 0x2194	     */		add	%o2,%g4,%o3
/* 0x2198	     */		add	%g3,64,%g3
/* 0x219c	     */		ldx	[%g5+32],%o2
/* 0x21a0	     */		add	%o0,%g2,%o0
/* 0x21a4	     */		sub	%o3,%o5,%o3
/* 0x21a8	     */		st	%o3,[%i0-8]
/* 0x21ac	     */		srlx	%o3,32,%o3
/* 0x21b0	     */		add	%g5,64,%g5
/* 0x21b4	     */		ldx	[%g3-16],%g2
/* 0x21b8	     */		add	%o0,%o1,%o0
/* 0x21bc	     */		ld	[%i1-4],%o1
/* 0x21c0	     */		add	%o0,%o3,%o0
/* 0x21c4	     */		ldx	[%g5-16],%o3
/* 0x21c8	     */		add	%o2,%g2,%o2
/* 0x21cc	     */		sub	%o0,%o5,%o0
/* 0x21d0	     */		st	%o0,[%i0-4]
/* 0x21d4	     */		ble,pt	%icc,.L900000165
/* 0x21d8	     */		srlx	%o0,32,%o0
                       .L900000168:
/* 0x21dc	 303 */		srl	%o0,0,%o4
/* 0x21e0	     */		ldx	[%g3],%o0
/* 0x21e4	     */		add	%o2,%o1,%o1
/* 0x21e8	     */		ld	[%i1],%o2
/* 0x21ec	     */		add	%o1,%o4,%o1
/* 0x21f0	     */		add	%i0,8,%i0
/* 0x21f4	     */		sub	%o1,%o5,%o1
/* 0x21f8	     */		add	%o3,%o0,%o0
/* 0x21fc	     */		st	%o1,[%i0-8]
/* 0x2200	     */		srlx	%o1,32,%o1
/* 0x2204	     */		add	%o0,%o2,%o0
/* 0x2208	     */		add	%o0,%o1,%o0
/* 0x220c	     */		cmp	%i2,%o7
/* 0x2210	     */		sub	%o0,%o5,%o0
/* 0x2214	     */		st	%o0,[%i0-4]
/* 0x2218	     */		bg,pn	%icc,.L77000088
/* 0x221c	     */		srlx	%o0,32,%g4
/* 0x2220	     */		add	%i1,4,%i1
/* 0x2224	     */		add	%g3,16,%g3
                       .L77000108:
/* 0x2228	 303 */		ldx	[%g5],%o0
                       .L900000191:
/* 0x222c	 303 */		sethi	%hi(0x80000000),%o2
/* 0x2230	     */		ldx	[%g3],%o3
/* 0x2234	     */		add	%i2,1,%i2
/* 0x2238	     */		sllx	%o2,32,%o5
/* 0x223c	     */		ld	[%i1],%o2
/* 0x2240	     */		add	%g5,16,%g5
/* 0x2244	     */		srl	%g4,0,%o1
/* 0x2248	     */		add	%o0,%o3,%o0
/* 0x224c	     */		add	%o0,%o2,%o0
/* 0x2250	     */		add	%i1,4,%i1
/* 0x2254	     */		add	%o0,%o1,%o0
/* 0x2258	     */		add	%g3,16,%g3
/* 0x225c	     */		sub	%o0,%o5,%o0
/* 0x2260	     */		st	%o0,[%i0]
/* 0x2264	     */		cmp	%i2,%o7
/* 0x2268	     */		srlx	%o0,32,%g4
/* 0x226c	     */		add	%i0,4,%i0
/* 0x2270	     */		ble,a,pt	%icc,.L900000191
/* 0x2274	     */		ldx	[%g5],%o0
                       .L77000088:

!  304		      !      }
!  306		      !      return uc;

/* 0x2278	 306 */		or	%g0,%g4,%i0
                       .L77000089:
/* 0x227c	     */		ret	! Result =  %i0
/* 0x2280	     */		restore	%g0,%g0,%g0
/* 0x2284	   0 */		.type	mul_add,2
/* 0x2284	     */		.size	mul_add,(.-mul_add)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	4
!
! SUBROUTINE mul_add_inp
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global mul_add_inp
                       mul_add_inp:
/* 000000	     */		or	%g0,%o2,%g1
/* 0x0004	     */		or	%g0,%o0,%g3
/* 0x0008	     */		or	%g0,%o1,%g2
/* 0x000c	     */		or	%g0,%o3,%o4

!  307		      !    }
!  308		      !  }
!  309		      !}
!  311		      !/***************************************************************/
!  313		      !t_u32 mul_add_inp(t_u32 *x, t_u32 *y, int n, t_u32 a)
!  314		      !{
!  315		      !  return mul_add(x, x, y, n, a);

/* 0x0010	 315 */		or	%g0,%g1,%o3
/* 0x0014	     */		or	%g0,%g3,%o1
/* 0x0018	     */		or	%g0,%g2,%o2
/* 0x001c	     */		or	%g0,%o7,%g1
/* 0x0020	     */		call	mul_add	! params =  %o0 %o1 %o2 %o3 %o4	! Result = 	! (tail call)
/* 0x0024	     */		or	%g0,%g1,%o7
/* 0x0028	   0 */		.type	mul_add_inp,2
/* 0x0028	     */		.size	mul_add_inp,(.-mul_add_inp)

! Begin Disassembling Stabs
	.xstabs	".stab.index","Xa ; O ; P ; V=3.1 ; R=WorkShop Compilers 5.0 98/12/15 C 5.0",60,0,0,0	! (/usr/tmp/acompAAA52aaHc:1)
	.xstabs	".stab.index","/h/interzone/d3/nelsonb/nss_28x/ns/security/lib/crypto; /tools/ns/workshop-5.0/bin/../SC5.0/bin/cc -fast -xO5 -xrestrict=%%all -xdepend -xchip=ultra -xarch=v8plusa -KPIC -mt -S vis_32.il  mul_add.c -W0,-xp",52,0,0,0	! (/usr/tmp/acompAAA52aaHc:2)
! End Disassembling Stabs

! Begin Disassembling Ident
	.ident	"cg: WorkShop Compilers 5.0 99/08/12 Compiler Common 5.0 Patch 107357-05"	! (NO SOURCE LINE)
	.ident	"@(#)vis_proto.h\t1.3\t97/03/30 SMI"	! (/usr/tmp/acompAAA52aaHc:4)
	.ident	"acomp: WorkShop Compilers 5.0 98/12/15 C 5.0"	! (/usr/tmp/acompAAA52aaHc:12)
! End Disassembling Ident
