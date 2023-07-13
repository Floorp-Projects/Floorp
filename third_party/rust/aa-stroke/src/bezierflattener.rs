// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.
#![allow(non_snake_case)]

use std::ops::{Sub, Mul, Add, AddAssign, SubAssign, MulAssign, Div};

macro_rules! IFC {
    ($e: expr) => {
        assert_eq!($e, S_OK);
    }
}

pub type HRESULT = i32;

pub const S_OK: i32 = 0;
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct GpPointR {
    pub x: f64,
    pub y: f64
}

impl Sub for GpPointR {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        GpPointR { x: self.x - rhs.x, y: self.y - rhs.y }
    }
}

impl Add for GpPointR {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        GpPointR { x: self.x + rhs.x, y: self.y + rhs.y }
    }
}

impl AddAssign for GpPointR {
    fn add_assign(&mut self, rhs: Self) {
        *self = *self + rhs;
    }
}

impl SubAssign for GpPointR {
    fn sub_assign(&mut self, rhs: Self) {
        *self = *self - rhs;
    }
}

impl MulAssign<f64> for GpPointR {
    fn mul_assign(&mut self, rhs: f64) {
        *self = *self * rhs;
    }
}


impl Mul<f64> for GpPointR {
    type Output = Self;

    fn mul(self, rhs: f64) -> Self::Output {
        GpPointR { x: self.x * rhs, y: self.y * rhs }
    }
}

impl Div<f64> for GpPointR {
    type Output = Self;

    fn div(self, rhs: f64) -> Self::Output {
        GpPointR { x: self.x / rhs, y: self.y / rhs }
    }
}


impl Mul for GpPointR {
    type Output = f64;

    fn mul(self, rhs: Self) -> Self::Output {
        self.x * rhs.x +  self.y * rhs.y
    }
}

impl GpPointR {
    pub fn ApproxNorm(&self) -> f64 {
        self.x.abs().max(self.y.abs())
    }
    pub fn Norm(&self) -> f64 {
        self.x.hypot(self.y)
    }
}

// Relative to this is relative to the tolerance squared. In other words, a vector
// whose length is less than .01*tolerance will be considered 0
const  SQ_LENGTH_FUZZ: f64 = 1.0e-4;

// Some of these constants need further thinking

//const FUZZ: f64 = 1.0e-6;           // Relative 0
// Minimum allowed tolerance - should probably be adjusted to the size of the
// geometry we are rendering, but for now ---

/* 
const FUZZ_DOUBLE: f64 = 1.0e-12;           // Double-precision relative 0
const MIN_TOLERANCE: f64 = 1.0e-6;
const DEFAULT_FLATTENING_TOLERANCE: f64 = 0.25;*/
const TWICE_MIN_BEZIER_STEP_SIZE: f64 = 1.0e-3; // The step size in the Bezier flattener should
                                                 // never go below half this amount.
//+-----------------------------------------------------------------------------
//

//
//  $TAG ENGR

//      $Module:    win_mil_graphics_geometry
//      $Keywords:
//
//  $Description:
//      Definition of CBezierFlattener.
//
//  $ENDTAG
//
//------------------------------------------------------------------------------

//+-----------------------------------------------------------------------------
//
//  Class:
//      CFlatteningSink
//
//  Synopsis:
//      Callback interface for the results of curve flattening
//
//  Notes:
//      Methods are implemented rather than pure, for callers who do not use all
//      of them.
//
//------------------------------------------------------------------------------
//
//  Definition of CFlatteningSink
//
//------------------------------------------------------------------------------
/* 
struct CFlatteningSink
{
public:
    CFlatteningSink() {}

    virtual ~CFlatteningSink() {}

    virtual HRESULT Begin(
        __in_ecount(1) const GpPointR &)
            // First point (transformed)
    {
        // Do nothing stub, should not be called
        RIP("Base class Begin called");
        return E_NOTIMPL;
    }

    virtual HRESULT AcceptPoint(
        __in_ecount(1) const GpPointR &pt,
            // The point
        IN GpReal t,
            // Parameter we're at
        __out_ecount(1) bool &fAborted)
            // Set to true to signal aborting
    {
        UNREFERENCED_PARAMETER(pt);
        UNREFERENCED_PARAMETER(t);
        UNREFERENCED_PARAMETER(fAborted);

        // Do nothing stub, should not be called
        RIP("Base class AcceptPoint called");
        return E_NOTIMPL;
    }

    virtual HRESULT AcceptPointAndTangent(
        __in_ecount(1) const GpPointR &,
            //The point
        __in_ecount(1) const GpPointR &,
            //The tangent there
        IN bool fLast)         // Is this the last point on the curve?
    {
        // Do nothing stub, should not be called
        RIP("Base class AcceptPointAndTangent called");
        return E_NOTIMPL;
    }
};



*/
#[derive(Clone, Debug)]

