// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

//+-----------------------------------------------------------------------------
//
//  class Bezier32
//
//  Bezier cracker.
//
//  A hybrid cubic Bezier curve flattener based on KirkO's error factor.
//  Generates line segments fast without using the stack.  Used to flatten a
//  path.
//
//  For an understanding of the methods used, see:
//
//  Kirk Olynyk, "..."
//  Goossen and Olynyk, "System and Method of Hybrid Forward
//      Differencing to Render Bezier Splines"
//  Lien, Shantz and Vaughan Pratt, "Adaptive Forward Differencing for
//  Rendering Curves and Surfaces", Computer Graphics, July 1987
//  Chang and Shantz, "Rendering Trimmed NURBS with Adaptive Forward
//      Differencing", Computer Graphics, August 1988
//  Foley and Van Dam, "Fundamentals of Interactive Computer Graphics"
//
//  Public Interface:
//      bInit(pptfx)                - pptfx points to 4 control points of
//                                    Bezier.  Current point is set to the first
//                                    point after the start-point.
//      Bezier32(pptfx)             - Constructor with initialization.
//      vGetCurrent(pptfx)          - Returns current polyline point.
//      bCurrentIsEndPoint()        - TRUE if current point is end-point.
//      vNext()                     - Moves to next polyline point.
//


#![allow(unused_parens)]
#![allow(non_upper_case_globals)]
//+-----------------------------------------------------------------------------
//

//
//  $TAG ENGR

//      $Module:    win_mil_graphics_geometry
//      $Keywords:
//
//  $Description:
//      Class for flattening a bezier.
//
//  $ENDTAG
//
//------------------------------------------------------------------------------

// First conversion from original 28.4 to 18.14 format
const HFD32_INITIAL_SHIFT: i32 = 10;

// Second conversion to 15.17 format
const HFD32_ADDITIONAL_SHIFT: i32 = 3;


// BEZIER_FLATTEN_GDI_COMPATIBLE:
//
// Don't turn on this switch without testing carefully. It's more for
// documentation's sake - to show the values that GDI used - for an error
// tolerance of 2/3.

// It turns out that 2/3 produces very noticable artifacts on antialiased lines,
// so we want to use 1/4 instead.
/* 
#ifdef BEZIER_FLATTEN_GDI_COMPATIBLE

// Flatten to an error of 2/3.  During initial phase, use 18.14 format.

#define TEST_MAGNITUDE_INITIAL    (6 * 0x00002aa0L)

// Error of 2/3.  During normal phase, use 15.17 format.

#define TEST_MAGNITUDE_NORMAL     (TEST_MAGNITUDE_INITIAL << 3)

#else
*/
use crate::types::*;
/* 
// Flatten to an error of 1/4.  During initial phase, use 18.14 format.

const TEST_MAGNITUDE_INITIAL: i32 =   (6 * 0x00001000);

// Error of 1/4.  During normal phase, use 15.17 format.

const TEST_MAGNITUDE_NORMAL: i32 =    (TEST_MAGNITUDE_INITIAL << 3);
*/

// I have modified the constants for HFD32 as part of fixing accuracy errors
// (Bug 816015).  Something similar could be done for the 64 bit hfd, but it ain't
// broke so I'd rather not fix it.

// The shift to the steady state 15.17 format
const HFD32_SHIFT: LONG = HFD32_INITIAL_SHIFT + HFD32_ADDITIONAL_SHIFT;

// Added to output numbers before rounding back to original representation
const HFD32_ROUND: LONG = 1 << (HFD32_SHIFT - 1);

// The error is tested on max(|e2|, |e3|), which represent 6 times the actual error.
// The flattening tolerance is hard coded to 1/4 in the original geometry space,
// which translates to 4 in 28.4 format.  So 6 times that is:

const HFD32_TOLERANCE: LONGLONG = 24;

// During the initial phase, while working in 18.14 format
const HFD32_INITIAL_TEST_MAGNITUDE: LONGLONG = HFD32_TOLERANCE << HFD32_INITIAL_SHIFT; 

// During the steady state, while working in 15.17 format
const HFD32_TEST_MAGNITUDE: LONGLONG = HFD32_INITIAL_TEST_MAGNITUDE << HFD32_ADDITIONAL_SHIFT; 

