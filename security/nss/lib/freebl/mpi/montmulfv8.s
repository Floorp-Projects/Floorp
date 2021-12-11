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
/* 000000	   0 */		.align	4
!
! SUBROUTINE conv_d16_to_i32
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global conv_d16_to_i32
                       conv_d16_to_i32:
/* 000000	     */		save	%sp,-128,%sp
! FILE montmulf.c

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

/* 0x0004	 115 */		ldd	[%i1],%f0
/* 0x0008	 110 */		or	%g0,%i1,%o0

!  116		      ! b=(long long)d16[1];
!  117		      ! for(i=0; i<ilen-1; i++)

/* 0x000c	 117 */		sub	%i3,1,%g2
/* 0x0010	     */		cmp	%g2,0
/* 0x0014	 114 */		or	%g0,0,%o4
/* 0x0018	 115 */		fdtox	%f0,%f0
/* 0x001c	     */		std	%f0,[%sp+120]
/* 0x0020	 117 */		or	%g0,0,%o7
/* 0x0024	 110 */		or	%g0,%i3,%o1
/* 0x0028	     */		sub	%i3,2,%o2
/* 0x002c	 116 */		ldd	[%o0+8],%f0
/* 0x0030	 110 */		sethi	%hi(0xfc00),%o1
/* 0x0034	     */		add	%o2,1,%g3
/* 0x0038	     */		add	%o1,1023,%o1
/* 0x003c	     */		or	%g0,%i0,%o5
/* 0x0040	 116 */		fdtox	%f0,%f0
/* 0x0044	     */		std	%f0,[%sp+112]
/* 0x0048	     */		ldx	[%sp+112],%g1
/* 0x004c	 115 */		ldx	[%sp+120],%g4
/* 0x0050	 117 */		ble,pt	%icc,.L900000117
/* 0x0054	     */		sethi	%hi(0xfc00),%g2
/* 0x0058	 110 */		or	%g0,-1,%g2
/* 0x005c	 117 */		cmp	%g3,3
/* 0x0060	 110 */		srl	%g2,0,%o3
/* 0x0064	 117 */		bl,pn	%icc,.L77000134
/* 0x0068	     */		or	%g0,%o0,%g2

!  118		      !   {
!  119		      !     c=(long long)d16[2*i+2];

/* 0x006c	 119 */		ldd	[%o0+16],%f0

!  120		      !     t1+=a&0xffffffff;
!  121		      !     t=(a>>32);
!  122		      !     d=(long long)d16[2*i+3];
!  123		      !     t1+=(b&0xffff)<<16;
!  124		      !     t+=(b>>16)+(t1>>32);
!  125		      !     i32[i]=t1&0xffffffff;
!  126		      !     t1=t;
!  127		      !     a=c;
!  128		      !     b=d;

/* 0x0070	 128 */		add	%o0,16,%g2
/* 0x0074	 123 */		and	%g1,%o1,%o0
/* 0x0078	     */		sllx	%o0,16,%g3
/* 0x007c	 120 */		and	%g4,%o3,%o0
/* 0x0080	 117 */		add	%o0,%g3,%o4
/* 0x0084	 119 */		fdtox	%f0,%f0
/* 0x0088	     */		std	%f0,[%sp+104]
/* 0x008c	 125 */		and	%o4,%o3,%g5
/* 0x0090	 122 */		ldd	[%g2+8],%f2
/* 0x0094	 128 */		add	%o5,4,%o5
/* 0x0098	 124 */		srax	%o4,32,%o4
/* 0x009c	     */		stx	%o4,[%sp+112]
/* 0x00a0	 122 */		fdtox	%f2,%f0
/* 0x00a4	     */		std	%f0,[%sp+96]
/* 0x00a8	 124 */		srax	%g1,16,%o0
/* 0x00ac	     */		ldx	[%sp+112],%o7
/* 0x00b0	 121 */		srax	%g4,32,%o4
/* 0x00b4	 124 */		add	%o0,%o7,%g4
/* 0x00b8	 128 */		or	%g0,1,%o7
/* 0x00bc	 119 */		ldx	[%sp+104],%g3
/* 0x00c0	 124 */		add	%o4,%g4,%o4
/* 0x00c4	 122 */		ldx	[%sp+96],%g1
/* 0x00c8	 125 */		st	%g5,[%o5-4]
/* 0x00cc	 127 */		or	%g0,%g3,%g4
                       .L900000112:
/* 0x00d0	 119 */		ldd	[%g2+16],%f0
/* 0x00d4	 128 */		add	%o7,1,%o7
/* 0x00d8	     */		add	%o5,4,%o5
/* 0x00dc	     */		cmp	%o7,%o2
/* 0x00e0	     */		add	%g2,16,%g2
/* 0x00e4	 119 */		fdtox	%f0,%f0
/* 0x00e8	     */		std	%f0,[%sp+104]
/* 0x00ec	 122 */		ldd	[%g2+8],%f0
/* 0x00f0	     */		fdtox	%f0,%f0
/* 0x00f4	     */		std	%f0,[%sp+96]
/* 0x00f8	 123 */		and	%g1,%o1,%g3
/* 0x00fc	     */		sllx	%g3,16,%g5
/* 0x0100	 120 */		and	%g4,%o3,%g3
/* 0x0104	 117 */		add	%g3,%g5,%g3
/* 0x0108	 124 */		srax	%g1,16,%g1
/* 0x010c	 117 */		add	%g3,%o4,%g3
/* 0x0110	 124 */		srax	%g3,32,%o4
/* 0x0114	     */		stx	%o4,[%sp+112]
/* 0x0118	 119 */		ldx	[%sp+104],%g5
/* 0x011c	 121 */		srax	%g4,32,%o4
/* 0x0120	 124 */		ldx	[%sp+112],%g4
/* 0x0124	     */		add	%g1,%g4,%g4
/* 0x0128	 122 */		ldx	[%sp+96],%g1
/* 0x012c	 124 */		add	%o4,%g4,%o4
/* 0x0130	 125 */		and	%g3,%o3,%g3
/* 0x0134	 127 */		or	%g0,%g5,%g4
/* 0x0138	 128 */		ble,pt	%icc,.L900000112
/* 0x013c	     */		st	%g3,[%o5-4]
                       .L900000115:
/* 0x0140	 128 */		ba	.L900000117
/* 0x0144	     */		sethi	%hi(0xfc00),%g2
                       .L77000134:
/* 0x0148	 119 */		ldd	[%g2+16],%f0
                       .L900000116:
/* 0x014c	 120 */		and	%g4,%o3,%o0
/* 0x0150	 123 */		and	%g1,%o1,%g3
/* 0x0154	 119 */		fdtox	%f0,%f0
/* 0x0158	 120 */		add	%o4,%o0,%o0
/* 0x015c	 119 */		std	%f0,[%sp+104]
/* 0x0160	 128 */		add	%o7,1,%o7
/* 0x0164	 123 */		sllx	%g3,16,%o4
/* 0x0168	 122 */		ldd	[%g2+24],%f2
/* 0x016c	 128 */		add	%g2,16,%g2
/* 0x0170	 123 */		add	%o0,%o4,%o0
/* 0x0174	 128 */		cmp	%o7,%o2
/* 0x0178	 125 */		and	%o0,%o3,%g3
/* 0x017c	 122 */		fdtox	%f2,%f0
/* 0x0180	     */		std	%f0,[%sp+96]
/* 0x0184	 124 */		srax	%o0,32,%o0
/* 0x0188	     */		stx	%o0,[%sp+112]
/* 0x018c	 121 */		srax	%g4,32,%o4
/* 0x0190	 122 */		ldx	[%sp+96],%o0
/* 0x0194	 124 */		srax	%g1,16,%g5
/* 0x0198	     */		ldx	[%sp+112],%g4
/* 0x019c	 119 */		ldx	[%sp+104],%g1
/* 0x01a0	 125 */		st	%g3,[%o5]
/* 0x01a4	 124 */		add	%g5,%g4,%g4
/* 0x01a8	 128 */		add	%o5,4,%o5
/* 0x01ac	 124 */		add	%o4,%g4,%o4
/* 0x01b0	 127 */		or	%g0,%g1,%g4
/* 0x01b4	 128 */		or	%g0,%o0,%g1
/* 0x01b8	     */		ble,a,pt	%icc,.L900000116
/* 0x01bc	     */		ldd	[%g2+16],%f0
                       .L77000127:

!  129		      !   }
!  130		      !     t1+=a&0xffffffff;
!  131		      !     t=(a>>32);
!  132		      !     t1+=(b&0xffff)<<16;
!  133		      !     i32[i]=t1&0xffffffff;

/* 0x01c0	 133 */		sethi	%hi(0xfc00),%g2
                       .L900000117:
/* 0x01c4	 133 */		or	%g0,-1,%g3
/* 0x01c8	     */		add	%g2,1023,%g2
/* 0x01cc	     */		srl	%g3,0,%g3
/* 0x01d0	     */		and	%g1,%g2,%g2
/* 0x01d4	     */		and	%g4,%g3,%g4
/* 0x01d8	     */		sllx	%g2,16,%g2
/* 0x01dc	     */		add	%o4,%g4,%g4
/* 0x01e0	     */		add	%g4,%g2,%g2
/* 0x01e4	     */		sll	%o7,2,%g4
/* 0x01e8	     */		and	%g2,%g3,%g2
/* 0x01ec	     */		st	%g2,[%i0+%g4]
/* 0x01f0	     */		ret	! Result = 
/* 0x01f4	     */		restore	%g0,%g0,%g0
/* 0x01f8	   0 */		.type	conv_d16_to_i32,2
/* 0x01f8	     */		.size	conv_d16_to_i32,(.-conv_d16_to_i32)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! CONSTANT POOL
!
                       .L_const_seg_900000201:
/* 000000	   0 */		.word	1127219200,0
/* 0x0008	   0 */		.align	4
/* 0x0008	     */		.skip	16
!
! SUBROUTINE conv_i32_to_d32
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global conv_i32_to_d32
                       conv_i32_to_d32:
/* 000000	     */		or	%g0,%o7,%g2

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
/* 0x0010	 142 */		or	%g0,0,%o5
/* 0x0014	 138 */		add	%g4,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000210-.)),%g4
/* 0x0018	     */		or	%g0,%o0,%g5
/* 0x001c	     */		add	%g4,%o7,%g1
/* 0x0020	 142 */		ble,pt	%icc,.L77000140
/* 0x0024	     */		or	%g0,%g2,%o7
/* 0x0028	     */		sethi	%hi(.L_const_seg_900000201),%g2
/* 0x002c	 138 */		or	%g0,%o1,%g4
/* 0x0030	 142 */		add	%g2,%lo(.L_const_seg_900000201),%g2
/* 0x0034	     */		sub	%o2,1,%g3
/* 0x0038	     */		ld	[%g1+%g2],%g2
/* 0x003c	     */		cmp	%o2,9
/* 0x0040	     */		bl,pn	%icc,.L77000144
/* 0x0044	     */		ldd	[%g2],%f8
/* 0x0048	     */		add	%o1,16,%g4
/* 0x004c	     */		sub	%o2,5,%g1
/* 0x0050	     */		ld	[%o1],%f7
/* 0x0054	     */		or	%g0,4,%o5
/* 0x0058	     */		ld	[%o1+4],%f5
/* 0x005c	     */		ld	[%o1+8],%f3
/* 0x0060	     */		fmovs	%f8,%f6
/* 0x0064	     */		ld	[%o1+12],%f1
                       .L900000205:
/* 0x0068	     */		ld	[%g4],%f11
/* 0x006c	     */		add	%o5,5,%o5
/* 0x0070	     */		add	%g4,20,%g4
/* 0x0074	     */		fsubd	%f6,%f8,%f6
/* 0x0078	     */		std	%f6,[%g5]
/* 0x007c	     */		cmp	%o5,%g1
/* 0x0080	     */		add	%g5,40,%g5
/* 0x0084	     */		fmovs	%f8,%f4
/* 0x0088	     */		ld	[%g4-16],%f7
/* 0x008c	     */		fsubd	%f4,%f8,%f12
/* 0x0090	     */		fmovs	%f8,%f2
/* 0x0094	     */		std	%f12,[%g5-32]
/* 0x0098	     */		ld	[%g4-12],%f5
/* 0x009c	     */		fsubd	%f2,%f8,%f12
/* 0x00a0	     */		fmovs	%f8,%f0
/* 0x00a4	     */		std	%f12,[%g5-24]
/* 0x00a8	     */		ld	[%g4-8],%f3
/* 0x00ac	     */		fsubd	%f0,%f8,%f12
/* 0x00b0	     */		fmovs	%f8,%f10
/* 0x00b4	     */		std	%f12,[%g5-16]
/* 0x00b8	     */		ld	[%g4-4],%f1
/* 0x00bc	     */		fsubd	%f10,%f8,%f10
/* 0x00c0	     */		fmovs	%f8,%f6
/* 0x00c4	     */		ble,pt	%icc,.L900000205
/* 0x00c8	     */		std	%f10,[%g5-8]
                       .L900000208:
/* 0x00cc	     */		fmovs	%f8,%f4
/* 0x00d0	     */		add	%g5,32,%g5
/* 0x00d4	     */		cmp	%o5,%g3
/* 0x00d8	     */		fmovs	%f8,%f2
/* 0x00dc	     */		fmovs	%f8,%f0
/* 0x00e0	     */		fsubd	%f6,%f8,%f6
/* 0x00e4	     */		std	%f6,[%g5-32]
/* 0x00e8	     */		fsubd	%f4,%f8,%f4
/* 0x00ec	     */		std	%f4,[%g5-24]
/* 0x00f0	     */		fsubd	%f2,%f8,%f2
/* 0x00f4	     */		std	%f2,[%g5-16]
/* 0x00f8	     */		fsubd	%f0,%f8,%f0
/* 0x00fc	     */		bg,pn	%icc,.L77000140
/* 0x0100	     */		std	%f0,[%g5-8]
                       .L77000144:
/* 0x0104	     */		ld	[%g4],%f1
                       .L900000211:
/* 0x0108	     */		ldd	[%g2],%f8
/* 0x010c	     */		add	%o5,1,%o5
/* 0x0110	     */		add	%g4,4,%g4
/* 0x0114	     */		cmp	%o5,%g3
/* 0x0118	     */		fmovs	%f8,%f0
/* 0x011c	     */		fsubd	%f0,%f8,%f0
/* 0x0120	     */		std	%f0,[%g5]
/* 0x0124	     */		add	%g5,8,%g5
/* 0x0128	     */		ble,a,pt	%icc,.L900000211
/* 0x012c	     */		ld	[%g4],%f1
                       .L77000140:
/* 0x0130	     */		retl	! Result = 
/* 0x0134	     */		nop
/* 0x0138	   0 */		.type	conv_i32_to_d32,2
/* 0x0138	     */		.size	conv_i32_to_d32,(.-conv_i32_to_d32)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! CONSTANT POOL
!
                       .L_const_seg_900000301:
/* 000000	   0 */		.word	1127219200,0
/* 0x0008	   0 */		.align	4
!
! SUBROUTINE conv_i32_to_d16
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global conv_i32_to_d16
                       conv_i32_to_d16:
/* 000000	     */		save	%sp,-104,%sp
/* 0x0004	     */		or	%g0,%i2,%o0

!  143		      !}
!  146		      !void conv_i32_to_d16(double *d16, unsigned int *i32, int len)
!  147		      !{
!  148		      !int i;
!  149		      !unsigned int a;
!  151		      !#pragma pipeloop(0)
!  152		      ! for(i=0;i<len;i++)
!  153		      !   {
!  154		      !     a=i32[i];
!  155		      !     d16[2*i]=(double)(a&0xffff);
!  156		      !     d16[2*i+1]=(double)(a>>16);

/* 0x0008	 156 */		sethi	%hi(.L_const_seg_900000301),%g2
                       .L900000310:
/* 0x000c	     */		call	.+8
/* 0x0010	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000310-.)),%g3
/* 0x0014	 152 */		cmp	%o0,0
/* 0x0018	 147 */		add	%g3,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000310-.)),%g3
/* 0x001c	 152 */		ble,pt	%icc,.L77000150
/* 0x0020	     */		add	%g3,%o7,%o2
/* 0x0024	     */		sub	%i2,1,%o5
/* 0x0028	 156 */		add	%g2,%lo(.L_const_seg_900000301),%o1
/* 0x002c	 152 */		sethi	%hi(0xfc00),%o0
/* 0x0030	     */		ld	[%o2+%o1],%o3
/* 0x0034	     */		add	%o5,1,%g2
/* 0x0038	     */		or	%g0,0,%g1
/* 0x003c	     */		cmp	%g2,3
/* 0x0040	     */		or	%g0,%i1,%o7
/* 0x0044	     */		add	%o0,1023,%o4
/* 0x0048	     */		or	%g0,%i0,%g3
/* 0x004c	     */		bl,pn	%icc,.L77000154
/* 0x0050	     */		add	%o7,4,%o0
/* 0x0054	 155 */		ldd	[%o3],%f0
/* 0x0058	 156 */		or	%g0,1,%g1
/* 0x005c	 154 */		ld	[%o0-4],%o1
/* 0x0060	   0 */		or	%g0,%o0,%o7
/* 0x0064	 155 */		and	%o1,%o4,%o0
                       .L900000306:
/* 0x0068	 155 */		st	%o0,[%sp+96]
/* 0x006c	 156 */		add	%g1,1,%g1
/* 0x0070	     */		add	%g3,16,%g3
/* 0x0074	     */		cmp	%g1,%o5
/* 0x0078	     */		add	%o7,4,%o7
/* 0x007c	 155 */		ld	[%sp+96],%f3
/* 0x0080	     */		fmovs	%f0,%f2
/* 0x0084	     */		fsubd	%f2,%f0,%f2
/* 0x0088	 156 */		srl	%o1,16,%o0
/* 0x008c	 155 */		std	%f2,[%g3-16]
/* 0x0090	 156 */		st	%o0,[%sp+92]
/* 0x0094	     */		ld	[%sp+92],%f3
/* 0x0098	 154 */		ld	[%o7-4],%o1
/* 0x009c	 156 */		fmovs	%f0,%f2
/* 0x00a0	     */		fsubd	%f2,%f0,%f2
/* 0x00a4	 155 */		and	%o1,%o4,%o0
/* 0x00a8	 156 */		ble,pt	%icc,.L900000306
/* 0x00ac	     */		std	%f2,[%g3-8]
                       .L900000309:
/* 0x00b0	 155 */		st	%o0,[%sp+96]
/* 0x00b4	     */		fmovs	%f0,%f2
/* 0x00b8	 156 */		add	%g3,16,%g3
/* 0x00bc	     */		srl	%o1,16,%o0
/* 0x00c0	 155 */		ld	[%sp+96],%f3
/* 0x00c4	     */		fsubd	%f2,%f0,%f2
/* 0x00c8	     */		std	%f2,[%g3-16]
/* 0x00cc	 156 */		st	%o0,[%sp+92]
/* 0x00d0	     */		fmovs	%f0,%f2
/* 0x00d4	     */		ld	[%sp+92],%f3
/* 0x00d8	     */		fsubd	%f2,%f0,%f0
/* 0x00dc	     */		std	%f0,[%g3-8]
/* 0x00e0	     */		ret	! Result = 
/* 0x00e4	     */		restore	%g0,%g0,%g0
                       .L77000154:
/* 0x00e8	 154 */		ld	[%o7],%o0
                       .L900000311:
/* 0x00ec	 155 */		and	%o0,%o4,%o1
/* 0x00f0	     */		st	%o1,[%sp+96]
/* 0x00f4	 156 */		add	%g1,1,%g1
/* 0x00f8	 155 */		ldd	[%o3],%f0
/* 0x00fc	 156 */		srl	%o0,16,%o0
/* 0x0100	     */		add	%o7,4,%o7
/* 0x0104	     */		cmp	%g1,%o5
/* 0x0108	 155 */		fmovs	%f0,%f2
/* 0x010c	     */		ld	[%sp+96],%f3
/* 0x0110	     */		fsubd	%f2,%f0,%f2
/* 0x0114	     */		std	%f2,[%g3]
/* 0x0118	 156 */		st	%o0,[%sp+92]
/* 0x011c	     */		fmovs	%f0,%f2
/* 0x0120	     */		ld	[%sp+92],%f3
/* 0x0124	     */		fsubd	%f2,%f0,%f0
/* 0x0128	     */		std	%f0,[%g3+8]
/* 0x012c	     */		add	%g3,16,%g3
/* 0x0130	     */		ble,a,pt	%icc,.L900000311
/* 0x0134	     */		ld	[%o7],%o0
                       .L77000150:
/* 0x0138	     */		ret	! Result = 
/* 0x013c	     */		restore	%g0,%g0,%g0
/* 0x0140	   0 */		.type	conv_i32_to_d16,2
/* 0x0140	     */		.size	conv_i32_to_d16,(.-conv_i32_to_d16)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	8
!
! CONSTANT POOL
!
                       .L_const_seg_900000401:
/* 000000	   0 */		.word	1127219200,0
/* 0x0008	   0 */		.align	4
/* 0x0008	     */		.skip	16
!
! SUBROUTINE conv_i32_to_d32_and_d16
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global conv_i32_to_d32_and_d16
                       conv_i32_to_d32_and_d16:
/* 000000	     */		save	%sp,-120,%sp
                       .L900000415:
/* 0x0004	     */		call	.+8
/* 0x0008	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000415-.)),%g4

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

/* 0x000c	 169 */		sub	%i3,3,%g2
/* 0x0010	     */		cmp	%g2,0
/* 0x0014	 163 */		add	%g4,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000415-.)),%g4

