!  
! This Source Code Form is subject to the terms of the Mozilla Public
! License, v. 2.0. If a copy of the MPL was not distributed with this
! file, You can obtain one at http://mozilla.org/MPL/2.0/.

	.section	".text",#alloc,#execinstr
	.file	"montmulf.c"

	.section	".rodata",#alloc
	.global	TwoTo16
	.align	8
!
! CONSTANT POOL
!
	.global TwoTo16
TwoTo16:
	.word	1089470464
	.word	0
	.type	TwoTo16,#object
	.size	TwoTo16,8
	.global	TwoToMinus16
!
! CONSTANT POOL
!
	.global TwoToMinus16
TwoToMinus16:
	.word	1055916032
	.word	0
	.type	TwoToMinus16,#object
	.size	TwoToMinus16,8
	.global	Zero
!
! CONSTANT POOL
!
	.global Zero
Zero:
	.word	0
	.word	0
	.type	Zero,#object
	.size	Zero,8
	.global	TwoTo32
!
! CONSTANT POOL
!
	.global TwoTo32
TwoTo32:
	.word	1106247680
	.word	0
	.type	TwoTo32,#object
	.size	TwoTo32,8
	.global	TwoToMinus32
!
! CONSTANT POOL
!
	.global TwoToMinus32
TwoToMinus32:
	.word	1039138816
	.word	0
	.type	TwoToMinus32,#object
	.size	TwoToMinus32,8

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.register	%g3,#scratch
/* 000000	     */		.register	%g2,#scratch
/* 000000	   0 */		.align	8
!
! SUBROUTINE conv_d16_to_i32
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global conv_d16_to_i32
                       conv_d16_to_i32:
/* 000000	     */		save	%sp,-208,%sp
! FILE montmulf.c

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
!   12		      ! * The Original Code is SPARC optimized Montgomery multiply functions.
!   13		      ! *
!   14		      ! * The Initial Developer of the Original Code is Sun Microsystems Inc.
!   15		      ! * Portions created by Sun Microsystems Inc. are 
!   16		      ! * Copyright (C) 1999-2000 Sun Microsystems Inc.  All Rights Reserved.
!   17		      ! * 
!   18		      ! * Contributor(s):
!   19		      ! *	Netscape Communications Corporation
!   20		      ! * 
!   21		      ! * Alternatively, the contents of this file may be used under the
!   22		      ! * terms of the GNU General Public License Version 2 or later (the
!   23		      ! * "GPL"), in which case the provisions of the GPL are applicable 
!   24		      ! * instead of those above.	If you wish to allow use of your 
!   25		      ! * version of this file only under the terms of the GPL and not to
!   26		      ! * allow others to use your version of this file under the MPL,
!   27		      ! * indicate your decision by deleting the provisions above and
!   28		      ! * replace them with the notice and other provisions required by
!   29		      ! * the GPL.  If you do not delete the provisions above, a recipient
!   30		      ! * may use your version of this file under either the MPL or the
!   31		      ! * GPL.
!   34		      ! */
!   36		      !#define RF_INLINE_MACROS
!   38		      !static const double TwoTo16=65536.0;
!   39		      !static const double TwoToMinus16=1.0/65536.0;
!   40		      !static const double Zero=0.0;
!   41		      !static const double TwoTo32=65536.0*65536.0;
!   42		      !static const double TwoToMinus32=1.0/(65536.0*65536.0);
!   44		      !#ifdef RF_INLINE_MACROS
!   46		      !double upper32(double);
!   47		      !double lower32(double, double);
!   48		      !double mod(double, double, double);
!   50		      !void i16_to_d16_and_d32x4(const double * /*1/(2^16)*/, 
!   51		      !			  const double * /* 2^16*/,
!   52		      !			  const double * /* 0 */,
!   53		      !			  double *       /*result16*/, 
!   54		      !			  double *       /* result32 */,
!   55		      !			  float *  /*source - should be unsigned int*
!   56		      !		          	       converted to float* */);
!   58		      !#else
!   60		      !static double upper32(double x)
!   61		      !{
!   62		      !  return floor(x*TwoToMinus32);
!   63		      !}
!   65		      !static double lower32(double x, double y)
!   66		      !{
!   67		      !  return x-TwoTo32*floor(x*TwoToMinus32);
!   68		      !}
!   70		      !static double mod(double x, double oneoverm, double m)
!   71		      !{
!   72		      !  return x-m*floor(x*oneoverm);
!   73		      !}
!   75		      !#endif
!   78		      !static void cleanup(double *dt, int from, int tlen)
!   79		      !{
!   80		      ! int i;
!   81		      ! double tmp,tmp1,x,x1;
!   83		      ! tmp=tmp1=Zero;
!   84		      ! /* original code **
!   85		      ! for(i=2*from;i<2*tlen-2;i++)
!   86		      !   {
!   87		      !     x=dt[i];
!   88		      !     dt[i]=lower32(x,Zero)+tmp1;
!   89		      !     tmp1=tmp;
!   90		      !     tmp=upper32(x);
!   91		      !   }
!   92		      ! dt[tlen-2]+=tmp1;
!   93		      ! dt[tlen-1]+=tmp;
!   94		      ! **end original code ***/
!   95		      ! /* new code ***/
!   96		      ! for(i=2*from;i<2*tlen;i+=2)
!   97		      !   {
!   98		      !     x=dt[i];
!   99		      !     x1=dt[i+1];
!  100		      !     dt[i]=lower32(x,Zero)+tmp;
!  101		      !     dt[i+1]=lower32(x1,Zero)+tmp1;
!  102		      !     tmp=upper32(x);
!  103		      !     tmp1=upper32(x1);
!  104		      !   }
!  105		      !  /** end new code **/
!  106		      !}
!  109		      !void conv_d16_to_i32(unsigned int *i32, double *d16, long long *tmp, int ilen)
!  110		      !{
!  111		      !int i;
!  112		      !long long t, t1, a, b, c, d;
!  114		      ! t1=0;
!  115		      ! a=(long long)d16[0];

/* 0x0004	 115 */		ldd	[%i1],%f2

!  116		      ! b=(long long)d16[1];
!  117		      ! for(i=0; i<ilen-1; i++)

/* 0x0008	 117 */		sub	%i3,1,%o1
/* 0x000c	 110 */		or	%g0,%i0,%g1
/* 0x0010	 116 */		ldd	[%i1+8],%f4
/* 0x0014	 117 */		cmp	%o1,0
/* 0x0018	 114 */		or	%g0,0,%g5
/* 0x001c	 115 */		fdtox	%f2,%f2
/* 0x0020	     */		std	%f2,[%sp+2247]
/* 0x0024	 117 */		or	%g0,0,%o0
/* 0x0028	 116 */		fdtox	%f4,%f2
/* 0x002c	     */		std	%f2,[%sp+2239]
/* 0x0030	 110 */		sub	%o1,1,%o7
/* 0x0034	     */		or	%g0,%i1,%o4
/* 0x0038	     */		sethi	%hi(0xfc00),%o3
/* 0x003c	     */		or	%g0,-1,%o1
/* 0x0040	     */		or	%g0,2,%i1
/* 0x0044	     */		srl	%o1,0,%g3
/* 0x0048	     */		or	%g0,%o4,%g4
/* 0x004c	 116 */		ldx	[%sp+2239],%i2
/* 0x0050	     */		add	%o3,1023,%o5
/* 0x0054	 117 */		sub	%o7,1,%o2
/* 0x0058	 115 */		ldx	[%sp+2247],%i3
/* 0x005c	 117 */		ble,pt	%icc,.L900000113
/* 0x0060	     */		sethi	%hi(0xfc00),%g2
/* 0x0064	     */		add	%o7,1,%g2

!  118		      !   {
!  119		      !     c=(long long)d16[2*i+2];
!  120		      !     t1+=a&0xffffffff;
!  121		      !     t=(a>>32);
!  122		      !     d=(long long)d16[2*i+3];
!  123		      !     t1+=(b&0xffff)<<16;

/* 0x0068	 123 */		and	%i2,%o5,%i4
/* 0x006c	     */		sllx	%i4,16,%o1
/* 0x0070	 117 */		cmp	%g2,6
/* 0x0074	     */		bl,pn	%icc,.L77000134
/* 0x0078	     */		or	%g0,3,%i0
/* 0x007c	 119 */		ldd	[%o4+16],%f0
/* 0x0080	 120 */		and	%i3,%g3,%o3

!  124		      !     t+=(b>>16)+(t1>>32);

/* 0x0084	 124 */		srax	%i2,16,%i5
/* 0x0088	 117 */		add	%o3,%o1,%i4
/* 0x008c	 121 */		srax	%i3,32,%i3
/* 0x0090	 119 */		fdtox	%f0,%f0
/* 0x0094	     */		std	%f0,[%sp+2231]

!  125		      !     i32[i]=t1&0xffffffff;

/* 0x0098	 125 */		and	%i4,%g3,%l0
/* 0x009c	 117 */		or	%g0,72,%o3
/* 0x00a0	 122 */		ldd	[%g4+24],%f0
/* 0x00a4	 117 */		or	%g0,64,%o4
/* 0x00a8	     */		or	%g0,4,%o1

!  126		      !     t1=t;
!  127		      !     a=c;
!  128		      !     b=d;

/* 0x00ac	 128 */		or	%g0,5,%i0
/* 0x00b0	     */		or	%g0,4,%i1
/* 0x00b4	 119 */		ldx	[%sp+2231],%g2
/* 0x00b8	 122 */		fdtox	%f0,%f0
/* 0x00bc	 128 */		or	%g0,4,%o0
/* 0x00c0	 122 */		std	%f0,[%sp+2223]
/* 0x00c4	     */		ldd	[%g4+40],%f2
/* 0x00c8	 120 */		and	%g2,%g3,%i2
/* 0x00cc	 119 */		ldd	[%g4+32],%f0
/* 0x00d0	 121 */		srax	%g2,32,%g2
/* 0x00d4	 122 */		ldd	[%g4+56],%f4
/* 0x00d8	     */		fdtox	%f2,%f2
/* 0x00dc	     */		ldx	[%sp+2223],%g5
/* 0x00e0	 119 */		fdtox	%f0,%f0
/* 0x00e4	 125 */		st	%l0,[%g1]
/* 0x00e8	 124 */		srax	%i4,32,%l0
/* 0x00ec	 122 */		fdtox	%f4,%f4
/* 0x00f0	     */		std	%f2,[%sp+2223]
/* 0x00f4	 123 */		and	%g5,%o5,%i4
/* 0x00f8	 124 */		add	%i5,%l0,%i5
/* 0x00fc	 119 */		std	%f0,[%sp+2231]
/* 0x0100	 123 */		sllx	%i4,16,%i4
/* 0x0104	 124 */		add	%i3,%i5,%i3
/* 0x0108	 119 */		ldd	[%g4+48],%f2
/* 0x010c	 124 */		srax	%g5,16,%g5
/* 0x0110	 117 */		add	%i2,%i4,%i2
/* 0x0114	 122 */		ldd	[%g4+72],%f0
/* 0x0118	 117 */		add	%i2,%i3,%i4
/* 0x011c	 124 */		srax	%i4,32,%i5
/* 0x0120	 119 */		fdtox	%f2,%f2
/* 0x0124	 125 */		and	%i4,%g3,%i4
/* 0x0128	 122 */		ldx	[%sp+2223],%i2
/* 0x012c	 124 */		add	%g5,%i5,%g5
/* 0x0130	 119 */		ldx	[%sp+2231],%i3
/* 0x0134	 124 */		add	%g2,%g5,%g5
/* 0x0138	 119 */		std	%f2,[%sp+2231]
/* 0x013c	 122 */		std	%f4,[%sp+2223]
/* 0x0140	 119 */		ldd	[%g4+64],%f2
/* 0x0144	 125 */		st	%i4,[%g1+4]
                       .L900000108:
/* 0x0148	 122 */		ldx	[%sp+2223],%i4
/* 0x014c	 128 */		add	%o0,2,%o0
/* 0x0150	     */		add	%i0,4,%i0
/* 0x0154	 119 */		ldx	[%sp+2231],%l0
/* 0x0158	 117 */		add	%o3,16,%o3
/* 0x015c	 123 */		and	%i2,%o5,%g2
/* 0x0160	     */		sllx	%g2,16,%i5
/* 0x0164	 120 */		and	%i3,%g3,%g2
/* 0x0168	 122 */		ldd	[%g4+%o3],%f4
/* 0x016c	     */		fdtox	%f0,%f0
/* 0x0170	     */		std	%f0,[%sp+2223]
/* 0x0174	 124 */		srax	%i2,16,%i2
/* 0x0178	 117 */		add	%g2,%i5,%g2
/* 0x017c	 119 */		fdtox	%f2,%f0
/* 0x0180	 117 */		add	%o4,16,%o4
/* 0x0184	 119 */		std	%f0,[%sp+2231]
/* 0x0188	 117 */		add	%g2,%g5,%g2
/* 0x018c	 119 */		ldd	[%g4+%o4],%f2
/* 0x0190	 124 */		srax	%g2,32,%i5
/* 0x0194	 128 */		cmp	%o0,%o2
/* 0x0198	 121 */		srax	%i3,32,%g5
/* 0x019c	 124 */		add	%i2,%i5,%i2
/* 0x01a0	     */		add	%g5,%i2,%i5
/* 0x01a4	 117 */		add	%o1,4,%o1
/* 0x01a8	 125 */		and	%g2,%g3,%g2
/* 0x01ac	 127 */		or	%g0,%l0,%g5
/* 0x01b0	 125 */		st	%g2,[%g1+%o1]
/* 0x01b4	 128 */		add	%i1,4,%i1
/* 0x01b8	 122 */		ldx	[%sp+2223],%i2
/* 0x01bc	 119 */		ldx	[%sp+2231],%i3
/* 0x01c0	 117 */		add	%o3,16,%o3
/* 0x01c4	 123 */		and	%i4,%o5,%g2
/* 0x01c8	     */		sllx	%g2,16,%l0
/* 0x01cc	 120 */		and	%g5,%g3,%g2
/* 0x01d0	 122 */		ldd	[%g4+%o3],%f0
/* 0x01d4	     */		fdtox	%f4,%f4
/* 0x01d8	     */		std	%f4,[%sp+2223]
/* 0x01dc	 124 */		srax	%i4,16,%i4
/* 0x01e0	 117 */		add	%g2,%l0,%g2
/* 0x01e4	 119 */		fdtox	%f2,%f2
/* 0x01e8	 117 */		add	%o4,16,%o4
/* 0x01ec	 119 */		std	%f2,[%sp+2231]
/* 0x01f0	 117 */		add	%g2,%i5,%g2
/* 0x01f4	 119 */		ldd	[%g4+%o4],%f2
/* 0x01f8	 124 */		srax	%g2,32,%i5
/* 0x01fc	 121 */		srax	%g5,32,%g5
/* 0x0200	 124 */		add	%i4,%i5,%i4
/* 0x0204	     */		add	%g5,%i4,%g5
/* 0x0208	 117 */		add	%o1,4,%o1
/* 0x020c	 125 */		and	%g2,%g3,%g2
/* 0x0210	 128 */		ble,pt	%icc,.L900000108
/* 0x0214	     */		st	%g2,[%g1+%o1]
                       .L900000111:
/* 0x0218	 122 */		ldx	[%sp+2223],%o2
/* 0x021c	 123 */		and	%i2,%o5,%i4
/* 0x0220	 120 */		and	%i3,%g3,%g2
/* 0x0224	 123 */		sllx	%i4,16,%i4
/* 0x0228	 119 */		ldx	[%sp+2231],%i5
/* 0x022c	 128 */		cmp	%o0,%o7
/* 0x0230	 124 */		srax	%i2,16,%i2
/* 0x0234	 117 */		add	%g2,%i4,%g2
/* 0x0238	 122 */		fdtox	%f0,%f4
/* 0x023c	     */		std	%f4,[%sp+2223]
/* 0x0240	 117 */		add	%g2,%g5,%g5
/* 0x0244	 123 */		and	%o2,%o5,%l0
/* 0x0248	 124 */		srax	%g5,32,%l1
/* 0x024c	 120 */		and	%i5,%g3,%i4
/* 0x0250	 119 */		fdtox	%f2,%f0
/* 0x0254	 121 */		srax	%i3,32,%g2
/* 0x0258	 119 */		std	%f0,[%sp+2231]
/* 0x025c	 124 */		add	%i2,%l1,%i2
/* 0x0260	 123 */		sllx	%l0,16,%i3
/* 0x0264	 124 */		add	%g2,%i2,%i2
/* 0x0268	     */		srax	%o2,16,%o2
/* 0x026c	 117 */		add	%o1,4,%g2
/* 0x0270	     */		add	%i4,%i3,%o1
/* 0x0274	 125 */		and	%g5,%g3,%g5
/* 0x0278	     */		st	%g5,[%g1+%g2]
/* 0x027c	 119 */		ldx	[%sp+2231],%i3
/* 0x0280	 117 */		add	%o1,%i2,%o1
/* 0x0284	     */		add	%g2,4,%g2
/* 0x0288	 124 */		srax	%o1,32,%i4
/* 0x028c	 122 */		ldx	[%sp+2223],%i2
/* 0x0290	 125 */		and	%o1,%g3,%g5
/* 0x0294	 121 */		srax	%i5,32,%o1
/* 0x0298	 124 */		add	%o2,%i4,%o2
/* 0x029c	 125 */		st	%g5,[%g1+%g2]
/* 0x02a0	 128 */		bg,pn	%icc,.L77000127
/* 0x02a4	     */		add	%o1,%o2,%g5
/* 0x02a8	     */		add	%i0,6,%i0
/* 0x02ac	     */		add	%i1,6,%i1
                       .L77000134:
/* 0x02b0	 119 */		sra	%i1,0,%o2
                       .L900000112:
/* 0x02b4	 119 */		sllx	%o2,3,%o3
/* 0x02b8	 120 */		and	%i3,%g3,%o1
/* 0x02bc	 119 */		ldd	[%g4+%o3],%f0
/* 0x02c0	 122 */		sra	%i0,0,%o3
/* 0x02c4	 123 */		and	%i2,%o5,%o2
/* 0x02c8	 122 */		sllx	%o3,3,%o3
/* 0x02cc	 120 */		add	%g5,%o1,%o1
/* 0x02d0	 119 */		fdtox	%f0,%f0
/* 0x02d4	     */		std	%f0,[%sp+2231]
/* 0x02d8	 123 */		sllx	%o2,16,%o2
/* 0x02dc	     */		add	%o1,%o2,%o2
/* 0x02e0	 128 */		add	%i1,2,%i1
/* 0x02e4	 122 */		ldd	[%g4+%o3],%f0
/* 0x02e8	 124 */		srax	%o2,32,%g2
/* 0x02ec	 125 */		and	%o2,%g3,%o3
/* 0x02f0	 124 */		srax	%i2,16,%o1
/* 0x02f4	 128 */		add	%i0,2,%i0
/* 0x02f8	 122 */		fdtox	%f0,%f0
/* 0x02fc	     */		std	%f0,[%sp+2223]
/* 0x0300	 125 */		sra	%o0,0,%o2
/* 0x0304	     */		sllx	%o2,2,%o2
/* 0x0308	 124 */		add	%o1,%g2,%g5
/* 0x030c	 121 */		srax	%i3,32,%g2
/* 0x0310	 128 */		add	%o0,1,%o0
/* 0x0314	 124 */		add	%g2,%g5,%g5
/* 0x0318	 128 */		cmp	%o0,%o7
/* 0x031c	 119 */		ldx	[%sp+2231],%o4
/* 0x0320	 122 */		ldx	[%sp+2223],%i2
/* 0x0324	 125 */		st	%o3,[%g1+%o2]
/* 0x0328	 127 */		or	%g0,%o4,%i3
/* 0x032c	 128 */		ble,pt	%icc,.L900000112
/* 0x0330	     */		sra	%i1,0,%o2
                       .L77000127:

!  129		      !   }
!  130		      !     t1+=a&0xffffffff;
!  131		      !     t=(a>>32);
!  132		      !     t1+=(b&0xffff)<<16;
!  133		      !     i32[i]=t1&0xffffffff;

/* 0x0334	 133 */		sethi	%hi(0xfc00),%g2
                       .L900000113:
/* 0x0338	 133 */		or	%g0,-1,%g3
/* 0x033c	     */		add	%g2,1023,%g2
/* 0x0340	     */		srl	%g3,0,%g3
/* 0x0344	     */		and	%i2,%g2,%g2
/* 0x0348	     */		and	%i3,%g3,%g4
/* 0x034c	     */		sllx	%g2,16,%g2
/* 0x0350	     */		add	%g5,%g4,%g4
/* 0x0354	     */		sra	%o0,0,%g5
/* 0x0358	     */		add	%g4,%g2,%g4
/* 0x035c	     */		sllx	%g5,2,%g2
/* 0x0360	     */		and	%g4,%g3,%g3
/* 0x0364	     */		st	%g3,[%g1+%g2]
/* 0x0368	     */		ret	! Result = 
/* 0x036c	     */		restore	%g0,%g0,%g0
/* 0x0370	   0 */		.type	conv_d16_to_i32,2
/* 0x0370	     */		.size	conv_d16_to_i32,(.-conv_d16_to_i32)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! CONSTANT POOL
!
                       .L_const_seg_900000201:
/* 000000	   0 */		.word	1127219200,0
/* 0x0008	   0 */		.align	8
/* 0x0008	     */		.skip	24
!
! SUBROUTINE conv_i32_to_d32
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global conv_i32_to_d32
                       conv_i32_to_d32:
/* 000000	     */		or	%g0,%o7,%g3

!  135		      !}
!  137		      !void conv_i32_to_d32(double *d32, unsigned int *i32, int len)
!  138		      !{
!  139		      !int i;
!  141		      !#pragma pipeloop(0)
!  142		      ! for(i=0;i<len;i++) d32[i]=(double)(i32[i]);

/* 0x0004	 142 */		cmp	%o2,0
                       .L900000210:
/* 0x0008	     */		call	.+8
/* 0x000c	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000210-.)),%g4
/* 0x0010	 142 */		or	%g0,0,%o3
/* 0x0014	 138 */		add	%g4,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000210-.)),%g4
/* 0x0018	 142 */		sub	%o2,1,%o4
/* 0x001c	 138 */		add	%g4,%o7,%g1
/* 0x0020	 142 */		ble,pt	%icc,.L77000140
/* 0x0024	     */		or	%g0,%g3,%o7
/* 0x0028	     */		sethi	%hi(.L_const_seg_900000201),%g3
/* 0x002c	     */		cmp	%o2,12
/* 0x0030	     */		add	%g3,%lo(.L_const_seg_900000201),%g2
/* 0x0034	     */		or	%g0,%o1,%g5
/* 0x0038	     */		ldx	[%g1+%g2],%g4
/* 0x003c	     */		or	%g0,0,%g1
/* 0x0040	     */		or	%g0,24,%g2
/* 0x0044	     */		bl,pn	%icc,.L77000144
/* 0x0048	     */		or	%g0,0,%g3
/* 0x004c	     */		ld	[%o1],%f13
/* 0x0050	     */		or	%g0,7,%o3
/* 0x0054	     */		ldd	[%g4],%f8
/* 0x0058	     */		sub	%o2,5,%g3
/* 0x005c	     */		or	%g0,8,%g1
/* 0x0060	     */		ld	[%o1+4],%f11
/* 0x0064	     */		ld	[%o1+8],%f7
/* 0x0068	     */		fmovs	%f8,%f12
/* 0x006c	     */		ld	[%o1+12],%f5
/* 0x0070	     */		fmovs	%f8,%f10
/* 0x0074	     */		ld	[%o1+16],%f3
/* 0x0078	     */		fmovs	%f8,%f6
/* 0x007c	     */		ld	[%o1+20],%f1
/* 0x0080	     */		fsubd	%f12,%f8,%f12
/* 0x0084	     */		std	%f12,[%o0]
/* 0x0088	     */		fsubd	%f10,%f8,%f10
/* 0x008c	     */		std	%f10,[%o0+8]
                       .L900000205:
/* 0x0090	     */		ld	[%o1+%g2],%f11
/* 0x0094	     */		add	%g1,8,%g1
/* 0x0098	     */		add	%o3,5,%o3
/* 0x009c	     */		fsubd	%f6,%f8,%f6
/* 0x00a0	     */		add	%g2,4,%g2
/* 0x00a4	     */		std	%f6,[%o0+%g1]
/* 0x00a8	     */		cmp	%o3,%g3
/* 0x00ac	     */		fmovs	%f8,%f4
/* 0x00b0	     */		ld	[%o1+%g2],%f7
/* 0x00b4	     */		fsubd	%f4,%f8,%f12
/* 0x00b8	     */		add	%g1,8,%g1
/* 0x00bc	     */		add	%g2,4,%g2
/* 0x00c0	     */		fmovs	%f8,%f2
/* 0x00c4	     */		std	%f12,[%o0+%g1]
/* 0x00c8	     */		ld	[%o1+%g2],%f5
/* 0x00cc	     */		fsubd	%f2,%f8,%f12
/* 0x00d0	     */		add	%g1,8,%g1
/* 0x00d4	     */		add	%g2,4,%g2
/* 0x00d8	     */		fmovs	%f8,%f0
/* 0x00dc	     */		std	%f12,[%o0+%g1]
/* 0x00e0	     */		ld	[%o1+%g2],%f3
/* 0x00e4	     */		fsubd	%f0,%f8,%f12
/* 0x00e8	     */		add	%g1,8,%g1
/* 0x00ec	     */		add	%g2,4,%g2
/* 0x00f0	     */		fmovs	%f8,%f10
/* 0x00f4	     */		std	%f12,[%o0+%g1]
/* 0x00f8	     */		ld	[%o1+%g2],%f1
/* 0x00fc	     */		fsubd	%f10,%f8,%f10
/* 0x0100	     */		add	%g1,8,%g1
/* 0x0104	     */		add	%g2,4,%g2
/* 0x0108	     */		std	%f10,[%o0+%g1]
/* 0x010c	     */		ble,pt	%icc,.L900000205
/* 0x0110	     */		fmovs	%f8,%f6
                       .L900000208:
/* 0x0114	     */		fmovs	%f8,%f4
/* 0x0118	     */		ld	[%o1+%g2],%f11
/* 0x011c	     */		add	%g1,8,%g3
/* 0x0120	     */		fmovs	%f8,%f2
/* 0x0124	     */		add	%g1,16,%g1
/* 0x0128	     */		cmp	%o3,%o4
/* 0x012c	     */		fmovs	%f8,%f0
/* 0x0130	     */		add	%g1,8,%o1
/* 0x0134	     */		add	%g1,16,%o2
/* 0x0138	     */		fmovs	%f8,%f10
/* 0x013c	     */		add	%g1,24,%g2
/* 0x0140	     */		fsubd	%f6,%f8,%f6
/* 0x0144	     */		std	%f6,[%o0+%g3]
/* 0x0148	     */		fsubd	%f4,%f8,%f4
/* 0x014c	     */		std	%f4,[%o0+%g1]
/* 0x0150	     */		sra	%o3,0,%g1
/* 0x0154	     */		fsubd	%f2,%f8,%f2
/* 0x0158	     */		std	%f2,[%o0+%o1]
/* 0x015c	     */		sllx	%g1,2,%g3
/* 0x0160	     */		fsubd	%f0,%f8,%f0
/* 0x0164	     */		std	%f0,[%o0+%o2]
/* 0x0168	     */		fsubd	%f10,%f8,%f0
/* 0x016c	     */		bg,pn	%icc,.L77000140
/* 0x0170	     */		std	%f0,[%o0+%g2]
                       .L77000144:
/* 0x0174	     */		ldd	[%g4],%f8
                       .L900000211:
/* 0x0178	     */		ld	[%g5+%g3],%f13
/* 0x017c	     */		sllx	%g1,3,%g2
/* 0x0180	     */		add	%o3,1,%o3
/* 0x0184	     */		sra	%o3,0,%g1
/* 0x0188	     */		cmp	%o3,%o4
/* 0x018c	     */		fmovs	%f8,%f12
/* 0x0190	     */		sllx	%g1,2,%g3
/* 0x0194	     */		fsubd	%f12,%f8,%f0
/* 0x0198	     */		std	%f0,[%o0+%g2]
/* 0x019c	     */		ble,a,pt	%icc,.L900000211
/* 0x01a0	     */		ldd	[%g4],%f8
                       .L77000140:
/* 0x01a4	     */		retl	! Result = 
/* 0x01a8	     */		nop
/* 0x01ac	   0 */		.type	conv_i32_to_d32,2
/* 0x01ac	     */		.size	conv_i32_to_d32,(.-conv_i32_to_d32)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! CONSTANT POOL
!
                       .L_const_seg_900000301:
/* 000000	   0 */		.word	1127219200,0
/* 0x0008	   0 */		.align	8
/* 0x0008	     */		.skip	24
!
! SUBROUTINE conv_i32_to_d16
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global conv_i32_to_d16
                       conv_i32_to_d16:
/* 000000	     */		save	%sp,-192,%sp
                       .L900000310:
/* 0x0004	     */		call	.+8
/* 0x0008	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000310-.)),%g3

!  143		      !}
!  146		      !void conv_i32_to_d16(double *d16, unsigned int *i32, int len)
!  147		      !{
!  148		      !int i;
!  149		      !unsigned int a;
!  151		      !#pragma pipeloop(0)
!  152		      ! for(i=0;i<len;i++)

/* 0x000c	 152 */		cmp	%i2,0
/* 0x0010	 147 */		add	%g3,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000310-.)),%g3
/* 0x0014	 152 */		ble,pt	%icc,.L77000150
/* 0x0018	     */		add	%g3,%o7,%o0