// We will stop halving the segment with basis e1, e2, e3, e4 when max(|e2|, |e3|)
// is less than HFD32_TOLERANCE.  The operation e2 = (e2 + e3) >> 3 in vHalveStepSize() may
// eat up 3 bits of accuracy. HfdBasis32 starts off with a pad of HFD32_SHIFT zeros, so
// we can stay exact up to HFD32_SHIFT/3 subdivisions.  Since every subdivision is guaranteed
// to shift max(|e2|, |e3|) at least by 2, we will subdivide no more than n times if the 
// initial max(|e2|, |e3|) is less than than HFD32_TOLERANCE << 2n.  But if the initial 
// max(|e2|, |e3|) is greater than HFD32_TOLERANCE >> (HFD32_SHIFT / 3) then we may not be
// able to flatten with the 32 bit hfd, so we need to resort to the 64 bit hfd. 

const HFD32_MAX_ERROR: INT = (HFD32_TOLERANCE as i32) << ((2 * HFD32_INITIAL_SHIFT) / 3);

// The maximum size of coefficients that can be handled by HfdBasis32.
const HFD32_MAX_SIZE: LONGLONG = 0xffffc000;

// Michka 9/12/03: I found this number in the the body of the code witout any explanation.
// My analysis suggests that we could get away with larger numbers, but if I'm wrong we 
// could be in big trouble, so let us stay conservative.
//
// In bInit() we subtract Min(Bezier coeffients) from the original coefficients, so after
// that 0 <= coefficients <= Bound, and the test will be Bound < HFD32_MAX_SIZE. When 
// switching to the HFD basis in bInit():
//   * e0 is the first Bezier coeffient, so abs(e0) <= Bound.
//   * e1 is a difference of non-negative coefficients so abs(e1) <= Bound. 
//   * e2 and e3 can be written as 12*(p - (q + r)/2) where p,q and r are coefficients.
//     0 <=(q + r)/2 <= Bound, so abs(p - (q + r)/2) <= 2*Bound, hence 
//     abs(e2), abs(e3) <= 12*Bound.
//
// During vLazyHalveStepSize we add e2 + e3, resulting in absolute value <= 24*Bound.
// Initially HfdBasis32 shifts the numbers by HFD32_INITIAL_SHIFT, so we need to handle 
// 24*bounds*(2^HFD32_SHIFT), and that needs to be less than 2^31. So the bounds need to
// be less than 2^(31-HFD32_INITIAL_SHIFT)/24).
//
// For speed, the algorithm uses & rather than < for comparison.  To facilitate that we 
// replace 24 by 32=2^5, and then the binary representation of the number is of the form
// 0...010...0 with HFD32_SHIFT+5 trailing zeros.  By subtracting that from 2^32 = 0xffffffff+1 
// we get a number that is 1..110...0 with the same number of trailing zeros, and that can be 
// used with an & for comparison.  So the number should be:
//
//      0xffffffffL - (1L << (31 - HFD32_INITIAL_SHIFT - 5)) + 1 = (1L << 16) + 1 = 0xffff0000
//
// For the current values of HFD32_INITIAL_SHIFT=10 and HFD32_ADDITIONAL_SHIFT=3, the steady
// state doesn't pose additional requirements, as shown below. 
//
// For some reason the current code uses 0xfffc0000 = (1L << 14) + 1.
//
// Here is why the steady state doesn't pose additional requirements:
//
// In vSteadyState we multiply e0 and e1 by 8, so the requirement is Bounds*2^13 < 2^31,
// or Bounds < 2^18, less stringent than the above.
//
// In vLazyHalveStepSize we cut the error down by subdivision, making abs(e2) and abs(e3) 
// less than HFD32_TEST_MAGNITUDE = 24*2^13,  well below 2^31.
//
// During all the steady-state operations - vTakeStep, vHalveStepSize and vDoubleStepSize, 
// e0 is on the curve and e1 is a difference of 2 points on the curve, so
// abs(e0), abs(e1) < Bounds * 2^13, which requires Bound < 2^(31-13) = 2^18.  e2 and e3
// are errors, kept below 6*HFD32_TEST_MAGNITUDE = 216*2^13.  Details:
//
// In vTakeStep e2 = 2e2 - e3 keeps abs(e2) < 3*HFD32_TEST_MAGNITUDE =  72*2^13,  
// well below 2^31
//
// In vHalveStepSize we add e2 + e3 when their absolute is < 3*HFD32_TEST_MAGNITUDE (because
// this comes after a step), so that keeps the result below 6*HFD32_TEST_MAGNITUDE = 216*2^13.
// 
// In vDoubleStepSize we know that abs(e2), abs(e3) < HFD32_TEST_MAGNITUDE/4, otherwise we
// would not have doubled the step.

#[derive(Default)]
struct HfdBasis32
{
    e0: LONG,
    e1: LONG,
    e2: LONG,
    e3: LONG,
}

impl HfdBasis32 {
    fn lParentErrorDividedBy4(&self) -> LONG { 
        self.e3.abs().max((self.e2 + self.e2 - self.e3).abs())
    }