pub struct CBezier
{
    /* 
public:
    CBezier()
    {
    }

    CBezier(
        __in_ecount(4) const GpPointR *pPt)
            // The defining Bezier points
    {
        Assert(pPt);
        memcpy(&m_ptB, pPt, 4 * sizeof(GpPointR)); 
    }

    CBezier(
        __in_ecount(1) const CBezier &other)
            // Another Bezier to copy
    {
        Copy(other); 
    }

    void Copy(
        __in_ecount(1) const CBezier &other)
            // Another Bezier to copy
    {
        memcpy(&m_ptB, other.m_ptB, 4 * sizeof(GpPointR)); 
    }

    void Initialize(
        __in_ecount(1) const GpPointR &ptFirst,
            // The first Bezier point
        __in_ecount(3) const GpPointR *pPt)
            // The remaining 3 Bezier points
    {
        m_ptB[0] = ptFirst;
        memcpy(m_ptB + 1, pPt, 3 * sizeof(GpPointR)); 
    }

    __outro_ecount(1) const GpPointR &GetControlPoint(__range(0, 3) UINT i) const
    {
        Assert(i < 4);
        return m_ptB[i];
    }

    __outro_ecount(1) const GpPointR &GetFirstPoint() const
    {
        return m_ptB[0];
    }
    
    __outro_ecount(1) const GpPointR &GetLastPoint() const
    {
        return m_ptB[3];
    }

    void GetPoint(
        _In_ double t,
            // Parameter value
        __out_ecount(1) GpPointR &pt) const; 
            // Point there

    void GetPointAndDerivatives(
        __in double t,
            // Parameter value
        __out_ecount(3) GpPointR *pValues) const;
                // Point, first derivative and second derivative there

    void TrimToStartAt(
        IN double t);             // Parameter value
        
    void TrimToEndAt(
        IN double t);             // Parameter value

    bool TrimBetween(
        __in double rStart,
            // Parameter value for the new start, must be between 0 and 1
        __in double rEnd);
            // Parameter value for the new end, must be between 0 and 1

    bool operator ==(__in_ecount(1) const CBezier &other) const
    {
        return (m_ptB[0] == other.m_ptB[0]) &&
               (m_ptB[1] == other.m_ptB[1]) &&
               (m_ptB[2] == other.m_ptB[2]) &&
               (m_ptB[3] == other.m_ptB[3]);
    }

    void AssertEqualOrNaN(__in_ecount(1) const CBezier &other) const
    {
        m_ptB[0].AssertEqualOrNaN(other.m_ptB[0]);
        m_ptB[1].AssertEqualOrNaN(other.m_ptB[1]);
        m_ptB[2].AssertEqualOrNaN(other.m_ptB[2]);
        m_ptB[3].AssertEqualOrNaN(other.m_ptB[3]);
    }

protected:
    */
    // Data
    m_ptB: [GpPointR; 4],
            // The defining Bezier points
}

impl CBezier {
    pub fn new(curve: [GpPointR; 4]) -> Self {
        Self { m_ptB: curve }
    }

    pub fn is_degenerate(&self) -> bool {
        self.m_ptB[0] == self.m_ptB[1] &&
            self.m_ptB[0] == self.m_ptB[2] &&
            self.m_ptB[0] == self.m_ptB[3]
    }
}

pub trait CFlatteningSink {
    fn AcceptPointAndTangent(&mut self,
        pt: &GpPointR,
            // The point
        vec: &GpPointR,
            // The tangent there
        fLast: bool
            // Is this the last point on the curve?
        ) -> HRESULT;

        fn AcceptPoint(&mut self,
            pt: &GpPointR,
                // The point
            t: f64,
                // Parameter we're at
            fAborted: &mut bool,
            lastPoint: bool
        ) -> HRESULT;
}

