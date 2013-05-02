!  
! This Source Code Form is subject to the terms of the Mozilla Public
! License, v. 2.0. If a copy of the MPL was not distributed with this
! file, You can obtain one at http://mozilla.org/MPL/2.0/.

!
! double upper32(double /*frs1*/);
!
        .inline upper32,8
	fdtox	%f0,%f10
	fitod	%f10,%f0
        .end

!
! double lower32(double /*frs1*/, double /* Zero */);
!
        .inline lower32,8
	fdtox	%f0,%f10
	fmovs	%f2,%f10
	fxtod	%f10,%f0
        .end

!
! double mod(double /*x*/, double /*1/m*/, double /*m*/);
!
        .inline mod,12
	fmuld	%f0,%f2,%f2
	fdtox	%f2,%f2
	fxtod	%f2,%f2
	fmuld	%f2,%f4,%f2
	fsubd	%f0,%f2,%f0
        .end


!
! void i16_to_d16_and_d32x4(double * /*1/(2^16)*/, double * /* 2^16*/,
!			    double * /* 0 */,
!			    double * /*result16*/, double * /* result32 */
!			    float *  /*source - should be unsigned int*
!		            	       converted to float* */);
!
        .inline i16_to_d16_and_d32x4,24
        ldd     [%o0],%f2  ! 1/(2^16)
        ldd     [%o1],%f4  ! 2^16
	ldd	[%o2],%f22

	fmovd	%f22,%f6
	ld	[%o5],%f7
	fmovd	%f22,%f10
	ld	[%o5+4],%f11
	fmovd	%f22,%f14
	ld	[%o5+8],%f15
	fmovd	%f22,%f18
	ld	[%o5+12],%f19
	fxtod	%f6,%f6
	std	%f6,[%o4]
	fxtod	%f10,%f10
	std	%f10,[%o4+8]
	fxtod	%f14,%f14
	std	%f14,[%o4+16]
	fxtod	%f18,%f18
	std	%f18,[%o4+24]
	fmuld	%f2,%f6,%f8
	fmuld	%f2,%f10,%f12
	fmuld	%f2,%f14,%f16
	fmuld	%f2,%f18,%f20
	fdtox	%f8,%f8
	fdtox	%f12,%f12
	fdtox	%f16,%f16
	fdtox	%f20,%f20
	fxtod	%f8,%f8
	std	%f8,[%o3+8]
	fxtod	%f12,%f12
	std	%f12,[%o3+24]
	fxtod	%f16,%f16
	std	%f16,[%o3+40]
	fxtod	%f20,%f20
	std	%f20,[%o3+56]
	fmuld	%f8,%f4,%f8
	fmuld	%f12,%f4,%f12
	fmuld	%f16,%f4,%f16
	fmuld	%f20,%f4,%f20
	fsubd	%f6,%f8,%f8
	std	%f8,[%o3]
	fsubd	%f10,%f12,%f12
	std	%f12,[%o3+16]
	fsubd	%f14,%f16,%f16
	std	%f16,[%o3+32]
	fsubd	%f18,%f20,%f20
	std	%f20,[%o3+48]
        .end