    fn lError(&self) -> LONG             
    { 
        self.e2.abs().max(self.e3.abs())
    }

    fn fxValue(&self) -> INT
    { 
        return((self.e0 + HFD32_ROUND) >> HFD32_SHIFT); 
    }

    fn bInit(&mut self, p1: INT, p2: INT, p3: INT, p4: INT) -> bool
    {
    // Change basis and convert from 28.4 to 18.14 format:
    
        self.e0 = (p1                     ) << HFD32_INITIAL_SHIFT;
        self.e1 = (p4 - p1                ) << HFD32_INITIAL_SHIFT;
        
        self.e2 = 6 * (p2 - p3 - p3 + p4);
        self.e3 = 6 * (p1 - p2 - p2 + p3);

        if (self.lError() >= HFD32_MAX_ERROR)
        {
            // Large error, will require too many subdivision for this 32 bit hfd
            return false;
        }
        
        self.e2 <<= HFD32_INITIAL_SHIFT;
        self.e3 <<= HFD32_INITIAL_SHIFT;

        return true; 
    }
    
    fn vLazyHalveStepSize(&mut self, cShift: LONG)
    {
        self.e2 = self.ExactShiftRight(self.e2 + self.e3,  1);
        self.e1 = self.ExactShiftRight(self.e1 - self.ExactShiftRight(self.e2, cShift), 1);
    }
    
    fn vSteadyState(&mut self, cShift: LONG)
    {
    // We now convert from 18.14 fixed format to 15.17:
    
        self.e0 <<= HFD32_ADDITIONAL_SHIFT;
        self.e1 <<= HFD32_ADDITIONAL_SHIFT;
    
        let mut lShift = cShift - HFD32_ADDITIONAL_SHIFT;
    
        if (lShift < 0)
        {
            lShift = -lShift;
            self.e2 <<= lShift;
            self.e3 <<= lShift;
        }
        else
        {
            self.e2 >>= lShift;
            self.e3 >>= lShift;
        }
    }
    
    fn vHalveStepSize(&mut self)
    {
        self.e2 = self.ExactShiftRight(self.e2 + self.e3, 3);
        self.e1 = self.ExactShiftRight(self.e1 - self.e2, 1);
        self.e3 = self.ExactShiftRight(self.e3, 2);
    }
    
    fn vDoubleStepSize(&mut self)
    {
        self.e1 += self.e1 + self.e2;
        self.e3 <<= 2;
        self.e2 = (self.e2 << 3) - self.e3;
    }
    
    fn vTakeStep(&mut self)
    {
        self.e0 += self.e1;
        let lTemp = self.e2;
        self.e1 += lTemp;
        self.e2 += lTemp - self.e3;
        self.e3 = lTemp;
    }

    fn ExactShiftRight(&self, num: i32, shift: i32) -> i32
    {
        // Performs a shift to the right while asserting that we're not 
        // losing significant bits
     
        assert!(num == (num >> shift) << shift); 
        return num >> shift;
    }
}

fn vBoundBox(
    aptfx: &[POINT; 4]) -> RECT
{
    let mut left = aptfx[0].x;
    let mut right = aptfx[0].x;
    let mut top = aptfx[0].y;
    let mut bottom = aptfx[0].y;

    for i in 1..4
    {
        left = left.min(aptfx[i].x);
        top = top.min(aptfx[i].y);
        right = right.max(aptfx[i].x);
        bottom = bottom.max(aptfx[i].y);
    }

    // We make the bounds one pixel loose for the nominal width 
    // stroke case, which increases the bounds by half a pixel 
    // in every dimension:

    RECT { left: left - 16, top: top - 16, right: right + 16, bottom: bottom + 16}
}



fn bIntersect(
    a: &RECT,
    b: &RECT) -> bool
{
    return((a.left < b.right) &&
           (a.top < b.bottom) &&
           (a.right > b.left) &&
           (a.bottom > b.top));
}