//+-----------------------------------------------------------------------------
//
//  Class:
//      CBezierFlattener
//
//  Synopsis:
//      Generates a polygonal apprximation to a given Bezier curve
//
//------------------------------------------------------------------------------
pub struct CBezierFlattener<'a>
{
    bezier: CBezier,
        // Flattening defining data
        m_pSink: &'a mut dyn CFlatteningSink,           // The recipient of the flattening data
        m_rTolerance: f64,       // Prescribed tolerance
        m_fWithTangents: bool,    // Generate tangent vectors if true
        m_rQuarterTolerance: f64,// Prescribed tolerance/4 (for doubling the step)
        m_rFuzz: f64,            // Computational zero
    
        // Flattening working data
        m_ptE: [GpPointR; 4],           // The moving basis of the curve definition
        m_cSteps: i32,           // The number of steps left to the end of the curve
        m_rParameter: f64,       // Parameter value
        m_rStepSize: f64,        // Steps size in parameter domain
}
impl<'a> CBezierFlattener<'a> {
    /*fn new(
        __in_ecount_opt(1) CFlatteningSink *pSink,
            // The reciptient of the flattened data
        IN GpReal          rTolerance)
            // Flattening tolerance 
    {
        Initialize(pSink, rTolerance);
    }*/
/* 
    void SetTarget(__in_ecount_opt(1) CFlatteningSink *pSink)
    {
        m_pSink = pSink;
    }

    void Initialize(
        __in_ecount_opt(1) CFlatteningSink *pSink,
            // The reciptient of the flattened data
        IN GpReal rTolerance);
        // Flattening tolerance 

    void SetPoint(
        __in UINT i,
            // index of the point (must be between 0 and 3)
        __in_ecount(1) const GpPointR &pt)
            // point value
    {
        Assert(i < 4);
        m_ptB[i] = pt;
    }

    HRESULT GetFirstTangent(
        __out_ecount(1) GpPointR &vecTangent) const;
            // Tangent vector there

    GpPointR GetLastTangent() const;

    HRESULT Flatten( 
        IN bool fWithTangents);   // Return tangents with the points if true

private:
    // Disallow copy constructor
    CBezierFlattener(__in_ecount(1) const CBezierFlattener &)
    {
        RIP("CBezierFlattener copy constructor reached.");
    }

protected:
*/
/*  fn Step(
        __out_ecount(1) bool &fAbort);   // Set to true if flattening should be aborted

    fn HalveTheStep();

    fn TryDoubleTheStep();*/

}




// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.


//+-----------------------------------------------------------------------------
//

//
//  $TAG ENGR

//      $Module:    win_mil_graphics_geometry
//      $Keywords:
//
//  $Description:
//      Implementation of CBezierFlattener.
//
//  $ENDTAG
//
//------------------------------------------------------------------------------