!  153		      !   {
!  154		      !     a=i32[i];
!  155		      !     d16[2*i]=(double)(a&0xffff);
!  156		      !     d16[2*i+1]=(double)(a>>16);

/* 0x001c	 156 */		sethi	%hi(.L_const_seg_900000301),%g2
/* 0x0020	 147 */		or	%g0,%i2,%o1
/* 0x0024	 152 */		sethi	%hi(0xfc00),%g3
/* 0x0028	 156 */		add	%g2,%lo(.L_const_seg_900000301),%g2
/* 0x002c	 152 */		or	%g0,%o1,%g4
/* 0x0030	 156 */		ldx	[%o0+%g2],%o5
/* 0x0034	 152 */		add	%g3,1023,%g1
/* 0x0038	 147 */		or	%g0,%i1,%o7
/* 0x003c	 152 */		or	%g0,0,%i2
/* 0x0040	     */		sub	%o1,1,%g5
/* 0x0044	     */		or	%g0,0,%g3
/* 0x0048	     */		or	%g0,1,%g2
/* 0x004c	 154 */		or	%g0,0,%o2
/* 0x0050	     */		cmp	%g4,6
/* 0x0054	 152 */		bl,pn	%icc,.L77000154
/* 0x0058	     */		ldd	[%o5],%f0
/* 0x005c	     */		sub	%o1,2,%o3
/* 0x0060	     */		or	%g0,16,%o2
/* 0x0064	 154 */		ld	[%i1],%o4
/* 0x0068	 156 */		or	%g0,3,%g2
/* 0x006c	     */		or	%g0,2,%g3
/* 0x0070	 155 */		fmovs	%f0,%f2
/* 0x0074	 156 */		or	%g0,4,%i2
/* 0x0078	 155 */		and	%o4,%g1,%o0
/* 0x007c	     */		st	%o0,[%sp+2227]
/* 0x0080	     */		fmovs	%f0,%f4
/* 0x0084	 156 */		srl	%o4,16,%i4
/* 0x0088	 152 */		or	%g0,12,%o4
/* 0x008c	     */		or	%g0,24,%o0
/* 0x0090	 155 */		ld	[%sp+2227],%f3
/* 0x0094	     */		fsubd	%f2,%f0,%f2
/* 0x0098	     */		std	%f2,[%i0]
/* 0x009c	 156 */		st	%i4,[%sp+2223]
/* 0x00a0	 154 */		ld	[%o7+4],%o1
/* 0x00a4	 156 */		fmovs	%f0,%f2
/* 0x00a8	 155 */		and	%o1,%g1,%i1
/* 0x00ac	 156 */		ld	[%sp+2223],%f3
/* 0x00b0	     */		srl	%o1,16,%o1
/* 0x00b4	     */		fsubd	%f2,%f0,%f2
/* 0x00b8	     */		std	%f2,[%i0+8]
/* 0x00bc	     */		st	%o1,[%sp+2223]
/* 0x00c0	 155 */		st	%i1,[%sp+2227]
/* 0x00c4	 154 */		ld	[%o7+8],%o1
/* 0x00c8	 156 */		fmovs	%f0,%f2
/* 0x00cc	 155 */		and	%o1,%g1,%g4
/* 0x00d0	     */		ld	[%sp+2227],%f5
/* 0x00d4	 156 */		srl	%o1,16,%o1
/* 0x00d8	     */		ld	[%sp+2223],%f3
/* 0x00dc	     */		st	%o1,[%sp+2223]
/* 0x00e0	 155 */		fsubd	%f4,%f0,%f4
/* 0x00e4	     */		st	%g4,[%sp+2227]
/* 0x00e8	 156 */		fsubd	%f2,%f0,%f2
/* 0x00ec	 154 */		ld	[%o7+12],%o1
/* 0x00f0	 155 */		std	%f4,[%i0+16]
/* 0x00f4	 156 */		std	%f2,[%i0+24]
                       .L900000306:
/* 0x00f8	 155 */		ld	[%sp+2227],%f5
/* 0x00fc	 156 */		add	%i2,2,%i2
/* 0x0100	     */		add	%g2,4,%g2
/* 0x0104	     */		ld	[%sp+2223],%f3
/* 0x0108	     */		cmp	%i2,%o3
/* 0x010c	     */		add	%g3,4,%g3
/* 0x0110	 155 */		and	%o1,%g1,%g4
/* 0x0114	 156 */		srl	%o1,16,%o1
/* 0x0118	 155 */		st	%g4,[%sp+2227]
/* 0x011c	 156 */		st	%o1,[%sp+2223]
/* 0x0120	 152 */		add	%o4,4,%o1
/* 0x0124	 154 */		ld	[%o7+%o1],%o4
/* 0x0128	 156 */		fmovs	%f0,%f2
/* 0x012c	 155 */		fmovs	%f0,%f4
/* 0x0130	     */		fsubd	%f4,%f0,%f4
/* 0x0134	 152 */		add	%o2,16,%o2
/* 0x0138	 156 */		fsubd	%f2,%f0,%f2
/* 0x013c	 155 */		std	%f4,[%i0+%o2]
/* 0x0140	 152 */		add	%o0,16,%o0
/* 0x0144	 156 */		std	%f2,[%i0+%o0]
/* 0x0148	 155 */		ld	[%sp+2227],%f5
/* 0x014c	 156 */		ld	[%sp+2223],%f3
/* 0x0150	 155 */		and	%o4,%g1,%g4
/* 0x0154	 156 */		srl	%o4,16,%o4
/* 0x0158	 155 */		st	%g4,[%sp+2227]
/* 0x015c	 156 */		st	%o4,[%sp+2223]
/* 0x0160	 152 */		add	%o1,4,%o4
/* 0x0164	 154 */		ld	[%o7+%o4],%o1
/* 0x0168	 156 */		fmovs	%f0,%f2
/* 0x016c	 155 */		fmovs	%f0,%f4
/* 0x0170	     */		fsubd	%f4,%f0,%f4
/* 0x0174	 152 */		add	%o2,16,%o2
/* 0x0178	 156 */		fsubd	%f2,%f0,%f2
/* 0x017c	 155 */		std	%f4,[%i0+%o2]
/* 0x0180	 152 */		add	%o0,16,%o0
/* 0x0184	 156 */		ble,pt	%icc,.L900000306
/* 0x0188	     */		std	%f2,[%i0+%o0]
                       .L900000309:
/* 0x018c	 155 */		ld	[%sp+2227],%f5
/* 0x0190	 156 */		fmovs	%f0,%f2
/* 0x0194	     */		srl	%o1,16,%o3
/* 0x0198	     */		ld	[%sp+2223],%f3
/* 0x019c	 155 */		and	%o1,%g1,%i1
/* 0x01a0	 152 */		add	%o2,16,%g4
/* 0x01a4	 155 */		fmovs	%f0,%f4
/* 0x01a8	     */		st	%i1,[%sp+2227]
/* 0x01ac	 152 */		add	%o0,16,%o2
/* 0x01b0	 156 */		st	%o3,[%sp+2223]
/* 0x01b4	 154 */		sra	%i2,0,%o3
/* 0x01b8	 152 */		add	%g4,16,%o1
/* 0x01bc	 155 */		fsubd	%f4,%f0,%f4
/* 0x01c0	     */		std	%f4,[%i0+%g4]
/* 0x01c4	 152 */		add	%o0,32,%o0
/* 0x01c8	 156 */		fsubd	%f2,%f0,%f2
/* 0x01cc	     */		std	%f2,[%i0+%o2]
/* 0x01d0	     */		sllx	%o3,2,%o2
/* 0x01d4	 155 */		ld	[%sp+2227],%f5
/* 0x01d8	 156 */		cmp	%i2,%g5
/* 0x01dc	     */		add	%g2,6,%g2
/* 0x01e0	     */		ld	[%sp+2223],%f3
/* 0x01e4	     */		add	%g3,6,%g3
/* 0x01e8	 155 */		fmovs	%f0,%f4
/* 0x01ec	 156 */		fmovs	%f0,%f2
/* 0x01f0	 155 */		fsubd	%f4,%f0,%f4
/* 0x01f4	     */		std	%f4,[%i0+%o1]
/* 0x01f8	 156 */		fsubd	%f2,%f0,%f0
/* 0x01fc	     */		bg,pn	%icc,.L77000150
/* 0x0200	     */		std	%f0,[%i0+%o0]
                       .L77000154:
/* 0x0204	 155 */		ldd	[%o5],%f0
                       .L900000311:
/* 0x0208	 154 */		ld	[%o7+%o2],%o0
/* 0x020c	 155 */		sra	%g3,0,%o1
/* 0x0210	     */		fmovs	%f0,%f2
/* 0x0214	     */		sllx	%o1,3,%o2
/* 0x0218	 156 */		add	%i2,1,%i2
/* 0x021c	 155 */		and	%o0,%g1,%o1
/* 0x0220	     */		st	%o1,[%sp+2227]
/* 0x0224	 156 */		add	%g3,2,%g3
/* 0x0228	     */		srl	%o0,16,%o1
/* 0x022c	     */		cmp	%i2,%g5
/* 0x0230	     */		sra	%g2,0,%o0
/* 0x0234	     */		add	%g2,2,%g2
/* 0x0238	     */		sllx	%o0,3,%o0
/* 0x023c	 155 */		ld	[%sp+2227],%f3
/* 0x0240	 154 */		sra	%i2,0,%o3
/* 0x0244	 155 */		fsubd	%f2,%f0,%f2
/* 0x0248	     */		std	%f2,[%i0+%o2]
/* 0x024c	     */		sllx	%o3,2,%o2
/* 0x0250	 156 */		st	%o1,[%sp+2223]
/* 0x0254	     */		fmovs	%f0,%f2
/* 0x0258	     */		ld	[%sp+2223],%f3
/* 0x025c	     */		fsubd	%f2,%f0,%f0
/* 0x0260	     */		std	%f0,[%i0+%o0]
/* 0x0264	     */		ble,a,pt	%icc,.L900000311
/* 0x0268	     */		ldd	[%o5],%f0
                       .L77000150:
/* 0x026c	     */		ret	! Result = 
/* 0x0270	     */		restore	%g0,%g0,%g0
/* 0x0274	   0 */		.type	conv_i32_to_d16,2
/* 0x0274	     */		.size	conv_i32_to_d16,(.-conv_i32_to_d16)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! CONSTANT POOL
!
                       .L_const_seg_900000401:
/* 000000	   0 */		.word	1127219200,0
/* 0x0008	   0 */		.align	8
/* 0x0008	     */		.skip	24
!
! SUBROUTINE conv_i32_to_d32_and_d16
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global conv_i32_to_d32_and_d16
                       conv_i32_to_d32_and_d16:
/* 000000	     */		save	%sp,-192,%sp
                       .L900000415:
/* 0x0004	     */		call	.+8
/* 0x0008	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000415-.)),%g3

!  157		      !   }
!  158		      !}
!  161		      !void conv_i32_to_d32_and_d16(double *d32, double *d16, 
!  162		      !			     unsigned int *i32, int len)
!  163		      !{
!  164		      !int i = 0;
!  165		      !unsigned int a;
!  167		      !#pragma pipeloop(0)
!  168		      !#ifdef RF_INLINE_MACROS
!  169		      ! for(;i<len-3;i+=4)
!  170		      !   {
!  171		      !     i16_to_d16_and_d32x4(&TwoToMinus16, &TwoTo16, &Zero,
!  172		      !			  &(d16[2*i]), &(d32[i]), (float *)(&(i32[i])));

/* 0x000c	 172 */		sethi	%hi(Zero),%g2
/* 0x0010	 163 */		add	%g3,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000415-.)),%g3
/* 0x0014	     */		or	%g0,%i3,%g5
/* 0x0018	     */		add	%g3,%o7,%o3
/* 0x001c	 172 */		add	%g2,%lo(Zero),%g2
/* 0x0020	     */		ldx	[%o3+%g2],%o0
/* 0x0024	     */		sethi	%hi(TwoToMinus16),%g3
/* 0x0028	 163 */		or	%g0,%i0,%i3
/* 0x002c	 169 */		sub	%g5,3,%o1
/* 0x0030	 172 */		sethi	%hi(TwoTo16),%g4
/* 0x0034	 163 */		or	%g0,%i2,%i0
/* 0x0038	 172 */		add	%g3,%lo(TwoToMinus16),%g2
/* 0x003c	     */		ldx	[%o3+%g2],%o2
/* 0x0040	 169 */		cmp	%o1,0
/* 0x0044	 164 */		or	%g0,0,%i2
/* 0x0048	 169 */		ble,pt	%icc,.L900000418
/* 0x004c	     */		cmp	%i2,%g5
/* 0x0050	     */		ldd	[%o0],%f2
/* 0x0054	 172 */		add	%g4,%lo(TwoTo16),%g3
/* 0x0058	     */		ldx	[%o3+%g3],%o1
/* 0x005c	 169 */		sub	%g5,4,%o4
/* 0x0060	     */		or	%g0,0,%o5
                       .L900000417:
/* 0x0064	 172 */		sra	%i2,0,%g2
/* 0x0068	     */		fmovd	%f2,%f14
/* 0x006c	     */		ldd	[%o2],%f0
/* 0x0070	     */		sllx	%g2,2,%g3
/* 0x0074	     */		fmovd	%f2,%f10
/* 0x0078	     */		ldd	[%o1],%f16
/* 0x007c	     */		ld	[%g3+%i0],%f15
/* 0x0080	     */		add	%i0,%g3,%g3
/* 0x0084	     */		fmovd	%f2,%f6
/* 0x0088	     */		ld	[%g3+4],%f11
/* 0x008c	     */		sra	%o5,0,%g4
/* 0x0090	     */		add	%i2,4,%i2
/* 0x0094	     */		ld	[%g3+8],%f7
/* 0x0098	     */		fxtod	%f14,%f14
/* 0x009c	     */		sllx	%g2,3,%g2
/* 0x00a0	     */		ld	[%g3+12],%f3
/* 0x00a4	     */		fxtod	%f10,%f10
/* 0x00a8	     */		sllx	%g4,3,%g3
/* 0x00ac	     */		fxtod	%f6,%f6
/* 0x00b0	     */		std	%f14,[%g2+%i3]
/* 0x00b4	     */		add	%i3,%g2,%g4
/* 0x00b8	     */		fxtod	%f2,%f2
/* 0x00bc	     */		fmuld	%f0,%f14,%f12
/* 0x00c0	     */		std	%f2,[%g4+24]
/* 0x00c4	     */		fmuld	%f0,%f10,%f8
/* 0x00c8	     */		std	%f10,[%g4+8]
/* 0x00cc	     */		add	%i1,%g3,%g2
/* 0x00d0	     */		fmuld	%f0,%f6,%f4
/* 0x00d4	     */		std	%f6,[%g4+16]
/* 0x00d8	     */		cmp	%i2,%o4
/* 0x00dc	     */		fmuld	%f0,%f2,%f0
/* 0x00e0	     */		fdtox	%f12,%f12
/* 0x00e4	     */		add	%o5,8,%o5
/* 0x00e8	     */		fdtox	%f8,%f8
/* 0x00ec	     */		fdtox	%f4,%f4
/* 0x00f0	     */		fdtox	%f0,%f0
/* 0x00f4	     */		fxtod	%f12,%f12
/* 0x00f8	     */		std	%f12,[%g2+8]
/* 0x00fc	     */		fxtod	%f8,%f8
/* 0x0100	     */		std	%f8,[%g2+24]
/* 0x0104	     */		fxtod	%f4,%f4
/* 0x0108	     */		std	%f4,[%g2+40]
/* 0x010c	     */		fxtod	%f0,%f0
/* 0x0110	     */		std	%f0,[%g2+56]
/* 0x0114	     */		fmuld	%f12,%f16,%f12
/* 0x0118	     */		fmuld	%f8,%f16,%f8
/* 0x011c	     */		fmuld	%f4,%f16,%f4
/* 0x0120	     */		fsubd	%f14,%f12,%f12
/* 0x0124	     */		std	%f12,[%g3+%i1]
/* 0x0128	     */		fmuld	%f0,%f16,%f0
/* 0x012c	     */		fsubd	%f10,%f8,%f8
/* 0x0130	     */		std	%f8,[%g2+16]
/* 0x0134	     */		fsubd	%f6,%f4,%f4
/* 0x0138	     */		std	%f4,[%g2+32]
/* 0x013c	     */		fsubd	%f2,%f0,%f0
/* 0x0140	     */		std	%f0,[%g2+48]
/* 0x0144	     */		ble,a,pt	%icc,.L900000417
/* 0x0148	     */		ldd	[%o0],%f2
                       .L77000159:

!  173		      !   }
!  174		      !#endif
!  175		      ! for(;i<len;i++)

/* 0x014c	 175 */		cmp	%i2,%g5
                       .L900000418:
/* 0x0150	 175 */		bge,pt	%icc,.L77000164
/* 0x0154	     */		nop