#[derive(Default)]
pub struct Bezier32
{
    cSteps: LONG,
    x: HfdBasis32,
    y: HfdBasis32,
    rcfxBound: RECT
}
impl Bezier32 {
    
fn bInit(&mut self,
    aptfxBez: &[POINT; 4],
        // Pointer to 4 control points
    prcfxClip: Option<&RECT>) -> bool
        // Bound box of visible region (optional)
{
    let mut aptfx;
    let mut cShift = 0;    // Keeps track of 'lazy' shifts

    self.cSteps = 1;         // Number of steps to do before reach end of curve

    self.rcfxBound = vBoundBox(aptfxBez);

    aptfx = aptfxBez.clone();

    {
        let mut fxOr;
        let mut fxOffset;

        // find out if the coordinates minus the bounding box
        // exceed 10 bits
        fxOffset = self.rcfxBound.left;
        fxOr  = {aptfx[0].x -= fxOffset; aptfx[0].x};
        fxOr |= {aptfx[1].x -= fxOffset; aptfx[1].x};
        fxOr |= {aptfx[2].x -= fxOffset; aptfx[2].x};
        fxOr |= {aptfx[3].x -= fxOffset; aptfx[3].x};

        fxOffset = self.rcfxBound.top;
        fxOr |= {aptfx[0].y -= fxOffset; aptfx[0].y};
        fxOr |= {aptfx[1].y -= fxOffset; aptfx[1].y};
        fxOr |= {aptfx[2].y -= fxOffset; aptfx[2].y};
        fxOr |= {aptfx[3].y -= fxOffset; aptfx[3].y};

    // This 32 bit cracker can only handle points in a 10 bit space:

        if ((fxOr as i64 & HFD32_MAX_SIZE) != 0) {
            return false;
        }
    }

    if (!self.x.bInit(aptfx[0].x, aptfx[1].x, aptfx[2].x, aptfx[3].x))
    {
        return false;
    }
    if (!self.y.bInit(aptfx[0].y, aptfx[1].y, aptfx[2].y, aptfx[3].y))
    {
        return false;
    }
    

    if (match prcfxClip { None => true, Some(clip) => bIntersect(&self.rcfxBound, clip)})
    {
        
        loop {
            let lTestMagnitude = (HFD32_INITIAL_TEST_MAGNITUDE << cShift) as LONG;

            if (self.x.lError() <= lTestMagnitude && self.y.lError() <= lTestMagnitude) {
                break;
            }

            cShift += 2;
            self.x.vLazyHalveStepSize(cShift);
            self.y.vLazyHalveStepSize(cShift);
            self.cSteps <<= 1;
        }
    }

    self.x.vSteadyState(cShift);
    self.y.vSteadyState(cShift);

// Note that this handles the case where the initial error for
// the Bezier is already less than HFD32_TEST_MAGNITUDE:

    self.x.vTakeStep();
    self.y.vTakeStep();
    self.cSteps-=1;

    return true;
}


fn cFlatten(&mut self,
    mut pptfx: &mut [POINT],
    pbMore: &mut bool) -> i32
{
    let mut cptfx = pptfx.len();
    assert!(cptfx > 0);

    let cptfxOriginal = cptfx;

    while {
    // Return current point:
    
        pptfx[0].x = self.x.fxValue() + self.rcfxBound.left;
        pptfx[0].y = self.y.fxValue() + self.rcfxBound.top;
        pptfx = &mut pptfx[1..];
    
    // If cSteps == 0, that was the end point in the curve!
    
        if (self.cSteps == 0)
        {
            *pbMore = false;

            // '+1' because we haven't decremented 'cptfx' yet:

            return(cptfxOriginal - cptfx + 1) as i32;
        }
    
    // Okay, we have to step:
    
        if (self.x.lError().max(self.y.lError()) > HFD32_TEST_MAGNITUDE as LONG)
        {
            self.x.vHalveStepSize();
            self.y.vHalveStepSize();
            self.cSteps <<= 1;
        }
    
        // We are here after vTakeStep.  Before that the error max(|e2|,|e3|) was less
        // than HFD32_TEST_MAGNITUDE.  vTakeStep changed e2 to 2e2-e3. Since 
        // |2e2-e3| < max(|e2|,|e3|) << 2 and vHalveStepSize is guaranteed to reduce 
        // max(|e2|,|e3|) by >> 2, no more than one subdivision should be required to 
        // bring the new max(|e2|,|e3|) back to within HFD32_TEST_MAGNITUDE, so:
        assert!(self.x.lError().max(self.y.lError()) <= HFD32_TEST_MAGNITUDE as LONG);
    
        while (!(self.cSteps & 1 != 0) &&
               self.x.lParentErrorDividedBy4() <= (HFD32_TEST_MAGNITUDE as LONG >> 2) &&
               self.y.lParentErrorDividedBy4() <= (HFD32_TEST_MAGNITUDE as LONG >> 2))
        {
            self.x.vDoubleStepSize();
            self.y.vDoubleStepSize();
            self.cSteps >>= 1;
        }
    
        self.cSteps -=1 ;
        self.x.vTakeStep();
        self.y.vTakeStep();
        cptfx -= 1;
        cptfx != 0
    } {}

    *pbMore = true;
    return cptfxOriginal as i32;
}
}