!  170		      !   {
!  171		      !     i16_to_d16_and_d32x4(&TwoToMinus16, &TwoTo16, &Zero,
!  172		      !			  &(d16[2*i]), &(d32[i]), (float *)(&(i32[i])));

/* 0x0018	 172 */		sethi	%hi(Zero),%g2
/* 0x001c	 163 */		add	%g4,%o7,%o4
/* 0x0020	 172 */		add	%g2,%lo(Zero),%g2
/* 0x0024	     */		sethi	%hi(TwoToMinus16),%g3
/* 0x0028	     */		ld	[%o4+%g2],%o1
/* 0x002c	     */		sethi	%hi(TwoTo16),%g4
/* 0x0030	     */		add	%g3,%lo(TwoToMinus16),%g2
/* 0x0034	     */		ld	[%o4+%g2],%o3
/* 0x0038	 164 */		or	%g0,0,%g5
/* 0x003c	 172 */		add	%g4,%lo(TwoTo16),%g3
/* 0x0040	     */		ld	[%o4+%g3],%o2
/* 0x0044	 163 */		or	%g0,%i0,%i4
/* 0x0048	 169 */		or	%g0,%i2,%o7
/* 0x004c	     */		ble,pt	%icc,.L900000418
/* 0x0050	     */		cmp	%g5,%i3
/* 0x0054	 172 */		stx	%o7,[%sp+104]
/* 0x0058	 169 */		sub	%i3,4,%o5
/* 0x005c	     */		or	%g0,0,%g4
/* 0x0060	     */		or	%g0,0,%g1
                       .L900000417:
/* 0x0064	     */		ldd	[%o1],%f2
/* 0x0068	 172 */		add	%i4,%g4,%g2
/* 0x006c	     */		add	%i1,%g1,%g3
/* 0x0070	     */		ldd	[%o3],%f0
/* 0x0074	     */		add	%g5,4,%g5
/* 0x0078	     */		fmovd	%f2,%f14
/* 0x007c	     */		ld	[%o7],%f15
/* 0x0080	     */		cmp	%g5,%o5
/* 0x0084	     */		fmovd	%f2,%f10
/* 0x0088	     */		ld	[%o7+4],%f11
/* 0x008c	     */		add	%o7,16,%o7
/* 0x0090	     */		ldx	[%sp+104],%o0
/* 0x0094	     */		fmovd	%f2,%f6
/* 0x0098	     */		stx	%o7,[%sp+112]
/* 0x009c	     */		fxtod	%f14,%f14
/* 0x00a0	     */		ld	[%o0+8],%f7
/* 0x00a4	     */		fxtod	%f10,%f10
/* 0x00a8	     */		ld	[%o0+12],%f3
/* 0x00ac	     */		fxtod	%f6,%f6
/* 0x00b0	     */		ldd	[%o2],%f16
/* 0x00b4	     */		fmuld	%f0,%f14,%f12
/* 0x00b8	     */		fxtod	%f2,%f2
/* 0x00bc	     */		fmuld	%f0,%f10,%f8
/* 0x00c0	     */		std	%f14,[%i4+%g4]
/* 0x00c4	     */		ldx	[%sp+112],%o7
/* 0x00c8	     */		add	%g4,32,%g4
/* 0x00cc	     */		fmuld	%f0,%f6,%f4
/* 0x00d0	     */		fdtox	%f12,%f12
/* 0x00d4	     */		std	%f10,[%g2+8]
/* 0x00d8	     */		fmuld	%f0,%f2,%f0
/* 0x00dc	     */		fdtox	%f8,%f8
/* 0x00e0	     */		std	%f6,[%g2+16]
/* 0x00e4	     */		std	%f2,[%g2+24]
/* 0x00e8	     */		fdtox	%f4,%f4
/* 0x00ec	     */		fdtox	%f0,%f0
/* 0x00f0	     */		fxtod	%f12,%f12
/* 0x00f4	     */		std	%f12,[%g3+8]
/* 0x00f8	     */		fxtod	%f8,%f8
/* 0x00fc	     */		std	%f8,[%g3+24]
/* 0x0100	     */		fxtod	%f4,%f4
/* 0x0104	     */		std	%f4,[%g3+40]
/* 0x0108	     */		fxtod	%f0,%f0
/* 0x010c	     */		std	%f0,[%g3+56]
/* 0x0110	     */		fmuld	%f12,%f16,%f12
/* 0x0114	     */		fmuld	%f8,%f16,%f8
/* 0x0118	     */		fmuld	%f4,%f16,%f4
/* 0x011c	     */		fsubd	%f14,%f12,%f12
/* 0x0120	     */		std	%f12,[%i1+%g1]
/* 0x0124	     */		fmuld	%f0,%f16,%f0
/* 0x0128	     */		fsubd	%f10,%f8,%f8
/* 0x012c	     */		std	%f8,[%g3+16]
/* 0x0130	     */		add	%g1,64,%g1
/* 0x0134	     */		fsubd	%f6,%f4,%f4
/* 0x0138	     */		std	%f4,[%g3+32]
/* 0x013c	     */		fsubd	%f2,%f0,%f0
/* 0x0140	     */		std	%f0,[%g3+48]
/* 0x0144	     */		ble,a,pt	%icc,.L900000417
/* 0x0148	     */		stx	%o7,[%sp+104]
                       .L77000159:

!  173		      !   }
!  174		      !#endif
!  175		      ! for(;i<len;i++)

/* 0x014c	 175 */		cmp	%g5,%i3
                       .L900000418:
/* 0x0150	 175 */		bge,pt	%icc,.L77000164
/* 0x0154	     */		nop

