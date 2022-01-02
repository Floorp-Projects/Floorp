!  
! This Source Code Form is subject to the terms of the Mozilla Public
! License, v. 2.0. If a copy of the MPL was not distributed with this
! file, You can obtain one at http://mozilla.org/MPL/2.0/.

	.section	".text",#alloc,#execinstr
	.file	"montmulf.c"

	.section	".data",#alloc,#write
	.align	8
TwoTo16:		/* frequency 1.0 confidence 0.0 */
	.word	1089470464
	.word	0
	.type	TwoTo16,#object
	.size	TwoTo16,8
TwoToMinus16:		/* frequency 1.0 confidence 0.0 */
	.word	1055916032
	.word	0
	.type	TwoToMinus16,#object
	.size	TwoToMinus16,8
Zero:		/* frequency 1.0 confidence 0.0 */
	.word	0
	.word	0
	.type	Zero,#object
	.size	Zero,8
TwoTo32:		/* frequency 1.0 confidence 0.0 */
	.word	1106247680
	.word	0
	.type	TwoTo32,#object
	.size	TwoTo32,8
TwoToMinus32:		/* frequency 1.0 confidence 0.0 */
	.word	1039138816
	.word	0
	.type	TwoToMinus32,#object
	.size	TwoToMinus32,8

	.section	".text",#alloc,#execinstr
/* 000000	   0 ( 0  0) */		.align	4
!
! SUBROUTINE cleanup
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION	(ISSUE TIME)	(COMPLETION TIME)

                                   	.global cleanup
                                   cleanup:		/* frequency 1.0 confidence 0.0 */
! FILE montmulf.c