///////////////////////////////////////////////////////////////////////////
// Bezier64
//
// All math is done using 64 bit fixed numbers in a 36.28 format.
//
// All drawing is done in a 31 bit space, then a 31 bit window offset
// is applied.  In the initial transform where we change to the HFD
// basis, e2 and e3 require the most bits precision: e2 = 6(p2 - 2p3 + p4).
// This requires an additional 4 bits precision -- hence we require 36 bits
// for the integer part, and the remaining 28 bits is given to the fraction.
//
// In rendering a Bezier, every 'subdivide' requires an extra 3 bits of
// fractional precision.  In order to be reversible, we can allow no
// error to creep in.  Since a INT coordinate is 32 bits, and we
// require an additional 4 bits as mentioned above, that leaves us
// 28 bits fractional precision -- meaning we can do a maximum of
// 9 subdivides.  Now, the maximum absolute error of a Bezier curve in 27
// bit integer space is 2^29 - 1.  But 9 subdivides reduces the error by a
// guaranteed factor of 2^18, meaning we can subdivide down only to an error
// of 2^11 before we overflow, when in fact we want to reduce error to less
// than 1.
//
// So what we do is HFD until we hit an error less than 2^11, reverse our
// basis transform to get the four control points of this smaller curve
// (rounding in the process to 32 bits), then invoke another copy of HFD
// on the reduced Bezier curve.  We again have enough precision, but since
// its starting error is less than 2^11, we can reduce error to 2^-7 before
// overflowing!  We'll start a low HFD after every step of the high HFD.
////////////////////////////////////////////////////////////////////////////
#[derive(Default)]
struct HfdBasis64
{
    e0: LONGLONG,
    e1: LONGLONG,
    e2: LONGLONG,
    e3: LONGLONG,
}

impl HfdBasis64 {
fn vParentError(&self) -> LONGLONG
{
    (self.e3 << 2).abs().max(((self.e2 << 3) - (self.e3 << 2)).abs())
}

fn vError(&self) -> LONGLONG
{
    self.e2.abs().max(self.e3.abs())
}

fn fxValue(&self) -> INT
{
// Convert from 36.28 and round:

    let mut eq = self.e0;
    eq += (1 << (BEZIER64_FRACTION - 1));
    eq >>= BEZIER64_FRACTION;
    return eq as LONG as INT;
}

fn vInit(&mut self, p1: INT, p2: INT, p3: INT, p4: INT)
{
    let mut eqTmp;
    let eqP2 = p2 as LONGLONG;
    let eqP3 = p3 as LONGLONG;

// e0 = p1
// e1 = p4 - p1
// e2 = 6(p2 - 2p3 + p4)
// e3 = 6(p1 - 2p2 + p3)

// Change basis:

    self.e0 = p1 as LONGLONG;                                        // e0 = p1
    self.e1 = p4 as LONGLONG;
    self.e2 = eqP2; self.e2 -= eqP3; self.e2 -= eqP3; self.e2 += self.e1;    // e2 = p2 - 2*p3 + p4
    self.e3 = self.e0;   self.e3 -= eqP2; self.e3 -= eqP2; self.e3 += eqP3;  // e3 = p1 - 2*p2 + p3
    self.e1 -= self.e0;                                       // e1 = p4 - p1

// Convert to 36.28 format and multiply e2 and e3 by six:

    self.e0 <<= BEZIER64_FRACTION;
    self.e1 <<= BEZIER64_FRACTION;
    eqTmp = self.e2; self.e2 += eqTmp; self.e2 += eqTmp; self.e2 <<= (BEZIER64_FRACTION + 1);
    eqTmp = self.e3; self.e3 += eqTmp; self.e3 += eqTmp; self.e3 <<= (BEZIER64_FRACTION + 1);
}

fn vUntransform<F: Fn(&mut POINT) -> &mut LONG>(&self,
    afx: &mut [POINT; 4], field: F)
{
// Declare some temps to hold our operations, since we can't modify e0..e3.

    let mut eqP0;
    let mut eqP1;
    let mut eqP2;
    let mut eqP3;

// p0 = e0
// p1 = e0 + (6e1 - e2 - 2e3)/18
// p2 = e0 + (12e1 - 2e2 - e3)/18
// p3 = e0 + e1

    eqP0 = self.e0;

// NOTE PERF: Convert this to a multiply by 6: [andrewgo]

    eqP2 = self.e1;
    eqP2 += self.e1;
    eqP2 += self.e1;
    eqP1 = eqP2;
    eqP1 += eqP2;           // 6e1
    eqP1 -= self.e2;             // 6e1 - e2
    eqP2 = eqP1;
    eqP2 += eqP1;           // 12e1 - 2e2
    eqP2 -= self.e3;             // 12e1 - 2e2 - e3
    eqP1 -= self.e3;
    eqP1 -= self.e3;             // 6e1 - e2 - 2e3

// NOTE: May just want to approximate these divides! [andrewgo]
// Or can do a 64 bit divide by 32 bit to get 32 bits right here.

    eqP1 /= 18;
    eqP2 /= 18;
    eqP1 += self.e0;
    eqP2 += self.e0;

    eqP3 = self.e0;
    eqP3 += self.e1;

// Convert from 36.28 format with rounding:

    eqP0 += (1 << (BEZIER64_FRACTION - 1)); eqP0 >>= BEZIER64_FRACTION; *field(&mut afx[0]) = eqP0 as LONG;
    eqP1 += (1 << (BEZIER64_FRACTION - 1)); eqP1 >>= BEZIER64_FRACTION; *field(&mut afx[1]) = eqP1 as LONG;
    eqP2 += (1 << (BEZIER64_FRACTION - 1)); eqP2 >>= BEZIER64_FRACTION; *field(&mut afx[2]) = eqP2 as LONG;
    eqP3 += (1 << (BEZIER64_FRACTION - 1)); eqP3 >>= BEZIER64_FRACTION; *field(&mut afx[3]) = eqP3 as LONG;
}

fn vHalveStepSize(&mut self)
{
// e2 = (e2 + e3) >> 3
// e1 = (e1 - e2) >> 1
// e3 >>= 2

    self.e2 += self.e3; self.e2 >>= 3;
    self.e1 -= self.e2; self.e1 >>= 1;
    self.e3 >>= 2;
}

fn vDoubleStepSize(&mut self)
{
// e1 = 2e1 + e2
// e3 = 4e3;
// e2 = 8e2 - e3

    self.e1 <<= 1; self.e1 += self.e2;
    self.e3 <<= 2;
    self.e2 <<= 3; self.e2 -= self.e3;
}

fn vTakeStep(&mut self)
{
    self.e0 += self.e1;
    let eqTmp = self.e2;
    self.e1 += self.e2;
    self.e2 += eqTmp; self.e2 -= self.e3;
    self.e3 = eqTmp;
}
}