!  176		      !   {
!  177		      !     a=i32[i];
!  178		      !     d32[i]=(double)(i32[i]);
!  179		      !     d16[2*i]=(double)(a&0xffff);
!  180		      !     d16[2*i+1]=(double)(a>>16);

/* 0x0158	 180 */		sethi	%hi(.L_const_seg_900000401),%g2
/* 0x015c	     */		add	%g2,%lo(.L_const_seg_900000401),%g2
/* 0x0160	 175 */		sethi	%hi(0xfc00),%g3
/* 0x0164	 180 */		ldx	[%o3+%g2],%g1
/* 0x0168	 175 */		sll	%i2,1,%i4
/* 0x016c	     */		sub	%g5,%i2,%g4
/* 0x0170	 177 */		sra	%i2,0,%o3
/* 0x0174	 175 */		add	%g3,1023,%g3
/* 0x0178	 178 */		ldd	[%g1],%f2
/* 0x017c	     */		sllx	%o3,2,%o2
/* 0x0180	 175 */		add	%i4,1,%g2
/* 0x0184	 177 */		or	%g0,%o3,%o1
/* 0x0188	     */		cmp	%g4,6
/* 0x018c	 175 */		bl,pn	%icc,.L77000161
/* 0x0190	     */		sra	%i2,0,%o3
/* 0x0194	 177 */		or	%g0,%o2,%o0
/* 0x0198	 178 */		ld	[%i0+%o2],%f5
/* 0x019c	 179 */		fmovs	%f2,%f8
/* 0x01a0	 175 */		add	%o0,4,%o3
/* 0x01a4	 177 */		ld	[%i0+%o0],%o7
/* 0x01a8	 180 */		fmovs	%f2,%f6
/* 0x01ac	 178 */		fmovs	%f2,%f4
/* 0x01b0	     */		sllx	%o1,3,%o2
/* 0x01b4	 175 */		add	%o3,4,%o5
/* 0x01b8	 179 */		sra	%i4,0,%o0
/* 0x01bc	 175 */		add	%o3,8,%o4
/* 0x01c0	 178 */		fsubd	%f4,%f2,%f4
/* 0x01c4	     */		std	%f4,[%i3+%o2]
/* 0x01c8	 179 */		sllx	%o0,3,%i5
/* 0x01cc	     */		and	%o7,%g3,%o0
/* 0x01d0	     */		st	%o0,[%sp+2227]
/* 0x01d4	 175 */		add	%i5,16,%o1
/* 0x01d8	 180 */		srl	%o7,16,%g4
/* 0x01dc	     */		add	%i2,1,%i2
/* 0x01e0	     */		sra	%g2,0,%o0
/* 0x01e4	 175 */		add	%o2,8,%o2
/* 0x01e8	 179 */		fmovs	%f2,%f4
/* 0x01ec	 180 */		sllx	%o0,3,%l0
/* 0x01f0	     */		add	%i4,3,%g2
/* 0x01f4	 179 */		ld	[%sp+2227],%f5
/* 0x01f8	 175 */		add	%l0,16,%o0
/* 0x01fc	 180 */		add	%i4,2,%i4
/* 0x0200	 175 */		sub	%g5,1,%o7
/* 0x0204	 180 */		add	%i2,3,%i2
/* 0x0208	 179 */		fsubd	%f4,%f2,%f4
/* 0x020c	     */		std	%f4,[%i1+%i5]
/* 0x0210	 180 */		st	%g4,[%sp+2223]
/* 0x0214	 177 */		ld	[%i0+%o3],%i5
/* 0x0218	 180 */		fmovs	%f2,%f4
/* 0x021c	     */		srl	%i5,16,%g4
/* 0x0220	 179 */		and	%i5,%g3,%i5
/* 0x0224	 180 */		ld	[%sp+2223],%f5
/* 0x0228	     */		fsubd	%f4,%f2,%f4
/* 0x022c	     */		std	%f4,[%i1+%l0]
/* 0x0230	     */		st	%g4,[%sp+2223]
/* 0x0234	 177 */		ld	[%i0+%o5],%g4
/* 0x0238	 179 */		st	%i5,[%sp+2227]
/* 0x023c	 178 */		fmovs	%f2,%f4
/* 0x0240	 180 */		srl	%g4,16,%i5
/* 0x0244	 179 */		and	%g4,%g3,%g4
/* 0x0248	 180 */		ld	[%sp+2223],%f7
/* 0x024c	     */		st	%i5,[%sp+2223]
/* 0x0250	 178 */		ld	[%i0+%o3],%f5
/* 0x0254	 180 */		fsubd	%f6,%f2,%f6
/* 0x0258	 177 */		ld	[%i0+%o4],%o3
/* 0x025c	 178 */		fsubd	%f4,%f2,%f4
/* 0x0260	 179 */		ld	[%sp+2227],%f9
/* 0x0264	 180 */		ld	[%sp+2223],%f1
/* 0x0268	 179 */		st	%g4,[%sp+2227]
/* 0x026c	     */		fsubd	%f8,%f2,%f8
/* 0x0270	     */		std	%f8,[%i1+%o1]
/* 0x0274	 180 */		std	%f6,[%i1+%o0]
/* 0x0278	 178 */		std	%f4,[%i3+%o2]
                       .L900000411:
/* 0x027c	 179 */		ld	[%sp+2227],%f13
/* 0x0280	 180 */		srl	%o3,16,%g4
/* 0x0284	     */		add	%i2,2,%i2
/* 0x0288	     */		st	%g4,[%sp+2223]
/* 0x028c	     */		cmp	%i2,%o7
/* 0x0290	     */		add	%g2,4,%g2
/* 0x0294	 178 */		ld	[%i0+%o5],%f11
/* 0x0298	 180 */		add	%i4,4,%i4
/* 0x029c	 175 */		add	%o4,4,%o5
/* 0x02a0	 177 */		ld	[%i0+%o5],%g4
/* 0x02a4	 179 */		and	%o3,%g3,%o3
/* 0x02a8	     */		st	%o3,[%sp+2227]
/* 0x02ac	 180 */		fmovs	%f2,%f0
/* 0x02b0	 179 */		fmovs	%f2,%f12
/* 0x02b4	 180 */		fsubd	%f0,%f2,%f8
/* 0x02b8	 179 */		fsubd	%f12,%f2,%f4
/* 0x02bc	 175 */		add	%o1,16,%o1
/* 0x02c0	 180 */		ld	[%sp+2223],%f7
/* 0x02c4	 178 */		fmovs	%f2,%f10
/* 0x02c8	 179 */		std	%f4,[%i1+%o1]
/* 0x02cc	 175 */		add	%o0,16,%o0
/* 0x02d0	 178 */		fsubd	%f10,%f2,%f4
/* 0x02d4	 175 */		add	%o2,8,%o2
/* 0x02d8	 180 */		std	%f8,[%i1+%o0]
/* 0x02dc	 178 */		std	%f4,[%i3+%o2]
/* 0x02e0	 179 */		ld	[%sp+2227],%f9
/* 0x02e4	 180 */		srl	%g4,16,%o3
/* 0x02e8	     */		st	%o3,[%sp+2223]
/* 0x02ec	 178 */		ld	[%i0+%o4],%f5
/* 0x02f0	 175 */		add	%o4,8,%o4
/* 0x02f4	 177 */		ld	[%i0+%o4],%o3
/* 0x02f8	 179 */		and	%g4,%g3,%g4
/* 0x02fc	     */		st	%g4,[%sp+2227]
/* 0x0300	 180 */		fmovs	%f2,%f6
/* 0x0304	 179 */		fmovs	%f2,%f8
/* 0x0308	 180 */		fsubd	%f6,%f2,%f6
/* 0x030c	 179 */		fsubd	%f8,%f2,%f8
/* 0x0310	 175 */		add	%o1,16,%o1
/* 0x0314	 180 */		ld	[%sp+2223],%f1
/* 0x0318	 178 */		fmovs	%f2,%f4
/* 0x031c	 179 */		std	%f8,[%i1+%o1]
/* 0x0320	 175 */		add	%o0,16,%o0
/* 0x0324	 178 */		fsubd	%f4,%f2,%f4
/* 0x0328	 175 */		add	%o2,8,%o2
/* 0x032c	 180 */		std	%f6,[%i1+%o0]
/* 0x0330	     */		bl,pt	%icc,.L900000411
/* 0x0334	     */		std	%f4,[%i3+%o2]
                       .L900000414:
/* 0x0338	 180 */		srl	%o3,16,%o7
/* 0x033c	     */		st	%o7,[%sp+2223]
/* 0x0340	 179 */		fmovs	%f2,%f12
/* 0x0344	 178 */		ld	[%i0+%o5],%f11
/* 0x0348	 180 */		fmovs	%f2,%f0
/* 0x034c	 179 */		and	%o3,%g3,%g4
/* 0x0350	 180 */		fmovs	%f2,%f6
/* 0x0354	 175 */		add	%o1,16,%o3
/* 0x0358	     */		add	%o0,16,%o7
/* 0x035c	 178 */		fmovs	%f2,%f10
/* 0x0360	 175 */		add	%o2,8,%o2
/* 0x0364	     */		add	%o1,32,%o5
/* 0x0368	 179 */		ld	[%sp+2227],%f13
/* 0x036c	 178 */		fmovs	%f2,%f4
/* 0x0370	 175 */		add	%o0,32,%o1
/* 0x0374	 180 */		ld	[%sp+2223],%f7
/* 0x0378	 175 */		add	%o2,8,%o0
/* 0x037c	 180 */		cmp	%i2,%g5
/* 0x0380	 179 */		st	%g4,[%sp+2227]
/* 0x0384	     */		fsubd	%f12,%f2,%f8
/* 0x0388	 180 */		add	%g2,6,%g2
/* 0x038c	 179 */		std	%f8,[%i1+%o3]
/* 0x0390	 180 */		fsubd	%f0,%f2,%f0
/* 0x0394	 177 */		sra	%i2,0,%o3
/* 0x0398	 180 */		std	%f0,[%i1+%o7]
/* 0x039c	 178 */		fsubd	%f10,%f2,%f0
/* 0x03a0	 180 */		add	%i4,6,%i4
/* 0x03a4	 178 */		std	%f0,[%i3+%o2]
/* 0x03a8	     */		sllx	%o3,2,%o2
/* 0x03ac	 179 */		ld	[%sp+2227],%f9
/* 0x03b0	 178 */		ld	[%i0+%o4],%f5
/* 0x03b4	 179 */		fmovs	%f2,%f8
/* 0x03b8	     */		fsubd	%f8,%f2,%f0
/* 0x03bc	     */		std	%f0,[%i1+%o5]
/* 0x03c0	 180 */		fsubd	%f6,%f2,%f0
/* 0x03c4	     */		std	%f0,[%i1+%o1]
/* 0x03c8	 178 */		fsubd	%f4,%f2,%f0
/* 0x03cc	 180 */		bge,pn	%icc,.L77000164
/* 0x03d0	     */		std	%f0,[%i3+%o0]
                       .L77000161:
/* 0x03d4	 178 */		ldd	[%g1],%f2
                       .L900000416:
/* 0x03d8	 178 */		ld	[%i0+%o2],%f5
/* 0x03dc	 179 */		sra	%i4,0,%o0
/* 0x03e0	 180 */		add	%i2,1,%i2
/* 0x03e4	 177 */		ld	[%i0+%o2],%o1
/* 0x03e8	 178 */		sllx	%o3,3,%o3
/* 0x03ec	 180 */		add	%i4,2,%i4
/* 0x03f0	 178 */		fmovs	%f2,%f4
/* 0x03f4	 179 */		sllx	%o0,3,%o4
/* 0x03f8	 180 */		cmp	%i2,%g5
/* 0x03fc	 179 */		and	%o1,%g3,%o0
/* 0x0400	 178 */		fsubd	%f4,%f2,%f0
/* 0x0404	     */		std	%f0,[%i3+%o3]
/* 0x0408	 180 */		srl	%o1,16,%o1
/* 0x040c	 179 */		st	%o0,[%sp+2227]
/* 0x0410	 180 */		sra	%g2,0,%o0
/* 0x0414	     */		add	%g2,2,%g2
/* 0x0418	 177 */		sra	%i2,0,%o3
/* 0x041c	 180 */		sllx	%o0,3,%o0
/* 0x0420	 179 */		fmovs	%f2,%f4
/* 0x0424	     */		sllx	%o3,2,%o2
/* 0x0428	     */		ld	[%sp+2227],%f5
/* 0x042c	     */		fsubd	%f4,%f2,%f0
/* 0x0430	     */		std	%f0,[%i1+%o4]
/* 0x0434	 180 */		st	%o1,[%sp+2223]
/* 0x0438	     */		fmovs	%f2,%f4
/* 0x043c	     */		ld	[%sp+2223],%f5
/* 0x0440	     */		fsubd	%f4,%f2,%f0
/* 0x0444	     */		std	%f0,[%i1+%o0]
/* 0x0448	     */		bl,a,pt	%icc,.L900000416
/* 0x044c	     */		ldd	[%g1],%f2
                       .L77000164:
/* 0x0450	     */		ret	! Result = 
/* 0x0454	     */		restore	%g0,%g0,%g0
/* 0x0458	   0 */		.type	conv_i32_to_d32_and_d16,2
/* 0x0458	     */		.size	conv_i32_to_d32_and_d16,(.-conv_i32_to_d32_and_d16)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! SUBROUTINE adjust_montf_result
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global adjust_montf_result
                       adjust_montf_result:
/* 000000	     */		save	%sp,-176,%sp
/* 0x0004	     */		or	%g0,%i2,%o1
/* 0x0008	     */		or	%g0,%i0,%i2

!  181		      !   }
!  182		      !}
!  185		      !void adjust_montf_result(unsigned int *i32, unsigned int *nint, int len)
!  186		      !{
!  187		      !long long acc;
!  188		      !int i;
!  190		      ! if(i32[len]>0) i=-1;

/* 0x000c	 190 */		sra	%o1,0,%g2
/* 0x0010	     */		or	%g0,-1,%o2
/* 0x0014	     */		sllx	%g2,2,%g2
/* 0x0018	     */		ld	[%i2+%g2],%g2
/* 0x001c	     */		cmp	%g2,0
/* 0x0020	     */		bleu,pn	%icc,.L77000175
/* 0x0024	     */		or	%g0,%i1,%i0
/* 0x0028	     */		ba	.L900000511
/* 0x002c	     */		cmp	%o2,0
                       .L77000175:

!  191		      ! else
!  192		      !   {
!  193		      !     for(i=len-1; i>=0; i--)

/* 0x0030	 193 */		sub	%o1,1,%o2
/* 0x0034	     */		cmp	%o2,0
/* 0x0038	     */		bl,pn	%icc,.L77000182
/* 0x003c	     */		sra	%o2,0,%g2
                       .L900000510:

!  194		      !       {
!  195		      !	 if(i32[i]!=nint[i]) break;

/* 0x0040	 195 */		sllx	%g2,2,%g2
/* 0x0044	     */		sub	%o2,1,%o0
/* 0x0048	     */		ld	[%i1+%g2],%g3
/* 0x004c	     */		ld	[%i2+%g2],%g2
/* 0x0050	     */		cmp	%g2,%g3
/* 0x0054	     */		bne,pn	%icc,.L77000182
/* 0x0058	     */		nop
/* 0x005c	   0 */		or	%g0,%o0,%o2
/* 0x0060	 195 */		cmp	%o0,0
/* 0x0064	     */		bge,pt	%icc,.L900000510
/* 0x0068	     */		sra	%o2,0,%g2
                       .L77000182:

!  196		      !       }
!  197		      !   }
!  198		      ! if((i<0)||(i32[i]>nint[i]))

/* 0x006c	 198 */		cmp	%o2,0
                       .L900000511:
/* 0x0070	 198 */		bl,pn	%icc,.L77000198
/* 0x0074	     */		sra	%o2,0,%g2
/* 0x0078	     */		sllx	%g2,2,%g2
/* 0x007c	     */		ld	[%i1+%g2],%g3
/* 0x0080	     */		ld	[%i2+%g2],%g2
/* 0x0084	     */		cmp	%g2,%g3
/* 0x0088	     */		bleu,pt	%icc,.L77000191
/* 0x008c	     */		nop
                       .L77000198:

!  199		      !   {
!  200		      !     acc=0;
!  201		      !     for(i=0;i<len;i++)

/* 0x0090	 201 */		cmp	%o1,0
/* 0x0094	     */		ble,pt	%icc,.L77000191
/* 0x0098	     */		nop
/* 0x009c	 198 */		or	%g0,-1,%g2
/* 0x00a0	 201 */		or	%g0,%o1,%g3
/* 0x00a4	 198 */		srl	%g2,0,%g2
/* 0x00a8	     */		sub	%o1,1,%g4
/* 0x00ac	     */		cmp	%o1,9
/* 0x00b0	 201 */		or	%g0,0,%i1
/* 0x00b4	 200 */		or	%g0,0,%g5

!  202		      !       {
!  203		      !	 acc=acc+(unsigned long long)(i32[i])-(unsigned long long)(nint[i]);

/* 0x00b8	 203 */		or	%g0,0,%o1
/* 0x00bc	 201 */		bl,pn	%icc,.L77000199
/* 0x00c0	     */		sub	%g3,4,%o7
/* 0x00c4	 203 */		ld	[%i2],%o1

!  204		      !	 i32[i]=acc&0xffffffff;
!  205		      !	 acc=acc>>32;

/* 0x00c8	 205 */		or	%g0,5,%i1
/* 0x00cc	 203 */		ld	[%i0],%o2
/* 0x00d0	 201 */		or	%g0,8,%o5
/* 0x00d4	     */		or	%g0,12,%o4
/* 0x00d8	 203 */		ld	[%i0+4],%o3
/* 0x00dc	 201 */		or	%g0,16,%g1
/* 0x00e0	 203 */		ld	[%i2+4],%o0
/* 0x00e4	 201 */		sub	%o1,%o2,%o1
/* 0x00e8	 203 */		ld	[%i0+8],%i3
/* 0x00ec	 204 */		and	%o1,%g2,%g5
/* 0x00f0	     */		st	%g5,[%i2]
/* 0x00f4	 205 */		srax	%o1,32,%g5
/* 0x00f8	 201 */		sub	%o0,%o3,%o0
/* 0x00fc	 203 */		ld	[%i0+12],%o2
/* 0x0100	 201 */		add	%o0,%g5,%o0
/* 0x0104	 204 */		and	%o0,%g2,%g5
/* 0x0108	     */		st	%g5,[%i2+4]
/* 0x010c	 205 */		srax	%o0,32,%o0
/* 0x0110	 203 */		ld	[%i2+8],%o1
/* 0x0114	     */		ld	[%i2+12],%o3
/* 0x0118	 201 */		sub	%o1,%i3,%o1
                       .L900000505:
/* 0x011c	     */		add	%g1,4,%g3
/* 0x0120	 203 */		ld	[%g1+%i2],%g5
/* 0x0124	 201 */		add	%o1,%o0,%o0
/* 0x0128	 203 */		ld	[%i0+%g1],%i3
/* 0x012c	 201 */		sub	%o3,%o2,%o1
/* 0x0130	 204 */		and	%o0,%g2,%o2
/* 0x0134	     */		st	%o2,[%o5+%i2]
/* 0x0138	 205 */		srax	%o0,32,%o2
/* 0x013c	     */		add	%i1,4,%i1
/* 0x0140	 201 */		add	%g1,8,%o5
/* 0x0144	 203 */		ld	[%g3+%i2],%o0
/* 0x0148	 201 */		add	%o1,%o2,%o1
/* 0x014c	 203 */		ld	[%i0+%g3],%o3
/* 0x0150	 201 */		sub	%g5,%i3,%o2
/* 0x0154	 204 */		and	%o1,%g2,%g5
/* 0x0158	     */		st	%g5,[%o4+%i2]
/* 0x015c	 205 */		srax	%o1,32,%g5
/* 0x0160	     */		cmp	%i1,%o7
/* 0x0164	 201 */		add	%g1,12,%o4
/* 0x0168	 203 */		ld	[%o5+%i2],%o1
/* 0x016c	 201 */		add	%o2,%g5,%o2
/* 0x0170	 203 */		ld	[%i0+%o5],%i3
/* 0x0174	 201 */		sub	%o0,%o3,%o0
/* 0x0178	 204 */		and	%o2,%g2,%o3
/* 0x017c	     */		st	%o3,[%g1+%i2]
/* 0x0180	 205 */		srax	%o2,32,%g5
/* 0x0184	 203 */		ld	[%o4+%i2],%o3
/* 0x0188	 201 */		add	%g1,16,%g1
/* 0x018c	     */		add	%o0,%g5,%o0
/* 0x0190	 203 */		ld	[%i0+%o4],%o2
/* 0x0194	 201 */		sub	%o1,%i3,%o1
/* 0x0198	 204 */		and	%o0,%g2,%g5
/* 0x019c	     */		st	%g5,[%g3+%i2]
/* 0x01a0	 205 */		ble,pt	%icc,.L900000505
/* 0x01a4	     */		srax	%o0,32,%o0
                       .L900000508:
/* 0x01a8	     */		add	%o1,%o0,%g3
/* 0x01ac	     */		sub	%o3,%o2,%o1
/* 0x01b0	 203 */		ld	[%g1+%i2],%o0
/* 0x01b4	     */		ld	[%i0+%g1],%o2
/* 0x01b8	 205 */		srax	%g3,32,%o7
/* 0x01bc	 204 */		and	%g3,%g2,%o3
/* 0x01c0	 201 */		add	%o1,%o7,%o1
/* 0x01c4	 204 */		st	%o3,[%o5+%i2]
/* 0x01c8	 205 */		cmp	%i1,%g4
/* 0x01cc	 201 */		sub	%o0,%o2,%o0
/* 0x01d0	 204 */		and	%o1,%g2,%o2
/* 0x01d4	     */		st	%o2,[%o4+%i2]
/* 0x01d8	 205 */		srax	%o1,32,%o1
/* 0x01dc	 203 */		sra	%i1,0,%o2
/* 0x01e0	 201 */		add	%o0,%o1,%o0
/* 0x01e4	 205 */		srax	%o0,32,%g5
/* 0x01e8	 204 */		and	%o0,%g2,%o1
/* 0x01ec	     */		st	%o1,[%g1+%i2]
/* 0x01f0	 205 */		bg,pn	%icc,.L77000191
/* 0x01f4	     */		sllx	%o2,2,%o1
                       .L77000199:
/* 0x01f8	   0 */		or	%g0,%o1,%g1
                       .L900000509:
/* 0x01fc	 203 */		ld	[%o1+%i2],%o0
/* 0x0200	 205 */		add	%i1,1,%i1
/* 0x0204	 203 */		ld	[%i0+%o1],%o1
/* 0x0208	     */		sra	%i1,0,%o2
/* 0x020c	 205 */		cmp	%i1,%g4
/* 0x0210	 203 */		add	%g5,%o0,%o0
/* 0x0214	     */		sub	%o0,%o1,%o0
/* 0x0218	 205 */		srax	%o0,32,%g5
/* 0x021c	 204 */		and	%o0,%g2,%o1
/* 0x0220	     */		st	%o1,[%g1+%i2]
/* 0x0224	     */		sllx	%o2,2,%o1
/* 0x0228	 205 */		ble,pt	%icc,.L900000509
/* 0x022c	     */		or	%g0,%o1,%g1
                       .L77000191:
/* 0x0230	     */		ret	! Result = 
/* 0x0234	     */		restore	%g0,%g0,%g0
/* 0x0238	   0 */		.type	adjust_montf_result,2
/* 0x0238	     */		.size	adjust_montf_result,(.-adjust_montf_result)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
/* 000000	     */		.skip	24
!
! SUBROUTINE mont_mulf_noconv
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global mont_mulf_noconv
                       mont_mulf_noconv:
/* 000000	     */		save	%sp,-224,%sp
                       .L900000643:
/* 0x0004	     */		call	.+8
/* 0x0008	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000643-.)),%g5
/* 0x000c	     */		ldx	[%fp+2223],%l0

!  206		      !       }
!  207		      !   }
!  208		      !}
!  213		      !/*
!  214		      !** the lengths of the input arrays should be at least the following:
!  215		      !** result[nlen+1], dm1[nlen], dm2[2*nlen+1], dt[4*nlen+2], dn[nlen], nint[nlen]
!  216		      !** all of them should be different from one another
!  217		      !**
!  218		      !*/
!  219		      !void mont_mulf_noconv(unsigned int *result,
!  220		      !		     double *dm1, double *dm2, double *dt,
!  221		      !		     double *dn, unsigned int *nint,
!  222		      !		     int nlen, double dn0)
!  223		      !{
!  224		      ! int i, j, jj;
!  225		      ! int tmp;
!  226		      ! double digit, m2j, nextm2j, a, b;
!  227		      ! double *dptmp, *pdm1, *pdm2, *pdn, *pdtj, pdn_0, pdm1_0;
!  229		      ! pdm1=&(dm1[0]);
!  230		      ! pdm2=&(dm2[0]);
!  231		      ! pdn=&(dn[0]);
!  232		      ! pdm2[2*nlen]=Zero;

/* 0x0010	 232 */		sethi	%hi(Zero),%g2
/* 0x0014	 223 */		fmovd	%f14,%f30
/* 0x0018	     */		add	%g5,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000643-.)),%g5
/* 0x001c	 232 */		add	%g2,%lo(Zero),%g2
/* 0x0020	     */		sll	%l0,1,%o3
/* 0x0024	 223 */		add	%g5,%o7,%o4
/* 0x0028	 232 */		sra	%o3,0,%g5
/* 0x002c	     */		ldx	[%o4+%g2],%o7