impl<'a> CBezierFlattener<'a> {
/////////////////////////////////////////////////////////////////////////////////
//
//              Implementation of CBezierFlattener

//+-----------------------------------------------------------------------------
//
//  Member:
//      CBezierFlattener::Initialize
//
//  Synopsis:
//      Initialize the sink and tolerance
//
//------------------------------------------------------------------------------
pub fn new(bezier: &CBezier,
    pSink: &'a mut dyn CFlatteningSink,
        // The reciptient of the flattened data
    rTolerance: f64)       // Flattening tolerance
    -> Self 
{
    let mut result = CBezierFlattener {
        bezier: bezier.clone(),
        // Flattening defining data
        m_pSink: pSink,           // The recipient of the flattening data
        m_rTolerance: 0.,       // Prescribed tolerance
        m_fWithTangents: false,    // Generate tangent vectors if true
        m_rQuarterTolerance: 0.,// Prescribed tolerance/4 (for doubling the step)
        m_rFuzz: 0.,            // Computational zero
    
        // Flattening working data
        m_ptE: [GpPointR { x: 0., y: 0.}; 4],           // The moving basis of the curve definition
        m_cSteps: 0,           // The number of steps left to the end of the curve
        m_rParameter: 0.,       // Parameter value
        m_rStepSize: 0.,        // Steps size in parameter domain
    };

    // If rTolerance == NaN or less than 0, we'll treat it as 0.
    result.m_rTolerance = if rTolerance >= 0.0 { rTolerance } else { 0.0 };
    result.m_rFuzz = rTolerance * rTolerance * SQ_LENGTH_FUZZ;
    
    // The error is tested on max(|e2|, |e2|), which represent 6 times the actual error, so:
    result.m_rTolerance *= 6.;
    result.m_rQuarterTolerance = result.m_rTolerance * 0.25;
    result
}

//+-----------------------------------------------------------------------------
//
//  Member:
//      CBezierFlattener::Flatten
//
//  Synopsis:
//      Flatten this curve
//
//  Notes:
                                                                                
// The algorithm is described in detail in the 1995 patent # 5367617 "System and 
// method of hybrid forward differencing to render Bezier splines" to be found 
// on the Microsoft legal dept. web site (LCAWEB).  Additional references are:
//     Lien, Shantz and Vaughan Pratt, "Adaptive Forward Differencing for
//     Rendering Curves and Surfaces", Computer Graphics, July 1987
//     Chang and Shantz, "Rendering Trimmed NURBS with Adaptive Forward
//         Differencing", Computer Graphics, August 1988
//     Foley and Van Dam, "Fundamentals of Interactive Computer Graphics"
//
// The basic idea is to replace the Bernstein basis (underlying Bezier curves) 
// with the Hybrid Forward Differencing (HFD) basis which is more efficient at 
// for flattening.  Each one of the 3 actions - Step, Halve and Double (step 
// size) this basis affords very efficient formulas for computing coefficients
// for the new interval.
//
// The coefficients of the HFD basis are defined in terms of the Bezier 
// coefficients as follows:
//
//          e0 = p0, e1 = p3 - p0, e2 = 6(p1 - 2p2 + p3), e3 = 6(p0 - 2p1 + p2),
//
// but formulas may be easier to understand by going through the power basis 
// representation:  f(t) = a*t + b*t + c * t^2 + d * t^3.
//
//  The conversion is then:            
//                               e0 = a
//                               e1 = f(1) - f(0) = b + c + d
//                               e2 = f"(1) = 2c + 6d
//                               e3 = f"(0) = 2c
//
// This is inverted to:
//                              a = e0
//                              c = e3 / 2
//                              d = (e2 - 2c) / 6 = (e2 - e3) / 6
//                              b = e1 - c - d = e1 - e2 / 6 - e3 / 3
//
// a, b, c, d for the new (halved, doubled or forwarded) interval are derived 
// and then converted to e0, e1, e2, e3 using these relationships.
//
// An exact integer version is implemented in Bezier.h and Bezier.cpp.
//
//------------------------------------------------------------------------------


pub fn Flatten(&mut self,
    fWithTangents: bool)   // Return tangents with the points if true
    -> HRESULT
{

    let hr = S_OK;
    let mut fAbort = false;

    /*if (!self.m_pSink)
    {
        return E_UNEXPECTED;
    }*/

    self.m_fWithTangents = fWithTangents;

    self.m_cSteps = 1;

    self.m_rParameter = 0.;
    self.m_rStepSize = 1.;

    // Compute the HFD basis
    self.m_ptE[0] = self.bezier.m_ptB[0]; 
    self.m_ptE[1] = self.bezier.m_ptB[3] - self.bezier.m_ptB[0]; 
    self.m_ptE[2] = (self.bezier.m_ptB[1] - self.bezier.m_ptB[2] * 2. + self.bezier.m_ptB[3]) * 6.;    // The second derivative at curve end
    self.m_ptE[3] = (self.bezier.m_ptB[0] - self.bezier.m_ptB[1] * 2. + self.bezier.m_ptB[2]) * 6.;    // The second derivative at curve start

    // Determine the initial step size
    self.m_cSteps = 1;
    while ((self.m_ptE[2].ApproxNorm() > self.m_rTolerance)  ||  (self.m_ptE[3].ApproxNorm() > self.m_rTolerance)) &&
           (self.m_rStepSize > TWICE_MIN_BEZIER_STEP_SIZE)
        
    {
        self.HalveTheStep();
    }

    while self.m_cSteps > 1
    {
        IFC!(self.Step(&mut fAbort));
        if fAbort {
            return hr;
        }

        // E[3] was already tested as E[2] in the previous step
        if self.m_ptE[2].ApproxNorm() > self.m_rTolerance &&
            self.m_rStepSize > TWICE_MIN_BEZIER_STEP_SIZE
        {
            // Halving the step once is provably sufficient (see Notes above), so ---
            self.HalveTheStep();
        }
        else
        {
            // --- but the step can possibly be more than doubled, hence the while loop
            while self.TryDoubleTheStep() {
                continue;
            }
        }
    }

    // Last point
    if self.m_fWithTangents
    {
        IFC!(self.m_pSink.AcceptPointAndTangent(&self.bezier.m_ptB[3], &self.GetLastTangent(), true /* last point */));
    }
    else
    {
        IFC!(self.m_pSink.AcceptPoint(&self.bezier.m_ptB[3], 1., &mut fAbort, true));
    }

    return hr;
}
//+-----------------------------------------------------------------------------
//
//  Member:
//      CBezierFlattener::Step
//
//  Synopsis:
//      Step forward on the polygonal approximation of the curve
//
//  Notes:
//      Taking a step means replacing a,b,c,d by coefficients of g(t) = f(t+1). 
//      Express those in terms of a,b,c,d and convert to e0, e1, e2, e3 to get:
//
//       New e0 = e0 + e1
//       New e1 = e1 + e2
//       New e2 = 2e2 - e3
//       New e3 = e2
//
//  The patent application (see above) explains why.
//
//  Getting a tangent vector is a minor enhancement along the same lines:
//      f'(0) = b = 6e1 - e2 - 2e3.
//
//------------------------------------------------------------------------------

fn Step(&mut self,
    fAbort: &mut bool)  -> HRESULT  // Set to true if flattening should be aborted, untouched otherwise
{
    let hr = S_OK;
    
    // Compute the basis for the same curve on the next interval
    let mut pt;

    self.m_ptE[0] += self.m_ptE[1];
    pt = self.m_ptE[2];
    self.m_ptE[1] += pt;
    self.m_ptE[2] += pt;  self.m_ptE[2] -= self.m_ptE[3];
    self.m_ptE[3] = pt;

    // Increment the parameter
    self.m_rParameter += self.m_rStepSize;

    // Generate the start point of the new interval
    if self.m_fWithTangents
    {
        // Compute the tangent there
        pt = self.m_ptE[1] * 6. - self.m_ptE[2] - self.m_ptE[3] * 2.;  //  = twice the derivative at E[0]
        IFC!(self.m_pSink.AcceptPointAndTangent(&self.m_ptE[0], &pt, false /* not the last point */));
    }
    else
    {
        IFC!(self.m_pSink.AcceptPoint(&self.m_ptE[0], self.m_rParameter, fAbort, false));
    }
    
    self.m_cSteps-=1;
    return hr;
}
//+-----------------------------------------------------------------------------
//
//  Member:
//      CBezierFlattener::HalveTheStep
//
//  Synopsis:
//      Halve the size of the step
//
//  Notes:
//      Halving the step means replacing a,b,c,d by coefficients of g(t) =
//      f(t/2). Experss those in terms of a,b,c,d and convert to e0, e1, e2, e3
//      to get:
//
//       New e0 = e0
//       New e1 = (e1 - e2) / 2
//       New e2 = (e2 + e3) / 8
//       New e3 = e3 / 4
//
//  The patent application (see above) explains why.
//
//------------------------------------------------------------------------------
fn HalveTheStep(&mut self)
{
    self.m_ptE[2] += self.m_ptE[3];   self.m_ptE[2] *= 0.125;
    self.m_ptE[1] -= self.m_ptE[2];   self.m_ptE[1] *= 0.5;
    self.m_ptE[3] *= 0.25;

    self.m_cSteps *= 2;  // Double the number of steps left
    self.m_rStepSize *= 0.5;
}
//+-----------------------------------------------------------------------------
//
//  Member:
//      CBezierFlattener::TryDoubleTheStep
//
//  Synopsis:
//      Double the step size if possible within tolerance.
//
//  Notes:
//      Coubling the step means replacing a,b,c,d by coefficients of g(t) =
//      f(2t). Experss those in terms of a,b,c,d and convert to e0, e1, e2, e3
//      to get:
//
//       New e0 = e0
//       New e1 = 2e1 + e2
//       New e2 = 8e2 - 4e3
//       New e3 = 4e3
//
//  The patent application (see above) explains why.  Note also that these
//  formulas are the inverse of those for halving the step.
//
//------------------------------------------------------------------------------
fn
TryDoubleTheStep(&mut self) -> bool
{
    let mut fDoubled = 0 == (self.m_cSteps & 1);
    if fDoubled
    {
        let ptTemp = self.m_ptE[2] * 2. - self.m_ptE[3];

        fDoubled = (self.m_ptE[3].ApproxNorm() <= self.m_rQuarterTolerance) && 
                   (ptTemp.ApproxNorm() <= self.m_rQuarterTolerance);

        if fDoubled
        {
            self.m_ptE[1] *= 2.;  self.m_ptE[1] += self.m_ptE[2];
            self.m_ptE[3] *= 4.;
            self.m_ptE[2] = ptTemp * 4.;

            self.m_cSteps /= 2;      // Halve the number of steps left
            self.m_rStepSize *= 2.;
        }
    }

    return fDoubled;
}
//+-----------------------------------------------------------------------------
//
//  Member:
//      CBezierFlattener::GetFirstTangent
//
//  Synopsis:
//      Get the tangent at curve start
//
//  Return:
//      WGXERR_ZEROVECTOR if the tangent vector has practically 0 length
//
//  Notes:
//      This method can return an error if all the points are bunched together.
//      The idea is that the caller will detect that, abandon this curve, and
//      never call GetLasttangent, which can therefore be presumed to succeed. 
//      The failure here is benign.
//
//------------------------------------------------------------------------------
#[allow(dead_code)]
fn GetFirstTangent(&self) -> Option<GpPointR> // Tangent vector there
    
{

    let mut vecTangent = self.bezier.m_ptB[1] - self.bezier.m_ptB[0];
    if vecTangent * vecTangent > self.m_rFuzz
    {
        return Some(vecTangent);  // - we're done
    }
    // Zero first derivative, go for the second
    vecTangent = self.bezier.m_ptB[2] - self.bezier.m_ptB[0];
    if vecTangent * vecTangent > self.m_rFuzz
    {
        return Some(vecTangent);  // - we're done
    }
    // Zero second derivative, go for the third
    vecTangent = self.bezier.m_ptB[3] - self.bezier.m_ptB[0];

    if vecTangent * vecTangent <= self.m_rFuzz
    {
        return None;
    }

    return Some(vecTangent);      // no RRETURN, error is expected
}
//+-----------------------------------------------------------------------------
//
//  Member:
//      CBezierFlattener::GetLastTangent
//
//  Synopsis:
//      Get the tangent at curve end
//
//  Return:
//      The tangent
//
//  Notes:
//      This method has no error return while GetFirstTangent returns
//      WGXERR_ZEROVECTOR if the tangent is zero.  The idea is that we should
//      only fail if all the control points coincide, that should have been
//      detected at GetFirstTangent, and then we should have not be called.
//
//------------------------------------------------------------------------------
fn GetLastTangent(&self) -> GpPointR
{
    let mut vecTangent = self.bezier.m_ptB[3] - self.bezier.m_ptB[2];
    
    // If the curve is degenerate, we should have detected it at curve-start, skipped this curve
    // altogether and not be here.  But the test in GetFirstTangent is for the point-differences
    // 1-0, 2-0 and 3-0, while here it is for points 3-2, 3-1 and 3-0, which is not quite the same.
    // Still, In a disk of radius r no 2 points are more than 2r apart.  The tests are done with
    // squared distance, and m_rFuzz is the minimal accepted squared distance.  GetFirstTangent()
    // succeeded, so there is a pair of points whose squared distance is greater than m_rfuzz.
    // So the squared radius of a disk about point 3 that contains the remaining points must be 
    // at least m_rFuzz/4.  Allowing some margin for arithmetic error:

    let rLastTangentFuzz = self.m_rFuzz/8.;

    if vecTangent * vecTangent <= rLastTangentFuzz
    {
        // Zero first derivative, go for the second
        vecTangent = self.bezier.m_ptB[3] - self.bezier.m_ptB[1];
        if vecTangent * vecTangent <= rLastTangentFuzz
        {
            // Zero second derivative, go for the third
            vecTangent = self.bezier.m_ptB[3] - self.bezier.m_ptB[0];
        }
    }

    debug_assert! (!(vecTangent * vecTangent < rLastTangentFuzz)); // Ignore NaNs

    return vecTangent;
}
}