const BEZIER64_FRACTION: LONG  = 28;

// The following is our 2^11 target error encoded as a 36.28 number
// (don't forget the additional 4 bits of fractional precision!) and
// the 6 times error multiplier:

const geqErrorHigh: LONGLONG  = (6 * (1 << 15) >> (32 - BEZIER64_FRACTION)) << 32;

/*#ifdef BEZIER_FLATTEN_GDI_COMPATIBLE

// The following is the default 2/3 error encoded as a 36.28 number,
// multiplied by 6, and leaving 4 bits for fraction:

const LONGLONG geqErrorLow = (LONGLONG)(4) << 32;

#else*/

// The following is the default 1/4 error encoded as a 36.28 number,
// multiplied by 6, and leaving 4 bits for fraction:

use crate::types::POINT;

const geqErrorLow: LONGLONG = (3) << 31;

//#endif
#[derive(Default)]
pub struct Bezier64
{
    xLow: HfdBasis64,
    yLow: HfdBasis64,
    xHigh: HfdBasis64,
    yHigh: HfdBasis64,

    eqErrorLow: LONGLONG,
    rcfxClip: Option<RECT>,

    cStepsHigh: LONG,
    cStepsLow: LONG
}

impl Bezier64 {

fn vInit(&mut self, 
    aptfx: &[POINT; 4],
        // Pointer to 4 control points
    prcfxVis: Option<&RECT>,
        // Pointer to bound box of visible area (may be NULL)
    eqError: LONGLONG)
        // Fractional maximum error (32.32 format)
{
    self.cStepsHigh = 1;
    self.cStepsLow  = 0;

    self.xHigh.vInit(aptfx[0].x, aptfx[1].x, aptfx[2].x, aptfx[3].x);
    self.yHigh.vInit(aptfx[0].y, aptfx[1].y, aptfx[2].y, aptfx[3].y);

// Initialize error:

    self.eqErrorLow = eqError;

    self.rcfxClip = prcfxVis.cloned();

    while (((self.xHigh.vError()) > geqErrorHigh) ||
           ((self.yHigh.vError()) > geqErrorHigh))
    {
        self.cStepsHigh <<= 1;
        self.xHigh.vHalveStepSize();
        self.yHigh.vHalveStepSize();
    }
}

fn cFlatten(
    &mut self,
    mut pptfx: &mut [POINT],
    pbMore: &mut bool) -> INT
{
    let mut aptfx: [POINT; 4] = Default::default();
    let mut cptfx = pptfx.len();
    let mut rcfxBound: RECT;
    let cptfxOriginal = cptfx;

    assert!(cptfx > 0);

    while {
        if (self.cStepsLow == 0)
        {
        // Optimization that if the bound box of the control points doesn't
        // intersect with the bound box of the visible area, render entire
        // curve as a single line:
    
            self.xHigh.vUntransform(&mut aptfx, |p| &mut p.x);
            self.yHigh.vUntransform(&mut aptfx, |p| &mut p.y);
    
            self.xLow.vInit(aptfx[0].x, aptfx[1].x, aptfx[2].x, aptfx[3].x);
            self.yLow.vInit(aptfx[0].y, aptfx[1].y, aptfx[2].y, aptfx[3].y);
            self.cStepsLow = 1;
    
            if (match &self.rcfxClip { None => true, Some(clip) => {rcfxBound = vBoundBox(&aptfx); bIntersect(&rcfxBound, &clip)}})
            {
                while (((self.xLow.vError()) > self.eqErrorLow) ||
                       ((self.yLow.vError()) > self.eqErrorLow))
                {
                    self.cStepsLow <<= 1;
                    self.xLow.vHalveStepSize();
                    self.yLow.vHalveStepSize();
                }
            }
    
        // This 'if' handles the case where the initial error for the Bezier
        // is already less than the target error:
    
            if ({self.cStepsHigh -= 1; self.cStepsHigh} != 0)
            {
                self.xHigh.vTakeStep();
                self.yHigh.vTakeStep();
    
                if (((self.xHigh.vError()) > geqErrorHigh) ||
                    ((self.yHigh.vError()) > geqErrorHigh))
                {
                    self.cStepsHigh <<= 1;
                    self.xHigh.vHalveStepSize();
                    self.yHigh.vHalveStepSize();
                }
    
                while (!(self.cStepsHigh & 1 != 0) &&
                       ((self.xHigh.vParentError()) <= geqErrorHigh) &&
                       ((self.yHigh.vParentError()) <= geqErrorHigh))
                {
                    self.xHigh.vDoubleStepSize();
                    self.yHigh.vDoubleStepSize();
                    self.cStepsHigh >>= 1;
                }
            }
        }
    
        self.xLow.vTakeStep();
        self.yLow.vTakeStep();
    
        pptfx[0].x = self.xLow.fxValue();
        pptfx[0].y = self.yLow.fxValue();
        pptfx = &mut pptfx[1..];
    
        self.cStepsLow-=1;
        if (self.cStepsLow == 0 && self.cStepsHigh == 0)
        {
            *pbMore = false;

            // '+1' because we haven't decremented 'cptfx' yet:

            return(cptfxOriginal - cptfx + 1) as INT;
        }
    
        if ((self.xLow.vError() > self.eqErrorLow) ||
            (self.yLow.vError() > self.eqErrorLow))
        {
            self.cStepsLow <<= 1;
            self.xLow.vHalveStepSize();
            self.yLow.vHalveStepSize();
        }
    
        while (!(self.cStepsLow & 1 != 0) &&
               ((self.xLow.vParentError()) <= self.eqErrorLow) &&
               ((self.yLow.vParentError()) <= self.eqErrorLow))
        {
            self.xLow.vDoubleStepSize();
            self.yLow.vDoubleStepSize();
            self.cStepsLow >>= 1;
        }
        cptfx -= 1;
        cptfx != 0
    } {};

    *pbMore = true;
    return(cptfxOriginal) as INT;
}
}