!  234		      ! if (nlen!=16)
!  235		      !   {
!  236		      !     for(i=0;i<4*nlen+2;i++) dt[i]=Zero;
!  238		      !     a=dt[0]=pdm1[0]*pdm2[0];
!  239		      !     digit=mod(lower32(a,Zero)*dn0,TwoToMinus16,TwoTo16);

/* 0x0030	 239 */		sethi	%hi(TwoToMinus16),%g3
/* 0x0034	     */		sethi	%hi(TwoTo16),%g4
/* 0x0038	     */		add	%g3,%lo(TwoToMinus16),%g2
/* 0x003c	 232 */		ldd	[%o7],%f0
/* 0x0040	 239 */		add	%g4,%lo(TwoTo16),%g3
/* 0x0044	 223 */		or	%g0,%i4,%o0
/* 0x0048	 232 */		sllx	%g5,3,%g4
/* 0x004c	 239 */		ldx	[%o4+%g2],%o5
/* 0x0050	 223 */		or	%g0,%i5,%l3
/* 0x0054	     */		or	%g0,%i0,%l2
/* 0x0058	 239 */		ldx	[%o4+%g3],%o4
/* 0x005c	 234 */		cmp	%l0,16
/* 0x0060	 232 */		std	%f0,[%i2+%g4]
/* 0x0064	 234 */		be,pn	%icc,.L77000279
/* 0x0068	     */		or	%g0,%i3,%l4
/* 0x006c	 236 */		sll	%l0,2,%g2
/* 0x0070	 223 */		or	%g0,%o0,%i5
/* 0x0074	 236 */		add	%g2,2,%o0
/* 0x0078	 223 */		or	%g0,%i1,%i4
/* 0x007c	 236 */		cmp	%o0,0
/* 0x0080	 223 */		or	%g0,%i2,%l1
/* 0x0084	 236 */		ble,a,pt	%icc,.L900000657
/* 0x0088	     */		ldd	[%i1],%f6

!  241		      !     pdtj=&(dt[0]);
!  242		      !     for(j=jj=0;j<2*nlen;j++,jj++,pdtj++)
!  243		      !       {
!  244		      !	 m2j=pdm2[j];
!  245		      !	 a=pdtj[0]+pdn[0]*digit;
!  246		      !	 b=pdtj[1]+pdm1[0]*pdm2[j+1]+a*TwoToMinus16;
!  247		      !	 pdtj[1]=b;
!  249		      !#pragma pipeloop(0)
!  250		      !	 for(i=1;i<nlen;i++)
!  251		      !	   {
!  252		      !	     pdtj[2*i]+=pdm1[i]*m2j+pdn[i]*digit;
!  253		      !	   }
!  254		      ! 	 if((jj==30)) {cleanup(dt,j/2+1,2*nlen+1); jj=0;}
!  255		      !	 
!  256		      !	 digit=mod(lower32(b,Zero)*dn0,TwoToMinus16,TwoTo16);
!  257		      !       }
!  258		      !   }
!  259		      ! else
!  260		      !   {
!  261		      !     a=dt[0]=pdm1[0]*pdm2[0];
!  263		      !     dt[65]=     dt[64]=     dt[63]=     dt[62]=     dt[61]=     dt[60]=
!  264		      !     dt[59]=     dt[58]=     dt[57]=     dt[56]=     dt[55]=     dt[54]=
!  265		      !     dt[53]=     dt[52]=     dt[51]=     dt[50]=     dt[49]=     dt[48]=
!  266		      !     dt[47]=     dt[46]=     dt[45]=     dt[44]=     dt[43]=     dt[42]=
!  267		      !     dt[41]=     dt[40]=     dt[39]=     dt[38]=     dt[37]=     dt[36]=
!  268		      !     dt[35]=     dt[34]=     dt[33]=     dt[32]=     dt[31]=     dt[30]=
!  269		      !     dt[29]=     dt[28]=     dt[27]=     dt[26]=     dt[25]=     dt[24]=
!  270		      !     dt[23]=     dt[22]=     dt[21]=     dt[20]=     dt[19]=     dt[18]=
!  271		      !     dt[17]=     dt[16]=     dt[15]=     dt[14]=     dt[13]=     dt[12]=
!  272		      !     dt[11]=     dt[10]=     dt[ 9]=     dt[ 8]=     dt[ 7]=     dt[ 6]=
!  273		      !     dt[ 5]=     dt[ 4]=     dt[ 3]=     dt[ 2]=     dt[ 1]=Zero;
!  275		      !     pdn_0=pdn[0];
!  276		      !     pdm1_0=pdm1[0];
!  278		      !     digit=mod(lower32(a,Zero)*dn0,TwoToMinus16,TwoTo16);
!  279		      !     pdtj=&(dt[0]);
!  281		      !     for(j=0;j<32;j++,pdtj++)

/* 0x008c	 281 */		or	%g0,%o0,%o1
/* 0x0090	 236 */		sub	%o0,1,%g1
/* 0x0094	     */		or	%g0,0,%g2
/* 0x0098	 281 */		cmp	%o1,5
/* 0x009c	     */		bl,pn	%icc,.L77000280
/* 0x00a0	     */		or	%g0,8,%o0
/* 0x00a4	     */		std	%f0,[%i3]
/* 0x00a8	     */		or	%g0,2,%g2
/* 0x00ac	     */		sub	%g1,2,%o1
                       .L900000627:
/* 0x00b0	     */		add	%o0,8,%g3
/* 0x00b4	     */		std	%f0,[%i3+%o0]
/* 0x00b8	     */		add	%g2,3,%g2
/* 0x00bc	     */		add	%o0,16,%o2
/* 0x00c0	     */		std	%f0,[%i3+%g3]
/* 0x00c4	     */		cmp	%g2,%o1
/* 0x00c8	     */		add	%o0,24,%o0
/* 0x00cc	     */		ble,pt	%icc,.L900000627
/* 0x00d0	     */		std	%f0,[%i3+%o2]
                       .L900000630:
/* 0x00d4	     */		cmp	%g2,%g1
/* 0x00d8	     */		bg,pn	%icc,.L77000285
/* 0x00dc	     */		std	%f0,[%i3+%o0]
                       .L77000280:
/* 0x00e0	     */		ldd	[%o7],%f0
                       .L900000656:
/* 0x00e4	     */		sra	%g2,0,%o0
/* 0x00e8	     */		add	%g2,1,%g2
/* 0x00ec	     */		sllx	%o0,3,%o0
/* 0x00f0	     */		cmp	%g2,%g1
/* 0x00f4	     */		std	%f0,[%i3+%o0]
/* 0x00f8	     */		ble,a,pt	%icc,.L900000656
/* 0x00fc	     */		ldd	[%o7],%f0
                       .L77000285:
/* 0x0100	 238 */		ldd	[%i1],%f6
                       .L900000657:
/* 0x0104	 238 */		ldd	[%i2],%f8
/* 0x0108	 242 */		cmp	%o3,0
/* 0x010c	     */		sub	%o3,1,%o1
/* 0x0110	 239 */		ldd	[%o7],%f10
/* 0x0114	     */		add	%o3,1,%o2
/* 0x0118	   0 */		or	%g0,0,%i2
/* 0x011c	 238 */		fmuld	%f6,%f8,%f6
/* 0x0120	     */		std	%f6,[%i3]
/* 0x0124	   0 */		or	%g0,0,%g3
/* 0x0128	 239 */		ldd	[%o5],%f8
/* 0x012c	   0 */		or	%g0,%o2,%g1
/* 0x0130	 236 */		sub	%l0,1,%i1
/* 0x0134	 239 */		ldd	[%o4],%f12
/* 0x0138	 236 */		or	%g0,1,%g4
/* 0x013c	     */		fdtox	%f6,%f0
/* 0x0140	     */		fmovs	%f10,%f0
/* 0x0144	     */		fxtod	%f0,%f6
/* 0x0148	 239 */		fmuld	%f6,%f14,%f6
/* 0x014c	     */		fmuld	%f6,%f8,%f8
/* 0x0150	     */		fdtox	%f8,%f8
/* 0x0154	     */		fxtod	%f8,%f8
/* 0x0158	     */		fmuld	%f8,%f12,%f8
/* 0x015c	     */		fsubd	%f6,%f8,%f20
/* 0x0160	 242 */		ble,pt	%icc,.L900000650
/* 0x0164	     */		sllx	%g5,3,%g2
/* 0x0168	   0 */		st	%o1,[%sp+2223]
/* 0x016c	 246 */		ldd	[%i5],%f6
                       .L900000651:
/* 0x0170	 246 */		sra	%g4,0,%g2
/* 0x0174	     */		fmuld	%f6,%f20,%f6
/* 0x0178	     */		ldd	[%i3],%f12
/* 0x017c	     */		sllx	%g2,3,%g2
/* 0x0180	     */		ldd	[%i4],%f8
/* 0x0184	 250 */		cmp	%l0,1
/* 0x0188	 246 */		ldd	[%l1+%g2],%f10
/* 0x018c	 244 */		sra	%i2,0,%g2
/* 0x0190	     */		add	%i2,1,%i0
/* 0x0194	 246 */		faddd	%f12,%f6,%f6
/* 0x0198	     */		ldd	[%o5],%f12
/* 0x019c	 244 */		sllx	%g2,3,%g2
/* 0x01a0	 246 */		fmuld	%f8,%f10,%f8
/* 0x01a4	     */		ldd	[%i3+8],%f10
/* 0x01a8	     */		srl	%i2,31,%o3
/* 0x01ac	 244 */		ldd	[%l1+%g2],%f18
/* 0x01b0	   0 */		or	%g0,1,%l5
/* 0x01b4	 236 */		or	%g0,2,%g2
/* 0x01b8	 246 */		fmuld	%f6,%f12,%f6
/* 0x01bc	 250 */		or	%g0,32,%o1
/* 0x01c0	     */		or	%g0,48,%o2
/* 0x01c4	 246 */		faddd	%f10,%f8,%f8
/* 0x01c8	     */		faddd	%f8,%f6,%f16
/* 0x01cc	 250 */		ble,pn	%icc,.L77000213
/* 0x01d0	     */		std	%f16,[%i3+8]
/* 0x01d4	     */		cmp	%i1,8
/* 0x01d8	     */		sub	%l0,3,%o3
/* 0x01dc	     */		bl,pn	%icc,.L77000284
/* 0x01e0	     */		or	%g0,8,%o0
/* 0x01e4	 252 */		ldd	[%i5+8],%f0
/* 0x01e8	     */		or	%g0,6,%l5
/* 0x01ec	     */		ldd	[%i4+8],%f2
/* 0x01f0	     */		or	%g0,4,%g2
/* 0x01f4	 250 */		or	%g0,40,%o0
/* 0x01f8	 252 */		ldd	[%i5+16],%f8
/* 0x01fc	     */		fmuld	%f0,%f20,%f10
/* 0x0200	     */		ldd	[%i4+16],%f4
/* 0x0204	     */		fmuld	%f2,%f18,%f2
/* 0x0208	     */		ldd	[%i3+16],%f0
/* 0x020c	     */		fmuld	%f8,%f20,%f12
/* 0x0210	     */		ldd	[%i4+24],%f6
/* 0x0214	     */		fmuld	%f4,%f18,%f4
/* 0x0218	     */		ldd	[%i5+24],%f8
/* 0x021c	     */		faddd	%f2,%f10,%f2
/* 0x0220	     */		ldd	[%i4+32],%f14
/* 0x0224	     */		fmuld	%f6,%f18,%f10
/* 0x0228	     */		ldd	[%i5+32],%f6
/* 0x022c	     */		faddd	%f4,%f12,%f4
/* 0x0230	     */		ldd	[%i4+40],%f12
/* 0x0234	     */		faddd	%f0,%f2,%f0
/* 0x0238	     */		std	%f0,[%i3+16]
/* 0x023c	     */		ldd	[%i3+32],%f0
/* 0x0240	     */		ldd	[%i3+48],%f2
                       .L900000639:
/* 0x0244	     */		add	%o2,16,%l6
/* 0x0248	 252 */		ldd	[%i5+%o0],%f22
/* 0x024c	     */		add	%l5,3,%l5
/* 0x0250	     */		fmuld	%f8,%f20,%f8
/* 0x0254	 250 */		add	%o0,8,%o0
/* 0x0258	 252 */		ldd	[%l6+%i3],%f26
/* 0x025c	     */		cmp	%l5,%o3
/* 0x0260	     */		ldd	[%i4+%o0],%f24
/* 0x0264	     */		faddd	%f0,%f4,%f0
/* 0x0268	     */		add	%g2,6,%g2
/* 0x026c	     */		faddd	%f10,%f8,%f10
/* 0x0270	     */		fmuld	%f14,%f18,%f4
/* 0x0274	     */		std	%f0,[%o1+%i3]
/* 0x0278	 250 */		add	%o2,32,%o1
/* 0x027c	 252 */		ldd	[%i5+%o0],%f8
/* 0x0280	     */		fmuld	%f6,%f20,%f6
/* 0x0284	 250 */		add	%o0,8,%o0
/* 0x0288	 252 */		ldd	[%o1+%i3],%f0
/* 0x028c	     */		ldd	[%i4+%o0],%f14
/* 0x0290	     */		faddd	%f2,%f10,%f2
/* 0x0294	     */		faddd	%f4,%f6,%f10
/* 0x0298	     */		fmuld	%f12,%f18,%f4
/* 0x029c	     */		std	%f2,[%o2+%i3]
/* 0x02a0	 250 */		add	%o2,48,%o2
/* 0x02a4	 252 */		ldd	[%i5+%o0],%f6
/* 0x02a8	     */		fmuld	%f22,%f20,%f22
/* 0x02ac	 250 */		add	%o0,8,%o0
/* 0x02b0	 252 */		ldd	[%o2+%i3],%f2
/* 0x02b4	     */		ldd	[%i4+%o0],%f12
/* 0x02b8	     */		faddd	%f26,%f10,%f10
/* 0x02bc	     */		std	%f10,[%l6+%i3]
/* 0x02c0	     */		fmuld	%f24,%f18,%f10
/* 0x02c4	     */		ble,pt	%icc,.L900000639
/* 0x02c8	     */		faddd	%f4,%f22,%f4
                       .L900000642:
/* 0x02cc	 252 */		fmuld	%f8,%f20,%f24
/* 0x02d0	     */		faddd	%f0,%f4,%f8
/* 0x02d4	 250 */		add	%o2,16,%o3
/* 0x02d8	 252 */		ldd	[%o3+%i3],%f4
/* 0x02dc	     */		fmuld	%f14,%f18,%f0
/* 0x02e0	     */		cmp	%l5,%i1
/* 0x02e4	     */		std	%f8,[%o1+%i3]
/* 0x02e8	     */		fmuld	%f12,%f18,%f8
/* 0x02ec	 250 */		add	%o2,32,%o1
/* 0x02f0	 252 */		faddd	%f10,%f24,%f12
/* 0x02f4	     */		ldd	[%i5+%o0],%f22
/* 0x02f8	     */		fmuld	%f6,%f20,%f6
/* 0x02fc	     */		add	%g2,8,%g2
/* 0x0300	     */		fmuld	%f22,%f20,%f10
/* 0x0304	     */		faddd	%f2,%f12,%f2
/* 0x0308	     */		faddd	%f0,%f6,%f6
/* 0x030c	     */		ldd	[%o1+%i3],%f0
/* 0x0310	     */		std	%f2,[%o2+%i3]
/* 0x0314	     */		faddd	%f8,%f10,%f2
/* 0x0318	     */		sra	%l5,0,%o2
/* 0x031c	     */		sllx	%o2,3,%o0
/* 0x0320	     */		faddd	%f4,%f6,%f4
/* 0x0324	     */		std	%f4,[%o3+%i3]
/* 0x0328	     */		faddd	%f0,%f2,%f0
/* 0x032c	     */		std	%f0,[%o1+%i3]
/* 0x0330	     */		bg,a,pn	%icc,.L77000213
/* 0x0334	     */		srl	%i2,31,%o3
                       .L77000284:
/* 0x0338	 252 */		ldd	[%i4+%o0],%f2
                       .L900000655:
/* 0x033c	 252 */		ldd	[%i5+%o0],%f0
/* 0x0340	     */		fmuld	%f2,%f18,%f2
/* 0x0344	     */		sra	%g2,0,%o0
/* 0x0348	     */		sllx	%o0,3,%o1
/* 0x034c	     */		add	%l5,1,%l5
/* 0x0350	     */		fmuld	%f0,%f20,%f4
/* 0x0354	     */		ldd	[%o1+%i3],%f0
/* 0x0358	     */		sra	%l5,0,%o2
/* 0x035c	     */		sllx	%o2,3,%o0
/* 0x0360	     */		add	%g2,2,%g2
/* 0x0364	     */		cmp	%l5,%i1
/* 0x0368	     */		faddd	%f2,%f4,%f2
/* 0x036c	     */		faddd	%f0,%f2,%f0
/* 0x0370	     */		std	%f0,[%o1+%i3]
/* 0x0374	     */		ble,a,pt	%icc,.L900000655
/* 0x0378	     */		ldd	[%i4+%o0],%f2
                       .L900000626:
/* 0x037c	     */		srl	%i2,31,%o3
/* 0x0380	 252 */		ba	.L900000654
/* 0x0384	     */		cmp	%g3,30
                       .L77000213:
/* 0x0388	 254 */		cmp	%g3,30
                       .L900000654:
/* 0x038c	     */		add	%i2,%o3,%o0
/* 0x0390	 254 */		bne,a,pt	%icc,.L900000653
/* 0x0394	     */		fdtox	%f16,%f0
/* 0x0398	 281 */		sra	%o0,1,%g2
/* 0x039c	     */		add	%g2,1,%g2
/* 0x03a0	     */		ldd	[%o7],%f0
/* 0x03a4	     */		sll	%g2,1,%o1
/* 0x03a8	     */		sll	%g1,1,%g2
/* 0x03ac	     */		or	%g0,%o1,%o2
/* 0x03b0	     */		fmovd	%f0,%f2
/* 0x03b4	     */		or	%g0,%g2,%o0
/* 0x03b8	     */		cmp	%o1,%o0
/* 0x03bc	     */		sub	%g2,1,%o0
/* 0x03c0	     */		bge,pt	%icc,.L77000215
/* 0x03c4	     */		or	%g0,0,%g3
/* 0x03c8	 254 */		add	%o1,1,%o1
/* 0x03cc	 281 */		sra	%o2,0,%g2
                       .L900000652:
/* 0x03d0	     */		sllx	%g2,3,%g2
/* 0x03d4	     */		ldd	[%o7],%f6
/* 0x03d8	     */		add	%o2,2,%o2
/* 0x03dc	     */		sra	%o1,0,%g3
/* 0x03e0	     */		ldd	[%g2+%l4],%f8
/* 0x03e4	     */		cmp	%o2,%o0
/* 0x03e8	     */		sllx	%g3,3,%g3
/* 0x03ec	     */		add	%o1,2,%o1
/* 0x03f0	     */		ldd	[%l4+%g3],%f10
/* 0x03f4	     */		fdtox	%f8,%f12
/* 0x03f8	     */		fdtox	%f10,%f4
/* 0x03fc	     */		fmovd	%f12,%f8
/* 0x0400	     */		fmovs	%f6,%f12
/* 0x0404	     */		fmovs	%f6,%f4
/* 0x0408	     */		fxtod	%f12,%f6
/* 0x040c	     */		fxtod	%f4,%f12
/* 0x0410	     */		fdtox	%f10,%f4
/* 0x0414	     */		faddd	%f6,%f2,%f6
/* 0x0418	     */		std	%f6,[%g2+%l4]
/* 0x041c	     */		faddd	%f12,%f0,%f6
/* 0x0420	     */		std	%f6,[%l4+%g3]
/* 0x0424	     */		fitod	%f8,%f2
/* 0x0428	     */		fitod	%f4,%f0
/* 0x042c	     */		ble,pt	%icc,.L900000652
/* 0x0430	     */		sra	%o2,0,%g2
                       .L77000233:
/* 0x0434	     */		or	%g0,0,%g3
                       .L77000215:
/* 0x0438	     */		fdtox	%f16,%f0
                       .L900000653:
/* 0x043c	 256 */		ldd	[%o7],%f6
/* 0x0440	     */		add	%g4,1,%g4
/* 0x0444	     */		or	%g0,%i0,%i2
/* 0x0448	     */		ldd	[%o5],%f8
/* 0x044c	     */		add	%g3,1,%g3
/* 0x0450	     */		add	%i3,8,%i3
/* 0x0454	     */		fmovs	%f6,%f0
/* 0x0458	     */		ldd	[%o4],%f10
/* 0x045c	     */		ld	[%sp+2223],%o0
/* 0x0460	     */		fxtod	%f0,%f6
/* 0x0464	     */		cmp	%i0,%o0
/* 0x0468	     */		fmuld	%f6,%f30,%f6
/* 0x046c	     */		fmuld	%f6,%f8,%f8
/* 0x0470	     */		fdtox	%f8,%f8
/* 0x0474	     */		fxtod	%f8,%f8
/* 0x0478	     */		fmuld	%f8,%f10,%f8
/* 0x047c	     */		fsubd	%f6,%f8,%f20
/* 0x0480	     */		ble,a,pt	%icc,.L900000651
/* 0x0484	     */		ldd	[%i5],%f6
                       .L900000625:
/* 0x0488	 256 */		ba	.L900000650
/* 0x048c	     */		sllx	%g5,3,%g2
                       .L77000279:
/* 0x0490	 261 */		ldd	[%i1],%f4
/* 0x0494	     */		ldd	[%i2],%f6
/* 0x0498	 273 */		std	%f0,[%i3+8]
/* 0x049c	     */		std	%f0,[%i3+16]
/* 0x04a0	 261 */		fmuld	%f4,%f6,%f6
/* 0x04a4	     */		std	%f6,[%i3]
/* 0x04a8	 273 */		std	%f0,[%i3+24]
/* 0x04ac	     */		std	%f0,[%i3+32]
/* 0x04b0	     */		fdtox	%f6,%f2
/* 0x04b4	     */		std	%f0,[%i3+40]
/* 0x04b8	     */		std	%f0,[%i3+48]
/* 0x04bc	     */		std	%f0,[%i3+56]
/* 0x04c0	     */		std	%f0,[%i3+64]
/* 0x04c4	     */		fmovs	%f0,%f2
/* 0x04c8	     */		std	%f0,[%i3+72]
/* 0x04cc	     */		std	%f0,[%i3+80]
/* 0x04d0	     */		std	%f0,[%i3+88]
/* 0x04d4	     */		std	%f0,[%i3+96]
/* 0x04d8	     */		std	%f0,[%i3+104]
/* 0x04dc	     */		std	%f0,[%i3+112]
/* 0x04e0	     */		std	%f0,[%i3+120]
/* 0x04e4	     */		std	%f0,[%i3+128]
/* 0x04e8	     */		std	%f0,[%i3+136]
/* 0x04ec	     */		std	%f0,[%i3+144]
/* 0x04f0	     */		std	%f0,[%i3+152]
/* 0x04f4	     */		std	%f0,[%i3+160]
/* 0x04f8	     */		std	%f0,[%i3+168]
/* 0x04fc	     */		fxtod	%f2,%f6
/* 0x0500	     */		std	%f0,[%i3+176]
/* 0x0504	 281 */		or	%g0,1,%o2
/* 0x0508	 273 */		std	%f0,[%i3+184]

!  282		      !       {
!  284		      !	 m2j=pdm2[j];
!  285		      !	 a=pdtj[0]+pdn_0*digit;
!  286		      !	 b=pdtj[1]+pdm1_0*pdm2[j+1]+a*TwoToMinus16;

/* 0x050c	 286 */		sra	%o2,0,%g2
/* 0x0510	 279 */		or	%g0,%i3,%o3
/* 0x0514	 273 */		std	%f0,[%i3+192]
/* 0x0518	 278 */		fmuld	%f6,%f14,%f6
/* 0x051c	 281 */		or	%g0,0,%g1
/* 0x0520	 273 */		std	%f0,[%i3+200]
/* 0x0524	     */		std	%f0,[%i3+208]
/* 0x0528	     */		std	%f0,[%i3+216]
/* 0x052c	     */		std	%f0,[%i3+224]
/* 0x0530	     */		std	%f0,[%i3+232]
/* 0x0534	     */		std	%f0,[%i3+240]
/* 0x0538	     */		std	%f0,[%i3+248]
/* 0x053c	     */		std	%f0,[%i3+256]
/* 0x0540	     */		std	%f0,[%i3+264]
/* 0x0544	     */		std	%f0,[%i3+272]
/* 0x0548	     */		std	%f0,[%i3+280]
/* 0x054c	     */		std	%f0,[%i3+288]
/* 0x0550	     */		std	%f0,[%i3+296]
/* 0x0554	     */		std	%f0,[%i3+304]
/* 0x0558	     */		std	%f0,[%i3+312]
/* 0x055c	     */		std	%f0,[%i3+320]
/* 0x0560	     */		std	%f0,[%i3+328]
/* 0x0564	     */		std	%f0,[%i3+336]
/* 0x0568	     */		std	%f0,[%i3+344]
/* 0x056c	     */		std	%f0,[%i3+352]
/* 0x0570	     */		std	%f0,[%i3+360]
/* 0x0574	     */		std	%f0,[%i3+368]
/* 0x0578	     */		std	%f0,[%i3+376]
/* 0x057c	     */		std	%f0,[%i3+384]
/* 0x0580	     */		std	%f0,[%i3+392]
/* 0x0584	     */		std	%f0,[%i3+400]
/* 0x0588	     */		std	%f0,[%i3+408]
/* 0x058c	     */		std	%f0,[%i3+416]
/* 0x0590	     */		std	%f0,[%i3+424]
/* 0x0594	     */		std	%f0,[%i3+432]
/* 0x0598	     */		std	%f0,[%i3+440]
/* 0x059c	     */		std	%f0,[%i3+448]
/* 0x05a0	     */		std	%f0,[%i3+456]
/* 0x05a4	     */		std	%f0,[%i3+464]
/* 0x05a8	     */		std	%f0,[%i3+472]
/* 0x05ac	     */		std	%f0,[%i3+480]
/* 0x05b0	     */		std	%f0,[%i3+488]
/* 0x05b4	     */		std	%f0,[%i3+496]
/* 0x05b8	 278 */		ldd	[%o5],%f8
/* 0x05bc	     */		ldd	[%o4],%f10
/* 0x05c0	     */		fmuld	%f6,%f8,%f8
/* 0x05c4	 273 */		std	%f0,[%i3+504]
/* 0x05c8	     */		std	%f0,[%i3+512]
/* 0x05cc	     */		std	%f0,[%i3+520]
/* 0x05d0	     */		fdtox	%f8,%f8
/* 0x05d4	 275 */		ldd	[%o0],%f0
/* 0x05d8	     */		fxtod	%f8,%f8
/* 0x05dc	     */		fmuld	%f8,%f10,%f8
/* 0x05e0	     */		fsubd	%f6,%f8,%f2

!  287		      !	 pdtj[1]=b;
!  289		      !	 /**** this loop will be fully unrolled:
!  290		      !	 for(i=1;i<16;i++)
!  291		      !	   {
!  292		      !	     pdtj[2*i]+=pdm1[i]*m2j+pdn[i]*digit;
!  293		      !	   }
!  294		      !	 *************************************/
!  295		      !	     pdtj[2]+=pdm1[1]*m2j+pdn[1]*digit;
!  296		      !	     pdtj[4]+=pdm1[2]*m2j+pdn[2]*digit;
!  297		      !	     pdtj[6]+=pdm1[3]*m2j+pdn[3]*digit;
!  298		      !	     pdtj[8]+=pdm1[4]*m2j+pdn[4]*digit;
!  299		      !	     pdtj[10]+=pdm1[5]*m2j+pdn[5]*digit;
!  300		      !	     pdtj[12]+=pdm1[6]*m2j+pdn[6]*digit;
!  301		      !	     pdtj[14]+=pdm1[7]*m2j+pdn[7]*digit;
!  302		      !	     pdtj[16]+=pdm1[8]*m2j+pdn[8]*digit;
!  303		      !	     pdtj[18]+=pdm1[9]*m2j+pdn[9]*digit;
!  304		      !	     pdtj[20]+=pdm1[10]*m2j+pdn[10]*digit;
!  305		      !	     pdtj[22]+=pdm1[11]*m2j+pdn[11]*digit;
!  306		      !	     pdtj[24]+=pdm1[12]*m2j+pdn[12]*digit;
!  307		      !	     pdtj[26]+=pdm1[13]*m2j+pdn[13]*digit;
!  308		      !	     pdtj[28]+=pdm1[14]*m2j+pdn[14]*digit;
!  309		      !	     pdtj[30]+=pdm1[15]*m2j+pdn[15]*digit;
!  310		      !	 /* no need for cleenup, cannot overflow */
!  311		      !	 digit=mod(lower32(b,Zero)*dn0,TwoToMinus16,TwoTo16);


	fmovd %f2,%f0		! hand modified
	fmovd %f30,%f18		! hand modified
	ldd [%o0],%f2
	ldd [%o3],%f8
	ldd [%i1],%f10
	ldd [%o5],%f14		! hand modified
	ldd [%o4],%f16		! hand modified
	ldd [%i2],%f24

	ldd [%i1+8],%f26
	ldd [%i1+16],%f40
	ldd [%i1+48],%f46
	ldd [%i1+56],%f30
	ldd [%i1+64],%f54
	ldd [%i1+104],%f34
	ldd [%i1+112],%f58

	ldd [%o0+8],%f28	
	ldd [%o0+104],%f38
	ldd [%o0+112],%f60

	.L99999999: 			!1
	ldd	[%i1+24],%f32
	fmuld	%f0,%f2,%f4 	!2
	ldd	[%o0+24],%f36
	fmuld	%f26,%f24,%f20 	!3
	ldd	[%i1+40],%f42
	fmuld	%f28,%f0,%f22 	!4
	ldd	[%o0+40],%f44
	fmuld	%f32,%f24,%f32 	!5
	ldd	[%i2+8],%f6
	faddd	%f4,%f8,%f4
	fmuld	%f36,%f0,%f36 	!6
	add	%i2,8,%i2
	ldd	[%o0+56],%f50
	fmuld	%f42,%f24,%f42 	!7
	ldd	[%i1+72],%f52
	faddd	%f20,%f22,%f20
	fmuld	%f44,%f0,%f44 	!8
	ldd	[%o3+16],%f22
	fmuld	%f10,%f6,%f12 	!9
	ldd	[%o0+72],%f56
	faddd	%f32,%f36,%f32
	fmuld	%f14,%f4,%f4 !10
	ldd	[%o3+48],%f36
	fmuld	%f30,%f24,%f48 	!11
	ldd	[%o3+8],%f8
	faddd	%f20,%f22,%f20
	fmuld	%f50,%f0,%f50	!12
	std	%f20,[%o3+16]
	faddd	%f42,%f44,%f42
	fmuld	%f52,%f24,%f52 	!13
	ldd	[%o3+80],%f44
	faddd	%f4,%f12,%f4
	fmuld	%f56,%f0,%f56 	!14
	ldd	[%i1+88],%f20
	faddd	%f32,%f36,%f32 	!15
	ldd	[%o0+88],%f22
	faddd	%f48,%f50,%f48 	!16
	ldd	[%o3+112],%f50
	faddd	%f52,%f56,%f52 	!17
	ldd	[%o3+144],%f56
	faddd	%f4,%f8,%f8
	fmuld	%f20,%f24,%f20 	!18
	std	%f32,[%o3+48]
	faddd	%f42,%f44,%f42
	fmuld	%f22,%f0,%f22 	!19
	std	%f42,[%o3+80]
	faddd	%f48,%f50,%f48
	fmuld	%f34,%f24,%f32 	!20
	std	%f48,[%o3+112]
	faddd	%f52,%f56,%f52
	fmuld	%f38,%f0,%f36 	!21
	ldd	[%i1+120],%f42
	fdtox	%f8,%f4 		!22
	std	%f52,[%o3+144]
	faddd	%f20,%f22,%f20 	!23
	ldd	[%o0+120],%f44 	!24
	ldd	[%o3+176],%f22
	faddd	%f32,%f36,%f32
	fmuld	%f42,%f24,%f42 	!25
	ldd	[%o0+16],%f50
	fmovs	%f17,%f4 	!26
	ldd	[%i1+32],%f52
	fmuld	%f44,%f0,%f44 	!27
	ldd	[%o0+32],%f56
	fmuld	%f40,%f24,%f48 	!28
	ldd	[%o3+208],%f36
	faddd	%f20,%f22,%f20
	fmuld	%f50,%f0,%f50 	!29
	std	%f20,[%o3+176]
	fxtod	%f4,%f4
	fmuld	%f52,%f24,%f52 	!30
	ldd	[%o0+48],%f22
	faddd	%f42,%f44,%f42
	fmuld	%f56,%f0,%f56 	!31
	ldd	[%o3+240],%f44
	faddd	%f32,%f36,%f32 	!32
	std	%f32,[%o3+208]
	faddd	%f48,%f50,%f48
	fmuld	%f46,%f24,%f20 	!33
	ldd	[%o3+32],%f50
	fmuld	%f4,%f18,%f12 	!34
	ldd	[%o0+64],%f36
	faddd	%f52,%f56,%f52
	fmuld	%f22,%f0,%f22 	!35
	ldd	[%o3+64],%f56
	faddd	%f42,%f44,%f42 	!36
	std	%f42,[%o3+240]
	faddd	%f48,%f50,%f48
	fmuld	%f54,%f24,%f32 	!37
	std	%f48,[%o3+32]
	fmuld	%f12,%f14,%f4 !38
	ldd	[%i1+80],%f42
	faddd	%f52,%f56,%f56	! yes, tmp52!
	fmuld	%f36,%f0,%f36 	!39
	ldd	[%o0+80],%f44
	faddd	%f20,%f22,%f20 	!40
	ldd	[%i1+96],%f48
	fmuld	%f58,%f24,%f52 	!41
	ldd	[%o0+96],%f50
	fdtox	%f4,%f4
	fmuld	%f42,%f24,%f42 	!42
	std	%f56,[%o3+64]	! yes, tmp52!
	faddd	%f32,%f36,%f32
	fmuld	%f44,%f0,%f44 	!43
	ldd	[%o3+96],%f22
	fmuld	%f48,%f24,%f48 	!44
	ldd	[%o3+128],%f36
	fmovd	%f6,%f24
	fmuld	%f50,%f0,%f50 	!45
	fxtod	%f4,%f4
	fmuld	%f60,%f0,%f56 	!46
	add	%o3,8,%o3
	faddd	%f42,%f44,%f42 	!47
	ldd	[%o3+160-8],%f44
	faddd	%f20,%f22,%f20 	!48
	std	%f20,[%o3+96-8]
	faddd	%f48,%f50,%f48 	!49
	ldd	[%o3+192-8],%f50
	faddd	%f52,%f56,%f52
	fmuld	%f4,%f16,%f4 	!50
	ldd	[%o3+224-8],%f56
	faddd	%f32,%f36,%f32 	!51
	std	%f32,[%o3+128-8]
	faddd	%f42,%f44,%f42 	!52
	add	%g1,1,%g1
	std	%f42,[%o3+160-8]
	faddd	%f48,%f50,%f48 	!53
	cmp	%g1,31
	std	%f48,[%o3+192-8]
	fsubd	%f12,%f4,%f0 	!54
	faddd	%f52,%f56,%f52
	ble,pt	%icc,.L99999999
	std	%f52,[%o3+224-8] 	!55
	std %f8,[%o3]
!  312		      !       }
!  313		      !   }
!  315		      ! conv_d16_to_i32(result,dt+2*nlen,(long long *)dt,nlen+1);