!  176		      !   {
!  177		      !     a=i32[i];
!  178		      !     d32[i]=(double)(i32[i]);
!  179		      !     d16[2*i]=(double)(a&0xffff);
!  180		      !     d16[2*i+1]=(double)(a>>16);

/* 0x0158	 180 */		sethi	%hi(.L_const_seg_900000401),%g2
/* 0x015c	     */		add	%g2,%lo(.L_const_seg_900000401),%o1
/* 0x0160	 175 */		sethi	%hi(0xfc00),%o0
/* 0x0164	     */		ld	[%o4+%o1],%o2
/* 0x0168	     */		sll	%g5,2,%o3
/* 0x016c	     */		sub	%i3,%g5,%g3
/* 0x0170	     */		sll	%g5,3,%g2
/* 0x0174	     */		add	%o0,1023,%o4
/* 0x0178	 178 */		ldd	[%o2],%f0
/* 0x017c	     */		add	%i2,%o3,%o0
/* 0x0180	 175 */		cmp	%g3,3
/* 0x0184	     */		add	%i4,%g2,%o3
/* 0x0188	     */		sub	%i3,1,%o1
/* 0x018c	     */		sll	%g5,4,%g4
/* 0x0190	     */		bl,pn	%icc,.L77000161
/* 0x0194	     */		add	%i1,%g4,%o5
/* 0x0198	 178 */		ld	[%o0],%f3
/* 0x019c	 180 */		add	%o3,8,%o3
/* 0x01a0	 177 */		ld	[%o0],%o7
/* 0x01a4	 180 */		add	%o5,16,%o5
/* 0x01a8	     */		add	%g5,1,%g5
/* 0x01ac	 178 */		fmovs	%f0,%f2
/* 0x01b0	 180 */		add	%o0,4,%o0
/* 0x01b4	 179 */		and	%o7,%o4,%g1
/* 0x01b8	 178 */		fsubd	%f2,%f0,%f2
/* 0x01bc	     */		std	%f2,[%o3-8]
/* 0x01c0	 180 */		srl	%o7,16,%o7
/* 0x01c4	 179 */		st	%g1,[%sp+96]
/* 0x01c8	     */		fmovs	%f0,%f2
/* 0x01cc	     */		ld	[%sp+96],%f3
/* 0x01d0	     */		fsubd	%f2,%f0,%f2
/* 0x01d4	     */		std	%f2,[%o5-16]
/* 0x01d8	 180 */		st	%o7,[%sp+92]
/* 0x01dc	     */		fmovs	%f0,%f2
/* 0x01e0	     */		ld	[%sp+92],%f3
/* 0x01e4	     */		fsubd	%f2,%f0,%f2
/* 0x01e8	     */		std	%f2,[%o5-8]
                       .L900000411:
/* 0x01ec	 178 */		ld	[%o0],%f3
/* 0x01f0	 180 */		add	%g5,2,%g5
/* 0x01f4	     */		add	%o5,32,%o5
/* 0x01f8	 177 */		ld	[%o0],%o7
/* 0x01fc	 180 */		cmp	%g5,%o1
/* 0x0200	     */		add	%o3,16,%o3
/* 0x0204	 178 */		fmovs	%f0,%f2
/* 0x0208	     */		fsubd	%f2,%f0,%f2
/* 0x020c	     */		std	%f2,[%o3-16]
/* 0x0210	 179 */		and	%o7,%o4,%g1
/* 0x0214	     */		st	%g1,[%sp+96]
/* 0x0218	     */		ld	[%sp+96],%f3
/* 0x021c	     */		fmovs	%f0,%f2
/* 0x0220	     */		fsubd	%f2,%f0,%f2
/* 0x0224	 180 */		srl	%o7,16,%o7
/* 0x0228	 179 */		std	%f2,[%o5-32]
/* 0x022c	 180 */		st	%o7,[%sp+92]
/* 0x0230	     */		ld	[%sp+92],%f3
/* 0x0234	     */		fmovs	%f0,%f2
/* 0x0238	     */		fsubd	%f2,%f0,%f2
/* 0x023c	     */		std	%f2,[%o5-24]
/* 0x0240	     */		add	%o0,4,%o0
/* 0x0244	 178 */		ld	[%o0],%f3
/* 0x0248	 177 */		ld	[%o0],%o7
/* 0x024c	 178 */		fmovs	%f0,%f2
/* 0x0250	     */		fsubd	%f2,%f0,%f2
/* 0x0254	     */		std	%f2,[%o3-8]
/* 0x0258	 179 */		and	%o7,%o4,%g1
/* 0x025c	     */		st	%g1,[%sp+96]
/* 0x0260	     */		ld	[%sp+96],%f3
/* 0x0264	     */		fmovs	%f0,%f2
/* 0x0268	     */		fsubd	%f2,%f0,%f2
/* 0x026c	 180 */		srl	%o7,16,%o7
/* 0x0270	 179 */		std	%f2,[%o5-16]
/* 0x0274	 180 */		st	%o7,[%sp+92]
/* 0x0278	     */		ld	[%sp+92],%f3
/* 0x027c	     */		fmovs	%f0,%f2
/* 0x0280	     */		fsubd	%f2,%f0,%f2
/* 0x0284	     */		std	%f2,[%o5-8]
/* 0x0288	     */		bl,pt	%icc,.L900000411
/* 0x028c	     */		add	%o0,4,%o0
                       .L900000414:
/* 0x0290	 180 */		cmp	%g5,%i3
/* 0x0294	     */		bge,pn	%icc,.L77000164
/* 0x0298	     */		nop
                       .L77000161:
/* 0x029c	 178 */		ld	[%o0],%f3
                       .L900000416:
/* 0x02a0	 178 */		ldd	[%o2],%f0
/* 0x02a4	 180 */		add	%g5,1,%g5
/* 0x02a8	 177 */		ld	[%o0],%o1
/* 0x02ac	 180 */		add	%o0,4,%o0
/* 0x02b0	     */		cmp	%g5,%i3
/* 0x02b4	 178 */		fmovs	%f0,%f2
/* 0x02b8	 179 */		and	%o1,%o4,%o7
/* 0x02bc	 178 */		fsubd	%f2,%f0,%f2
/* 0x02c0	     */		std	%f2,[%o3]
/* 0x02c4	 180 */		srl	%o1,16,%o1
/* 0x02c8	 179 */		st	%o7,[%sp+96]
/* 0x02cc	 180 */		add	%o3,8,%o3
/* 0x02d0	 179 */		fmovs	%f0,%f2
/* 0x02d4	     */		ld	[%sp+96],%f3
/* 0x02d8	     */		fsubd	%f2,%f0,%f2
/* 0x02dc	     */		std	%f2,[%o5]
/* 0x02e0	 180 */		st	%o1,[%sp+92]
/* 0x02e4	     */		fmovs	%f0,%f2
/* 0x02e8	     */		ld	[%sp+92],%f3
/* 0x02ec	     */		fsubd	%f2,%f0,%f0
/* 0x02f0	     */		std	%f0,[%o5+8]
/* 0x02f4	     */		add	%o5,16,%o5
/* 0x02f8	     */		bl,a,pt	%icc,.L900000416
/* 0x02fc	     */		ld	[%o0],%f3
                       .L77000164:
/* 0x0300	     */		ret	! Result = 
/* 0x0304	     */		restore	%g0,%g0,%g0
/* 0x0308	   0 */		.type	conv_i32_to_d32_and_d16,2
/* 0x0308	     */		.size	conv_i32_to_d32_and_d16,(.-conv_i32_to_d32_and_d16)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	4
!
! SUBROUTINE adjust_montf_result
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global adjust_montf_result
                       adjust_montf_result:
/* 000000	     */		or	%g0,%o2,%g5

!  181		      !   }
!  182		      !}
!  185		      !void adjust_montf_result(unsigned int *i32, unsigned int *nint, int len)
!  186		      !{
!  187		      !long long acc;
!  188		      !int i;
!  190		      ! if(i32[len]>0) i=-1;

/* 0x0004	 190 */		or	%g0,-1,%g4
/* 0x0008	     */		sll	%o2,2,%g1
/* 0x000c	     */		ld	[%o0+%g1],%g1
/* 0x0010	     */		cmp	%g1,0
/* 0x0014	     */		bleu,pn	%icc,.L77000175
/* 0x0018	     */		or	%g0,%o1,%o3
/* 0x001c	     */		ba	.L900000511
/* 0x0020	     */		cmp	%g4,0
                       .L77000175:

!  191		      ! else
!  192		      !   {
!  193		      !     for(i=len-1; i>=0; i--)

/* 0x0024	 193 */		sub	%o2,1,%g4
/* 0x0028	     */		sll	%g4,2,%g1
/* 0x002c	     */		cmp	%g4,0
/* 0x0030	     */		bl,pt	%icc,.L900000511
/* 0x0034	     */		cmp	%g4,0
/* 0x0038	     */		add	%o1,%g1,%g2

!  194		      !       {
!  195		      !	 if(i32[i]!=nint[i]) break;

/* 0x003c	 195 */		ld	[%g2],%o5
/* 0x0040	 193 */		add	%o0,%g1,%g3
                       .L900000510:
/* 0x0044	 195 */		ld	[%g3],%o2
/* 0x0048	     */		sub	%g4,1,%g1
/* 0x004c	     */		sub	%g2,4,%g2
/* 0x0050	     */		sub	%g3,4,%g3
/* 0x0054	     */		cmp	%o2,%o5
/* 0x0058	     */		bne,pn	%icc,.L77000182
/* 0x005c	     */		nop
/* 0x0060	   0 */		or	%g0,%g1,%g4
/* 0x0064	 195 */		cmp	%g1,0
/* 0x0068	     */		bge,a,pt	%icc,.L900000510
/* 0x006c	     */		ld	[%g2],%o5
                       .L77000182:

!  196		      !       }
!  197		      !   }
!  198		      ! if((i<0)||(i32[i]>nint[i]))

/* 0x0070	 198 */		cmp	%g4,0
                       .L900000511:
/* 0x0074	 198 */		bl,pn	%icc,.L77000198
/* 0x0078	     */		sll	%g4,2,%g2
/* 0x007c	     */		ld	[%o1+%g2],%g1
/* 0x0080	     */		ld	[%o0+%g2],%g2
/* 0x0084	     */		cmp	%g2,%g1
/* 0x0088	     */		bleu,pt	%icc,.L77000191
/* 0x008c	     */		nop
                       .L77000198:

!  199		      !   {
!  200		      !     acc=0;
!  201		      !     for(i=0;i<len;i++)

/* 0x0090	 201 */		cmp	%g5,0
/* 0x0094	     */		ble,pt	%icc,.L77000191
/* 0x0098	     */		nop
/* 0x009c	     */		or	%g0,%g5,%g1
/* 0x00a0	 198 */		or	%g0,-1,%g2
/* 0x00a4	     */		srl	%g2,0,%g3
/* 0x00a8	     */		sub	%g5,1,%g4
/* 0x00ac	 200 */		or	%g0,0,%g5
/* 0x00b0	 201 */		or	%g0,0,%o5
/* 0x00b4	 198 */		or	%g0,%o0,%o4
/* 0x00b8	     */		cmp	%g1,3
/* 0x00bc	 201 */		bl,pn	%icc,.L77000199
/* 0x00c0	     */		add	%o0,8,%g1
/* 0x00c4	     */		add	%o1,4,%g2

!  202		      !       {
!  203		      !	 acc=acc+(unsigned long long)(i32[i])-(unsigned long long)(nint[i]);

/* 0x00c8	 203 */		ld	[%o0],%o2
/* 0x00cc	     */		ld	[%o1],%o1
/* 0x00d0	   0 */		or	%g0,%g1,%o4
/* 0x00d4	     */		or	%g0,%g2,%o3
/* 0x00d8	 203 */		ld	[%o0+4],%g1

!  204		      !	 i32[i]=acc&0xffffffff;
!  205		      !	 acc=acc>>32;

/* 0x00dc	 205 */		or	%g0,2,%o5
/* 0x00e0	 201 */		sub	%o2,%o1,%o2
/* 0x00e4	     */		or	%g0,%o2,%g5
/* 0x00e8	 204 */		and	%o2,%g3,%o2
/* 0x00ec	     */		st	%o2,[%o0]
/* 0x00f0	 205 */		srax	%g5,32,%g5
                       .L900000505:
/* 0x00f4	 203 */		ld	[%o3],%o2
/* 0x00f8	 205 */		add	%o5,1,%o5
/* 0x00fc	     */		add	%o3,4,%o3
/* 0x0100	     */		cmp	%o5,%g4
/* 0x0104	     */		add	%o4,4,%o4
/* 0x0108	 201 */		sub	%g1,%o2,%g1
/* 0x010c	     */		add	%g1,%g5,%g5
/* 0x0110	 204 */		and	%g5,%g3,%o2
/* 0x0114	 203 */		ld	[%o4-4],%g1
/* 0x0118	 204 */		st	%o2,[%o4-8]
/* 0x011c	 205 */		ble,pt	%icc,.L900000505
/* 0x0120	     */		srax	%g5,32,%g5
                       .L900000508:
/* 0x0124	 203 */		ld	[%o3],%g2
/* 0x0128	 201 */		sub	%g1,%g2,%g1
/* 0x012c	     */		add	%g1,%g5,%g1
/* 0x0130	 204 */		and	%g1,%g3,%g2
/* 0x0134	     */		retl	! Result = 
/* 0x0138	     */		st	%g2,[%o4-4]
                       .L77000199:
/* 0x013c	 203 */		ld	[%o4],%g1
                       .L900000509:
/* 0x0140	 203 */		ld	[%o3],%g2
/* 0x0144	     */		add	%g5,%g1,%g1
/* 0x0148	 205 */		add	%o5,1,%o5
/* 0x014c	     */		add	%o3,4,%o3
/* 0x0150	     */		cmp	%o5,%g4
/* 0x0154	 203 */		sub	%g1,%g2,%g1
/* 0x0158	 204 */		and	%g1,%g3,%g2
/* 0x015c	     */		st	%g2,[%o4]
/* 0x0160	 205 */		add	%o4,4,%o4
/* 0x0164	     */		srax	%g1,32,%g5
/* 0x0168	     */		ble,a,pt	%icc,.L900000509
/* 0x016c	     */		ld	[%o4],%g1
                       .L77000191:
/* 0x0170	     */		retl	! Result = 
/* 0x0174	     */		nop
/* 0x0178	   0 */		.type	adjust_montf_result,2
/* 0x0178	     */		.size	adjust_montf_result,(.-adjust_montf_result)

	.section	".text",#alloc,#execinstr
/* 000000	   0 */		.align	4
/* 000000	     */		.skip	16
!
! SUBROUTINE mont_mulf_noconv
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global mont_mulf_noconv
                       mont_mulf_noconv:
/* 000000	     */		save	%sp,-144,%sp
                       .L900000646:
/* 0x0004	     */		call	.+8
/* 0x0008	     */		sethi	/*X*/%hi(_GLOBAL_OFFSET_TABLE_-(.L900000646-.)),%g5

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

/* 0x000c	 232 */		ld	[%fp+92],%o1
/* 0x0010	     */		sethi	%hi(Zero),%g2
/* 0x0014	 223 */		ldd	[%fp+96],%f2
/* 0x0018	     */		add	%g5,/*X*/%lo(_GLOBAL_OFFSET_TABLE_-(.L900000646-.)),%g5
/* 0x001c	 232 */		add	%g2,%lo(Zero),%g2
/* 0x0020	 223 */		st	%i0,[%fp+68]
/* 0x0024	     */		add	%g5,%o7,%o3

!  234		      ! if (nlen!=16)
!  235		      !   {
!  236		      !     for(i=0;i<4*nlen+2;i++) dt[i]=Zero;
!  238		      !     a=dt[0]=pdm1[0]*pdm2[0];
!  239		      !     digit=mod(lower32(a,Zero)*dn0,TwoToMinus16,TwoTo16);

/* 0x0028	 239 */		sethi	%hi(TwoToMinus16),%g3
/* 0x002c	 232 */		ld	[%o3+%g2],%l0
/* 0x0030	 239 */		sethi	%hi(TwoTo16),%g4
/* 0x0034	 223 */		or	%g0,%i2,%o2
/* 0x0038	     */		fmovd	%f2,%f16
/* 0x003c	     */		st	%i5,[%fp+88]
/* 0x0040	 239 */		add	%g3,%lo(TwoToMinus16),%g2
/* 0x0044	 223 */		or	%g0,%i1,%i2
/* 0x0048	 232 */		ldd	[%l0],%f0
/* 0x004c	 239 */		add	%g4,%lo(TwoTo16),%g3
/* 0x0050	 223 */		or	%g0,%i3,%o0
/* 0x0054	 232 */		sll	%o1,4,%g4
/* 0x0058	 239 */		ld	[%o3+%g2],%g5
/* 0x005c	 223 */		or	%g0,%i3,%i1
/* 0x0060	 239 */		ld	[%o3+%g3],%g1
/* 0x0064	 232 */		or	%g0,%o1,%i0
/* 0x0068	     */		or	%g0,%o2,%i3
/* 0x006c	 234 */		cmp	%o1,16
/* 0x0070	     */		be,pn	%icc,.L77000279
/* 0x0074	     */		std	%f0,[%o2+%g4]
/* 0x0078	 236 */		sll	%o1,2,%g2
/* 0x007c	     */		or	%g0,%o0,%o3
/* 0x0080	 232 */		sll	%o1,1,%o1
/* 0x0084	 236 */		add	%g2,2,%o2
/* 0x0088	     */		cmp	%o2,0
/* 0x008c	     */		ble,a,pt	%icc,.L900000660
/* 0x0090	     */		ldd	[%i2],%f0

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

/* 0x0094	 281 */		add	%g2,2,%o0
/* 0x0098	 236 */		add	%g2,1,%o2
/* 0x009c	 281 */		cmp	%o0,3
/* 0x00a0	     */		bl,pn	%icc,.L77000280
/* 0x00a4	     */		or	%g0,1,%o0
/* 0x00a8	     */		add	%o3,8,%o3
/* 0x00ac	     */		or	%g0,1,%o4
/* 0x00b0	     */		std	%f0,[%o3-8]
                       .L900000630:
/* 0x00b4	     */		std	%f0,[%o3]
/* 0x00b8	     */		add	%o4,2,%o4
/* 0x00bc	     */		add	%o3,16,%o3
/* 0x00c0	     */		cmp	%o4,%g2
/* 0x00c4	     */		ble,pt	%icc,.L900000630
/* 0x00c8	     */		std	%f0,[%o3-8]
                       .L900000633:
/* 0x00cc	     */		cmp	%o4,%o2
/* 0x00d0	     */		bg,pn	%icc,.L77000285
/* 0x00d4	     */		add	%o4,1,%o0
                       .L77000280:
/* 0x00d8	     */		std	%f0,[%o3]
                       .L900000659:
/* 0x00dc	     */		ldd	[%l0],%f0
/* 0x00e0	     */		cmp	%o0,%o2
/* 0x00e4	     */		add	%o3,8,%o3
/* 0x00e8	     */		add	%o0,1,%o0
/* 0x00ec	     */		ble,a,pt	%icc,.L900000659
/* 0x00f0	     */		std	%f0,[%o3]
                       .L77000285:
/* 0x00f4	 238 */		ldd	[%i2],%f0
                       .L900000660:
/* 0x00f8	 238 */		ldd	[%i3],%f2
/* 0x00fc	     */		add	%o1,1,%o2
/* 0x0100	 242 */		cmp	%o1,0
/* 0x0104	     */		sll	%o2,1,%o0
/* 0x0108	     */		sub	%o1,1,%o1
/* 0x010c	 238 */		fmuld	%f0,%f2,%f0
/* 0x0110	     */		std	%f0,[%i1]
/* 0x0114	   0 */		or	%g0,0,%l1
/* 0x0118	     */		ldd	[%l0],%f6
/* 0x011c	     */		or	%g0,0,%g4
/* 0x0120	     */		or	%g0,%o2,%i5
/* 0x0124	     */		ldd	[%g5],%f2
/* 0x0128	     */		or	%g0,%o1,%g3
/* 0x012c	     */		or	%g0,%o0,%o3
/* 0x0130	     */		fdtox	%f0,%f0
/* 0x0134	     */		ldd	[%g1],%f4
/* 0x0138	 246 */		add	%i3,8,%o4
/* 0x013c	     */		or	%g0,0,%l2
/* 0x0140	     */		or	%g0,%i1,%o5
/* 0x0144	     */		sub	%i0,1,%o7
/* 0x0148	     */		fmovs	%f6,%f0
/* 0x014c	     */		fxtod	%f0,%f0
/* 0x0150	 239 */		fmuld	%f0,%f16,%f0
/* 0x0154	     */		fmuld	%f0,%f2,%f2
/* 0x0158	     */		fdtox	%f2,%f2
/* 0x015c	     */		fxtod	%f2,%f2
/* 0x0160	     */		fmuld	%f2,%f4,%f2
/* 0x0164	     */		fsubd	%f0,%f2,%f22
/* 0x0168	 242 */		ble,pt	%icc,.L900000653
/* 0x016c	     */		sll	%i0,4,%g2
/* 0x0170	 246 */		ldd	[%i4],%f0
                       .L900000654:
/* 0x0174	 246 */		fmuld	%f0,%f22,%f8
/* 0x0178	     */		ldd	[%i2],%f0
/* 0x017c	 250 */		cmp	%i0,1
/* 0x0180	 246 */		ldd	[%o4+%l2],%f6
/* 0x0184	     */		add	%i2,8,%o0
/* 0x0188	 250 */		or	%g0,1,%o1
/* 0x018c	 246 */		ldd	[%o5],%f2
/* 0x0190	     */		add	%o5,16,%l3
/* 0x0194	     */		fmuld	%f0,%f6,%f6
/* 0x0198	     */		ldd	[%g5],%f4
/* 0x019c	     */		faddd	%f2,%f8,%f2
/* 0x01a0	     */		ldd	[%o5+8],%f0
/* 0x01a4	 244 */		ldd	[%i3+%l2],%f20
/* 0x01a8	 246 */		faddd	%f0,%f6,%f0
/* 0x01ac	     */		fmuld	%f2,%f4,%f2
/* 0x01b0	     */		faddd	%f0,%f2,%f18
/* 0x01b4	 247 */		std	%f18,[%o5+8]
/* 0x01b8	 250 */		ble,pt	%icc,.L900000658
/* 0x01bc	     */		srl	%g4,31,%g2
/* 0x01c0	     */		cmp	%o7,7
/* 0x01c4	 246 */		add	%i4,8,%g2
/* 0x01c8	 250 */		bl,pn	%icc,.L77000284
/* 0x01cc	     */		add	%g2,24,%o2
/* 0x01d0	 252 */		ldd	[%o0+24],%f12
/* 0x01d4	     */		add	%o5,48,%l3
/* 0x01d8	     */		ldd	[%o0],%f2
/* 0x01dc	   0 */		or	%g0,%o2,%g2
/* 0x01e0	 250 */		sub	%o7,2,%o2
/* 0x01e4	 252 */		ldd	[%g2-24],%f0
/* 0x01e8	     */		or	%g0,5,%o1
/* 0x01ec	     */		ldd	[%o0+8],%f6
/* 0x01f0	     */		fmuld	%f2,%f20,%f2
/* 0x01f4	     */		ldd	[%o0+16],%f14
/* 0x01f8	     */		fmuld	%f0,%f22,%f4
/* 0x01fc	     */		add	%o0,32,%o0
/* 0x0200	     */		ldd	[%g2-16],%f8
/* 0x0204	     */		fmuld	%f6,%f20,%f10
/* 0x0208	     */		ldd	[%o5+16],%f0
/* 0x020c	     */		ldd	[%g2-8],%f6
/* 0x0210	     */		faddd	%f2,%f4,%f4
/* 0x0214	     */		ldd	[%o5+32],%f2
                       .L900000642:
/* 0x0218	 252 */		ldd	[%g2],%f24
/* 0x021c	     */		add	%o1,3,%o1
/* 0x0220	     */		add	%g2,24,%g2
/* 0x0224	     */		fmuld	%f8,%f22,%f8
/* 0x0228	     */		ldd	[%l3],%f28
/* 0x022c	     */		cmp	%o1,%o2
/* 0x0230	     */		add	%o0,24,%o0
/* 0x0234	     */		ldd	[%o0-24],%f26
/* 0x0238	     */		faddd	%f0,%f4,%f0
/* 0x023c	     */		add	%l3,48,%l3
/* 0x0240	     */		faddd	%f10,%f8,%f10
/* 0x0244	     */		fmuld	%f14,%f20,%f4
/* 0x0248	     */		std	%f0,[%l3-80]
/* 0x024c	     */		ldd	[%g2-16],%f8
/* 0x0250	     */		fmuld	%f6,%f22,%f6
/* 0x0254	     */		ldd	[%l3-32],%f0
/* 0x0258	     */		ldd	[%o0-16],%f14
/* 0x025c	     */		faddd	%f2,%f10,%f2
/* 0x0260	     */		faddd	%f4,%f6,%f10
/* 0x0264	     */		fmuld	%f12,%f20,%f4
/* 0x0268	     */		std	%f2,[%l3-64]
/* 0x026c	     */		ldd	[%g2-8],%f6
/* 0x0270	     */		fmuld	%f24,%f22,%f24
/* 0x0274	     */		ldd	[%l3-16],%f2
/* 0x0278	     */		ldd	[%o0-8],%f12
/* 0x027c	     */		faddd	%f28,%f10,%f10
/* 0x0280	     */		std	%f10,[%l3-48]
/* 0x0284	     */		fmuld	%f26,%f20,%f10
/* 0x0288	     */		ble,pt	%icc,.L900000642
/* 0x028c	     */		faddd	%f4,%f24,%f4
                       .L900000645:
/* 0x0290	 252 */		fmuld	%f8,%f22,%f28
/* 0x0294	     */		ldd	[%g2],%f24
/* 0x0298	     */		faddd	%f0,%f4,%f26
/* 0x029c	     */		fmuld	%f12,%f20,%f8
/* 0x02a0	     */		add	%l3,32,%l3
/* 0x02a4	     */		cmp	%o1,%o7
/* 0x02a8	     */		fmuld	%f14,%f20,%f14
/* 0x02ac	     */		ldd	[%l3-32],%f4
/* 0x02b0	     */		add	%g2,8,%g2
/* 0x02b4	     */		faddd	%f10,%f28,%f12
/* 0x02b8	     */		fmuld	%f6,%f22,%f6
/* 0x02bc	     */		ldd	[%l3-16],%f0
/* 0x02c0	     */		fmuld	%f24,%f22,%f10
/* 0x02c4	     */		std	%f26,[%l3-64]
/* 0x02c8	     */		faddd	%f2,%f12,%f2
/* 0x02cc	     */		std	%f2,[%l3-48]
/* 0x02d0	     */		faddd	%f14,%f6,%f6
/* 0x02d4	     */		faddd	%f8,%f10,%f2
/* 0x02d8	     */		faddd	%f4,%f6,%f4
/* 0x02dc	     */		std	%f4,[%l3-32]
/* 0x02e0	     */		faddd	%f0,%f2,%f0
/* 0x02e4	     */		bg,pn	%icc,.L77000213
/* 0x02e8	     */		std	%f0,[%l3-16]
                       .L77000284:
/* 0x02ec	 252 */		ldd	[%o0],%f0
                       .L900000657:
/* 0x02f0	 252 */		ldd	[%g2],%f4
/* 0x02f4	     */		fmuld	%f0,%f20,%f2
/* 0x02f8	     */		add	%o1,1,%o1
/* 0x02fc	     */		ldd	[%l3],%f0
/* 0x0300	     */		add	%o0,8,%o0
/* 0x0304	     */		add	%g2,8,%g2
/* 0x0308	     */		fmuld	%f4,%f22,%f4
/* 0x030c	     */		cmp	%o1,%o7
/* 0x0310	     */		faddd	%f2,%f4,%f2
/* 0x0314	     */		faddd	%f0,%f2,%f0
/* 0x0318	     */		std	%f0,[%l3]
/* 0x031c	     */		add	%l3,16,%l3
/* 0x0320	     */		ble,a,pt	%icc,.L900000657
/* 0x0324	     */		ldd	[%o0],%f0
                       .L77000213:
/* 0x0328	     */		srl	%g4,31,%g2
                       .L900000658:
/* 0x032c	 254 */		cmp	%l1,30
/* 0x0330	     */		bne,a,pt	%icc,.L900000656
/* 0x0334	     */		fdtox	%f18,%f0
/* 0x0338	     */		add	%g4,%g2,%g2
/* 0x033c	     */		sra	%g2,1,%o0
/* 0x0340	 281 */		ldd	[%l0],%f0
/* 0x0344	     */		sll	%i5,1,%o2
/* 0x0348	     */		add	%o0,1,%g2
/* 0x034c	     */		sll	%g2,1,%o0
/* 0x0350	 254 */		sub	%o2,1,%o2
/* 0x0354	 281 */		fmovd	%f0,%f2
/* 0x0358	     */		sll	%g2,4,%o1
/* 0x035c	     */		cmp	%o0,%o3
/* 0x0360	     */		bge,pt	%icc,.L77000215
/* 0x0364	     */		or	%g0,0,%l1
/* 0x0368	 254 */		add	%i1,%o1,%o1
/* 0x036c	 281 */		ldd	[%o1],%f6
                       .L900000655:
/* 0x0370	     */		fdtox	%f6,%f10
/* 0x0374	     */		ldd	[%o1+8],%f4
/* 0x0378	     */		add	%o0,2,%o0
/* 0x037c	     */		ldd	[%l0],%f12
/* 0x0380	     */		fdtox	%f6,%f6
/* 0x0384	     */		cmp	%o0,%o2
/* 0x0388	     */		fdtox	%f4,%f8
/* 0x038c	     */		fdtox	%f4,%f4
/* 0x0390	     */		fmovs	%f12,%f10
/* 0x0394	     */		fmovs	%f12,%f8
/* 0x0398	     */		fxtod	%f10,%f10
/* 0x039c	     */		fxtod	%f8,%f8
/* 0x03a0	     */		faddd	%f10,%f2,%f2
/* 0x03a4	     */		std	%f2,[%o1]
/* 0x03a8	     */		faddd	%f8,%f0,%f0
/* 0x03ac	     */		std	%f0,[%o1+8]
/* 0x03b0	     */		add	%o1,16,%o1
/* 0x03b4	     */		fitod	%f6,%f2
/* 0x03b8	     */		fitod	%f4,%f0
/* 0x03bc	     */		ble,a,pt	%icc,.L900000655
/* 0x03c0	     */		ldd	[%o1],%f6
                       .L77000233:
/* 0x03c4	     */		or	%g0,0,%l1
                       .L77000215:
/* 0x03c8	     */		fdtox	%f18,%f0
                       .L900000656:
/* 0x03cc	     */		ldd	[%l0],%f6
/* 0x03d0	 256 */		add	%g4,1,%g4
/* 0x03d4	     */		add	%l2,8,%l2
/* 0x03d8	     */		ldd	[%g5],%f2
/* 0x03dc	     */		add	%l1,1,%l1
/* 0x03e0	     */		add	%o5,8,%o5
/* 0x03e4	     */		fmovs	%f6,%f0
/* 0x03e8	     */		ldd	[%g1],%f4
/* 0x03ec	     */		cmp	%g4,%g3
/* 0x03f0	     */		fxtod	%f0,%f0
/* 0x03f4	     */		fmuld	%f0,%f16,%f0
/* 0x03f8	     */		fmuld	%f0,%f2,%f2
/* 0x03fc	     */		fdtox	%f2,%f2
/* 0x0400	     */		fxtod	%f2,%f2
/* 0x0404	     */		fmuld	%f2,%f4,%f2
/* 0x0408	     */		fsubd	%f0,%f2,%f22
/* 0x040c	     */		ble,a,pt	%icc,.L900000654
/* 0x0410	     */		ldd	[%i4],%f0
                       .L900000629:
/* 0x0414	 256 */		ba	.L900000653
/* 0x0418	     */		sll	%i0,4,%g2
                       .L77000279:
/* 0x041c	 261 */		ldd	[%o2],%f6
/* 0x0420	 279 */		or	%g0,%o0,%o4
/* 0x0424	 281 */		or	%g0,0,%o3
/* 0x0428	 261 */		ldd	[%i2],%f4
/* 0x042c	 273 */		std	%f0,[%o0+8]
/* 0x0430	     */		std	%f0,[%o0+16]
/* 0x0434	 261 */		fmuld	%f4,%f6,%f4
/* 0x0438	     */		std	%f4,[%o0]
/* 0x043c	 273 */		std	%f0,[%o0+24]
/* 0x0440	     */		std	%f0,[%o0+32]
/* 0x0444	     */		fdtox	%f4,%f4
/* 0x0448	     */		std	%f0,[%o0+40]
/* 0x044c	     */		std	%f0,[%o0+48]
/* 0x0450	     */		std	%f0,[%o0+56]
/* 0x0454	     */		std	%f0,[%o0+64]
/* 0x0458	     */		std	%f0,[%o0+72]
/* 0x045c	     */		std	%f0,[%o0+80]
/* 0x0460	     */		std	%f0,[%o0+88]
/* 0x0464	     */		std	%f0,[%o0+96]
/* 0x0468	     */		std	%f0,[%o0+104]
/* 0x046c	     */		std	%f0,[%o0+112]
/* 0x0470	     */		std	%f0,[%o0+120]
/* 0x0474	     */		std	%f0,[%o0+128]
/* 0x0478	     */		std	%f0,[%o0+136]
/* 0x047c	     */		std	%f0,[%o0+144]
/* 0x0480	     */		std	%f0,[%o0+152]
/* 0x0484	     */		std	%f0,[%o0+160]
/* 0x0488	     */		std	%f0,[%o0+168]
/* 0x048c	     */		fmovs	%f0,%f4
/* 0x0490	     */		std	%f0,[%o0+176]
/* 0x0494	 281 */		or	%g0,0,%o1
/* 0x0498	 273 */		std	%f0,[%o0+184]
/* 0x049c	     */		fxtod	%f4,%f4
/* 0x04a0	     */		std	%f0,[%o0+192]
/* 0x04a4	     */		std	%f0,[%o0+200]
/* 0x04a8	     */		std	%f0,[%o0+208]
/* 0x04ac	 278 */		fmuld	%f4,%f2,%f2
/* 0x04b0	 273 */		std	%f0,[%o0+216]
/* 0x04b4	     */		std	%f0,[%o0+224]
/* 0x04b8	     */		std	%f0,[%o0+232]
/* 0x04bc	     */		std	%f0,[%o0+240]
/* 0x04c0	     */		std	%f0,[%o0+248]
/* 0x04c4	     */		std	%f0,[%o0+256]
/* 0x04c8	     */		std	%f0,[%o0+264]
/* 0x04cc	     */		std	%f0,[%o0+272]
/* 0x04d0	     */		std	%f0,[%o0+280]
/* 0x04d4	     */		std	%f0,[%o0+288]
/* 0x04d8	     */		std	%f0,[%o0+296]
/* 0x04dc	     */		std	%f0,[%o0+304]
/* 0x04e0	     */		std	%f0,[%o0+312]
/* 0x04e4	     */		std	%f0,[%o0+320]
/* 0x04e8	     */		std	%f0,[%o0+328]
/* 0x04ec	     */		std	%f0,[%o0+336]
/* 0x04f0	     */		std	%f0,[%o0+344]
/* 0x04f4	     */		std	%f0,[%o0+352]
/* 0x04f8	     */		std	%f0,[%o0+360]
/* 0x04fc	     */		std	%f0,[%o0+368]
/* 0x0500	     */		std	%f0,[%o0+376]
/* 0x0504	     */		std	%f0,[%o0+384]
/* 0x0508	     */		std	%f0,[%o0+392]
/* 0x050c	     */		std	%f0,[%o0+400]
/* 0x0510	     */		std	%f0,[%o0+408]
/* 0x0514	     */		std	%f0,[%o0+416]
/* 0x0518	     */		std	%f0,[%o0+424]
/* 0x051c	     */		std	%f0,[%o0+432]
/* 0x0520	     */		std	%f0,[%o0+440]
/* 0x0524	     */		std	%f0,[%o0+448]
/* 0x0528	     */		std	%f0,[%o0+456]
/* 0x052c	     */		std	%f0,[%o0+464]
/* 0x0530	     */		std	%f0,[%o0+472]
/* 0x0534	     */		std	%f0,[%o0+480]
/* 0x0538	     */		std	%f0,[%o0+488]
/* 0x053c	     */		std	%f0,[%o0+496]
/* 0x0540	     */		std	%f0,[%o0+504]
/* 0x0544	     */		std	%f0,[%o0+512]
/* 0x0548	     */		std	%f0,[%o0+520]
/* 0x054c	     */		ldd	[%g5],%f0
/* 0x0550	     */		ldd	[%g1],%f8
/* 0x0554	     */		fmuld	%f2,%f0,%f6
/* 0x0558	 275 */		ldd	[%i4],%f4
/* 0x055c	 276 */		ldd	[%i2],%f0
/* 0x0560	     */		fdtox	%f6,%f6
/* 0x0564	     */		fxtod	%f6,%f6
/* 0x0568	     */		fmuld	%f6,%f8,%f6
/* 0x056c	     */		fsubd	%f2,%f6,%f2
/* 0x0570	 286 */		fmuld	%f4,%f2,%f12

!  282		      !       {
!  284		      !	 m2j=pdm2[j];
!  285		      !	 a=pdtj[0]+pdn_0*digit;
!  286		      !	 b=pdtj[1]+pdm1_0*pdm2[j+1]+a*TwoToMinus16;

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
	fmovd %f16,%f18			! hand modified
	ldd [%i4],%f2
	ldd [%o4],%f8
	ldd [%i2],%f10
	ldd [%g5],%f14		! hand modified
	ldd [%g1],%f16		! hand modified
	ldd [%i3],%f24

	ldd [%i2+8],%f26
	ldd [%i2+16],%f40
	ldd [%i2+48],%f46
	ldd [%i2+56],%f30
	ldd [%i2+64],%f54
	ldd [%i2+104],%f34
	ldd [%i2+112],%f58

	ldd [%i4+8],%f28	
	ldd [%i4+104],%f38
	ldd [%i4+112],%f60

	.L99999999: 			!1
	ldd	[%i2+24],%f32
	fmuld	%f0,%f2,%f4 	!2
	ldd	[%i4+24],%f36
	fmuld	%f26,%f24,%f20 	!3
	ldd	[%i2+40],%f42
	fmuld	%f28,%f0,%f22 	!4
	ldd	[%i4+40],%f44
	fmuld	%f32,%f24,%f32 	!5
	ldd	[%i3+8],%f6
	faddd	%f4,%f8,%f4
	fmuld	%f36,%f0,%f36 	!6
	add	%i3,8,%i3
	ldd	[%i4+56],%f50
	fmuld	%f42,%f24,%f42 	!7
	ldd	[%i2+72],%f52
	faddd	%f20,%f22,%f20
	fmuld	%f44,%f0,%f44 	!8
	ldd	[%o4+16],%f22
	fmuld	%f10,%f6,%f12 	!9
	ldd	[%i4+72],%f56
	faddd	%f32,%f36,%f32
	fmuld	%f14,%f4,%f4 !10
	ldd	[%o4+48],%f36
	fmuld	%f30,%f24,%f48 	!11
	ldd	[%o4+8],%f8
	faddd	%f20,%f22,%f20
	fmuld	%f50,%f0,%f50	!12
	std	%f20,[%o4+16]
	faddd	%f42,%f44,%f42
	fmuld	%f52,%f24,%f52 	!13
	ldd	[%o4+80],%f44
	faddd	%f4,%f12,%f4
	fmuld	%f56,%f0,%f56 	!14
	ldd	[%i2+88],%f20
	faddd	%f32,%f36,%f32 	!15
	ldd	[%i4+88],%f22
	faddd	%f48,%f50,%f48 	!16
	ldd	[%o4+112],%f50
	faddd	%f52,%f56,%f52 	!17
	ldd	[%o4+144],%f56
	faddd	%f4,%f8,%f8
	fmuld	%f20,%f24,%f20 	!18
	std	%f32,[%o4+48]
	faddd	%f42,%f44,%f42
	fmuld	%f22,%f0,%f22 	!19
	std	%f42,[%o4+80]
	faddd	%f48,%f50,%f48
	fmuld	%f34,%f24,%f32 	!20
	std	%f48,[%o4+112]
	faddd	%f52,%f56,%f52
	fmuld	%f38,%f0,%f36 	!21
	ldd	[%i2+120],%f42
	fdtox	%f8,%f4 		!22
	std	%f52,[%o4+144]
	faddd	%f20,%f22,%f20 	!23
	ldd	[%i4+120],%f44 	!24
	ldd	[%o4+176],%f22
	faddd	%f32,%f36,%f32
	fmuld	%f42,%f24,%f42 	!25
	ldd	[%i4+16],%f50
	fmovs	%f17,%f4 	!26
	ldd	[%i2+32],%f52
	fmuld	%f44,%f0,%f44 	!27
	ldd	[%i4+32],%f56
	fmuld	%f40,%f24,%f48 	!28
	ldd	[%o4+208],%f36
	faddd	%f20,%f22,%f20
	fmuld	%f50,%f0,%f50 	!29
	std	%f20,[%o4+176]
	fxtod	%f4,%f4
	fmuld	%f52,%f24,%f52 	!30
	ldd	[%i4+48],%f22
	faddd	%f42,%f44,%f42
	fmuld	%f56,%f0,%f56 	!31
	ldd	[%o4+240],%f44
	faddd	%f32,%f36,%f32 	!32
	std	%f32,[%o4+208]
	faddd	%f48,%f50,%f48
	fmuld	%f46,%f24,%f20 	!33
	ldd	[%o4+32],%f50
	fmuld	%f4,%f18,%f12 	!34
	ldd	[%i4+64],%f36
	faddd	%f52,%f56,%f52
	fmuld	%f22,%f0,%f22 	!35
	ldd	[%o4+64],%f56
	faddd	%f42,%f44,%f42 	!36
	std	%f42,[%o4+240]
	faddd	%f48,%f50,%f48
	fmuld	%f54,%f24,%f32 	!37
	std	%f48,[%o4+32]
	fmuld	%f12,%f14,%f4 !38
	ldd	[%i2+80],%f42
	faddd	%f52,%f56,%f56	! yes, tmp52!
	fmuld	%f36,%f0,%f36 	!39
	ldd	[%i4+80],%f44
	faddd	%f20,%f22,%f20 	!40
	ldd	[%i2+96],%f48
	fmuld	%f58,%f24,%f52 	!41
	ldd	[%i4+96],%f50
	fdtox	%f4,%f4
	fmuld	%f42,%f24,%f42 	!42
	std	%f56,[%o4+64]	! yes, tmp52!
	faddd	%f32,%f36,%f32
	fmuld	%f44,%f0,%f44 	!43
	ldd	[%o4+96],%f22
	fmuld	%f48,%f24,%f48 	!44
	ldd	[%o4+128],%f36
	fmovd	%f6,%f24
	fmuld	%f50,%f0,%f50 	!45
	fxtod	%f4,%f4
	fmuld	%f60,%f0,%f56 	!46
	add	%o4,8,%o4
	faddd	%f42,%f44,%f42 	!47
	ldd	[%o4+160-8],%f44
	faddd	%f20,%f22,%f20 	!48
	std	%f20,[%o4+96-8]
	faddd	%f48,%f50,%f48 	!49
	ldd	[%o4+192-8],%f50
	faddd	%f52,%f56,%f52
	fmuld	%f4,%f16,%f4 	!50
	ldd	[%o4+224-8],%f56
	faddd	%f32,%f36,%f32 	!51
	std	%f32,[%o4+128-8]
	faddd	%f42,%f44,%f42 	!52
	add	%o3,1,%o3
	std	%f42,[%o4+160-8]
	faddd	%f48,%f50,%f48 	!53
	cmp	%o3,31
	std	%f48,[%o4+192-8]
	fsubd	%f12,%f4,%f0 	!54
	faddd	%f52,%f56,%f52
	ble,pt	%icc,.L99999999
	std	%f52,[%o4+224-8] 	!55
	std %f8,[%o4]

!  312		      !       }
!  313		      !   }
!  315		      ! conv_d16_to_i32(result,dt+2*nlen,(long long *)dt,nlen+1);

/* 0x07c8	 315 */		sll	%i0,4,%g2
                       .L900000653:
/* 0x07cc	 315 */		add	%i1,%g2,%i1
/* 0x07d0	 242 */		ld	[%fp+68],%o0
/* 0x07d4	 315 */		or	%g0,0,%o4
/* 0x07d8	     */		ldd	[%i1],%f0
/* 0x07dc	     */		or	%g0,0,%g5
/* 0x07e0	     */		cmp	%i0,0
/* 0x07e4	 242 */		or	%g0,%o0,%o3
/* 0x07e8	 311 */		sub	%i0,1,%g1
/* 0x07ec	 315 */		fdtox	%f0,%f0
/* 0x07f0	     */		std	%f0,[%sp+120]
/* 0x07f4	 311 */		sethi	%hi(0xfc00),%o1
/* 0x07f8	     */		add	%g1,1,%g3
/* 0x07fc	     */		or	%g0,%o0,%g4
/* 0x0800	 315 */		ldd	[%i1+8],%f0
/* 0x0804	     */		add	%o1,1023,%o1
/* 0x0808	     */		fdtox	%f0,%f0
/* 0x080c	     */		std	%f0,[%sp+112]
/* 0x0810	     */		ldx	[%sp+112],%o5
/* 0x0814	     */		ldx	[%sp+120],%o7
/* 0x0818	     */		ble,pt	%icc,.L900000651
/* 0x081c	     */		sethi	%hi(0xfc00),%g2
/* 0x0820	 311 */		or	%g0,-1,%g2
/* 0x0824	 315 */		cmp	%g3,3
/* 0x0828	 311 */		srl	%g2,0,%o2
/* 0x082c	 315 */		bl,pn	%icc,.L77000287
/* 0x0830	     */		or	%g0,%i1,%g2
/* 0x0834	     */		ldd	[%i1+16],%f0
/* 0x0838	     */		and	%o5,%o1,%o0
/* 0x083c	     */		add	%i1,16,%g2
/* 0x0840	     */		sllx	%o0,16,%g3
/* 0x0844	     */		and	%o7,%o2,%o0
/* 0x0848	     */		fdtox	%f0,%f0
/* 0x084c	     */		std	%f0,[%sp+104]
/* 0x0850	     */		add	%o0,%g3,%o4
/* 0x0854	     */		ldd	[%i1+24],%f2
/* 0x0858	     */		srax	%o5,16,%o0
/* 0x085c	     */		add	%o3,4,%g4
/* 0x0860	     */		stx	%o0,[%sp+128]
/* 0x0864	     */		and	%o4,%o2,%o0
/* 0x0868	     */		stx	%o0,[%sp+112]
/* 0x086c	     */		srax	%o4,32,%o0
/* 0x0870	     */		fdtox	%f2,%f0
/* 0x0874	     */		stx	%o0,[%sp+136]
/* 0x0878	     */		srax	%o7,32,%o4
/* 0x087c	     */		std	%f0,[%sp+96]
/* 0x0880	     */		ldx	[%sp+128],%g5
/* 0x0884	     */		ldx	[%sp+136],%o7
/* 0x0888	     */		ldx	[%sp+104],%g3
/* 0x088c	     */		add	%g5,%o7,%o0
/* 0x0890	     */		or	%g0,1,%g5
/* 0x0894	     */		ldx	[%sp+112],%o7
/* 0x0898	     */		add	%o4,%o0,%o4
/* 0x089c	     */		ldx	[%sp+96],%o5
/* 0x08a0	     */		st	%o7,[%o3]
/* 0x08a4	     */		or	%g0,%g3,%o7
                       .L900000634:
/* 0x08a8	     */		ldd	[%g2+16],%f0
/* 0x08ac	     */		add	%g5,1,%g5
/* 0x08b0	     */		add	%g4,4,%g4
/* 0x08b4	     */		cmp	%g5,%g1
/* 0x08b8	     */		add	%g2,16,%g2
/* 0x08bc	     */		fdtox	%f0,%f0
/* 0x08c0	     */		std	%f0,[%sp+104]
/* 0x08c4	     */		ldd	[%g2+8],%f0
/* 0x08c8	     */		fdtox	%f0,%f0
/* 0x08cc	     */		std	%f0,[%sp+96]
/* 0x08d0	     */		and	%o5,%o1,%g3
/* 0x08d4	     */		sllx	%g3,16,%g3
/* 0x08d8	     */		stx	%g3,[%sp+120]
/* 0x08dc	     */		and	%o7,%o2,%g3
/* 0x08e0	     */		stx	%o7,[%sp+128]
/* 0x08e4	     */		ldx	[%sp+120],%o7
/* 0x08e8	     */		add	%g3,%o7,%g3
/* 0x08ec	     */		ldx	[%sp+128],%o7
/* 0x08f0	     */		srax	%o5,16,%o5
/* 0x08f4	     */		add	%g3,%o4,%g3
/* 0x08f8	     */		srax	%g3,32,%o4
/* 0x08fc	     */		stx	%o4,[%sp+112]
/* 0x0900	     */		srax	%o7,32,%o4
/* 0x0904	     */		ldx	[%sp+112],%o7
/* 0x0908	     */		add	%o5,%o7,%o7
/* 0x090c	     */		ldx	[%sp+96],%o5
/* 0x0910	     */		add	%o4,%o7,%o4
/* 0x0914	     */		and	%g3,%o2,%g3
/* 0x0918	     */		ldx	[%sp+104],%o7
/* 0x091c	     */		ble,pt	%icc,.L900000634
/* 0x0920	     */		st	%g3,[%g4-4]
                       .L900000637:
/* 0x0924	     */		ba	.L900000651
/* 0x0928	     */		sethi	%hi(0xfc00),%g2
                       .L77000287:
/* 0x092c	     */		ldd	[%g2+16],%f0
                       .L900000650:
/* 0x0930	     */		and	%o7,%o2,%o0
/* 0x0934	     */		and	%o5,%o1,%g3
/* 0x0938	     */		fdtox	%f0,%f0
/* 0x093c	     */		add	%o4,%o0,%o0
/* 0x0940	     */		std	%f0,[%sp+104]
/* 0x0944	     */		add	%g5,1,%g5
/* 0x0948	     */		sllx	%g3,16,%o4
/* 0x094c	     */		ldd	[%g2+24],%f2
/* 0x0950	     */		add	%g2,16,%g2
/* 0x0954	     */		add	%o0,%o4,%o4
/* 0x0958	     */		cmp	%g5,%g1
/* 0x095c	     */		srax	%o5,16,%o0
/* 0x0960	     */		stx	%o0,[%sp+112]
/* 0x0964	     */		and	%o4,%o2,%g3
/* 0x0968	     */		srax	%o4,32,%o5
/* 0x096c	     */		fdtox	%f2,%f0
/* 0x0970	     */		std	%f0,[%sp+96]
/* 0x0974	     */		srax	%o7,32,%o4
/* 0x0978	     */		ldx	[%sp+112],%o7
/* 0x097c	     */		add	%o7,%o5,%o7
/* 0x0980	     */		ldx	[%sp+104],%o5
/* 0x0984	     */		add	%o4,%o7,%o4
/* 0x0988	     */		ldx	[%sp+96],%o0
/* 0x098c	     */		st	%g3,[%g4]
/* 0x0990	     */		or	%g0,%o5,%o7
/* 0x0994	     */		add	%g4,4,%g4
/* 0x0998	     */		or	%g0,%o0,%o5
/* 0x099c	     */		ble,a,pt	%icc,.L900000650
/* 0x09a0	     */		ldd	[%g2+16],%f0
                       .L77000236:
/* 0x09a4	     */		sethi	%hi(0xfc00),%g2
                       .L900000651:
/* 0x09a8	     */		or	%g0,-1,%o0
/* 0x09ac	     */		add	%g2,1023,%g2
/* 0x09b0	     */		ld	[%fp+88],%o1
/* 0x09b4	     */		srl	%o0,0,%g3
/* 0x09b8	     */		and	%o5,%g2,%g2
/* 0x09bc	     */		and	%o7,%g3,%g4

!  317		      ! adjust_montf_result(result,nint,nlen); 

/* 0x09c0	 317 */		or	%g0,-1,%o5
/* 0x09c4	 311 */		sllx	%g2,16,%g2
/* 0x09c8	     */		add	%o4,%g4,%g4
/* 0x09cc	     */		add	%g4,%g2,%g2
/* 0x09d0	     */		sll	%g5,2,%g4
/* 0x09d4	     */		and	%g2,%g3,%g2
/* 0x09d8	     */		st	%g2,[%o3+%g4]
/* 0x09dc	 317 */		sll	%i0,2,%g2
/* 0x09e0	     */		ld	[%o3+%g2],%g2
/* 0x09e4	     */		cmp	%g2,0
/* 0x09e8	     */		bleu,pn	%icc,.L77000241
/* 0x09ec	     */		or	%g0,%o1,%o2
/* 0x09f0	     */		ba	.L900000649
/* 0x09f4	     */		cmp	%o5,0
                       .L77000241:
/* 0x09f8	     */		sub	%i0,1,%o5
/* 0x09fc	     */		sll	%o5,2,%g2
/* 0x0a00	     */		cmp	%o5,0
/* 0x0a04	     */		bl,pt	%icc,.L900000649
/* 0x0a08	     */		cmp	%o5,0
/* 0x0a0c	     */		add	%o1,%g2,%o1
/* 0x0a10	     */		add	%o3,%g2,%o4
/* 0x0a14	     */		ld	[%o1],%g2
                       .L900000648:
/* 0x0a18	     */		ld	[%o4],%g3
/* 0x0a1c	     */		sub	%o5,1,%o0
/* 0x0a20	     */		sub	%o1,4,%o1
/* 0x0a24	     */		sub	%o4,4,%o4
/* 0x0a28	     */		cmp	%g3,%g2
/* 0x0a2c	     */		bne,pn	%icc,.L77000244
/* 0x0a30	     */		nop
/* 0x0a34	   0 */		or	%g0,%o0,%o5
/* 0x0a38	 317 */		cmp	%o0,0
/* 0x0a3c	     */		bge,a,pt	%icc,.L900000648
/* 0x0a40	     */		ld	[%o1],%g2
                       .L77000244:
/* 0x0a44	     */		cmp	%o5,0
                       .L900000649:
/* 0x0a48	     */		bl,pn	%icc,.L77000288
/* 0x0a4c	     */		sll	%o5,2,%g2
/* 0x0a50	     */		ld	[%o2+%g2],%g3
/* 0x0a54	     */		ld	[%o3+%g2],%g2
/* 0x0a58	     */		cmp	%g2,%g3
/* 0x0a5c	     */		bleu,pt	%icc,.L77000224
/* 0x0a60	     */		nop
                       .L77000288:
/* 0x0a64	     */		cmp	%i0,0
/* 0x0a68	     */		ble,pt	%icc,.L77000224
/* 0x0a6c	     */		nop
/* 0x0a70	 317 */		sub	%i0,1,%o7
/* 0x0a74	     */		or	%g0,-1,%g2
/* 0x0a78	     */		srl	%g2,0,%o4
/* 0x0a7c	     */		add	%o7,1,%o0
/* 0x0a80	 315 */		or	%g0,0,%o5
/* 0x0a84	     */		or	%g0,0,%g1
/* 0x0a88	     */		cmp	%o0,3
/* 0x0a8c	     */		bl,pn	%icc,.L77000289
/* 0x0a90	     */		add	%o3,8,%o1
/* 0x0a94	     */		add	%o2,4,%o0
/* 0x0a98	     */		ld	[%o1-8],%g2
/* 0x0a9c	   0 */		or	%g0,%o1,%o3
/* 0x0aa0	 315 */		ld	[%o0-4],%g3
/* 0x0aa4	   0 */		or	%g0,%o0,%o2
/* 0x0aa8	 315 */		or	%g0,2,%g1
/* 0x0aac	     */		ld	[%o3-4],%o0
/* 0x0ab0	     */		sub	%g2,%g3,%g2
/* 0x0ab4	     */		or	%g0,%g2,%o5
/* 0x0ab8	     */		and	%g2,%o4,%g2
/* 0x0abc	     */		st	%g2,[%o3-8]
/* 0x0ac0	     */		srax	%o5,32,%o5
                       .L900000638:
/* 0x0ac4	     */		ld	[%o2],%g2
/* 0x0ac8	     */		add	%g1,1,%g1
/* 0x0acc	     */		add	%o2,4,%o2
/* 0x0ad0	     */		cmp	%g1,%o7
/* 0x0ad4	     */		add	%o3,4,%o3
/* 0x0ad8	     */		sub	%o0,%g2,%o0
/* 0x0adc	     */		add	%o0,%o5,%o5
/* 0x0ae0	     */		and	%o5,%o4,%g2
/* 0x0ae4	     */		ld	[%o3-4],%o0
/* 0x0ae8	     */		st	%g2,[%o3-8]
/* 0x0aec	     */		ble,pt	%icc,.L900000638
/* 0x0af0	     */		srax	%o5,32,%o5
                       .L900000641:
/* 0x0af4	     */		ld	[%o2],%o1
/* 0x0af8	     */		sub	%o0,%o1,%o0
/* 0x0afc	     */		add	%o0,%o5,%o0
/* 0x0b00	     */		and	%o0,%o4,%o1
/* 0x0b04	     */		st	%o1,[%o3-4]
/* 0x0b08	     */		ret	! Result = 
/* 0x0b0c	     */		restore	%g0,%g0,%g0
                       .L77000289:
/* 0x0b10	     */		ld	[%o3],%o0
                       .L900000647:
/* 0x0b14	     */		ld	[%o2],%o1
/* 0x0b18	     */		add	%o5,%o0,%o0
/* 0x0b1c	     */		add	%g1,1,%g1
/* 0x0b20	     */		add	%o2,4,%o2
/* 0x0b24	     */		cmp	%g1,%o7
/* 0x0b28	     */		sub	%o0,%o1,%o0
/* 0x0b2c	     */		and	%o0,%o4,%o1
/* 0x0b30	     */		st	%o1,[%o3]
/* 0x0b34	     */		add	%o3,4,%o3
/* 0x0b38	     */		srax	%o0,32,%o5
/* 0x0b3c	     */		ble,a,pt	%icc,.L900000647
/* 0x0b40	     */		ld	[%o3],%o0
                       .L77000224:
/* 0x0b44	     */		ret	! Result = 
/* 0x0b48	     */		restore	%g0,%g0,%g0
/* 0x0b4c	   0 */		.type	mont_mulf_noconv,2
/* 0x0b4c	     */		.size	mont_mulf_noconv,(.-mont_mulf_noconv)