!    1		                    !#define RF_INLINE_MACROS
!    3		                    !static double TwoTo16=65536.0;
!    4		                    !static double TwoToMinus16=1.0/65536.0;
!    5		                    !static double Zero=0.0;
!    6		                    !static double TwoTo32=65536.0*65536.0;
!    7		                    !static double TwoToMinus32=1.0/(65536.0*65536.0);
!    9		                    !#ifdef RF_INLINE_MACROS
!   11		                    !double upper32(double);
!   12		                    !double lower32(double, double);
!   13		                    !double mod(double, double, double);
!   15		                    !#else
!   17		                    !static double upper32(double x)
!   18		                    !{
!   19		                    !  return floor(x*TwoToMinus32);
!   20		                    !}
!   22		                    !static double lower32(double x, double y)
!   23		                    !{
!   24		                    !  return x-TwoTo32*floor(x*TwoToMinus32);
!   25		                    !}
!   27		                    !static double mod(double x, double oneoverm, double m)
!   28		                    !{
!   29		                    !  return x-m*floor(x*oneoverm);
!   30		                    !}
!   32		                    !#endif
!   35		                    !void cleanup(double *dt, int from, int tlen)
!   36		                    !{
!   37		                    ! int i;
!   38		                    ! double tmp,tmp1,x,x1;
!   40		                    ! tmp=tmp1=Zero;

/* 000000	  40 ( 0  1) */		sethi	%hi(Zero),%g2

!   41		                    ! /* original code **
!   42		                    ! for(i=2*from;i<2*tlen-2;i++)
!   43		                    !   {
!   44		                    !     x=dt[i];
!   45		                    !     dt[i]=lower32(x,Zero)+tmp1;
!   46		                    !     tmp1=tmp;
!   47		                    !     tmp=upper32(x);
!   48		                    !   }
!   49		                    ! dt[tlen-2]+=tmp1;
!   50		                    ! dt[tlen-1]+=tmp;
!   51		                    ! **end original code ***/
!   52		                    ! /* new code ***/
!   53		                    ! for(i=2*from;i<2*tlen;i+=2)

/* 0x0004	  53 ( 1  2) */		sll	%o2,1,%g3
/* 0x0008	  40 ( 1  4) */		ldd	[%g2+%lo(Zero)],%f0
/* 0x000c	     ( 1  2) */		add	%g2,%lo(Zero),%g2
/* 0x0010	  53 ( 2  3) */		sll	%o1,1,%g4
/* 0x0014	  36 ( 3  4) */		sll	%o1,4,%g1
/* 0x0018	  40 ( 3  4) */		fmovd	%f0,%f4
/* 0x001c	  53 ( 3  4) */		cmp	%g4,%g3
/* 0x0020	     ( 3  4) */		bge,pt	%icc,.L77000116	! tprob=0.56
/* 0x0024	     ( 4  5) */		fmovd	%f0,%f2
/* 0x0028	  36 ( 4  5) */		add	%o0,%g1,%g1
/* 0x002c	     ( 4  5) */		sub	%g3,1,%g3

!   54		                    !   {
!   55		                    !     x=dt[i];

/* 0x0030	  55 ( 5  8) */		ldd	[%g1],%f8
                                   .L900000114:		/* frequency 6.4 confidence 0.0 */
/* 0x0034	     ( 0  3) */		fdtox	%f8,%f6

!   56		                    !     x1=dt[i+1];

/* 0x0038	  56 ( 0  3) */		ldd	[%g1+8],%f10

!   57		                    !     dt[i]=lower32(x,Zero)+tmp;
!   58		                    !     dt[i+1]=lower32(x1,Zero)+tmp1;
!   59		                    !     tmp=upper32(x);
!   60		                    !     tmp1=upper32(x1);

/* 0x003c	  60 ( 0  1) */		add	%g4,2,%g4
/* 0x0040	     ( 1  4) */		fdtox	%f8,%f8
/* 0x0044	     ( 1  2) */		cmp	%g4,%g3
/* 0x0048	     ( 5  6) */		fmovs	%f0,%f6
/* 0x004c	     ( 7 10) */		fxtod	%f6,%f6
/* 0x0050	     ( 8 11) */		fdtox	%f10,%f0
/* 0x0054	  57 (10 13) */		faddd	%f6,%f2,%f2
/* 0x0058	     (10 11) */		std	%f2,[%g1]
/* 0x005c	     (12 15) */		ldd	[%g2],%f2
/* 0x0060	     (14 15) */		fmovs	%f2,%f0
/* 0x0064	     (16 19) */		fxtod	%f0,%f6
/* 0x0068	     (17 20) */		fdtox	%f10,%f0
/* 0x006c	     (18 21) */		fitod	%f8,%f2
/* 0x0070	  58 (19 22) */		faddd	%f6,%f4,%f4
/* 0x0074	     (19 20) */		std	%f4,[%g1+8]
/* 0x0078	  60 (19 20) */		add	%g1,16,%g1
/* 0x007c	     (20 23) */		fitod	%f0,%f4
/* 0x0080	     (20 23) */		ldd	[%g2],%f0
/* 0x0084	     (20 21) */		ble,a,pt	%icc,.L900000114	! tprob=0.86
/* 0x0088	     (21 24) */		ldd	[%g1],%f8
                                   .L77000116:		/* frequency 1.0 confidence 0.0 */
/* 0x008c	     ( 0  2) */		retl	! Result = 
/* 0x0090	     ( 1  2) */		nop
/* 0x0094	   0 ( 0  0) */		.type	cleanup,2
/* 0x0094	     ( 0  0) */		.size	cleanup,(.-cleanup)

	.section	".text",#alloc,#execinstr
/* 000000	   0 ( 0  0) */		.align	4
!
! SUBROUTINE conv_d16_to_i32
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION	(ISSUE TIME)	(COMPLETION TIME)

                                   	.global conv_d16_to_i32
                                   conv_d16_to_i32:		/* frequency 1.0 confidence 0.0 */
/* 000000	     ( 0  1) */		save	%sp,-136,%sp

!   61		                    !   }
!   62		                    !  /** end new code **/
!   63		                    !}
!   66		                    !void conv_d16_to_i32(unsigned int *i32, double *d16, long long *tmp, int ilen)
!   67		                    !{
!   68		                    !int i;
!   69		                    !long long t, t1, a, b, c, d;
!   71		                    ! t1=0;
!   72		                    ! a=(long long)d16[0];

/* 0x0004	  72 ( 1  4) */		ldd	[%i1],%f0

!   73		                    ! b=(long long)d16[1];
!   74		                    ! for(i=0; i<ilen-1; i++)

/* 0x0008	  74 ( 1  2) */		sub	%i3,1,%g2
/* 0x000c	  67 ( 1  2) */		or	%g0,%i0,%g5
/* 0x0010	  74 ( 2  3) */		cmp	%g2,0
/* 0x0014	  71 ( 2  3) */		or	%g0,0,%o4
/* 0x0018	  72 ( 3  6) */		fdtox	%f0,%f0
/* 0x001c	     ( 3  4) */		std	%f0,[%sp+120]
/* 0x0020	  74 ( 3  4) */		or	%g0,0,%o7
/* 0x0024	  67 ( 4  5) */		or	%g0,%i3,%o0
/* 0x0028	     ( 4  5) */		sub	%i3,2,%o2
/* 0x002c	  73 ( 5  8) */		ldd	[%i1+8],%f0
/* 0x0030	  67 ( 5  6) */		sethi	%hi(0xfc00),%o0
/* 0x0034	     ( 5  6) */		add	%o2,1,%g3
/* 0x0038	     ( 6  7) */		add	%o0,1023,%o1
/* 0x003c	     ( 6  7) */		or	%g0,%g5,%o5
/* 0x0040	  73 ( 7 10) */		fdtox	%f0,%f0
/* 0x0044	     ( 7  8) */		std	%f0,[%sp+112]
/* 0x0048	  72 (11 13) */		ldx	[%sp+120],%g4
/* 0x004c	  73 (12 14) */		ldx	[%sp+112],%g1
/* 0x0050	  74 (12 13) */		ble,pt	%icc,.L900000214	! tprob=0.56
/* 0x0054	     (12 13) */		sethi	%hi(0xfc00),%g2
/* 0x0058	  67 (13 14) */		or	%g0,-1,%g2
/* 0x005c	  74 (13 14) */		cmp	%g3,3
/* 0x0060	  67 (14 15) */		srl	%g2,0,%o3
/* 0x0064	     (14 15) */		or	%g0,%i1,%g2
/* 0x0068	  74 (14 15) */		bl,pn	%icc,.L77000134	! tprob=0.44
/* 0x006c	     (15 18) */		ldd	[%g2+16],%f0

!   75		                    !   {
!   76		                    !     c=(long long)d16[2*i+2];
!   77		                    !     t1+=a&0xffffffff;
!   78		                    !     t=(a>>32);
!   79		                    !     d=(long long)d16[2*i+3];
!   80		                    !     t1+=(b&0xffff)<<16;

/* 0x0070	  80 (15 16) */		and	%g1,%o1,%o0

!   81		                    !     t+=(b>>16)+(t1>>32);
!   82		                    !     i32[i]=t1&0xffffffff;
!   83		                    !     t1=t;
!   84		                    !     a=c;
!   85		                    !     b=d;

/* 0x0074	  85 (15 16) */		add	%g2,16,%g2
/* 0x0078	  80 (16 17) */		sllx	%o0,16,%g3
/* 0x007c	  77 (16 17) */		and	%g4,%o3,%o0
/* 0x0080	  76 (17 20) */		fdtox	%f0,%f0
/* 0x0084	     (17 18) */		std	%f0,[%sp+104]
/* 0x0088	  74 (17 18) */		add	%o0,%g3,%o4
/* 0x008c	  79 (18 21) */		ldd	[%g2+8],%f2
/* 0x0090	  81 (18 19) */		srax	%g1,16,%o0
/* 0x0094	  82 (18 19) */		and	%o4,%o3,%o7
/* 0x0098	  81 (19 20) */		stx	%o0,[%sp+112]
/* 0x009c	     (19 20) */		srax	%o4,32,%o0
/* 0x00a0	  85 (19 20) */		add	%g5,4,%o5
/* 0x00a4	  81 (20 21) */		stx	%o0,[%sp+120]
/* 0x00a8	  78 (20 21) */		srax	%g4,32,%o4
/* 0x00ac	  79 (20 23) */		fdtox	%f2,%f0
/* 0x00b0	     (21 22) */		std	%f0,[%sp+96]
/* 0x00b4	  81 (22 24) */		ldx	[%sp+112],%o0
/* 0x00b8	     (23 25) */		ldx	[%sp+120],%g4
/* 0x00bc	  76 (25 27) */		ldx	[%sp+104],%g3
/* 0x00c0	  81 (25 26) */		add	%o0,%g4,%g4
/* 0x00c4	  79 (26 28) */		ldx	[%sp+96],%g1
/* 0x00c8	  81 (26 27) */		add	%o4,%g4,%o4
/* 0x00cc	  82 (27 28) */		st	%o7,[%g5]
/* 0x00d0	     (27 28) */		or	%g0,1,%o7
/* 0x00d4	  84 (27 28) */		or	%g0,%g3,%g4
                                   .L900000209:		/* frequency 64.0 confidence 0.0 */
/* 0x00d8	  76 (17 19) */		ldd	[%g2+16],%f0
/* 0x00dc	  85 (17 18) */		add	%o7,1,%o7
/* 0x00e0	     (17 18) */		add	%o5,4,%o5
/* 0x00e4	     (18 18) */		cmp	%o7,%o2
/* 0x00e8	     (18 19) */		add	%g2,16,%g2
/* 0x00ec	  76 (19 22) */		fdtox	%f0,%f0
/* 0x00f0	     (20 21) */		std	%f0,[%sp+104]
/* 0x00f4	  79 (21 23) */		ldd	[%g2+8],%f0
/* 0x00f8	     (23 26) */		fdtox	%f0,%f0
/* 0x00fc	     (24 25) */		std	%f0,[%sp+96]
/* 0x0100	  80 (25 26) */		and	%g1,%o1,%g3
/* 0x0104	     (26 27) */		sllx	%g3,16,%g3
/* 0x0108	     ( 0  0) */		stx	%g3,[%sp+120]
/* 0x010c	  77 (26 27) */		and	%g4,%o3,%g3
/* 0x0110	  74 ( 0  0) */		stx	%o7,[%sp+128]
/* 0x0114	     ( 0  0) */		ldx	[%sp+120],%o7
/* 0x0118	     (27 27) */		add	%g3,%o7,%g3
/* 0x011c	     ( 0  0) */		ldx	[%sp+128],%o7
/* 0x0120	  81 (28 29) */		srax	%g1,16,%g1
/* 0x0124	  74 (28 28) */		add	%g3,%o4,%g3
/* 0x0128	  81 (29 30) */		srax	%g3,32,%o4
/* 0x012c	     ( 0  0) */		stx	%o4,[%sp+112]
/* 0x0130	  78 (30 31) */		srax	%g4,32,%o4
/* 0x0134	  81 ( 0  0) */		ldx	[%sp+112],%g4
/* 0x0138	     (30 31) */		add	%g1,%g4,%g4
/* 0x013c	  79 (31 33) */		ldx	[%sp+96],%g1
/* 0x0140	  81 (31 32) */		add	%o4,%g4,%o4
/* 0x0144	  82 (32 33) */		and	%g3,%o3,%g3
/* 0x0148	  84 ( 0  0) */		ldx	[%sp+104],%g4
/* 0x014c	  85 (33 34) */		ble,pt	%icc,.L900000209	! tprob=0.50
/* 0x0150	     (33 34) */		st	%g3,[%o5-4]
                                   .L900000212:		/* frequency 8.0 confidence 0.0 */
/* 0x0154	  85 ( 0  1) */		ba	.L900000214	! tprob=1.00
/* 0x0158	     ( 0  1) */		sethi	%hi(0xfc00),%g2
                                   .L77000134:		/* frequency 0.7 confidence 0.0 */
                                   .L900000213:		/* frequency 6.4 confidence 0.0 */
/* 0x015c	  77 ( 0  1) */		and	%g4,%o3,%o0
/* 0x0160	  80 ( 0  1) */		and	%g1,%o1,%g3
/* 0x0164	  76 ( 0  3) */		fdtox	%f0,%f0
/* 0x0168	  77 ( 1  2) */		add	%o4,%o0,%o0
/* 0x016c	  76 ( 1  2) */		std	%f0,[%sp+104]
/* 0x0170	  85 ( 1  2) */		add	%o7,1,%o7
/* 0x0174	  80 ( 2  3) */		sllx	%g3,16,%o4
/* 0x0178	  79 ( 2  5) */		ldd	[%g2+24],%f2
/* 0x017c	  85 ( 2  3) */		add	%g2,16,%g2
/* 0x0180	  80 ( 3  4) */		add	%o0,%o4,%o4
/* 0x0184	  81 ( 3  4) */		stx	%o7,[%sp+128]
/* 0x0188	     ( 4  5) */		srax	%g1,16,%o0
/* 0x018c	     ( 4  5) */		stx	%o0,[%sp+112]
/* 0x0190	  82 ( 4  5) */		and	%o4,%o3,%g3
/* 0x0194	  81 ( 5  6) */		srax	%o4,32,%o0
/* 0x0198	     ( 5  6) */		stx	%o0,[%sp+120]
/* 0x019c	  79 ( 5  8) */		fdtox	%f2,%f0
/* 0x01a0	     ( 6  7) */		std	%f0,[%sp+96]
/* 0x01a4	  78 ( 6  7) */		srax	%g4,32,%o4
/* 0x01a8	  81 ( 7  9) */		ldx	[%sp+120],%o7
/* 0x01ac	     ( 8 10) */		ldx	[%sp+112],%g4
/* 0x01b0	  76 (10 12) */		ldx	[%sp+104],%g1
/* 0x01b4	  81 (10 11) */		add	%g4,%o7,%g4
/* 0x01b8	     (11 13) */		ldx	[%sp+128],%o7
/* 0x01bc	     (11 12) */		add	%o4,%g4,%o4
/* 0x01c0	  79 (12 14) */		ldx	[%sp+96],%o0
/* 0x01c4	  84 (12 13) */		or	%g0,%g1,%g4
/* 0x01c8	  82 (13 14) */		st	%g3,[%o5]
/* 0x01cc	  85 (13 14) */		add	%o5,4,%o5
/* 0x01d0	     (13 14) */		cmp	%o7,%o2
/* 0x01d4	     (14 15) */		or	%g0,%o0,%g1
/* 0x01d8	     (14 15) */		ble,a,pt	%icc,.L900000213	! tprob=0.86
/* 0x01dc	     (14 17) */		ldd	[%g2+16],%f0
                                   .L77000127:		/* frequency 1.0 confidence 0.0 */

!   86		                    !   }
!   87		                    !     t1+=a&0xffffffff;
!   88		                    !     t=(a>>32);
!   89		                    !     t1+=(b&0xffff)<<16;
!   90		                    !     i32[i]=t1&0xffffffff;

/* 0x01e0	  90 ( 0  1) */		sethi	%hi(0xfc00),%g2
                                   .L900000214:		/* frequency 1.0 confidence 0.0 */
/* 0x01e4	  90 ( 0  1) */		or	%g0,-1,%g3
/* 0x01e8	     ( 0  1) */		add	%g2,1023,%g2
/* 0x01ec	     ( 1  2) */		srl	%g3,0,%g3
/* 0x01f0	     ( 1  2) */		and	%g1,%g2,%g2
/* 0x01f4	     ( 2  3) */		and	%g4,%g3,%g4
/* 0x01f8	     ( 3  4) */		sllx	%g2,16,%g2
/* 0x01fc	     ( 3  4) */		add	%o4,%g4,%g4
/* 0x0200	     ( 4  5) */		add	%g4,%g2,%g2
/* 0x0204	     ( 5  6) */		sll	%o7,2,%g4
/* 0x0208	     ( 5  6) */		and	%g2,%g3,%g2
/* 0x020c	     ( 6  7) */		st	%g2,[%g5+%g4]
/* 0x0210	     ( 7  9) */		ret	! Result = 
/* 0x0214	     ( 9 10) */		restore	%g0,%g0,%g0
/* 0x0218	   0 ( 0  0) */		.type	conv_d16_to_i32,2
/* 0x0218	     ( 0  0) */		.size	conv_d16_to_i32,(.-conv_d16_to_i32)

	.section	".text",#alloc,#execinstr
/* 000000	   0 ( 0  0) */		.align	8
!
! CONSTANT POOL
!
                                   .L_const_seg_900000301:		/* frequency 1.0 confidence 0.0 */
/* 000000	   0 ( 0  0) */		.word	1127219200,0
/* 0x0008	   0 ( 0  0) */		.align	4
!
! SUBROUTINE conv_i32_to_d32
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION	(ISSUE TIME)	(COMPLETION TIME)

                                   	.global conv_i32_to_d32
                                   conv_i32_to_d32:		/* frequency 1.0 confidence 0.0 */
/* 000000	     ( 0  1) */		orcc	%g0,%o2,%g1

!   92		                    !}
!   94		                    !void conv_i32_to_d32(double *d32, unsigned int *i32, int len)
!   95		                    !{
!   96		                    !int i;
!   98		                    !#pragma pipeloop(0)
!   99		                    ! for(i=0;i<len;i++) d32[i]=(double)(i32[i]);

/* 0x0004	  99 ( 0  1) */		ble,pt	%icc,.L77000140	! tprob=0.56
/* 0x0008	     ( 0  1) */		nop
/* 0x000c	     ( 1  2) */		sethi	%hi(.L_const_seg_900000301),%g2
/* 0x0010	  95 ( 1  2) */		or	%g0,%o1,%g4
/* 0x0014	  99 ( 2  3) */		add	%g2,%lo(.L_const_seg_900000301),%g2
/* 0x0018	     ( 2  3) */		or	%g0,0,%o5
/* 0x001c	  95 ( 3  4) */		or	%g0,%o0,%g5
/* 0x0020	  99 ( 3  4) */		sub	%o2,1,%g3
/* 0x0024	     ( 4  5) */		cmp	%o2,9
/* 0x0028	     ( 4  5) */		bl,pn	%icc,.L77000144	! tprob=0.44
/* 0x002c	     ( 4  7) */		ldd	[%g2],%f8
/* 0x0030	     ( 5  8) */		ld	[%o1],%f7
/* 0x0034	     ( 5  6) */		add	%o1,16,%g4
/* 0x0038	     ( 5  6) */		sub	%o2,5,%g1
/* 0x003c	     ( 6  9) */		ld	[%o1+4],%f5
/* 0x0040	     ( 6  7) */		or	%g0,4,%o5
/* 0x0044	     ( 7 10) */		ld	[%o1+8],%f3
/* 0x0048	     ( 7  8) */		fmovs	%f8,%f6
/* 0x004c	     ( 8 11) */		ld	[%o1+12],%f1
                                   .L900000305:		/* frequency 64.0 confidence 0.0 */
/* 0x0050	     ( 8 16) */		ld	[%g4],%f11
/* 0x0054	     ( 8  9) */		add	%o5,5,%o5
/* 0x0058	     ( 8  9) */		add	%g4,20,%g4
/* 0x005c	     ( 8 11) */		fsubd	%f6,%f8,%f6
/* 0x0060	     ( 9 10) */		std	%f6,[%g5]
/* 0x0064	     ( 9  9) */		cmp	%o5,%g1
/* 0x0068	     ( 9 10) */		add	%g5,40,%g5
/* 0x006c	     ( 0  0) */		fmovs	%f8,%f4
/* 0x0070	     (10 18) */		ld	[%g4-16],%f7
/* 0x0074	     (10 13) */		fsubd	%f4,%f8,%f12
/* 0x0078	     ( 0  0) */		fmovs	%f8,%f2
/* 0x007c	     (11 12) */		std	%f12,[%g5-32]
/* 0x0080	     (12 20) */		ld	[%g4-12],%f5
/* 0x0084	     (12 15) */		fsubd	%f2,%f8,%f12
/* 0x0088	     ( 0  0) */		fmovs	%f8,%f0
/* 0x008c	     (13 14) */		std	%f12,[%g5-24]
/* 0x0090	     (14 22) */		ld	[%g4-8],%f3
/* 0x0094	     (14 17) */		fsubd	%f0,%f8,%f12
/* 0x0098	     ( 0  0) */		fmovs	%f8,%f10
/* 0x009c	     (15 16) */		std	%f12,[%g5-16]
/* 0x00a0	     (16 24) */		ld	[%g4-4],%f1
/* 0x00a4	     (16 19) */		fsubd	%f10,%f8,%f10
/* 0x00a8	     ( 0  0) */		fmovs	%f8,%f6
/* 0x00ac	     (17 18) */		ble,pt	%icc,.L900000305	! tprob=0.50
/* 0x00b0	     (17 18) */		std	%f10,[%g5-8]
                                   .L900000308:		/* frequency 8.0 confidence 0.0 */
/* 0x00b4	     ( 0  1) */		fmovs	%f8,%f4
/* 0x00b8	     ( 0  1) */		add	%g5,32,%g5
/* 0x00bc	     ( 0  1) */		cmp	%o5,%g3
/* 0x00c0	     ( 1  2) */		fmovs	%f8,%f2
/* 0x00c4	     ( 2  3) */		fmovs	%f8,%f0
/* 0x00c8	     ( 4  7) */		fsubd	%f6,%f8,%f6
/* 0x00cc	     ( 4  5) */		std	%f6,[%g5-32]
/* 0x00d0	     ( 5  8) */		fsubd	%f4,%f8,%f4
/* 0x00d4	     ( 5  6) */		std	%f4,[%g5-24]
/* 0x00d8	     ( 6  9) */		fsubd	%f2,%f8,%f2
/* 0x00dc	     ( 6  7) */		std	%f2,[%g5-16]
/* 0x00e0	     ( 7 10) */		fsubd	%f0,%f8,%f0
/* 0x00e4	     ( 7  8) */		bg,pn	%icc,.L77000140	! tprob=0.14
/* 0x00e8	     ( 7  8) */		std	%f0,[%g5-8]
                                   .L77000144:		/* frequency 0.7 confidence 0.0 */
/* 0x00ec	     ( 0  3) */		ld	[%g4],%f1
                                   .L900000309:		/* frequency 6.4 confidence 0.0 */
/* 0x00f0	     ( 0  3) */		ldd	[%g2],%f8
/* 0x00f4	     ( 0  1) */		add	%o5,1,%o5
/* 0x00f8	     ( 0  1) */		add	%g4,4,%g4
/* 0x00fc	     ( 1  2) */		cmp	%o5,%g3
/* 0x0100	     ( 2  3) */		fmovs	%f8,%f0
/* 0x0104	     ( 4  7) */		fsubd	%f0,%f8,%f0
/* 0x0108	     ( 4  5) */		std	%f0,[%g5]
/* 0x010c	     ( 4  5) */		add	%g5,8,%g5
/* 0x0110	     ( 4  5) */		ble,a,pt	%icc,.L900000309	! tprob=0.86
/* 0x0114	     ( 6  9) */		ld	[%g4],%f1
                                   .L77000140:		/* frequency 1.0 confidence 0.0 */
/* 0x0118	     ( 0  2) */		retl	! Result = 
/* 0x011c	     ( 1  2) */		nop
/* 0x0120	   0 ( 0  0) */		.type	conv_i32_to_d32,2
/* 0x0120	     ( 0  0) */		.size	conv_i32_to_d32,(.-conv_i32_to_d32)

	.section	".text",#alloc,#execinstr
/* 000000	   0 ( 0  0) */		.align	8
!
! CONSTANT POOL
!
                                   .L_const_seg_900000401:		/* frequency 1.0 confidence 0.0 */
/* 000000	   0 ( 0  0) */		.word	1127219200,0
/* 0x0008	   0 ( 0  0) */		.align	4
!
! SUBROUTINE conv_i32_to_d16
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION	(ISSUE TIME)	(COMPLETION TIME)

                                   	.global conv_i32_to_d16
                                   conv_i32_to_d16:		/* frequency 1.0 confidence 0.0 */
/* 000000	     ( 0  1) */		save	%sp,-104,%sp
/* 0x0004	     ( 1  2) */		orcc	%g0,%i2,%o0

!  100		                    !}
!  103		                    !void conv_i32_to_d16(double *d16, unsigned int *i32, int len)
!  104		                    !{
!  105		                    !int i;
!  106		                    !unsigned int a;
!  108		                    !#pragma pipeloop(0)
!  109		                    ! for(i=0;i<len;i++)

/* 0x0008	 109 ( 1  2) */		ble,pt	%icc,.L77000150	! tprob=0.56
/* 0x000c	     ( 1  2) */		nop
/* 0x0010	     ( 2  3) */		sub	%o0,1,%o5
/* 0x0014	     ( 2  3) */		sethi	%hi(0xfc00),%g2

!  110		                    !   {
!  111		                    !     a=i32[i];
!  112		                    !     d16[2*i]=(double)(a&0xffff);
!  113		                    !     d16[2*i+1]=(double)(a>>16);

/* 0x0018	 113 ( 3  4) */		sethi	%hi(.L_const_seg_900000401),%o0
/* 0x001c	     ( 3  4) */		add	%o5,1,%g3
/* 0x0020	     ( 4  5) */		add	%g2,1023,%o4
/* 0x0024	 109 ( 4  5) */		or	%g0,0,%g1
/* 0x0028	     ( 5  6) */		cmp	%g3,3
/* 0x002c	     ( 5  6) */		or	%g0,%i1,%o7
/* 0x0030	     ( 6  7) */		add	%o0,%lo(.L_const_seg_900000401),%o3
/* 0x0034	     ( 6  7) */		or	%g0,%i0,%g2
/* 0x0038	     ( 6  7) */		bl,pn	%icc,.L77000154	! tprob=0.44
/* 0x003c	     ( 7  8) */		add	%o7,4,%o0
/* 0x0040	 112 ( 7 10) */		ldd	[%o3],%f0
/* 0x0044	 113 ( 7  8) */		or	%g0,1,%g1
/* 0x0048	 111 ( 8 11) */		ld	[%o0-4],%o1
/* 0x004c	   0 ( 8  9) */		or	%g0,%o0,%o7
/* 0x0050	 112 (10 11) */		and	%o1,%o4,%o0
                                   .L900000406:		/* frequency 64.0 confidence 0.0 */
/* 0x0054	 112 (22 23) */		st	%o0,[%sp+96]
/* 0x0058	 113 (22 23) */		add	%g1,1,%g1
/* 0x005c	     (22 23) */		add	%g2,16,%g2
/* 0x0060	     (23 23) */		cmp	%g1,%o5
/* 0x0064	     (23 24) */		add	%o7,4,%o7
/* 0x0068	 112 (29 31) */		ld	[%sp+96],%f3
/* 0x006c	     ( 0  0) */		fmovs	%f0,%f2
/* 0x0070	     (31 34) */		fsubd	%f2,%f0,%f2
/* 0x0074	 113 (32 33) */		srl	%o1,16,%o0
/* 0x0078	 112 (32 33) */		std	%f2,[%g2-16]
/* 0x007c	 113 (33 34) */		st	%o0,[%sp+92]
/* 0x0080	     (40 42) */		ld	[%sp+92],%f3
/* 0x0084	 111 (41 43) */		ld	[%o7-4],%o1
/* 0x0088	 113 ( 0  0) */		fmovs	%f0,%f2
/* 0x008c	     (42 45) */		fsubd	%f2,%f0,%f2
/* 0x0090	 112 (43 44) */		and	%o1,%o4,%o0
/* 0x0094	 113 (43 44) */		ble,pt	%icc,.L900000406	! tprob=0.50
/* 0x0098	     (43 44) */		std	%f2,[%g2-8]
                                   .L900000409:		/* frequency 8.0 confidence 0.0 */
/* 0x009c	 112 ( 0  1) */		st	%o0,[%sp+96]
/* 0x00a0	     ( 0  1) */		fmovs	%f0,%f2
/* 0x00a4	 113 ( 0  1) */		add	%g2,16,%g2
/* 0x00a8	     ( 1  2) */		srl	%o1,16,%o0
/* 0x00ac	 112 ( 4  7) */		ld	[%sp+96],%f3
/* 0x00b0	     ( 6  9) */		fsubd	%f2,%f0,%f2
/* 0x00b4	     ( 6  7) */		std	%f2,[%g2-16]
/* 0x00b8	 113 ( 7  8) */		st	%o0,[%sp+92]
/* 0x00bc	     (10 11) */		fmovs	%f0,%f2
/* 0x00c0	     (11 14) */		ld	[%sp+92],%f3
/* 0x00c4	     (13 16) */		fsubd	%f2,%f0,%f0
/* 0x00c8	     (13 14) */		std	%f0,[%g2-8]
/* 0x00cc	     (14 16) */		ret	! Result = 
/* 0x00d0	     (16 17) */		restore	%g0,%g0,%g0
                                   .L77000154:		/* frequency 0.7 confidence 0.0 */
/* 0x00d4	 111 ( 0  3) */		ld	[%o7],%o0
                                   .L900000410:		/* frequency 6.4 confidence 0.0 */
/* 0x00d8	 112 ( 0  1) */		and	%o0,%o4,%o1
/* 0x00dc	     ( 0  1) */		st	%o1,[%sp+96]
/* 0x00e0	 113 ( 0  1) */		add	%g1,1,%g1
/* 0x00e4	 112 ( 1  4) */		ldd	[%o3],%f0
/* 0x00e8	 113 ( 1  2) */		srl	%o0,16,%o0
/* 0x00ec	     ( 1  2) */		add	%o7,4,%o7
/* 0x00f0	     ( 2  3) */		cmp	%g1,%o5
/* 0x00f4	 112 ( 3  4) */		fmovs	%f0,%f2
/* 0x00f8	     ( 4  7) */		ld	[%sp+96],%f3
/* 0x00fc	     ( 6  9) */		fsubd	%f2,%f0,%f2
/* 0x0100	     ( 6  7) */		std	%f2,[%g2]
/* 0x0104	 113 ( 7  8) */		st	%o0,[%sp+92]
/* 0x0108	     (10 11) */		fmovs	%f0,%f2
/* 0x010c	     (11 14) */		ld	[%sp+92],%f3
/* 0x0110	     (13 16) */		fsubd	%f2,%f0,%f0
/* 0x0114	     (13 14) */		std	%f0,[%g2+8]
/* 0x0118	     (13 14) */		add	%g2,16,%g2
/* 0x011c	     (13 14) */		ble,a,pt	%icc,.L900000410	! tprob=0.86
/* 0x0120	     (14 17) */		ld	[%o7],%o0
                                   .L77000150:		/* frequency 1.0 confidence 0.0 */
/* 0x0124	     ( 0  2) */		ret	! Result = 
/* 0x0128	     ( 2  3) */		restore	%g0,%g0,%g0
/* 0x012c	   0 ( 0  0) */		.type	conv_i32_to_d16,2
/* 0x012c	     ( 0  0) */		.size	conv_i32_to_d16,(.-conv_i32_to_d16)

	.section	".text",#alloc,#execinstr
/* 000000	   0 ( 0  0) */		.align	8
!
! CONSTANT POOL
!
                                   .L_const_seg_900000501:		/* frequency 1.0 confidence 0.0 */
/* 000000	   0 ( 0  0) */		.word	1127219200,0
/* 0x0008	   0 ( 0  0) */		.align	4
!
! SUBROUTINE conv_i32_to_d32_and_d16
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION	(ISSUE TIME)	(COMPLETION TIME)

                                   	.global conv_i32_to_d32_and_d16
                                   conv_i32_to_d32_and_d16:		/* frequency 1.0 confidence 0.0 */
/* 000000	     ( 0  1) */		save	%sp,-104,%sp
/* 0x0004	     ( 1  2) */		or	%g0,%i3,%i4
/* 0x0008	     ( 1  2) */		or	%g0,%i2,%g1

!  114		                    !   }
!  115		                    !}
!  118		                    !void i16_to_d16_and_d32x4(double * /*1/(2^16)*/, double * /* 2^16*/,
!  119		                    !			  double * /* 0 */,
!  120		                    !			  double * /*result16*/, double * /* result32 */,
!  121		                    !			  float *  /*source - should be unsigned int*
!  122		                    !		          	       converted to float* */);
!  126		                    !void conv_i32_to_d32_and_d16(double *d32, double *d16, 
!  127		                    !			     unsigned int *i32, int len)
!  128		                    !{
!  129		                    !int i;
!  130		                    !unsigned int a;
!  132		                    !#pragma pipeloop(0)
!  133		                    ! for(i=0;i<len-3;i+=4)

/* 0x000c	 133 ( 2  3) */		sub	%i4,3,%g2
/* 0x0010	     ( 2  3) */		or	%g0,0,%o7
/* 0x0014	     ( 3  4) */		cmp	%g2,0
/* 0x0018	 128 ( 3  4) */		or	%g0,%i0,%i3
/* 0x001c	 133 ( 3  4) */		ble,pt	%icc,.L900000515	! tprob=0.56
/* 0x0020	     ( 4  5) */		cmp	%o7,%i4

!  134		                    !   {
!  135		                    !     i16_to_d16_and_d32x4(&TwoToMinus16, &TwoTo16, &Zero,
!  136		                    !			  &(d16[2*i]), &(d32[i]), (float *)(&(i32[i])));

/* 0x0024	 136 ( 4  5) */		sethi	%hi(Zero),%g2
/* 0x0028	 133 ( 5  6) */		or	%g0,%g1,%o3
/* 0x002c	     ( 5  6) */		sub	%i4,4,%o2
/* 0x0030	 136 ( 6  7) */		add	%g2,%lo(Zero),%o1
/* 0x0034	 133 ( 6  7) */		or	%g0,0,%o5
/* 0x0038	     ( 7  8) */		or	%g0,0,%o4
/* 0x003c	 136 ( 7  8) */		or	%g0,%o3,%g4
                                   .L900000514:		/* frequency 6.4 confidence 0.0 */
/* 0x0040	     ( 0  3) */		ldd	[%o1],%f2
/* 0x0044	 136 ( 0  1) */		add	%i3,%o5,%g2
/* 0x0048	     ( 0  1) */		add	%i1,%o4,%g3
/* 0x004c	     ( 1  4) */		ldd	[%o1-8],%f0
/* 0x0050	     ( 1  2) */		add	%o7,4,%o7
/* 0x0054	     ( 1  2) */		add	%o3,16,%o3
/* 0x0058	     ( 2  3) */		fmovd	%f2,%f14
/* 0x005c	     ( 2  5) */		ld	[%g4],%f15
/* 0x0060	     ( 2  3) */		cmp	%o7,%o2
/* 0x0064	     ( 3  4) */		fmovd	%f2,%f10
/* 0x0068	     ( 3  6) */		ld	[%g4+4],%f11
/* 0x006c	     ( 4  5) */		fmovd	%f2,%f6
/* 0x0070	     ( 4  7) */		ld	[%g4+8],%f7
/* 0x0074	     ( 5  8) */		ld	[%g4+12],%f3
/* 0x0078	     ( 5  8) */		fxtod	%f14,%f14
/* 0x007c	     ( 6  9) */		fxtod	%f10,%f10
/* 0x0080	     ( 6  9) */		ldd	[%o1-16],%f16
/* 0x0084	     ( 7 10) */		fxtod	%f6,%f6
/* 0x0088	     ( 7  8) */		std	%f14,[%i3+%o5]
/* 0x008c	     ( 7  8) */		add	%o5,32,%o5
/* 0x0090	     ( 8 11) */		fxtod	%f2,%f2
/* 0x0094	     ( 8 11) */		fmuld	%f0,%f14,%f12
/* 0x0098	     ( 8  9) */		std	%f10,[%g2+8]
/* 0x009c	     ( 9 12) */		fmuld	%f0,%f10,%f8
/* 0x00a0	     ( 9 10) */		std	%f6,[%g2+16]
/* 0x00a4	     (10 13) */		fmuld	%f0,%f6,%f4
/* 0x00a8	     (10 11) */		std	%f2,[%g2+24]
/* 0x00ac	     (11 14) */		fmuld	%f0,%f2,%f0
/* 0x00b0	     (11 14) */		fdtox	%f12,%f12
/* 0x00b4	     (12 15) */		fdtox	%f8,%f8
/* 0x00b8	     (13 16) */		fdtox	%f4,%f4
/* 0x00bc	     (14 17) */		fdtox	%f0,%f0
/* 0x00c0	     (15 18) */		fxtod	%f12,%f12
/* 0x00c4	     (15 16) */		std	%f12,[%g3+8]
/* 0x00c8	     (16 19) */		fxtod	%f8,%f8
/* 0x00cc	     (16 17) */		std	%f8,[%g3+24]
/* 0x00d0	     (17 20) */		fxtod	%f4,%f4
/* 0x00d4	     (17 18) */		std	%f4,[%g3+40]
/* 0x00d8	     (18 21) */		fxtod	%f0,%f0
/* 0x00dc	     (18 21) */		fmuld	%f12,%f16,%f12
/* 0x00e0	     (18 19) */		std	%f0,[%g3+56]
/* 0x00e4	     (19 22) */		fmuld	%f8,%f16,%f8
/* 0x00e8	     (20 23) */		fmuld	%f4,%f16,%f4
/* 0x00ec	     (21 24) */		fmuld	%f0,%f16,%f0
/* 0x00f0	     (21 24) */		fsubd	%f14,%f12,%f12
/* 0x00f4	     (21 22) */		std	%f12,[%i1+%o4]
/* 0x00f8	     (22 25) */		fsubd	%f10,%f8,%f8
/* 0x00fc	     (22 23) */		std	%f8,[%g3+16]
/* 0x0100	     (22 23) */		add	%o4,64,%o4
/* 0x0104	     (23 26) */		fsubd	%f6,%f4,%f4
/* 0x0108	     (23 24) */		std	%f4,[%g3+32]
/* 0x010c	     (24 27) */		fsubd	%f2,%f0,%f0
/* 0x0110	     (24 25) */		std	%f0,[%g3+48]
/* 0x0114	     (24 25) */		ble,pt	%icc,.L900000514	! tprob=0.86
/* 0x0118	     (25 26) */		or	%g0,%o3,%g4
                                   .L77000159:		/* frequency 1.0 confidence 0.0 */

!  137		                    !   }
!  138		                    ! for(;i<len;i++)

/* 0x011c	 138 ( 0  1) */		cmp	%o7,%i4
                                   .L900000515:		/* frequency 1.0 confidence 0.0 */
/* 0x0120	 138 ( 0  1) */		bge,pt	%icc,.L77000164	! tprob=0.56
/* 0x0124	     ( 0  1) */		nop

!  139		                    !   {
!  140		                    !     a=i32[i];
!  141		                    !     d32[i]=(double)(i32[i]);
!  142		                    !     d16[2*i]=(double)(a&0xffff);
!  143		                    !     d16[2*i+1]=(double)(a>>16);

/* 0x0128	 143 ( 0  1) */		sethi	%hi(.L_const_seg_900000501),%o1
/* 0x012c	 138 ( 1  2) */		sethi	%hi(0xfc00),%o0
/* 0x0130	 141 ( 1  4) */		ldd	[%o1+%lo(.L_const_seg_900000501)],%f0
/* 0x0134	 138 ( 1  2) */		sub	%i4,%o7,%g3
/* 0x0138	     ( 2  3) */		sll	%o7,2,%g2
/* 0x013c	     ( 2  3) */		add	%o0,1023,%o3
/* 0x0140	     ( 3  4) */		sll	%o7,3,%g4
/* 0x0144	     ( 3  4) */		cmp	%g3,3
/* 0x0148	     ( 4  5) */		add	%g1,%g2,%o0
/* 0x014c	     ( 4  5) */		add	%o1,%lo(.L_const_seg_900000501),%o2
/* 0x0150	     ( 5  6) */		add	%i3,%g4,%o4
/* 0x0154	     ( 5  6) */		sub	%i4,1,%o1
/* 0x0158	     ( 6  7) */		sll	%o7,4,%g5
/* 0x015c	     ( 6  7) */		bl,pn	%icc,.L77000161	! tprob=0.44
/* 0x0160	     ( 7  8) */		add	%i1,%g5,%o5
/* 0x0164	 141 ( 7 10) */		ld	[%g1+%g2],%f3
/* 0x0168	 143 ( 7  8) */		add	%o4,8,%o4
/* 0x016c	 140 ( 8 11) */		ld	[%g1+%g2],%g1
/* 0x0170	 143 ( 8  9) */		add	%o5,16,%o5
/* 0x0174	     ( 8  9) */		add	%o7,1,%o7
/* 0x0178	 141 ( 9 10) */		fmovs	%f0,%f2
/* 0x017c	 143 ( 9 10) */		add	%o0,4,%o0
/* 0x0180	 142 (10 11) */		and	%g1,%o3,%g2
/* 0x0184	 141 (11 14) */		fsubd	%f2,%f0,%f2
/* 0x0188	     (11 12) */		std	%f2,[%o4-8]
/* 0x018c	 143 (11 12) */		srl	%g1,16,%g1
/* 0x0190	 142 (12 13) */		st	%g2,[%sp+96]
/* 0x0194	     (15 16) */		fmovs	%f0,%f2
/* 0x0198	     (16 19) */		ld	[%sp+96],%f3
/* 0x019c	     (18 21) */		fsubd	%f2,%f0,%f2
/* 0x01a0	     (18 19) */		std	%f2,[%o5-16]
/* 0x01a4	 143 (19 20) */		st	%g1,[%sp+92]
/* 0x01a8	     (22 23) */		fmovs	%f0,%f2
/* 0x01ac	     (23 26) */		ld	[%sp+92],%f3
/* 0x01b0	     (25 28) */		fsubd	%f2,%f0,%f2
/* 0x01b4	     (25 26) */		std	%f2,[%o5-8]
                                   .L900000509:		/* frequency 64.0 confidence 0.0 */
/* 0x01b8	 141 (26 28) */		ld	[%o0],%f3
/* 0x01bc	 143 (26 27) */		add	%o7,2,%o7
/* 0x01c0	     (26 27) */		add	%o5,32,%o5
/* 0x01c4	 140 (27 29) */		ld	[%o0],%g1
/* 0x01c8	 143 (27 27) */		cmp	%o7,%o1
/* 0x01cc	     (27 28) */		add	%o4,16,%o4
/* 0x01d0	 141 ( 0  0) */		fmovs	%f0,%f2
/* 0x01d4	     (28 31) */		fsubd	%f2,%f0,%f2
/* 0x01d8	     (29 30) */		std	%f2,[%o4-16]
/* 0x01dc	 142 (29 30) */		and	%g1,%o3,%g2
/* 0x01e0	     (30 31) */		st	%g2,[%sp+96]
/* 0x01e4	     (37 39) */		ld	[%sp+96],%f3
/* 0x01e8	     ( 0  0) */		fmovs	%f0,%f2
/* 0x01ec	     (39 42) */		fsubd	%f2,%f0,%f2
/* 0x01f0	 143 (40 41) */		srl	%g1,16,%g1
/* 0x01f4	 142 (40 41) */		std	%f2,[%o5-32]
/* 0x01f8	 143 (41 42) */		st	%g1,[%sp+92]
/* 0x01fc	     (48 50) */		ld	[%sp+92],%f3
/* 0x0200	     ( 0  0) */		fmovs	%f0,%f2
/* 0x0204	     (50 53) */		fsubd	%f2,%f0,%f2
/* 0x0208	     (51 52) */		std	%f2,[%o5-24]
/* 0x020c	     (51 52) */		add	%o0,4,%o0
/* 0x0210	 141 (52 54) */		ld	[%o0],%f3
/* 0x0214	 140 (53 55) */		ld	[%o0],%g1
/* 0x0218	 141 ( 0  0) */		fmovs	%f0,%f2
/* 0x021c	     (54 57) */		fsubd	%f2,%f0,%f2
/* 0x0220	     (55 56) */		std	%f2,[%o4-8]
/* 0x0224	 142 (55 56) */		and	%g1,%o3,%g2
/* 0x0228	     (56 57) */		st	%g2,[%sp+96]
/* 0x022c	     (63 65) */		ld	[%sp+96],%f3
/* 0x0230	     ( 0  0) */		fmovs	%f0,%f2
/* 0x0234	     (65 68) */		fsubd	%f2,%f0,%f2
/* 0x0238	 143 (66 67) */		srl	%g1,16,%g1
/* 0x023c	 142 (66 67) */		std	%f2,[%o5-16]
/* 0x0240	 143 (67 68) */		st	%g1,[%sp+92]
/* 0x0244	     (74 76) */		ld	[%sp+92],%f3
/* 0x0248	     ( 0  0) */		fmovs	%f0,%f2
/* 0x024c	     (76 79) */		fsubd	%f2,%f0,%f2
/* 0x0250	     (77 78) */		std	%f2,[%o5-8]
/* 0x0254	     (77 78) */		bl,pt	%icc,.L900000509	! tprob=0.50
/* 0x0258	     (77 78) */		add	%o0,4,%o0
                                   .L900000512:		/* frequency 8.0 confidence 0.0 */
/* 0x025c	 143 ( 0  1) */		cmp	%o7,%i4
/* 0x0260	     ( 0  1) */		bge,pn	%icc,.L77000164	! tprob=0.14
/* 0x0264	     ( 0  1) */		nop
                                   .L77000161:		/* frequency 0.7 confidence 0.0 */
/* 0x0268	 141 ( 0  3) */		ld	[%o0],%f3
                                   .L900000513:		/* frequency 6.4 confidence 0.0 */
/* 0x026c	 141 ( 0  3) */		ldd	[%o2],%f0
/* 0x0270	 143 ( 0  1) */		add	%o7,1,%o7
/* 0x0274	 140 ( 1  4) */		ld	[%o0],%o1
/* 0x0278	 143 ( 1  2) */		add	%o0,4,%o0
/* 0x027c	     ( 1  2) */		cmp	%o7,%i4
/* 0x0280	 141 ( 2  3) */		fmovs	%f0,%f2
/* 0x0284	 142 ( 3  4) */		and	%o1,%o3,%g1
/* 0x0288	 141 ( 4  7) */		fsubd	%f2,%f0,%f2
/* 0x028c	     ( 4  5) */		std	%f2,[%o4]
/* 0x0290	 143 ( 4  5) */		srl	%o1,16,%o1
/* 0x0294	 142 ( 5  6) */		st	%g1,[%sp+96]
/* 0x0298	 143 ( 5  6) */		add	%o4,8,%o4
/* 0x029c	 142 ( 8  9) */		fmovs	%f0,%f2
/* 0x02a0	     ( 9 12) */		ld	[%sp+96],%f3
/* 0x02a4	     (11 14) */		fsubd	%f2,%f0,%f2
/* 0x02a8	     (11 12) */		std	%f2,[%o5]
/* 0x02ac	 143 (12 13) */		st	%o1,[%sp+92]
/* 0x02b0	     (15 16) */		fmovs	%f0,%f2
/* 0x02b4	     (16 19) */		ld	[%sp+92],%f3
/* 0x02b8	     (18 21) */		fsubd	%f2,%f0,%f0
/* 0x02bc	     (18 19) */		std	%f0,[%o5+8]
/* 0x02c0	     (18 19) */		add	%o5,16,%o5
/* 0x02c4	     (18 19) */		bl,a,pt	%icc,.L900000513	! tprob=0.86
/* 0x02c8	     (19 22) */		ld	[%o0],%f3
                                   .L77000164:		/* frequency 1.0 confidence 0.0 */
/* 0x02cc	     ( 0  2) */		ret	! Result = 
/* 0x02d0	     ( 2  3) */		restore	%g0,%g0,%g0
/* 0x02d4	   0 ( 0  0) */		.type	conv_i32_to_d32_and_d16,2
/* 0x02d4	     ( 0  0) */		.size	conv_i32_to_d32_and_d16,(.-conv_i32_to_d32_and_d16)

	.section	".text",#alloc,#execinstr
/* 000000	   0 ( 0  0) */		.align	4
!
! SUBROUTINE adjust_montf_result
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION	(ISSUE TIME)	(COMPLETION TIME)

                                   	.global adjust_montf_result
                                   adjust_montf_result:		/* frequency 1.0 confidence 0.0 */

!  144		                    !   }
!  145		                    !}
!  148		                    !void adjust_montf_result(unsigned int *i32, unsigned int *nint, int len)
!  149		                    !{
!  150		                    !long long acc;
!  151		                    !int i;
!  153		                    ! if(i32[len]>0) i=-1;

/* 000000	 153 ( 0  1) */		sll	%o2,2,%g1
/* 0x0004	     ( 0  1) */		or	%g0,-1,%g3
/* 0x0008	     ( 1  4) */		ld	[%o0+%g1],%g1
/* 0x000c	     ( 3  4) */		cmp	%g1,0
/* 0x0010	     ( 3  4) */		bleu,pn	%icc,.L77000175	! tprob=0.50
/* 0x0014	     ( 3  4) */		or	%g0,%o1,%o3
/* 0x0018	     ( 4  5) */		ba	.L900000611	! tprob=1.00
/* 0x001c	     ( 4  5) */		cmp	%g3,0
                                   .L77000175:		/* frequency 0.8 confidence 0.0 */

!  154		                    ! else
!  155		                    !   {
!  156		                    !     for(i=len-1; i>=0; i++)

/* 0x0020	 156 ( 0  1) */		subcc	%o2,1,%g3
/* 0x0024	     ( 0  1) */		bneg,pt	%icc,.L900000611	! tprob=0.60
/* 0x0028	     ( 1  2) */		cmp	%g3,0
/* 0x002c	     ( 1  2) */		sll	%g3,2,%g1
/* 0x0030	     ( 2  3) */		add	%o0,%g1,%g2
/* 0x0034	     ( 2  3) */		add	%o1,%g1,%g1

!  157		                    !       {
!  158		                    !	 if(i32[i]!=nint[i]) break;

/* 0x0038	 158 ( 3  6) */		ld	[%g1],%g5
                                   .L900000610:		/* frequency 5.3 confidence 0.0 */
/* 0x003c	 158 ( 0  3) */		ld	[%g2],%o5
/* 0x0040	     ( 0  1) */		add	%g1,4,%g1
/* 0x0044	     ( 0  1) */		add	%g2,4,%g2
/* 0x0048	     ( 2  3) */		cmp	%o5,%g5
/* 0x004c	     ( 2  3) */		bne,pn	%icc,.L77000182	! tprob=0.16
/* 0x0050	     ( 2  3) */		nop
/* 0x0054	     ( 3  4) */		addcc	%g3,1,%g3
/* 0x0058	     ( 3  4) */		bpos,a,pt	%icc,.L900000610	! tprob=0.84
/* 0x005c	     ( 3  6) */		ld	[%g1],%g5
                                   .L77000182:		/* frequency 1.0 confidence 0.0 */

!  159		                    !       }
!  160		                    !   }
!  161		                    ! if((i<0)||(i32[i]>nint[i]))

/* 0x0060	 161 ( 0  1) */		cmp	%g3,0
                                   .L900000611:		/* frequency 1.0 confidence 0.0 */
/* 0x0064	 161 ( 0  1) */		bl,pn	%icc,.L77000198	! tprob=0.50
/* 0x0068	     ( 0  1) */		sll	%g3,2,%g2
/* 0x006c	     ( 1  4) */		ld	[%o1+%g2],%g1
/* 0x0070	     ( 2  5) */		ld	[%o0+%g2],%g2
/* 0x0074	     ( 4  5) */		cmp	%g2,%g1
/* 0x0078	     ( 4  5) */		bleu,pt	%icc,.L77000191	! tprob=0.56
/* 0x007c	     ( 4  5) */		nop
                                   .L77000198:		/* frequency 0.8 confidence 0.0 */

!  162		                    !   {
!  163		                    !     acc=0;
!  164		                    !     for(i=0;i<len;i++)

/* 0x0080	 164 ( 0  1) */		cmp	%o2,0
/* 0x0084	     ( 0  1) */		ble,pt	%icc,.L77000191	! tprob=0.60
/* 0x0088	     ( 0  1) */		nop
/* 0x008c	 161 ( 1  2) */		or	%g0,-1,%g2
/* 0x0090	     ( 1  2) */		sub	%o2,1,%g4
/* 0x0094	     ( 2  3) */		srl	%g2,0,%g3
/* 0x0098	 163 ( 2  3) */		or	%g0,0,%g5
/* 0x009c	 164 ( 3  4) */		or	%g0,0,%o5
/* 0x00a0	 161 ( 3  4) */		or	%g0,%o0,%o4
/* 0x00a4	     ( 4  5) */		cmp	%o2,3
/* 0x00a8	     ( 4  5) */		add	%o1,4,%g2
/* 0x00ac	 164 ( 4  5) */		bl,pn	%icc,.L77000199	! tprob=0.40
/* 0x00b0	     ( 5  6) */		add	%o0,8,%g1

!  165		                    !       {
!  166		                    !	 acc=acc+(unsigned long long)(i32[i])-(unsigned long long)(nint[i]);

/* 0x00b4	 166 ( 5  8) */		ld	[%o0],%o2
/* 0x00b8	   0 ( 5  6) */		or	%g0,%g2,%o3
/* 0x00bc	 166 ( 6  9) */		ld	[%o1],%o1
/* 0x00c0	   0 ( 6  7) */		or	%g0,%g1,%o4

!  167		                    !	 i32[i]=acc&0xffffffff;
!  168		                    !	 acc=acc>>32;

/* 0x00c4	 168 ( 6  7) */		or	%g0,2,%o5
/* 0x00c8	 166 ( 7 10) */		ld	[%o0+4],%g1
/* 0x00cc	 164 ( 8  9) */		sub	%o2,%o1,%o2
/* 0x00d0	     ( 9 10) */		or	%g0,%o2,%g5
/* 0x00d4	 167 ( 9 10) */		and	%o2,%g3,%o2
/* 0x00d8	     ( 9 10) */		st	%o2,[%o0]
/* 0x00dc	 168 (10 11) */		srax	%g5,32,%g5
                                   .L900000605:		/* frequency 64.0 confidence 0.0 */
/* 0x00e0	 166 (12 20) */		ld	[%o3],%o2
/* 0x00e4	 168 (12 13) */		add	%o5,1,%o5
/* 0x00e8	     (12 13) */		add	%o3,4,%o3
/* 0x00ec	     (13 13) */		cmp	%o5,%g4
/* 0x00f0	     (13 14) */		add	%o4,4,%o4
/* 0x00f4	 164 (14 14) */		sub	%g1,%o2,%g1
/* 0x00f8	     (15 15) */		add	%g1,%g5,%g5
/* 0x00fc	 167 (16 17) */		and	%g5,%g3,%o2
/* 0x0100	 166 (16 24) */		ld	[%o4-4],%g1
/* 0x0104	 167 (17 18) */		st	%o2,[%o4-8]
/* 0x0108	 168 (17 18) */		ble,pt	%icc,.L900000605	! tprob=0.50
/* 0x010c	     (17 18) */		srax	%g5,32,%g5
                                   .L900000608:		/* frequency 8.0 confidence 0.0 */
/* 0x0110	 166 ( 0  3) */		ld	[%o3],%g2
/* 0x0114	 164 ( 2  3) */		sub	%g1,%g2,%g1
/* 0x0118	     ( 3  4) */		add	%g1,%g5,%g1
/* 0x011c	 167 ( 4  5) */		and	%g1,%g3,%g2
/* 0x0120	     ( 5  7) */		retl	! Result = 
/* 0x0124	     ( 6  7) */		st	%g2,[%o4-4]
                                   .L77000199:		/* frequency 0.6 confidence 0.0 */
/* 0x0128	 166 ( 0  3) */		ld	[%o4],%g1
                                   .L900000609:		/* frequency 5.3 confidence 0.0 */
/* 0x012c	 166 ( 0  3) */		ld	[%o3],%g2
/* 0x0130	     ( 0  1) */		add	%g5,%g1,%g1
/* 0x0134	 168 ( 0  1) */		add	%o5,1,%o5
/* 0x0138	     ( 1  2) */		add	%o3,4,%o3
/* 0x013c	     ( 1  2) */		cmp	%o5,%g4
/* 0x0140	 166 ( 2  3) */		sub	%g1,%g2,%g1
/* 0x0144	 167 ( 3  4) */		and	%g1,%g3,%g2
/* 0x0148	     ( 3  4) */		st	%g2,[%o4]
/* 0x014c	 168 ( 3  4) */		add	%o4,4,%o4
/* 0x0150	     ( 4  5) */		srax	%g1,32,%g5
/* 0x0154	     ( 4  5) */		ble,a,pt	%icc,.L900000609	! tprob=0.84
/* 0x0158	     ( 4  7) */		ld	[%o4],%g1
                                   .L77000191:		/* frequency 1.0 confidence 0.0 */
/* 0x015c	     ( 0  2) */		retl	! Result = 
/* 0x0160	     ( 1  2) */		nop
/* 0x0164	   0 ( 0  0) */		.type	adjust_montf_result,2
/* 0x0164	     ( 0  0) */		.size	adjust_montf_result,(.-adjust_montf_result)

	.section	".text",#alloc,#execinstr
/* 000000	   0 ( 0  0) */		.align	32
!
! SUBROUTINE mont_mulf_noconv
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION	(ISSUE TIME)	(COMPLETION TIME)

                                   	.global mont_mulf_noconv
                                   mont_mulf_noconv:		/* frequency 1.0 confidence 0.0 */
/* 000000	     ( 0  1) */		save	%sp,-144,%sp
/* 0x0004	     ( 1  2) */		st	%i0,[%fp+68]

!  169		                    !       }
!  170		                    !   }
!  171		                    !}
!  175		                    !void cleanup(double *dt, int from, int tlen);
!  177		                    !/*
!  178		                    !** the lengths of the input arrays should be at least the following:
!  179		                    !** result[nlen+1], dm1[nlen], dm2[2*nlen+1], dt[4*nlen+2], dn[nlen], nint[nlen]
!  180		                    !** all of them should be different from one another
!  181		                    !**
!  182		                    !*/
!  183		                    !void mont_mulf_noconv(unsigned int *result,
!  184		                    !		     double *dm1, double *dm2, double *dt,
!  185		                    !		     double *dn, unsigned int *nint,
!  186		                    !		     int nlen, double dn0)
!  187		                    !{
!  188		                    ! int i, j, jj;
!  189		                    ! int tmp;
!  190		                    ! double digit, m2j, nextm2j, a, b;
!  191		                    ! double *dptmp, *pdm1, *pdm2, *pdn, *pdtj, pdn_0, pdm1_0;
!  193		                    ! pdm1=&(dm1[0]);
!  194		                    ! pdm2=&(dm2[0]);
!  195		                    ! pdn=&(dn[0]);
!  196		                    ! pdm2[2*nlen]=Zero;

/* 0x0008	 196 ( 1  2) */		sethi	%hi(Zero),%g2
/* 0x000c	 187 ( 1  2) */		or	%g0,%i2,%o1
/* 0x0010	     ( 2  3) */		st	%i5,[%fp+88]
/* 0x0014	     ( 2  3) */		or	%g0,%i3,%o2
/* 0x0018	 196 ( 2  3) */		add	%g2,%lo(Zero),%g4
/* 0x001c	     ( 3  6) */		ldd	[%g2+%lo(Zero)],%f2
/* 0x0020	 187 ( 3  4) */		or	%g0,%o2,%g5
/* 0x0024	 196 ( 3  4) */		or	%g0,%o1,%i0
/* 0x0028	 187 ( 4  5) */		or	%g0,%i4,%i2

!  198		                    ! if (nlen!=16)
!  199		                    !   {
!  200		                    !     for(i=0;i<4*nlen+2;i++) dt[i]=Zero;
!  202		                    !     a=dt[0]=pdm1[0]*pdm2[0];
!  203		                    !     digit=mod(lower32(a,Zero)*dn0,TwoToMinus16,TwoTo16);
!  205		                    !     pdtj=&(dt[0]);
!  206		                    !     for(j=jj=0;j<2*nlen;j++,jj++,pdtj++)
!  207		                    !       {
!  208		                    !	 m2j=pdm2[j];
!  209		                    !	 a=pdtj[0]+pdn[0]*digit;
!  210		                    !	 b=pdtj[1]+pdm1[0]*pdm2[j+1]+a*TwoToMinus16;
!  211		                    !	 pdtj[1]=b;
!  213		                    !#pragma pipeloop(0)
!  214		                    !	 for(i=1;i<nlen;i++)
!  215		                    !	   {
!  216		                    !	     pdtj[2*i]+=pdm1[i]*m2j+pdn[i]*digit;
!  217		                    !	   }
!  218		                    ! 	 if((jj==30)) {cleanup(dt,j/2+1,2*nlen+1); jj=0;}
!  219		                    !	 
!  220		                    !	 digit=mod(lower32(b,Zero)*dn0,TwoToMinus16,TwoTo16);
!  221		                    !       }
!  222		                    !   }
!  223		                    ! else
!  224		                    !   {
!  225		                    !     a=dt[0]=pdm1[0]*pdm2[0];
!  227		                    !     dt[65]=     dt[64]=     dt[63]=     dt[62]=     dt[61]=     dt[60]=
!  228		                    !     dt[59]=     dt[58]=     dt[57]=     dt[56]=     dt[55]=     dt[54]=
!  229		                    !     dt[53]=     dt[52]=     dt[51]=     dt[50]=     dt[49]=     dt[48]=
!  230		                    !     dt[47]=     dt[46]=     dt[45]=     dt[44]=     dt[43]=     dt[42]=
!  231		                    !     dt[41]=     dt[40]=     dt[39]=     dt[38]=     dt[37]=     dt[36]=
!  232		                    !     dt[35]=     dt[34]=     dt[33]=     dt[32]=     dt[31]=     dt[30]=
!  233		                    !     dt[29]=     dt[28]=     dt[27]=     dt[26]=     dt[25]=     dt[24]=
!  234		                    !     dt[23]=     dt[22]=     dt[21]=     dt[20]=     dt[19]=     dt[18]=
!  235		                    !     dt[17]=     dt[16]=     dt[15]=     dt[14]=     dt[13]=     dt[12]=
!  236		                    !     dt[11]=     dt[10]=     dt[ 9]=     dt[ 8]=     dt[ 7]=     dt[ 6]=
!  237		                    !     dt[ 5]=     dt[ 4]=     dt[ 3]=     dt[ 2]=     dt[ 1]=Zero;
!  239		                    !     pdn_0=pdn[0];
!  240		                    !     pdm1_0=pdm1[0];
!  242		                    !     digit=mod(lower32(a,Zero)*dn0,TwoToMinus16,TwoTo16);
!  243		                    !     pdtj=&(dt[0]);
!  245		                    !     for(j=0;j<32;j++,pdtj++)
!  246		                    !       {
!  248		                    !	 m2j=pdm2[j];
!  249		                    !	 a=pdtj[0]+pdn_0*digit;
!  250		                    !	 b=pdtj[1]+pdm1_0*pdm2[j+1]+a*TwoToMinus16;
!  251		                    !	 pdtj[1]=b;
!  253		                    !	 /**** this loop will be fully unrolled:
!  254		                    !	 for(i=1;i<16;i++)
!  255		                    !	   {
!  256		                    !	     pdtj[2*i]+=pdm1[i]*m2j+pdn[i]*digit;
!  257		                    !	   }
!  258		                    !	 *************************************/
!  259		                    !	     pdtj[2]+=pdm1[1]*m2j+pdn[1]*digit;
!  260		                    !	     pdtj[4]+=pdm1[2]*m2j+pdn[2]*digit;
!  261		                    !	     pdtj[6]+=pdm1[3]*m2j+pdn[3]*digit;
!  262		                    !	     pdtj[8]+=pdm1[4]*m2j+pdn[4]*digit;
!  263		                    !	     pdtj[10]+=pdm1[5]*m2j+pdn[5]*digit;
!  264		                    !	     pdtj[12]+=pdm1[6]*m2j+pdn[6]*digit;
!  265		                    !	     pdtj[14]+=pdm1[7]*m2j+pdn[7]*digit;
!  266		                    !	     pdtj[16]+=pdm1[8]*m2j+pdn[8]*digit;
!  267		                    !	     pdtj[18]+=pdm1[9]*m2j+pdn[9]*digit;
!  268		                    !	     pdtj[20]+=pdm1[10]*m2j+pdn[10]*digit;
!  269		                    !	     pdtj[22]+=pdm1[11]*m2j+pdn[11]*digit;
!  270		                    !	     pdtj[24]+=pdm1[12]*m2j+pdn[12]*digit;
!  271		                    !	     pdtj[26]+=pdm1[13]*m2j+pdn[13]*digit;
!  272		                    !	     pdtj[28]+=pdm1[14]*m2j+pdn[14]*digit;
!  273		                    !	     pdtj[30]+=pdm1[15]*m2j+pdn[15]*digit;
!  274		                    !	 /* no need for cleenup, cannot overflow */
!  275		                    !	 digit=mod(lower32(b,Zero)*dn0,TwoToMinus16,TwoTo16);
!  276		                    !       }
!  277		                    !   }
!  279		                    ! conv_d16_to_i32(result,dt+2*nlen,(long long *)dt,nlen+1);
!  281		                    ! adjust_montf_result(result,nint,nlen); 

/* 0x002c	 281 ( 4  5) */		or	%g0,1,%o4
/* 0x0030	 187 ( 6  9) */		ldd	[%fp+96],%f0
/* 0x0034	 196 ( 7 10) */		ld	[%fp+92],%o0
/* 0x0038	 187 ( 8  9) */		fmovd	%f0,%f16
/* 0x003c	 196 ( 9 10) */		sll	%o0,4,%g2
/* 0x0040	     ( 9 10) */		or	%g0,%o0,%g1
/* 0x0044	 198 (10 11) */		cmp	%o0,16
/* 0x0048	     (10 11) */		be,pn	%icc,.L77000289	! tprob=0.50
/* 0x004c	     (10 11) */		std	%f2,[%o1+%g2]
/* 0x0050	 200 (11 12) */		sll	%o0,2,%g2
/* 0x0054	     (11 14) */		ldd	[%g4],%f2
/* 0x0058	     (12 13) */		add	%g2,2,%o1
/* 0x005c	     (12 13) */		add	%g2,1,%o3
/* 0x0060	 196 (13 14) */		sll	%o0,1,%o7
/* 0x0064	 200 (13 14) */		cmp	%o1,0
/* 0x0068	     (13 14) */		ble,a,pt	%icc,.L900000755	! tprob=0.55
/* 0x006c	     (14 17) */		ldd	[%i1],%f0
/* 0x0070	     (14 15) */		cmp	%o1,3
/* 0x0074	 281 (14 15) */		or	%g0,1,%o1
/* 0x0078	     (14 15) */		bl,pn	%icc,.L77000279	! tprob=0.40
/* 0x007c	     (15 16) */		add	%o2,8,%o0
/* 0x0080	     (15 16) */		std	%f2,[%g5]
/* 0x0084	   0 (16 17) */		or	%g0,%o0,%o2
                                   .L900000726:		/* frequency 64.0 confidence 0.0 */
/* 0x0088	     ( 3  5) */		ldd	[%g4],%f0
/* 0x008c	     ( 3  4) */		add	%o4,1,%o4
/* 0x0090	     ( 3  4) */		add	%o2,8,%o2
/* 0x0094	     ( 4  4) */		cmp	%o4,%o3
/* 0x0098	     ( 5  6) */		ble,pt	%icc,.L900000726	! tprob=0.50
/* 0x009c	     ( 5  6) */		std	%f0,[%o2-8]
                                   .L900000729:		/* frequency 8.0 confidence 0.0 */
/* 0x00a0	     ( 0  1) */		ba	.L900000755	! tprob=1.00
/* 0x00a4	     ( 0  3) */		ldd	[%i1],%f0
                                   .L77000279:		/* frequency 0.6 confidence 0.0 */
/* 0x00a8	     ( 0  1) */		std	%f2,[%o2]
                                   .L900000754:		/* frequency 5.3 confidence 0.0 */
/* 0x00ac	     ( 0  3) */		ldd	[%g4],%f2
/* 0x00b0	     ( 0  1) */		cmp	%o1,%o3
/* 0x00b4	     ( 0  1) */		add	%o2,8,%o2
/* 0x00b8	     ( 1  2) */		add	%o1,1,%o1
/* 0x00bc	     ( 1  2) */		ble,a,pt	%icc,.L900000754	! tprob=0.87
/* 0x00c0	     ( 3  4) */		std	%f2,[%o2]
                                   .L77000284:		/* frequency 0.8 confidence 0.0 */
/* 0x00c4	 202 ( 0  3) */		ldd	[%i1],%f0
                                   .L900000755:		/* frequency 0.8 confidence 0.0 */
/* 0x00c8	 202 ( 0  3) */		ldd	[%i0],%f2
/* 0x00cc	     ( 0  1) */		add	%o7,1,%o2
/* 0x00d0	 206 ( 0  1) */		cmp	%o7,0
/* 0x00d4	     ( 1  2) */		sll	%o2,1,%o0
/* 0x00d8	     ( 1  2) */		sub	%o7,1,%o1
/* 0x00dc	 202 ( 2  5) */		fmuld	%f0,%f2,%f0
/* 0x00e0	     ( 2  3) */		std	%f0,[%g5]
/* 0x00e4	     ( 2  3) */		sub	%g1,1,%o7
/* 0x00e8	     ( 3  6) */		ldd	[%g4],%f6
/* 0x00ec	   0 ( 3  4) */		or	%g0,%o7,%g3
/* 0x00f0	     ( 3  4) */		or	%g0,0,%l0
/* 0x00f4	     ( 4  7) */		ldd	[%g4-8],%f2
/* 0x00f8	     ( 4  5) */		or	%g0,0,%i5
/* 0x00fc	     ( 4  5) */		or	%g0,%o1,%o5
/* 0x0100	     ( 5  8) */		fdtox	%f0,%f0
/* 0x0104	     ( 5  8) */		ldd	[%g4-16],%f4
/* 0x0108	     ( 5  6) */		or	%g0,%o0,%o3
/* 0x010c	 210 ( 6  7) */		add	%i0,8,%o4
/* 0x0110	     ( 6  7) */		or	%g0,0,%i4
/* 0x0114	     ( 9 10) */		fmovs	%f6,%f0
/* 0x0118	     (11 14) */		fxtod	%f0,%f0
/* 0x011c	 203 (14 17) */		fmuld	%f0,%f16,%f0
/* 0x0120	     (17 20) */		fmuld	%f0,%f2,%f2
/* 0x0124	     (20 23) */		fdtox	%f2,%f2
/* 0x0128	     (23 26) */		fxtod	%f2,%f2
/* 0x012c	     (26 29) */		fmuld	%f2,%f4,%f2
/* 0x0130	     (29 32) */		fsubd	%f0,%f2,%f22
/* 0x0134	 206 (29 30) */		ble,pt	%icc,.L900000748	! tprob=0.60
/* 0x0138	     (29 30) */		sll	%g1,4,%g2
/* 0x013c	 210 (30 33) */		ldd	[%i2],%f0
                                   .L900000749:		/* frequency 5.3 confidence 0.0 */
/* 0x0140	 210 ( 0  3) */		fmuld	%f0,%f22,%f8
/* 0x0144	     ( 0  3) */		ldd	[%i1],%f0
/* 0x0148	 214 ( 0  1) */		cmp	%g1,1
/* 0x014c	 210 ( 1  4) */		ldd	[%o4+%i4],%f6
/* 0x0150	     ( 1  2) */		add	%i1,8,%o0
/* 0x0154	 214 ( 1  2) */		or	%g0,1,%o1
/* 0x0158	 210 ( 2  5) */		ldd	[%i3],%f2
/* 0x015c	     ( 2  3) */		add	%i3,16,%l1
/* 0x0160	     ( 3  6) */		fmuld	%f0,%f6,%f6
/* 0x0164	     ( 3  6) */		ldd	[%g4-8],%f4
/* 0x0168	     ( 4  7) */		faddd	%f2,%f8,%f2
/* 0x016c	     ( 4  7) */		ldd	[%i3+8],%f0
/* 0x0170	 208 ( 5  8) */		ldd	[%i0+%i4],%f20
/* 0x0174	 210 ( 6  9) */		faddd	%f0,%f6,%f0
/* 0x0178	     ( 7 10) */		fmuld	%f2,%f4,%f2
/* 0x017c	     (10 13) */		faddd	%f0,%f2,%f18
/* 0x0180	 211 (10 11) */		std	%f18,[%i3+8]
/* 0x0184	 214 (10 11) */		ble,pt	%icc,.L900000753	! tprob=0.54
/* 0x0188	     (11 12) */		srl	%i5,31,%g2
/* 0x018c	     (11 12) */		cmp	%g3,7
/* 0x0190	 210 (12 13) */		add	%i2,8,%g2
/* 0x0194	 214 (12 13) */		bl,pn	%icc,.L77000281	! tprob=0.36
/* 0x0198	     (13 14) */		add	%g2,24,%o2
/* 0x019c	 216 (13 16) */		ldd	[%o0+16],%f14
/* 0x01a0	     (13 14) */		add	%i3,48,%l1
/* 0x01a4	     (14 17) */		ldd	[%o0+24],%f12
/* 0x01a8	   0 (14 15) */		or	%g0,%o2,%g2
/* 0x01ac	 214 (14 15) */		sub	%g1,3,%o2
/* 0x01b0	 216 (15 18) */		ldd	[%o0],%f2
/* 0x01b4	     (15 16) */		or	%g0,5,%o1
/* 0x01b8	     (16 19) */		ldd	[%g2-24],%f0
/* 0x01bc	     (17 20) */		ldd	[%o0+8],%f6
/* 0x01c0	     (17 20) */		fmuld	%f2,%f20,%f2
/* 0x01c4	     (17 18) */		add	%o0,32,%o0
/* 0x01c8	     (18 21) */		ldd	[%g2-16],%f8
/* 0x01cc	     (18 21) */		fmuld	%f0,%f22,%f4
/* 0x01d0	     (19 22) */		ldd	[%i3+16],%f0
/* 0x01d4	     (19 22) */		fmuld	%f6,%f20,%f10
/* 0x01d8	     (20 23) */		ldd	[%g2-8],%f6
/* 0x01dc	     (21 24) */		faddd	%f2,%f4,%f4
/* 0x01e0	     (21 24) */		ldd	[%i3+32],%f2
                                   .L900000738:		/* frequency 512.0 confidence 0.0 */
/* 0x01e4	 216 (16 24) */		ldd	[%g2],%f24
/* 0x01e8	     (16 17) */		add	%o1,3,%o1
/* 0x01ec	     (16 17) */		add	%g2,24,%g2
/* 0x01f0	     (16 19) */		fmuld	%f8,%f22,%f8
/* 0x01f4	     (17 25) */		ldd	[%l1],%f28
/* 0x01f8	     (17 17) */		cmp	%o1,%o2
/* 0x01fc	     (17 18) */		add	%o0,24,%o0
/* 0x0200	     (18 26) */		ldd	[%o0-24],%f26
/* 0x0204	     (18 21) */		faddd	%f0,%f4,%f0
/* 0x0208	     (18 19) */		add	%l1,48,%l1
/* 0x020c	     (19 22) */		faddd	%f10,%f8,%f10
/* 0x0210	     (19 22) */		fmuld	%f14,%f20,%f4
/* 0x0214	     (19 20) */		std	%f0,[%l1-80]
/* 0x0218	     (20 28) */		ldd	[%g2-16],%f8
/* 0x021c	     (20 23) */		fmuld	%f6,%f22,%f6
/* 0x0220	     (21 29) */		ldd	[%l1-32],%f0
/* 0x0224	     (22 30) */		ldd	[%o0-16],%f14
/* 0x0228	     (22 25) */		faddd	%f2,%f10,%f2
/* 0x022c	     (23 26) */		faddd	%f4,%f6,%f10
/* 0x0230	     (23 26) */		fmuld	%f12,%f20,%f4
/* 0x0234	     (23 24) */		std	%f2,[%l1-64]
/* 0x0238	     (24 32) */		ldd	[%g2-8],%f6
/* 0x023c	     (24 27) */		fmuld	%f24,%f22,%f24
/* 0x0240	     (25 33) */		ldd	[%l1-16],%f2
/* 0x0244	     (26 34) */		ldd	[%o0-8],%f12
/* 0x0248	     (26 29) */		faddd	%f28,%f10,%f10
/* 0x024c	     (27 28) */		std	%f10,[%l1-48]
/* 0x0250	     (27 30) */		fmuld	%f26,%f20,%f10
/* 0x0254	     (27 28) */		ble,pt	%icc,.L900000738	! tprob=0.50
/* 0x0258	     (27 30) */		faddd	%f4,%f24,%f4
                                   .L900000741:		/* frequency 64.0 confidence 0.0 */
/* 0x025c	 216 ( 0  3) */		fmuld	%f8,%f22,%f28
/* 0x0260	     ( 0  3) */		ldd	[%g2],%f24
/* 0x0264	     ( 0  3) */		faddd	%f0,%f4,%f26
/* 0x0268	     ( 1  4) */		fmuld	%f12,%f20,%f8
/* 0x026c	     ( 1  2) */		add	%l1,32,%l1
/* 0x0270	     ( 1  2) */		cmp	%o1,%g3
/* 0x0274	     ( 2  5) */		fmuld	%f14,%f20,%f14
/* 0x0278	     ( 2  5) */		ldd	[%l1-32],%f4
/* 0x027c	     ( 2  3) */		add	%g2,8,%g2
/* 0x0280	     ( 3  6) */		faddd	%f10,%f28,%f12
/* 0x0284	     ( 3  6) */		fmuld	%f6,%f22,%f6
/* 0x0288	     ( 3  6) */		ldd	[%l1-16],%f0
/* 0x028c	     ( 4  7) */		fmuld	%f24,%f22,%f10
/* 0x0290	     ( 4  5) */		std	%f26,[%l1-64]
/* 0x0294	     ( 6  9) */		faddd	%f2,%f12,%f2
/* 0x0298	     ( 6  7) */		std	%f2,[%l1-48]
/* 0x029c	     ( 7 10) */		faddd	%f14,%f6,%f6
/* 0x02a0	     ( 8 11) */		faddd	%f8,%f10,%f2
/* 0x02a4	     (10 13) */		faddd	%f4,%f6,%f4
/* 0x02a8	     (10 11) */		std	%f4,[%l1-32]
/* 0x02ac	     (11 14) */		faddd	%f0,%f2,%f0
/* 0x02b0	     (11 12) */		bg,pn	%icc,.L77000213	! tprob=0.13
/* 0x02b4	     (11 12) */		std	%f0,[%l1-16]
                                   .L77000281:		/* frequency 4.0 confidence 0.0 */
/* 0x02b8	 216 ( 0  3) */		ldd	[%o0],%f0
                                   .L900000752:		/* frequency 36.6 confidence 0.0 */
/* 0x02bc	 216 ( 0  3) */		ldd	[%g2],%f4
/* 0x02c0	     ( 0  3) */		fmuld	%f0,%f20,%f2
/* 0x02c4	     ( 0  1) */		add	%o1,1,%o1
/* 0x02c8	     ( 1  4) */		ldd	[%l1],%f0
/* 0x02cc	     ( 1  2) */		add	%o0,8,%o0
/* 0x02d0	     ( 1  2) */		add	%g2,8,%g2
/* 0x02d4	     ( 2  5) */		fmuld	%f4,%f22,%f4
/* 0x02d8	     ( 2  3) */		cmp	%o1,%g3
/* 0x02dc	     ( 5  8) */		faddd	%f2,%f4,%f2
/* 0x02e0	     ( 8 11) */		faddd	%f0,%f2,%f0
/* 0x02e4	     ( 8  9) */		std	%f0,[%l1]
/* 0x02e8	     ( 8  9) */		add	%l1,16,%l1
/* 0x02ec	     ( 8  9) */		ble,a,pt	%icc,.L900000752	! tprob=0.87
/* 0x02f0	     (10 13) */		ldd	[%o0],%f0
                                   .L77000213:		/* frequency 5.3 confidence 0.0 */
/* 0x02f4	     ( 0  1) */		srl	%i5,31,%g2
                                   .L900000753:		/* frequency 5.3 confidence 0.0 */
/* 0x02f8	 218 ( 0  1) */		cmp	%l0,30
/* 0x02fc	     ( 0  1) */		bne,a,pt	%icc,.L900000751	! tprob=0.54
/* 0x0300	     ( 0  3) */		fdtox	%f18,%f0
/* 0x0304	     ( 1  2) */		add	%i5,%g2,%g2
/* 0x0308	     ( 1  2) */		sub	%o3,1,%o2
/* 0x030c	     ( 2  3) */		sra	%g2,1,%o0
/* 0x0310	 216 ( 2  5) */		ldd	[%g4],%f0
/* 0x0314	     ( 3  4) */		add	%o0,1,%g2
/* 0x0318	     ( 4  5) */		sll	%g2,1,%o0
/* 0x031c	     ( 4  5) */		fmovd	%f0,%f2
/* 0x0320	     ( 5  6) */		sll	%g2,4,%o1
/* 0x0324	     ( 5  6) */		cmp	%o0,%o3
/* 0x0328	     ( 5  6) */		bge,pt	%icc,.L77000215	! tprob=0.53
/* 0x032c	     ( 6  7) */		or	%g0,0,%l0
/* 0x0330	 218 ( 6  7) */		add	%g5,%o1,%o1
/* 0x0334	 216 ( 7 10) */		ldd	[%o1],%f8
                                   .L900000750:		/* frequency 32.0 confidence 0.0 */
/* 0x0338	     ( 0  3) */		fdtox	%f8,%f6
/* 0x033c	     ( 0  3) */		ldd	[%g4],%f10
/* 0x0340	     ( 0  1) */		add	%o0,2,%o0
/* 0x0344	     ( 1  4) */		ldd	[%o1+8],%f4
/* 0x0348	     ( 1  4) */		fdtox	%f8,%f8
/* 0x034c	     ( 1  2) */		cmp	%o0,%o2
/* 0x0350	     ( 5  6) */		fmovs	%f10,%f6
/* 0x0354	     ( 7 10) */		fxtod	%f6,%f10
/* 0x0358	     ( 8 11) */		fdtox	%f4,%f6
/* 0x035c	     ( 9 12) */		fdtox	%f4,%f4
/* 0x0360	     (10 13) */		faddd	%f10,%f2,%f2
/* 0x0364	     (10 11) */		std	%f2,[%o1]
/* 0x0368	     (12 15) */		ldd	[%g4],%f2
/* 0x036c	     (14 15) */		fmovs	%f2,%f6
/* 0x0370	     (16 19) */		fxtod	%f6,%f6
/* 0x0374	     (17 20) */		fitod	%f8,%f2
/* 0x0378	     (19 22) */		faddd	%f6,%f0,%f0
/* 0x037c	     (19 20) */		std	%f0,[%o1+8]
/* 0x0380	     (19 20) */		add	%o1,16,%o1
/* 0x0384	     (20 23) */		fitod	%f4,%f0
/* 0x0388	     (20 21) */		ble,a,pt	%icc,.L900000750	! tprob=0.87
/* 0x038c	     (20 23) */		ldd	[%o1],%f8
                                   .L77000233:		/* frequency 4.6 confidence 0.0 */
/* 0x0390	     ( 0  0) */		or	%g0,0,%l0
                                   .L77000215:		/* frequency 5.3 confidence 0.0 */
/* 0x0394	     ( 0  3) */		fdtox	%f18,%f0
                                   .L900000751:		/* frequency 5.3 confidence 0.0 */
/* 0x0398	     ( 0  3) */		ldd	[%g4],%f6
/* 0x039c	 220 ( 0  1) */		add	%i5,1,%i5
/* 0x03a0	     ( 0  1) */		add	%i4,8,%i4
/* 0x03a4	     ( 1  4) */		ldd	[%g4-8],%f2
/* 0x03a8	     ( 1  2) */		add	%l0,1,%l0
/* 0x03ac	     ( 1  2) */		add	%i3,8,%i3
/* 0x03b0	     ( 2  3) */		fmovs	%f6,%f0
/* 0x03b4	     ( 2  5) */		ldd	[%g4-16],%f4
/* 0x03b8	     ( 2  3) */		cmp	%i5,%o5
/* 0x03bc	     ( 4  7) */		fxtod	%f0,%f0
/* 0x03c0	     ( 7 10) */		fmuld	%f0,%f16,%f0
/* 0x03c4	     (10 13) */		fmuld	%f0,%f2,%f2
/* 0x03c8	     (13 16) */		fdtox	%f2,%f2
/* 0x03cc	     (16 19) */		fxtod	%f2,%f2
/* 0x03d0	     (19 22) */		fmuld	%f2,%f4,%f2
/* 0x03d4	     (22 25) */		fsubd	%f0,%f2,%f22
/* 0x03d8	     (22 23) */		ble,a,pt	%icc,.L900000749	! tprob=0.89
/* 0x03dc	     (22 25) */		ldd	[%i2],%f0
                                   .L900000725:		/* frequency 0.7 confidence 0.0 */
/* 0x03e0	 220 ( 0  1) */		ba	.L900000748	! tprob=1.00
/* 0x03e4	     ( 0  1) */		sll	%g1,4,%g2

	
                                   .L77000289:		/* frequency 0.8 confidence 0.0 */
/* 0x03e8	 225 ( 0  3) */		ldd	[%o1],%f6
/* 0x03ec	 242 ( 0  1) */		add	%g4,-8,%g2
/* 0x03f0	     ( 0  1) */		add	%g4,-16,%g3
/* 0x03f4	 225 ( 1  4) */		ldd	[%i1],%f2
/* 0x03f8	 245 ( 1  2) */		or	%g0,0,%o3
/* 0x03fc	     ( 1  2) */		or	%g0,0,%o0
/* 0x0400	 225 ( 3  6) */		fmuld	%f2,%f6,%f2
/* 0x0404	     ( 3  4) */		std	%f2,[%o2]
/* 0x0408	     ( 4  7) */		ldd	[%g4],%f6
/* 0x040c	 237 ( 7  8) */		std	%f6,[%o2+8]
/* 0x0410	     ( 8  9) */		std	%f6,[%o2+16]
/* 0x0414	     ( 9 10) */		std	%f6,[%o2+24]
/* 0x0418	     (10 11) */		std	%f6,[%o2+32]
/* 0x041c	     (11 12) */		std	%f6,[%o2+40]
/* 0x0420	     (12 13) */		std	%f6,[%o2+48]
/* 0x0424	     (13 14) */		std	%f6,[%o2+56]
/* 0x0428	     (14 15) */		std	%f6,[%o2+64]
/* 0x042c	     (15 16) */		std	%f6,[%o2+72]
!	prefetch	[%i4],0
!	prefetch	[%i4+32],0
!	prefetch	[%i4+64],0
!	prefetch	[%i4+96],0
!	prefetch	[%i4+120],0
!	prefetch	[%i1],0
!	prefetch	[%i1+32],0
!	prefetch	[%i1+64],0
!	prefetch	[%i1+96],0
!	prefetch	[%i1+120],0
/* 0x0430	     (16 17) */		std	%f6,[%o2+80]
/* 0x0434	     (17 18) */		std	%f6,[%o2+88]
/* 0x0438	     (18 19) */		std	%f6,[%o2+96]
/* 0x043c	     (19 20) */		std	%f6,[%o2+104]
/* 0x0440	     (20 21) */		std	%f6,[%o2+112]
/* 0x0444	     (21 22) */		std	%f6,[%o2+120]
/* 0x0448	     (22 23) */		std	%f6,[%o2+128]
/* 0x044c	     (23 24) */		std	%f6,[%o2+136]
/* 0x0450	     (24 25) */		std	%f6,[%o2+144]
/* 0x0454	     (25 26) */		std	%f6,[%o2+152]
/* 0x0458	     (26 27) */		std	%f6,[%o2+160]
/* 0x045c	     (27 28) */		std	%f6,[%o2+168]
/* 0x0460	     (27 30) */		fdtox	%f2,%f2
/* 0x0464	     (28 29) */		std	%f6,[%o2+176]
/* 0x0468	     (29 30) */		std	%f6,[%o2+184]
/* 0x046c	     (30 31) */		std	%f6,[%o2+192]
/* 0x0470	     (31 32) */		std	%f6,[%o2+200]
/* 0x0474	     (32 33) */		std	%f6,[%o2+208]
/* 0x0478	     (33 34) */		std	%f6,[%o2+216]
/* 0x047c	     (34 35) */		std	%f6,[%o2+224]
/* 0x0480	     (35 36) */		std	%f6,[%o2+232]
/* 0x0484	     (36 37) */		std	%f6,[%o2+240]
/* 0x0488	     (37 38) */		std	%f6,[%o2+248]
/* 0x048c	     (38 39) */		std	%f6,[%o2+256]
/* 0x0490	     (39 40) */		std	%f6,[%o2+264]
/* 0x0494	     (40 41) */		std	%f6,[%o2+272]
/* 0x0498	     (41 42) */		std	%f6,[%o2+280]
/* 0x049c	     (42 43) */		std	%f6,[%o2+288]
/* 0x04a0	     (43 44) */		std	%f6,[%o2+296]
/* 0x04a4	     (44 45) */		std	%f6,[%o2+304]
/* 0x04a8	     (45 46) */		std	%f6,[%o2+312]
/* 0x04ac	     (46 47) */		std	%f6,[%o2+320]
/* 0x04b0	     (47 48) */		std	%f6,[%o2+328]
/* 0x04b4	     (48 49) */		std	%f6,[%o2+336]
/* 0x04b8	     (49 50) */		std	%f6,[%o2+344]
/* 0x04bc	     (50 51) */		std	%f6,[%o2+352]
/* 0x04c0	     (51 52) */		std	%f6,[%o2+360]
/* 0x04c4	     (52 53) */		std	%f6,[%o2+368]
/* 0x04c8	     (53 54) */		std	%f6,[%o2+376]
/* 0x04cc	     (54 55) */		std	%f6,[%o2+384]
/* 0x04d0	     (55 56) */		std	%f6,[%o2+392]
/* 0x04d4	     (56 57) */		std	%f6,[%o2+400]
/* 0x04d8	     (57 58) */		std	%f6,[%o2+408]
/* 0x04dc	     (58 59) */		std	%f6,[%o2+416]
/* 0x04e0	     (59 60) */		std	%f6,[%o2+424]
/* 0x04e4	     (60 61) */		std	%f6,[%o2+432]
/* 0x04e8	     (61 62) */		std	%f6,[%o2+440]
/* 0x04ec	     (62 63) */		std	%f6,[%o2+448]
/* 0x04f0	     (63 64) */		std	%f6,[%o2+456]
/* 0x04f4	     (64 65) */		std	%f6,[%o2+464]
/* 0x04f8	     (65 66) */		std	%f6,[%o2+472]
/* 0x04fc	     (66 67) */		std	%f6,[%o2+480]
/* 0x0500	     (67 68) */		std	%f6,[%o2+488]
/* 0x0504	     (68 69) */		std	%f6,[%o2+496]
/* 0x0508	     (69 70) */		std	%f6,[%o2+504]
/* 0x050c	     (70 71) */		std	%f6,[%o2+512]
/* 0x0510	     (71 72) */		std	%f6,[%o2+520]
/* 0x0514	 242 (72 75) */		ld	[%g4],%f2 ! dalign
/* 0x0518	     (73 76) */		ld	[%g2],%f6 ! dalign
/* 0x051c	     (74 77) */		fxtod	%f2,%f10
/* 0x0520	     (74 77) */		ld	[%g2+4],%f7
/* 0x0524	     (75 78) */		ld	[%g3],%f8 ! dalign
/* 0x0528	     (76 79) */		ld	[%g3+4],%f9
/* 0x052c	     (77 80) */		fmuld	%f10,%f0,%f0
/* 0x0530	 239 (77 80) */		ldd	[%i4],%f4
/* 0x0534	 240 (78 81) */		ldd	[%i1],%f2
/* 0x0538	     (80 83) */		fmuld	%f0,%f6,%f6
/* 0x053c	     (83 86) */		fdtox	%f6,%f6
/* 0x0540	     (86 89) */		fxtod	%f6,%f6
/* 0x0544	     (89 92) */		fmuld	%f6,%f8,%f6
/* 0x0548	     (92 95) */		fsubd	%f0,%f6,%f0
/* 0x054c	 250 (95 98) */		fmuld	%f4,%f0,%f10
                                   .L900000747:		/* frequency 6.4 confidence 0.0 */


	fmovd %f0,%f0
	fmovd %f16,%f18
	ldd [%i4],%f2
	ldd [%o2],%f8
	ldd [%i1],%f10
	ldd [%g4-8],%f14
	ldd [%g4-16],%f16
	ldd [%o1],%f24

	ldd [%i1+8],%f26
	ldd [%i1+16],%f40
	ldd [%i1+48],%f46
	ldd [%i1+56],%f30
	ldd [%i1+64],%f54
	ldd [%i1+104],%f34
	ldd [%i1+112],%f58

	ldd [%i4+112],%f60
	ldd [%i4+8],%f28	
	ldd [%i4+104],%f38

	nop
	nop
!
	.L99999999:
!1
!!!
	ldd	[%i1+24],%f32
	fmuld	%f0,%f2,%f4
!2
!!!
	ldd	[%i4+24],%f36
	fmuld	%f26,%f24,%f20
!3
!!!
	ldd	[%i1+40],%f42
	fmuld	%f28,%f0,%f22
!4
!!!
	ldd	[%i4+40],%f44
	fmuld	%f32,%f24,%f32
!5
!!!
	ldd	[%o1+8],%f6
	faddd	%f4,%f8,%f4
	fmuld	%f36,%f0,%f36
!6
!!!
	add	%o1,8,%o1
	ldd	[%i4+56],%f50
	fmuld	%f42,%f24,%f42
!7
!!!
	ldd	[%i1+72],%f52
	faddd	%f20,%f22,%f20
	fmuld	%f44,%f0,%f44
!8
!!!
	ldd	[%o2+16],%f22
	fmuld	%f10,%f6,%f12
!9
!!!
	ldd	[%i4+72],%f56
	faddd	%f32,%f36,%f32
	fmuld	%f14,%f4,%f4
!10
!!!
	ldd	[%o2+48],%f36
	fmuld	%f30,%f24,%f48
!11
!!!
	ldd	[%o2+8],%f8
	faddd	%f20,%f22,%f20
	fmuld	%f50,%f0,%f50	
!12
!!!
	std	%f20,[%o2+16]
	faddd	%f42,%f44,%f42
	fmuld	%f52,%f24,%f52
!13
!!!
	ldd	[%o2+80],%f44
	faddd	%f4,%f12,%f4
	fmuld	%f56,%f0,%f56
!14
!!!
	ldd	[%i1+88],%f20
	faddd	%f32,%f36,%f32
!15
!!!
	ldd	[%i4+88],%f22
	faddd	%f48,%f50,%f48
!16
!!!
	ldd	[%o2+112],%f50
	faddd	%f52,%f56,%f52
!17
!!!
	ldd	[%o2+144],%f56
	faddd	%f4,%f8,%f8
	fmuld	%f20,%f24,%f20
!18
!!!
	std	%f32,[%o2+48]
	faddd	%f42,%f44,%f42
	fmuld	%f22,%f0,%f22
!19
!!!
	std	%f42,[%o2+80]
	faddd	%f48,%f50,%f48
	fmuld	%f34,%f24,%f32
!20
!!!
	std	%f48,[%o2+112]
	faddd	%f52,%f56,%f52
	fmuld	%f38,%f0,%f36
!21
!!!
	ldd	[%i1+120],%f42
	fdtox	%f8,%f4
!22
!!!
	std	%f52,[%o2+144]
	faddd	%f20,%f22,%f20
!23
!!!
	ldd	[%i4+120],%f44
!24
!!!
	ldd	[%o2+176],%f22
	faddd	%f32,%f36,%f32
	fmuld	%f42,%f24,%f42
!25
!!!
	ldd	[%i4+16],%f50
	fmovs	%f17,%f4
!26
!!!
	ldd	[%i1+32],%f52
	fmuld	%f44,%f0,%f44
!27
!!!
	ldd	[%i4+32],%f56
	fmuld	%f40,%f24,%f48
!28
!!!
	ldd	[%o2+208],%f36
	faddd	%f20,%f22,%f20
	fmuld	%f50,%f0,%f50
!29
!!!
	std	%f20,[%o2+176]
	fxtod	%f4,%f4
	fmuld	%f52,%f24,%f52
!30
!!!
	ldd	[%i4+48],%f22
	faddd	%f42,%f44,%f42
	fmuld	%f56,%f0,%f56
!31
!!!
	ldd	[%o2+240],%f44
	faddd	%f32,%f36,%f32
!32
!!!
	std	%f32,[%o2+208]
	faddd	%f48,%f50,%f48
	fmuld	%f46,%f24,%f20
!33
!!!
	ldd	[%o2+32],%f50
	fmuld	%f4,%f18,%f12
!34
!!!
	ldd	[%i4+64],%f36
	faddd	%f52,%f56,%f52
	fmuld	%f22,%f0,%f22
!35
!!!
	ldd	[%o2+64],%f56
	faddd	%f42,%f44,%f42
!36
!!!
	std	%f42,[%o2+240]
	faddd	%f48,%f50,%f48
	fmuld	%f54,%f24,%f32
!37
!!!
	std	%f48,[%o2+32]
	fmuld	%f12,%f14,%f4
!38
!!!
	ldd	[%i1+80],%f42
	faddd	%f52,%f56,%f56	! yes, tmp52!
	fmuld	%f36,%f0,%f36
!39
!!!
	ldd	[%i4+80],%f44
	faddd	%f20,%f22,%f20
!40
!!!
	ldd	[%i1+96],%f48
	fmuld	%f58,%f24,%f52
!41
!!!
	ldd	[%i4+96],%f50
	fdtox	%f4,%f4
	fmuld	%f42,%f24,%f42
!42
!!!
	std	%f56,[%o2+64]	! yes, tmp52!
	faddd	%f32,%f36,%f32
	fmuld	%f44,%f0,%f44
!43
!!!
	ldd	[%o2+96],%f22
	fmuld	%f48,%f24,%f48
!44
!!!
	ldd	[%o2+128],%f36
	fmovd	%f6,%f24
	fmuld	%f50,%f0,%f50
!45
!!!
	fxtod	%f4,%f4
	fmuld	%f60,%f0,%f56
!46
!!!
	add	%o2,8,%o2
	faddd	%f42,%f44,%f42
!47
!!!
	ldd	[%o2+160-8],%f44
	faddd	%f20,%f22,%f20
!48
!!!
	std	%f20,[%o2+96-8]
	faddd	%f48,%f50,%f48
!49
!!!
	ldd	[%o2+192-8],%f50
	faddd	%f52,%f56,%f52
	fmuld	%f4,%f16,%f4
!50
!!!
	ldd	[%o2+224-8],%f56
	faddd	%f32,%f36,%f32
!51
!!!
	std	%f32,[%o2+128-8]
	faddd	%f42,%f44,%f42
!52
	add	%o3,1,%o3
	std	%f42,[%o2+160-8]
	faddd	%f48,%f50,%f48
!53
!!!
	cmp	%o3,31
	std	%f48,[%o2+192-8]
	faddd	%f52,%f56,%f52
!54
	std	%f52,[%o2+224-8]
	ble,pt	%icc,.L99999999
	fsubd	%f12,%f4,%f0



!55
	std %f8,[%o2]

	
	
	
	
	
	                                   .L77000285:		/* frequency 1.0 confidence 0.0 */
/* 0x07a8	 279 ( 0  1) */		sll	%g1,4,%g2
                                   .L900000748:		/* frequency 1.0 confidence 0.0 */
/* 0x07ac	 279 ( 0  3) */		ldd	[%g5+%g2],%f0
/* 0x07b0	     ( 0  1) */		add	%g5,%g2,%i1
/* 0x07b4	     ( 0  1) */		or	%g0,0,%o4
/* 0x07b8	 206 ( 1  4) */		ld	[%fp+68],%o0
/* 0x07bc	 279 ( 1  2) */		or	%g0,0,%i0
/* 0x07c0	     ( 1  2) */		cmp	%g1,0
/* 0x07c4	     ( 2  5) */		fdtox	%f0,%f0
/* 0x07c8	     ( 2  3) */		std	%f0,[%sp+120]
/* 0x07cc	 275 ( 2  3) */		sethi	%hi(0xfc00),%o1
/* 0x07d0	 206 ( 3  4) */		or	%g0,%o0,%o3
/* 0x07d4	 275 ( 3  4) */		sub	%g1,1,%g4
/* 0x07d8	 279 ( 4  7) */		ldd	[%i1+8],%f0
/* 0x07dc	     ( 4  5) */		or	%g0,%o0,%g5
/* 0x07e0	     ( 4  5) */		add	%o1,1023,%o1
/* 0x07e4	     ( 6  9) */		fdtox	%f0,%f0
/* 0x07e8	     ( 6  7) */		std	%f0,[%sp+112]
/* 0x07ec	     (10 12) */		ldx	[%sp+112],%o5
/* 0x07f0	     (11 13) */		ldx	[%sp+120],%o7
/* 0x07f4	     (11 12) */		ble,pt	%icc,.L900000746	! tprob=0.56
/* 0x07f8	     (11 12) */		sethi	%hi(0xfc00),%g2
/* 0x07fc	 275 (12 13) */		or	%g0,-1,%g2
/* 0x0800	 279 (12 13) */		cmp	%g1,3
/* 0x0804	 275 (13 14) */		srl	%g2,0,%o2
/* 0x0808	 279 (13 14) */		bl,pn	%icc,.L77000286	! tprob=0.44
/* 0x080c	     (13 14) */		or	%g0,%i1,%g2
/* 0x0810	     (14 17) */		ldd	[%i1+16],%f0
/* 0x0814	     (14 15) */		and	%o5,%o1,%o0
/* 0x0818	     (14 15) */		add	%i1,16,%g2
/* 0x081c	     (15 16) */		sllx	%o0,16,%g3
/* 0x0820	     (15 16) */		and	%o7,%o2,%o0
/* 0x0824	     (16 19) */		fdtox	%f0,%f0
/* 0x0828	     (16 17) */		std	%f0,[%sp+104]
/* 0x082c	     (16 17) */		add	%o0,%g3,%o4
/* 0x0830	     (17 20) */		ldd	[%i1+24],%f2
/* 0x0834	     (17 18) */		srax	%o5,16,%o0
/* 0x0838	     (17 18) */		add	%o3,4,%g5
/* 0x083c	     (18 19) */		stx	%o0,[%sp+128]
/* 0x0840	     (18 19) */		and	%o4,%o2,%o0
/* 0x0844	     (18 19) */		or	%g0,1,%i0
/* 0x0848	     (19 20) */		stx	%o0,[%sp+112]
/* 0x084c	     (19 20) */		srax	%o4,32,%o0
/* 0x0850	     (19 22) */		fdtox	%f2,%f0
/* 0x0854	     (20 21) */		stx	%o0,[%sp+136]
/* 0x0858	     (20 21) */		srax	%o7,32,%o4
/* 0x085c	     (21 22) */		std	%f0,[%sp+96]
/* 0x0860	     (22 24) */		ldx	[%sp+136],%o7
/* 0x0864	     (23 25) */		ldx	[%sp+128],%o0
/* 0x0868	     (25 27) */		ldx	[%sp+104],%g3
/* 0x086c	     (25 26) */		add	%o0,%o7,%o0
/* 0x0870	     (26 28) */		ldx	[%sp+112],%o7
/* 0x0874	     (26 27) */		add	%o4,%o0,%o4
/* 0x0878	     (27 29) */		ldx	[%sp+96],%o5
/* 0x087c	     (28 29) */		st	%o7,[%o3]
/* 0x0880	     (28 29) */		or	%g0,%g3,%o7
                                   .L900000730:		/* frequency 64.0 confidence 0.0 */
/* 0x0884	     (17 19) */		ldd	[%g2+16],%f0
/* 0x0888	     (17 18) */		add	%i0,1,%i0
/* 0x088c	     (17 18) */		add	%g5,4,%g5
/* 0x0890	     (18 18) */		cmp	%i0,%g4
/* 0x0894	     (18 19) */		add	%g2,16,%g2
/* 0x0898	     (19 22) */		fdtox	%f0,%f0
/* 0x089c	     (20 21) */		std	%f0,[%sp+104]
/* 0x08a0	     (21 23) */		ldd	[%g2+8],%f0
/* 0x08a4	     (23 26) */		fdtox	%f0,%f0
/* 0x08a8	     (24 25) */		std	%f0,[%sp+96]
/* 0x08ac	     (25 26) */		and	%o5,%o1,%g3
/* 0x08b0	     (26 27) */		sllx	%g3,16,%g3
/* 0x08b4	     ( 0  0) */		stx	%g3,[%sp+120]
/* 0x08b8	     (26 27) */		and	%o7,%o2,%g3
/* 0x08bc	     ( 0  0) */		stx	%o7,[%sp+128]
/* 0x08c0	     ( 0  0) */		ldx	[%sp+120],%o7
/* 0x08c4	     (27 27) */		add	%g3,%o7,%g3
/* 0x08c8	     ( 0  0) */		ldx	[%sp+128],%o7
/* 0x08cc	     (28 29) */		srax	%o5,16,%o5
/* 0x08d0	     (28 28) */		add	%g3,%o4,%g3
/* 0x08d4	     (29 30) */		srax	%g3,32,%o4
/* 0x08d8	     ( 0  0) */		stx	%o4,[%sp+112]
/* 0x08dc	     (30 31) */		srax	%o7,32,%o4
/* 0x08e0	     ( 0  0) */		ldx	[%sp+112],%o7
/* 0x08e4	     (30 31) */		add	%o5,%o7,%o7
/* 0x08e8	     (31 33) */		ldx	[%sp+96],%o5
/* 0x08ec	     (31 32) */		add	%o4,%o7,%o4
/* 0x08f0	     (32 33) */		and	%g3,%o2,%g3
/* 0x08f4	     ( 0  0) */		ldx	[%sp+104],%o7
/* 0x08f8	     (33 34) */		ble,pt	%icc,.L900000730	! tprob=0.50
/* 0x08fc	     (33 34) */		st	%g3,[%g5-4]
                                   .L900000733:		/* frequency 8.0 confidence 0.0 */
/* 0x0900	     ( 0  1) */		ba	.L900000746	! tprob=1.00
/* 0x0904	     ( 0  1) */		sethi	%hi(0xfc00),%g2
                                   .L77000286:		/* frequency 0.7 confidence 0.0 */
/* 0x0908	     ( 0  3) */		ldd	[%g2+16],%f0
                                   .L900000745:		/* frequency 6.4 confidence 0.0 */
/* 0x090c	     ( 0  1) */		and	%o7,%o2,%o0
/* 0x0910	     ( 0  1) */		and	%o5,%o1,%g3
/* 0x0914	     ( 0  3) */		fdtox	%f0,%f0
/* 0x0918	     ( 1  2) */		add	%o4,%o0,%o0
/* 0x091c	     ( 1  2) */		std	%f0,[%sp+104]
/* 0x0920	     ( 1  2) */		add	%i0,1,%i0
/* 0x0924	     ( 2  3) */		sllx	%g3,16,%o4
/* 0x0928	     ( 2  5) */		ldd	[%g2+24],%f2
/* 0x092c	     ( 2  3) */		add	%g2,16,%g2
/* 0x0930	     ( 3  4) */		add	%o0,%o4,%o4
/* 0x0934	     ( 3  4) */		cmp	%i0,%g4
/* 0x0938	     ( 4  5) */		srax	%o5,16,%o0
/* 0x093c	     ( 4  5) */		stx	%o0,[%sp+112]
/* 0x0940	     ( 4  5) */		and	%o4,%o2,%g3
/* 0x0944	     ( 5  6) */		srax	%o4,32,%o5
/* 0x0948	     ( 5  8) */		fdtox	%f2,%f0
/* 0x094c	     ( 5  6) */		std	%f0,[%sp+96]
/* 0x0950	     ( 6  7) */		srax	%o7,32,%o4
/* 0x0954	     ( 6  8) */		ldx	[%sp+112],%o7
/* 0x0958	     ( 8  9) */		add	%o7,%o5,%o7
/* 0x095c	     ( 9 11) */		ldx	[%sp+104],%o5
/* 0x0960	     ( 9 10) */		add	%o4,%o7,%o4
/* 0x0964	     (10 12) */		ldx	[%sp+96],%o0
/* 0x0968	     (11 12) */		st	%g3,[%g5]
/* 0x096c	     (11 12) */		or	%g0,%o5,%o7
/* 0x0970	     (11 12) */		add	%g5,4,%g5
/* 0x0974	     (12 13) */		or	%g0,%o0,%o5
/* 0x0978	     (12 13) */		ble,a,pt	%icc,.L900000745	! tprob=0.86
/* 0x097c	     (12 15) */		ldd	[%g2+16],%f0
                                   .L77000236:		/* frequency 1.0 confidence 0.0 */
/* 0x0980	     ( 0  1) */		sethi	%hi(0xfc00),%g2
                                   .L900000746:		/* frequency 1.0 confidence 0.0 */
/* 0x0984	     ( 0  1) */		or	%g0,-1,%o0
/* 0x0988	     ( 0  1) */		add	%g2,1023,%g2
/* 0x098c	     ( 0  3) */		ld	[%fp+88],%o1
/* 0x0990	     ( 1  2) */		srl	%o0,0,%g3
/* 0x0994	     ( 1  2) */		and	%o5,%g2,%g2
/* 0x0998	     ( 2  3) */		and	%o7,%g3,%g4
/* 0x099c	 281 ( 2  3) */		or	%g0,-1,%o5
/* 0x09a0	 275 ( 3  4) */		sllx	%g2,16,%g2
/* 0x09a4	     ( 3  4) */		add	%o4,%g4,%g4
/* 0x09a8	     ( 4  5) */		add	%g4,%g2,%g2
/* 0x09ac	     ( 5  6) */		sll	%i0,2,%g4
/* 0x09b0	     ( 5  6) */		and	%g2,%g3,%g2
/* 0x09b4	     ( 6  7) */		st	%g2,[%o3+%g4]
/* 0x09b8	 281 ( 6  7) */		sll	%g1,2,%g2
/* 0x09bc	     ( 7 10) */		ld	[%o3+%g2],%g2
/* 0x09c0	     ( 9 10) */		cmp	%g2,0
/* 0x09c4	     ( 9 10) */		bleu,pn	%icc,.L77000241	! tprob=0.50
/* 0x09c8	     ( 9 10) */		or	%g0,%o1,%o2
/* 0x09cc	     (10 11) */		ba	.L900000744	! tprob=1.00
/* 0x09d0	     (10 11) */		cmp	%o5,0
                                   .L77000241:		/* frequency 0.8 confidence 0.0 */
/* 0x09d4	     ( 0  1) */		subcc	%g1,1,%o5
/* 0x09d8	     ( 0  1) */		bneg,pt	%icc,.L900000744	! tprob=0.60
/* 0x09dc	     ( 1  2) */		cmp	%o5,0
/* 0x09e0	     ( 1  2) */		sll	%o5,2,%g2
/* 0x09e4	     ( 2  3) */		add	%o1,%g2,%o0
/* 0x09e8	     ( 2  3) */		add	%o3,%g2,%o4
/* 0x09ec	     ( 3  6) */		ld	[%o0],%g2
                                   .L900000743:		/* frequency 5.3 confidence 0.0 */
/* 0x09f0	     ( 0  3) */		ld	[%o4],%g3
/* 0x09f4	     ( 0  1) */		add	%o0,4,%o0
/* 0x09f8	     ( 0  1) */		add	%o4,4,%o4
/* 0x09fc	     ( 2  3) */		cmp	%g3,%g2
/* 0x0a00	     ( 2  3) */		bne,pn	%icc,.L77000244	! tprob=0.16
/* 0x0a04	     ( 2  3) */		nop
/* 0x0a08	     ( 3  4) */		addcc	%o5,1,%o5
/* 0x0a0c	     ( 3  4) */		bpos,a,pt	%icc,.L900000743	! tprob=0.84
/* 0x0a10	     ( 3  6) */		ld	[%o0],%g2
                                   .L77000244:		/* frequency 1.0 confidence 0.0 */
/* 0x0a14	     ( 0  1) */		cmp	%o5,0
                                   .L900000744:		/* frequency 1.0 confidence 0.0 */
/* 0x0a18	     ( 0  1) */		bl,pn	%icc,.L77000287	! tprob=0.50
/* 0x0a1c	     ( 0  1) */		sll	%o5,2,%g2
/* 0x0a20	     ( 1  4) */		ld	[%o2+%g2],%g3
/* 0x0a24	     ( 2  5) */		ld	[%o3+%g2],%g2
/* 0x0a28	     ( 4  5) */		cmp	%g2,%g3
/* 0x0a2c	     ( 4  5) */		bleu,pt	%icc,.L77000224	! tprob=0.56
/* 0x0a30	     ( 4  5) */		nop
                                   .L77000287:		/* frequency 0.8 confidence 0.0 */
/* 0x0a34	     ( 0  1) */		cmp	%g1,0
/* 0x0a38	     ( 0  1) */		ble,pt	%icc,.L77000224	! tprob=0.60
/* 0x0a3c	     ( 0  1) */		nop
/* 0x0a40	 281 ( 1  2) */		sub	%g1,1,%o7
/* 0x0a44	     ( 1  2) */		or	%g0,-1,%g2
/* 0x0a48	     ( 2  3) */		srl	%g2,0,%o4
/* 0x0a4c	     ( 2  3) */		add	%o7,1,%o0
/* 0x0a50	 279 ( 3  4) */		or	%g0,0,%o5
/* 0x0a54	     ( 3  4) */		or	%g0,0,%g1
/* 0x0a58	     ( 4  5) */		cmp	%o0,3
/* 0x0a5c	     ( 4  5) */		bl,pn	%icc,.L77000288	! tprob=0.40
/* 0x0a60	     ( 4  5) */		add	%o3,8,%o1
/* 0x0a64	     ( 5  6) */		add	%o2,4,%o0
/* 0x0a68	     ( 5  8) */		ld	[%o1-8],%g2
/* 0x0a6c	   0 ( 5  6) */		or	%g0,%o1,%o3
/* 0x0a70	 279 ( 6  9) */		ld	[%o0-4],%g3
/* 0x0a74	   0 ( 6  7) */		or	%g0,%o0,%o2
/* 0x0a78	 279 ( 6  7) */		or	%g0,2,%g1
/* 0x0a7c	     ( 7 10) */		ld	[%o3-4],%o0
/* 0x0a80	     ( 8  9) */		sub	%g2,%g3,%g2
/* 0x0a84	     ( 9 10) */		or	%g0,%g2,%o5
/* 0x0a88	     ( 9 10) */		and	%g2,%o4,%g2
/* 0x0a8c	     ( 9 10) */		st	%g2,[%o3-8]
/* 0x0a90	     (10 11) */		srax	%o5,32,%o5
                                   .L900000734:		/* frequency 64.0 confidence 0.0 */
/* 0x0a94	     (12 20) */		ld	[%o2],%g2
/* 0x0a98	     (12 13) */		add	%g1,1,%g1
/* 0x0a9c	     (12 13) */		add	%o2,4,%o2
/* 0x0aa0	     (13 13) */		cmp	%g1,%o7
/* 0x0aa4	     (13 14) */		add	%o3,4,%o3
/* 0x0aa8	     (14 14) */		sub	%o0,%g2,%o0
/* 0x0aac	     (15 15) */		add	%o0,%o5,%o5
/* 0x0ab0	     (16 17) */		and	%o5,%o4,%g2
/* 0x0ab4	     (16 24) */		ld	[%o3-4],%o0
/* 0x0ab8	     (17 18) */		st	%g2,[%o3-8]
/* 0x0abc	     (17 18) */		ble,pt	%icc,.L900000734	! tprob=0.50
/* 0x0ac0	     (17 18) */		srax	%o5,32,%o5
                                   .L900000737:		/* frequency 8.0 confidence 0.0 */
/* 0x0ac4	     ( 0  3) */		ld	[%o2],%o1
/* 0x0ac8	     ( 2  3) */		sub	%o0,%o1,%o0
/* 0x0acc	     ( 3  4) */		add	%o0,%o5,%o0
/* 0x0ad0	     ( 4  5) */		and	%o0,%o4,%o1
/* 0x0ad4	     ( 4  5) */		st	%o1,[%o3-4]
/* 0x0ad8	     ( 5  7) */		ret	! Result = 
/* 0x0adc	     ( 7  8) */		restore	%g0,%g0,%g0
                                   .L77000288:		/* frequency 0.6 confidence 0.0 */
/* 0x0ae0	     ( 0  3) */		ld	[%o3],%o0
                                   .L900000742:		/* frequency 5.3 confidence 0.0 */
/* 0x0ae4	     ( 0  3) */		ld	[%o2],%o1
/* 0x0ae8	     ( 0  1) */		add	%o5,%o0,%o0
/* 0x0aec	     ( 0  1) */		add	%g1,1,%g1
/* 0x0af0	     ( 1  2) */		add	%o2,4,%o2
/* 0x0af4	     ( 1  2) */		cmp	%g1,%o7
/* 0x0af8	     ( 2  3) */		sub	%o0,%o1,%o0
/* 0x0afc	     ( 3  4) */		and	%o0,%o4,%o1
/* 0x0b00	     ( 3  4) */		st	%o1,[%o3]
/* 0x0b04	     ( 3  4) */		add	%o3,4,%o3
/* 0x0b08	     ( 4  5) */		srax	%o0,32,%o5
/* 0x0b0c	     ( 4  5) */		ble,a,pt	%icc,.L900000742	! tprob=0.84
/* 0x0b10	     ( 4  7) */		ld	[%o3],%o0
                                   .L77000224:		/* frequency 1.0 confidence 0.0 */
/* 0x0b14	     ( 0  2) */		ret	! Result = 
/* 0x0b18	     ( 2  3) */		restore	%g0,%g0,%g0
/* 0x0b1c	   0 ( 0  0) */		.type	mont_mulf_noconv,2
/* 0x0b1c	     ( 0  0) */		.size	mont_mulf_noconv,(.-mont_mulf_noconv)