//+-----------------------------------------------------------------------------
//
//  class CMILBezier
//
//  Bezier cracker.  Flattens any Bezier in our 28.4 device space down to a
//  smallest 'error' of 2^-7 = 0.0078.  Will use fast 32 bit cracker for small
//  curves and slower 64 bit cracker for big curves.
//
//  Public Interface:
//      vInit(aptfx, prcfxClip, peqError)
//          - pptfx points to 4 control points of Bezier.  The first point
//            retrieved by bNext() is the the first point in the approximation
//            after the start-point.
//
//          - prcfxClip is an optional pointer to the bound box of the visible
//            region.  This is used to optimize clipping of Bezier curves that
//            won't be seen.  Note that this value should account for the pen's
//            width!
//
//          - optional maximum error in 32.32 format, corresponding to Kirko's
//            error factor.
//
//      bNext(pptfx)
//          - pptfx points to where next point in approximation will be
//            returned.  Returns FALSE if the point is the end-point of the
//            curve.
//
pub (crate) enum CMILBezier
{
    Bezier64(Bezier64),
    Bezier32(Bezier32)
}

impl CMILBezier {
    // All coordinates must be in 28.4 format:
    pub fn new(aptfxBez: &[POINT; 4], prcfxClip: Option<&RECT>) -> Self {
        let mut bez32 = Bezier32::default();
        let bBez32 = bez32.bInit(aptfxBez, prcfxClip);
        if bBez32 {
            CMILBezier::Bezier32(bez32)
        } else {
            let mut bez64 = Bezier64::default();
            bez64.vInit(aptfxBez, prcfxClip, geqErrorLow);
            CMILBezier::Bezier64(bez64)
        }
    }