/* 0x0844	 315 */		sllx	%g5,3,%g2
                       .L900000650:
/* 0x0848	 315 */		ldd	[%g2+%l4],%f2
/* 0x084c	     */		add	%l4,%g2,%o0
/* 0x0850	     */		or	%g0,0,%g1
/* 0x0854	     */		ldd	[%o0+8],%f4
/* 0x0858	     */		or	%g0,0,%i2
/* 0x085c	     */		cmp	%l0,0
/* 0x0860	     */		fdtox	%f2,%f2
/* 0x0864	     */		std	%f2,[%sp+2255]
/* 0x0868	 311 */		sethi	%hi(0xfc00),%o3
/* 0x086c	 315 */		fdtox	%f4,%f2
/* 0x0870	     */		std	%f2,[%sp+2247]
/* 0x0874	 311 */		or	%g0,-1,%o2
/* 0x0878	     */		srl	%o2,0,%o5
/* 0x087c	     */		or	%g0,2,%g5
/* 0x0880	     */		sub	%l0,1,%g3
/* 0x0884	     */		or	%g0,%o0,%o7
/* 0x0888	     */		add	%o3,1023,%o4
/* 0x088c	 315 */		or	%g0,64,%o3
/* 0x0890	     */		ldx	[%sp+2255],%i0
/* 0x0894	     */		sub	%l0,2,%o1
/* 0x0898	     */		ldx	[%sp+2247],%i1
/* 0x089c	     */		ble,pt	%icc,.L900000648
/* 0x08a0	     */		sethi	%hi(0xfc00),%g2
/* 0x08a4	     */		cmp	%l0,6
/* 0x08a8	     */		and	%i0,%o5,%o2
/* 0x08ac	     */		bl,pn	%icc,.L77000287
/* 0x08b0	     */		or	%g0,3,%g4
/* 0x08b4	     */		ldd	[%o7+16],%f0
/* 0x08b8	     */		and	%i1,%o4,%i3
/* 0x08bc	     */		sllx	%i3,16,%o0
/* 0x08c0	     */		or	%g0,5,%g4
/* 0x08c4	     */		srax	%i1,16,%i4
/* 0x08c8	     */		fdtox	%f0,%f0
/* 0x08cc	     */		std	%f0,[%sp+2239]
/* 0x08d0	     */		srax	%i0,32,%i1
/* 0x08d4	     */		add	%o2,%o0,%i5
/* 0x08d8	     */		ldd	[%o7+24],%f0
/* 0x08dc	     */		and	%i5,%o5,%l1
/* 0x08e0	     */		or	%g0,72,%o2
/* 0x08e4	     */		or	%g0,4,%o0
/* 0x08e8	     */		or	%g0,4,%g5
/* 0x08ec	     */		ldx	[%sp+2239],%g1
/* 0x08f0	     */		fdtox	%f0,%f0
/* 0x08f4	     */		or	%g0,4,%i2
/* 0x08f8	     */		std	%f0,[%sp+2231]
/* 0x08fc	     */		ldd	[%o7+40],%f2
/* 0x0900	     */		and	%g1,%o5,%i3
/* 0x0904	     */		ldd	[%o7+32],%f0
/* 0x0908	     */		srax	%g1,32,%g1
/* 0x090c	     */		ldd	[%o7+56],%f4
/* 0x0910	     */		fdtox	%f2,%f2
/* 0x0914	     */		ldx	[%sp+2231],%g2
/* 0x0918	     */		fdtox	%f0,%f0
/* 0x091c	     */		st	%l1,[%l2]
/* 0x0920	     */		srax	%i5,32,%l1
/* 0x0924	     */		fdtox	%f4,%f4
/* 0x0928	     */		std	%f2,[%sp+2231]
/* 0x092c	     */		and	%g2,%o4,%i5
/* 0x0930	     */		add	%i4,%l1,%i4
/* 0x0934	     */		std	%f0,[%sp+2239]
/* 0x0938	     */		sllx	%i5,16,%i0
/* 0x093c	     */		add	%i1,%i4,%i1
/* 0x0940	     */		ldd	[%o7+48],%f2
/* 0x0944	     */		srax	%g2,16,%g2
/* 0x0948	     */		add	%i3,%i0,%i0
/* 0x094c	     */		ldd	[%o7+72],%f0
/* 0x0950	     */		add	%i0,%i1,%i3
/* 0x0954	     */		srax	%i3,32,%i4
/* 0x0958	     */		fdtox	%f2,%f2
/* 0x095c	     */		and	%i3,%o5,%i3
/* 0x0960	     */		ldx	[%sp+2231],%i1
/* 0x0964	     */		add	%g2,%i4,%g2
/* 0x0968	     */		ldx	[%sp+2239],%i0
/* 0x096c	     */		add	%g1,%g2,%g1
/* 0x0970	     */		std	%f2,[%sp+2239]
/* 0x0974	     */		std	%f4,[%sp+2231]
/* 0x0978	     */		ldd	[%o7+64],%f2
/* 0x097c	     */		st	%i3,[%l2+4]
                       .L900000631:
/* 0x0980	     */		ldx	[%sp+2231],%i3
/* 0x0984	     */		add	%i2,2,%i2
/* 0x0988	     */		add	%g4,4,%g4
/* 0x098c	     */		ldx	[%sp+2239],%i5
/* 0x0990	     */		add	%o2,16,%o2
/* 0x0994	     */		and	%i1,%o4,%g2
/* 0x0998	     */		sllx	%g2,16,%i4
/* 0x099c	     */		and	%i0,%o5,%g2
/* 0x09a0	     */		ldd	[%o7+%o2],%f4
/* 0x09a4	     */		fdtox	%f0,%f0
/* 0x09a8	     */		std	%f0,[%sp+2231]
/* 0x09ac	     */		srax	%i1,16,%i1
/* 0x09b0	     */		add	%g2,%i4,%g2
/* 0x09b4	     */		fdtox	%f2,%f0
/* 0x09b8	     */		add	%o3,16,%o3
/* 0x09bc	     */		std	%f0,[%sp+2239]
/* 0x09c0	     */		add	%g2,%g1,%g1
/* 0x09c4	     */		ldd	[%o7+%o3],%f2
/* 0x09c8	     */		srax	%g1,32,%i4
/* 0x09cc	     */		cmp	%i2,%o1
/* 0x09d0	     */		srax	%i0,32,%g2
/* 0x09d4	     */		add	%i1,%i4,%i0
/* 0x09d8	     */		add	%g2,%i0,%i4
/* 0x09dc	     */		add	%o0,4,%o0
/* 0x09e0	     */		and	%g1,%o5,%g2
/* 0x09e4	     */		or	%g0,%i5,%g1
/* 0x09e8	     */		st	%g2,[%l2+%o0]
/* 0x09ec	     */		add	%g5,4,%g5
/* 0x09f0	     */		ldx	[%sp+2231],%i1
/* 0x09f4	     */		ldx	[%sp+2239],%i0
/* 0x09f8	     */		add	%o2,16,%o2
/* 0x09fc	     */		and	%i3,%o4,%g2
/* 0x0a00	     */		sllx	%g2,16,%i5
/* 0x0a04	     */		and	%g1,%o5,%g2
/* 0x0a08	     */		ldd	[%o7+%o2],%f0
/* 0x0a0c	     */		fdtox	%f4,%f4
/* 0x0a10	     */		std	%f4,[%sp+2231]
/* 0x0a14	     */		srax	%i3,16,%i3
/* 0x0a18	     */		add	%g2,%i5,%g2
/* 0x0a1c	     */		fdtox	%f2,%f2
/* 0x0a20	     */		add	%o3,16,%o3
/* 0x0a24	     */		std	%f2,[%sp+2239]
/* 0x0a28	     */		add	%g2,%i4,%g2
/* 0x0a2c	     */		ldd	[%o7+%o3],%f2
/* 0x0a30	     */		srax	%g2,32,%i4
/* 0x0a34	     */		srax	%g1,32,%g1
/* 0x0a38	     */		add	%i3,%i4,%i3
/* 0x0a3c	     */		add	%g1,%i3,%g1
/* 0x0a40	     */		add	%o0,4,%o0
/* 0x0a44	     */		and	%g2,%o5,%g2
/* 0x0a48	     */		ble,pt	%icc,.L900000631
/* 0x0a4c	     */		st	%g2,[%l2+%o0]
                       .L900000634:
/* 0x0a50	     */		srax	%i1,16,%i5
/* 0x0a54	     */		ldx	[%sp+2231],%o1
/* 0x0a58	     */		and	%i1,%o4,%i3
/* 0x0a5c	     */		sllx	%i3,16,%i3
/* 0x0a60	     */		ldx	[%sp+2239],%i4
/* 0x0a64	     */		and	%i0,%o5,%g2
/* 0x0a68	     */		add	%g2,%i3,%g2
/* 0x0a6c	     */		and	%o1,%o4,%i3
/* 0x0a70	     */		fdtox	%f0,%f4
/* 0x0a74	     */		sllx	%i3,16,%i3
/* 0x0a78	     */		std	%f4,[%sp+2231]
/* 0x0a7c	     */		add	%g2,%g1,%g2
/* 0x0a80	     */		srax	%g2,32,%l1
/* 0x0a84	     */		and	%i4,%o5,%i1
/* 0x0a88	     */		fdtox	%f2,%f0
/* 0x0a8c	     */		srax	%i0,32,%g1
/* 0x0a90	     */		std	%f0,[%sp+2239]
/* 0x0a94	     */		add	%i5,%l1,%i0
/* 0x0a98	     */		srax	%o1,16,%o1
/* 0x0a9c	     */		add	%g1,%i0,%i0
/* 0x0aa0	     */		add	%o0,4,%g1
/* 0x0aa4	     */		add	%i1,%i3,%o0
/* 0x0aa8	     */		and	%g2,%o5,%g2
/* 0x0aac	     */		st	%g2,[%l2+%g1]
/* 0x0ab0	     */		add	%o0,%i0,%o0
/* 0x0ab4	     */		srax	%o0,32,%i3
/* 0x0ab8	     */		ldx	[%sp+2231],%i1
/* 0x0abc	     */		add	%g1,4,%g1
/* 0x0ac0	     */		ldx	[%sp+2239],%i0
/* 0x0ac4	     */		and	%o0,%o5,%g2
/* 0x0ac8	     */		add	%o1,%i3,%o1
/* 0x0acc	     */		srax	%i4,32,%o0
/* 0x0ad0	     */		cmp	%i2,%g3
/* 0x0ad4	     */		st	%g2,[%l2+%g1]
/* 0x0ad8	     */		bg,pn	%icc,.L77000236
/* 0x0adc	     */		add	%o0,%o1,%g1
/* 0x0ae0	     */		add	%g4,6,%g4
/* 0x0ae4	     */		add	%g5,6,%g5
                       .L77000287:
/* 0x0ae8	     */		sra	%g5,0,%o1
                       .L900000647:
/* 0x0aec	     */		sllx	%o1,3,%o2
/* 0x0af0	     */		and	%i0,%o5,%o0
/* 0x0af4	     */		ldd	[%o7+%o2],%f0
/* 0x0af8	     */		sra	%g4,0,%o2
/* 0x0afc	     */		and	%i1,%o4,%o1
/* 0x0b00	     */		sllx	%o2,3,%o2
/* 0x0b04	     */		add	%g1,%o0,%o0
/* 0x0b08	     */		fdtox	%f0,%f0
/* 0x0b0c	     */		std	%f0,[%sp+2239]
/* 0x0b10	     */		sllx	%o1,16,%o1
/* 0x0b14	     */		add	%o0,%o1,%o1
/* 0x0b18	     */		add	%g5,2,%g5
/* 0x0b1c	     */		ldd	[%o7+%o2],%f0
/* 0x0b20	     */		srax	%o1,32,%g1
/* 0x0b24	     */		and	%o1,%o5,%o2
/* 0x0b28	     */		srax	%i1,16,%o0
/* 0x0b2c	     */		add	%g4,2,%g4
/* 0x0b30	     */		fdtox	%f0,%f0
/* 0x0b34	     */		std	%f0,[%sp+2231]
/* 0x0b38	     */		sra	%i2,0,%o1
/* 0x0b3c	     */		sllx	%o1,2,%o1
/* 0x0b40	     */		add	%o0,%g1,%g2
/* 0x0b44	     */		srax	%i0,32,%g1
/* 0x0b48	     */		add	%i2,1,%i2
/* 0x0b4c	     */		add	%g1,%g2,%g1
/* 0x0b50	     */		cmp	%i2,%g3
/* 0x0b54	     */		ldx	[%sp+2239],%o3
/* 0x0b58	     */		ldx	[%sp+2231],%i1
/* 0x0b5c	     */		st	%o2,[%l2+%o1]
/* 0x0b60	     */		or	%g0,%o3,%i0
/* 0x0b64	     */		ble,pt	%icc,.L900000647
/* 0x0b68	     */		sra	%g5,0,%o1
                       .L77000236:
/* 0x0b6c	     */		sethi	%hi(0xfc00),%g2
                       .L900000648:
/* 0x0b70	     */		or	%g0,-1,%o0
/* 0x0b74	     */		add	%g2,1023,%g2
/* 0x0b78	     */		srl	%o0,0,%g3
/* 0x0b7c	     */		and	%i1,%g2,%g2
/* 0x0b80	     */		and	%i0,%g3,%g4
/* 0x0b84	     */		sllx	%g2,16,%g2
/* 0x0b88	     */		add	%g1,%g4,%g4
/* 0x0b8c	     */		sra	%i2,0,%g5
/* 0x0b90	     */		add	%g4,%g2,%g4
/* 0x0b94	     */		sllx	%g5,2,%g2
/* 0x0b98	     */		and	%g4,%g3,%g3
/* 0x0b9c	     */		st	%g3,[%l2+%g2]

!  317		      ! adjust_montf_result(result,nint,nlen); 

/* 0x0ba0	 317 */		sra	%l0,0,%g4
/* 0x0ba4	     */		sllx	%g4,2,%g2
/* 0x0ba8	     */		ld	[%l2+%g2],%g2
/* 0x0bac	     */		cmp	%g2,0
/* 0x0bb0	     */		bleu,pn	%icc,.L77000241
/* 0x0bb4	     */		or	%g0,-1,%o1
/* 0x0bb8	     */		ba	.L900000646
/* 0x0bbc	     */		cmp	%o1,0
                       .L77000241:
/* 0x0bc0	     */		sub	%l0,1,%o1
/* 0x0bc4	     */		cmp	%o1,0
/* 0x0bc8	     */		bl,pn	%icc,.L77000244
/* 0x0bcc	     */		sra	%o1,0,%g2
                       .L900000645:
/* 0x0bd0	     */		sllx	%g2,2,%g2
/* 0x0bd4	     */		sub	%o1,1,%o0
/* 0x0bd8	     */		ld	[%l3+%g2],%g3
/* 0x0bdc	     */		ld	[%l2+%g2],%g2
/* 0x0be0	     */		cmp	%g2,%g3
/* 0x0be4	     */		bne,pn	%icc,.L77000244
/* 0x0be8	     */		nop
/* 0x0bec	   0 */		or	%g0,%o0,%o1
/* 0x0bf0	 317 */		cmp	%o0,0
/* 0x0bf4	     */		bge,pt	%icc,.L900000645
/* 0x0bf8	     */		sra	%o1,0,%g2
                       .L77000244:
/* 0x0bfc	     */		cmp	%o1,0
                       .L900000646:
/* 0x0c00	     */		bl,pn	%icc,.L77000288
/* 0x0c04	     */		sra	%o1,0,%g2
/* 0x0c08	     */		sllx	%g2,2,%g2
/* 0x0c0c	     */		ld	[%l3+%g2],%g3
/* 0x0c10	     */		ld	[%l2+%g2],%g2
/* 0x0c14	     */		cmp	%g2,%g3
/* 0x0c18	     */		bleu,pt	%icc,.L77000224
/* 0x0c1c	     */		nop
                       .L77000288:
/* 0x0c20	     */		cmp	%l0,0
/* 0x0c24	     */		ble,pt	%icc,.L77000224
/* 0x0c28	     */		nop
/* 0x0c2c	 317 */		or	%g0,-1,%g2
/* 0x0c30	 315 */		or	%g0,0,%i0
/* 0x0c34	 317 */		srl	%g2,0,%g2
/* 0x0c38	 315 */		or	%g0,0,%g4
/* 0x0c3c	     */		or	%g0,0,%o1
/* 0x0c40	 317 */		sub	%l0,1,%g5
/* 0x0c44	     */		cmp	%l0,9
/* 0x0c48	 315 */		or	%g0,8,%o5
/* 0x0c4c	     */		bl,pn	%icc,.L77000289
/* 0x0c50	     */		sub	%l0,4,%o7
/* 0x0c54	     */		ld	[%l2],%o1
/* 0x0c58	     */		or	%g0,5,%i0
/* 0x0c5c	     */		ld	[%l3],%o2
/* 0x0c60	     */		or	%g0,12,%o4
/* 0x0c64	     */		or	%g0,16,%g1
/* 0x0c68	     */		ld	[%l3+4],%o3
/* 0x0c6c	     */		ld	[%l2+4],%o0
/* 0x0c70	     */		sub	%o1,%o2,%o1
/* 0x0c74	     */		ld	[%l3+8],%i1
/* 0x0c78	     */		and	%o1,%g2,%g4
/* 0x0c7c	     */		st	%g4,[%l2]
/* 0x0c80	     */		srax	%o1,32,%g4
/* 0x0c84	     */		sub	%o0,%o3,%o0
/* 0x0c88	     */		ld	[%l3+12],%o2
/* 0x0c8c	     */		add	%o0,%g4,%o0
/* 0x0c90	     */		and	%o0,%g2,%g4
/* 0x0c94	     */		st	%g4,[%l2+4]
/* 0x0c98	     */		srax	%o0,32,%o0
/* 0x0c9c	     */		ld	[%l2+8],%o1
/* 0x0ca0	     */		ld	[%l2+12],%o3
/* 0x0ca4	     */		sub	%o1,%i1,%o1
                       .L900000635:
/* 0x0ca8	     */		add	%g1,4,%g3
/* 0x0cac	     */		ld	[%g1+%l2],%g4
/* 0x0cb0	     */		add	%o1,%o0,%o0
/* 0x0cb4	     */		ld	[%l3+%g1],%i1
/* 0x0cb8	     */		sub	%o3,%o2,%o1
/* 0x0cbc	     */		and	%o0,%g2,%o2
/* 0x0cc0	     */		st	%o2,[%o5+%l2]
/* 0x0cc4	     */		srax	%o0,32,%o2
/* 0x0cc8	     */		add	%i0,4,%i0
/* 0x0ccc	     */		add	%g1,8,%o5
/* 0x0cd0	     */		ld	[%g3+%l2],%o0
/* 0x0cd4	     */		add	%o1,%o2,%o1
/* 0x0cd8	     */		ld	[%l3+%g3],%o3
/* 0x0cdc	     */		sub	%g4,%i1,%o2
/* 0x0ce0	     */		and	%o1,%g2,%g4
/* 0x0ce4	     */		st	%g4,[%o4+%l2]
/* 0x0ce8	     */		srax	%o1,32,%g4
/* 0x0cec	     */		cmp	%i0,%o7
/* 0x0cf0	     */		add	%g1,12,%o4
/* 0x0cf4	     */		ld	[%o5+%l2],%o1
/* 0x0cf8	     */		add	%o2,%g4,%o2
/* 0x0cfc	     */		ld	[%l3+%o5],%i1
/* 0x0d00	     */		sub	%o0,%o3,%o0
/* 0x0d04	     */		and	%o2,%g2,%o3
/* 0x0d08	     */		st	%o3,[%g1+%l2]
/* 0x0d0c	     */		srax	%o2,32,%g4
/* 0x0d10	     */		ld	[%o4+%l2],%o3
/* 0x0d14	     */		add	%g1,16,%g1
/* 0x0d18	     */		add	%o0,%g4,%o0
/* 0x0d1c	     */		ld	[%l3+%o4],%o2
/* 0x0d20	     */		sub	%o1,%i1,%o1
/* 0x0d24	     */		and	%o0,%g2,%g4
/* 0x0d28	     */		st	%g4,[%g3+%l2]
/* 0x0d2c	     */		ble,pt	%icc,.L900000635
/* 0x0d30	     */		srax	%o0,32,%o0
                       .L900000638:
/* 0x0d34	     */		add	%o1,%o0,%g3
/* 0x0d38	     */		sub	%o3,%o2,%o1
/* 0x0d3c	     */		ld	[%g1+%l2],%o0
/* 0x0d40	     */		ld	[%l3+%g1],%o2
/* 0x0d44	     */		srax	%g3,32,%o7
/* 0x0d48	     */		and	%g3,%g2,%o3
/* 0x0d4c	     */		add	%o1,%o7,%o1
/* 0x0d50	     */		st	%o3,[%o5+%l2]
/* 0x0d54	     */		cmp	%i0,%g5
/* 0x0d58	     */		sub	%o0,%o2,%o0
/* 0x0d5c	     */		and	%o1,%g2,%o2
/* 0x0d60	     */		st	%o2,[%o4+%l2]
/* 0x0d64	     */		srax	%o1,32,%o1
/* 0x0d68	     */		sra	%i0,0,%o2
/* 0x0d6c	     */		add	%o0,%o1,%o0
/* 0x0d70	     */		srax	%o0,32,%g4
/* 0x0d74	     */		and	%o0,%g2,%o1
/* 0x0d78	     */		st	%o1,[%g1+%l2]
/* 0x0d7c	     */		bg,pn	%icc,.L77000224
/* 0x0d80	     */		sllx	%o2,2,%o1
                       .L77000289:
/* 0x0d84	   0 */		or	%g0,%o1,%g1
                       .L900000644:
/* 0x0d88	     */		ld	[%o1+%l2],%o0
/* 0x0d8c	     */		add	%i0,1,%i0
/* 0x0d90	     */		ld	[%l3+%o1],%o1
/* 0x0d94	     */		sra	%i0,0,%o2
/* 0x0d98	     */		cmp	%i0,%g5
/* 0x0d9c	     */		add	%g4,%o0,%o0
/* 0x0da0	     */		sub	%o0,%o1,%o0
/* 0x0da4	     */		srax	%o0,32,%g4
/* 0x0da8	     */		and	%o0,%g2,%o1
/* 0x0dac	     */		st	%o1,[%g1+%l2]
/* 0x0db0	     */		sllx	%o2,2,%o1
/* 0x0db4	     */		ble,pt	%icc,.L900000644
/* 0x0db8	     */		or	%g0,%o1,%g1
                       .L77000224:
/* 0x0dbc	     */		ret	! Result = 
/* 0x0dc0	     */		restore	%g0,%g0,%g0
/* 0x0dc4	   0 */		.type	mont_mulf_noconv,2
/* 0x0dc4	     */		.size	mont_mulf_noconv,(.-mont_mulf_noconv)