    // Returns the number of points filled in. This will never be zero.
    //
    // The last point returned may not be exactly the last control
    //            point. The workaround is for calling code to add an extra
    //            point if this is the case.
    pub fn Flatten(    &mut self,
        pptfx: &mut [POINT],
        pbMore: &mut bool) -> INT {
            match self {
                CMILBezier::Bezier32(bez) => bez.cFlatten(pptfx, pbMore),
                CMILBezier::Bezier64(bez) => bez.cFlatten(pptfx, pbMore)
            }
        }
}

#[test]
fn flatten() {
    let curve: [POINT; 4] = [
    POINT{x: 1715, y: 6506},
    POINT{x: 1692, y: 6506},
    POINT{x: 1227, y: 5148},
    POINT{x: 647, y: 5211}];
    let mut bez = CMILBezier::new(&curve, None);
    let mut result: [POINT; 32] = Default::default();
    let mut more: bool = false;
    let count = bez.Flatten(&mut result, &mut more);
    assert_eq!(count, 21);
    assert_eq!(more, false);
}

#[test]
fn split_flatten32() {
    // make sure that flattening a curve into two small buffers matches
    // doing it into a large buffer
    let curve: [POINT; 4] = [
    POINT{x: 1795, y: 8445},
    POINT{x: 1795, y: 8445},
    POINT{x: 1908, y: 8683},
    POINT{x: 2043, y: 8705}];

    let mut bez = CMILBezier::new(&curve, None);
    let mut result: [POINT; 8] = Default::default();
    let mut more: bool = false;
    let count = bez.Flatten(&mut result[..5], &mut more);
    assert_eq!(count, 5);
    assert_eq!(more, true);
    let count = bez.Flatten(&mut result[5..], &mut more);
    assert_eq!(count, 3);
    assert_eq!(more, false);

    let mut bez = CMILBezier::new(&curve, None);
    let mut full_result: [POINT; 8] = Default::default();
    let mut more: bool = false;
    let count = bez.Flatten(&mut full_result, &mut more);
    assert_eq!(count, 8);
    assert_eq!(more, false);
    assert!(result == full_result);
}

#[test]
fn flatten32() {
    let curve: [POINT; 4] = [
    POINT{x: 100, y: 100},
    POINT{x: 110, y: 100},
    POINT{x: 110, y: 110},
    POINT{x: 110, y: 100}];
    let mut bez = CMILBezier::new(&curve, None);
    let mut result: [POINT; 32] = Default::default();
    let mut more: bool = false;
    let count = bez.Flatten(&mut result, &mut more);
    assert_eq!(count, 3);
    assert_eq!(more, false);
}

#[test]
fn flatten32_double_step_size() {
    let curve: [POINT; 4] = [
    POINT{x: 1761, y: 8152},
    POINT{x: 1761, y: 8152},
    POINT{x: 1750, y: 8355},
    POINT{x: 1795, y: 8445}];
    let mut bez = CMILBezier::new(&curve, None);
    let mut result: [POINT; 32] = Default::default();
    let mut more: bool = false;
    let count = bez.Flatten(&mut result, &mut more);
    assert_eq!(count, 7);
    assert_eq!(more, false);
}

#[test]
fn bezier64_init_high_num_steps() {
    let curve: [POINT; 4] = [
    POINT{x: 33, y: -1},
    POINT{x: -1, y: -1},
    POINT{x: -1, y: -16385},
    POINT{x: -226, y: 10}];
    let mut bez = CMILBezier::new(&curve, None);
    let mut result: [POINT; 32] = Default::default();
    let mut more: bool = false;
    let count = bez.Flatten(&mut result, &mut more);
    assert_eq!(count, 32);
    assert_eq!(more, true);
}

#[test]
fn bezier64_high_error() {
    let curve: [POINT; 4] = [
    POINT{x: -1, y: -1},
    POINT{x: -4097, y: -1},
    POINT{x: 65471, y: -256},
    POINT{x: -1, y: 0}];
    let mut bez = CMILBezier::new(&curve, None);
    let mut result: [POINT; 32] = Default::default();
    let mut more: bool = false;
    let count = bez.Flatten(&mut result, &mut more);
    assert_eq!(count, 32);
    assert_eq!(more, true);
}