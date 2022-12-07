// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#![allow(unused_parens)]

use crate::aacoverage::{CCoverageBuffer, c_rInvShiftSize, c_antiAliasMode, c_nShift, CCoverageInterval, c_nShiftMask, c_nShiftSize, c_nHalfShiftSize};
use crate::hwvertexbuffer::CHwVertexBufferBuilder;
use crate::matrix::{CMILMatrix, CMatrix};
use crate::nullable_ref::Ref;
use crate::aarasterizer::*;
use crate::geometry_sink::IGeometrySink;
use crate::helpers::Int32x32To64;
use crate::types::*;
use typed_arena_nomut::Arena;

//-----------------------------------------------------------------------------
//

//
//  Description:
//      Trapezoidal anti-aliasing implementation
//
//  >>>> Note that some of this code is duplicated in sw\aarasterizer.cpp,
//  >>>> so changes to this file may need to propagate.
//
//   pursue reduced code duplication
//

macro_rules! MIL_THR {
    ($e: expr) => {
        $e//assert_eq!($e, S_OK);
    }
}


//
// Optimize for speed instead of size for these critical methods
//


//-------------------------------------------------------------------------
//
// Coordinate system encoding
//
// All points/coordinates are named as follows:
//
//    <HungarianType><CoordinateSystem>[X|Y][Left|Right|Top|Bottom]VariableName
//
//    Common hungarian types:
//        n - INT
//        u - UINT
//        r - FLOAT
//
//    Coordinate systems:
//        Pixel - Device pixel space assuming integer coordinates in the pixel top left corner.
//        Subpixel - Overscaled space.
//
//        To convert between Pixel to Subpixel, we have:
//            nSubpixelCoordinate = nPixelCoordinate << c_nShift;
//            nPixelCoordinate = nSubpixelCoordinate >> c_nShift;
//
//        Note that the conversion to nPixelCoordinate needs to also track
//        (nSubpixelCoordinate & c_nShiftMask) to maintain the full value.
//
//        Note that since trapezoidal only supports 8x8, c_nShiftSize is always equal to 8.  So,
//        (1, 2) in pixel space would become (8, 16) in subpixel space.
//
//    [X|Y]
//        Indicates which coordinate is being referred to.
//
//    [Left|Right|Top|Bottom]
//        When referring to trapezoids or rectangular regions, this
//        component indicates which edge is being referred to.
//
//    VariableName
//       Descriptive portion of the variable name
//
//-------------------------------------------------------------------------


//-------------------------------------------------------------------------
//
//  Function:   IsFractionGreaterThan
//
//  Synopsis:
//     Determine if nNumeratorA/nDenominatorA > nNumeratorB/nDenominatorB
//
//     Note that we assume all denominators are strictly greater than zero.
//
//-------------------------------------------------------------------------
fn IsFractionGreaterThan(
    nNumeratorA: INT,                    // Left hand side numerator
   /* __in_range(>=, 1) */ nDenominatorA: INT, // Left hand side denominator
    nNumeratorB: INT,                    // Right hand side numerator
   /* __in_range(>=, 1) */ nDenominatorB: INT,  // Right hand side denominator
    ) -> bool
{
    //
    // nNumeratorA/nDenominatorA > nNumeratorB/nDenominatorB
    // iff nNumeratorA*nDenominatorB/nDenominatorA > nNumeratorB, since nDenominatorB > 0
    // iff nNumeratorA*nDenominatorB > nNumeratorB*nDenominatorA, since nDenominatorA > 0
    //
    // Now, all input parameters are 32-bit integers, so we need to use
    // a 64-bit result to compute the product.
    //

    let lNumeratorAxDenominatorB = Int32x32To64(nNumeratorA, nDenominatorB);
    let lNumeratorBxDenominatorA = Int32x32To64(nNumeratorB, nDenominatorA);

    return (lNumeratorAxDenominatorB > lNumeratorBxDenominatorA);
}

//-------------------------------------------------------------------------
//
//  Function:   IsFractionLessThan
//
//  Synopsis:
//     Determine if nNumeratorA/nDenominatorA < nNumeratorB/nDenominatorB
//
//     Note that we assume all denominators are strictly greater than zero.
//
//-------------------------------------------------------------------------
fn
IsFractionLessThan(
    nNumeratorA: INT,                    // Left hand side numerator
    /* __in_range(>=, 1) */ nDenominatorA: INT, // Left hand side denominator
    nNumeratorB: INT,                    // Right hand side numerator
    /* __in_range(>=, 1) */ nDenominatorB: INT,  // Right hand side denominator
) -> bool
{
    //
    // Same check as previous function with less than comparision instead of
    // a greater than comparison.
    //

    let lNumeratorAxDenominatorB = Int32x32To64(nNumeratorA, nDenominatorB);
    let lNumeratorBxDenominatorA = Int32x32To64(nNumeratorB, nDenominatorA);

    return (lNumeratorAxDenominatorB < lNumeratorBxDenominatorA);
}


//-------------------------------------------------------------------------
//
//  Function:   AdvanceDDAMultipleSteps
//
//  Synopsis:
//     Advance the DDA by multiple steps
//
//-------------------------------------------------------------------------
fn
AdvanceDDAMultipleSteps(
    pEdgeLeft: &CEdge,         // Left edge from active edge list
    pEdgeRight: &CEdge,        // Right edge from active edge list
    nSubpixelYAdvance: INT,                    // Number of steps to advance the DDA
    nSubpixelXLeftBottom: &mut INT,     // Resulting left x position
    nSubpixelErrorLeftBottom: &mut INT, // Resulting left x position error
    nSubpixelXRightBottom: &mut INT,    // Resulting right x position
    nSubpixelErrorRightBottom: &mut INT // Resulting right x position error
    )
{
    //
    // In this method, we need to be careful of overflow.  Expected input ranges for values are:
    //
    //      edge points: x and y subpixel space coordinates are between [-2^26, 2^26]
    //                   since we start with 28.4 space (and are now in subpixel space,
    //                   i.e., no 16x scale) and assume 2 bits of working space.
    //
    //                   This assumption is ensured by TransformRasterizerPointsTo28_4.
    //
    #[cfg(debug_assertions)]
    {
    let nDbgPixelCoordinateMax = (1 << 26);
    let nDbgPixelCoordinateMin = -nDbgPixelCoordinateMax;

    assert!(pEdgeLeft.X.get() >= nDbgPixelCoordinateMin && pEdgeLeft.X.get() <= nDbgPixelCoordinateMax);
    assert!(pEdgeLeft.EndY >= nDbgPixelCoordinateMin && pEdgeLeft.EndY <= nDbgPixelCoordinateMax);
    assert!(pEdgeRight.X.get() >= nDbgPixelCoordinateMin && pEdgeRight.X.get() <= nDbgPixelCoordinateMax);
    assert!(pEdgeRight.EndY >= nDbgPixelCoordinateMin && pEdgeRight.EndY <= nDbgPixelCoordinateMax);

    //
    //        errorDown: (0, 2^30)
    //                   Since errorDown is the edge delta y in 28.4 space (not subpixel space
    //                   like the end points), we have a larger range of (0, 2^32) for the positive
    //                   error down.  With 2 bits of work space (which TransformRasterizerPointsTo28_4
    //                   ensures), we know we are between (0, 2^30)
    //

    let nDbgErrorDownMax: INT = (1 << 30);
    assert!(pEdgeLeft.ErrorDown  > 0 && pEdgeLeft.ErrorDown  < nDbgErrorDownMax);
    assert!(pEdgeRight.ErrorDown > 0 && pEdgeRight.ErrorDown < nDbgErrorDownMax);

    //
    //          errorUp: [0, errorDown)
    //
    assert!(pEdgeLeft.ErrorUp  >= 0 && pEdgeLeft.ErrorUp  < pEdgeLeft.ErrorDown);
    assert!(pEdgeRight.ErrorUp >= 0 && pEdgeRight.ErrorUp < pEdgeRight.ErrorDown);
    }

    //
    // Advance the left edge
    //

    // Since each point on the edge is withing 28.4 space, the following computation can't overflow.
    *nSubpixelXLeftBottom = pEdgeLeft.X.get() + nSubpixelYAdvance*pEdgeLeft.Dx;

    // Since the error values can be close to 2^30, we can get an overflow by multiplying with yAdvance.
    // So, we need to use a 64-bit temporary in this case.
    let mut llSubpixelErrorBottom: LONGLONG = pEdgeLeft.Error.get() as LONGLONG + Int32x32To64(nSubpixelYAdvance, pEdgeLeft.ErrorUp);
    if (llSubpixelErrorBottom >= 0)
    {
        let llSubpixelXLeftDelta = llSubpixelErrorBottom / (pEdgeLeft.ErrorDown as LONGLONG);

        // The delta should remain in range since it still represents a delta along the edge which
        // we know fits entirely in 28.4.  Note that we add one here since the error must end up
        // less than 0.
        assert!(llSubpixelXLeftDelta < INT::MAX as LONGLONG);
        let nSubpixelXLeftDelta: INT = (llSubpixelXLeftDelta as INT) + 1;

        *nSubpixelXLeftBottom += nSubpixelXLeftDelta;
        llSubpixelErrorBottom -= Int32x32To64(pEdgeLeft.ErrorDown, nSubpixelXLeftDelta);
    }

    // At this point, the subtraction above should have generated an error that is within
    // (-pLeft->ErrorDown, 0)

    assert!((llSubpixelErrorBottom > -pEdgeLeft.ErrorDown as LONGLONG) && (llSubpixelErrorBottom < 0));
    *nSubpixelErrorLeftBottom = (llSubpixelErrorBottom as INT);

    //
    // Advance the right edge
    //

    // Since each point on the edge is withing 28.4 space, the following computation can't overflow.
    *nSubpixelXRightBottom = pEdgeRight.X.get() + nSubpixelYAdvance*pEdgeRight.Dx;

    // Since the error values can be close to 2^30, we can get an overflow by multiplying with yAdvance.
    // So, we need to use a 64-bit temporary in this case.
    llSubpixelErrorBottom = pEdgeRight.Error.get() as LONGLONG + Int32x32To64(nSubpixelYAdvance, pEdgeRight.ErrorUp);
    if (llSubpixelErrorBottom >= 0)
    {
        let llSubpixelXRightDelta: LONGLONG = llSubpixelErrorBottom / (pEdgeRight.ErrorDown as LONGLONG);

        // The delta should remain in range since it still represents a delta along the edge which
        // we know fits entirely in 28.4.  Note that we add one here since the error must end up
        // less than 0.
        assert!(llSubpixelXRightDelta < INT::MAX as LONGLONG);
        let nSubpixelXRightDelta: INT = (llSubpixelXRightDelta as INT) + 1;

        *nSubpixelXRightBottom += nSubpixelXRightDelta;
        llSubpixelErrorBottom -= Int32x32To64(pEdgeRight.ErrorDown, nSubpixelXRightDelta);
    }

    // At this point, the subtraction above should have generated an error that is within
    // (-pRight->ErrorDown, 0)

    assert!((llSubpixelErrorBottom > -pEdgeRight.ErrorDown as LONGLONG) && (llSubpixelErrorBottom < 0));
    *nSubpixelErrorRightBottom = (llSubpixelErrorBottom as INT);
}

//-------------------------------------------------------------------------
//
//  Function:   ComputeDeltaUpperBound
//
//  Synopsis:
//     Compute some value that is >= nSubpixelAdvanceY*|1/m| where m is the
//     slope defined by the edge below.
//
//-------------------------------------------------------------------------
fn
ComputeDeltaUpperBound(
    pEdge: &CEdge,  // Edge containing 1/m value used for computation
    nSubpixelYAdvance: INT          // Multiplier in synopsis expression
    ) -> INT
{
    let nSubpixelDeltaUpperBound: INT;

    //
    // Compute the delta bound
    //

    if (pEdge.ErrorUp == 0)
    {
        //
        // No errorUp, so simply compute bound based on dx value
        //

        nSubpixelDeltaUpperBound = nSubpixelYAdvance*(pEdge.Dx).abs();
    }
    else
    {
        let nAbsDx: INT;
        let nAbsErrorUp: INT;

        //
        // Compute abs of (dx, error)
        //
        // Here, we can assume errorUp > 0
        //

        assert!(pEdge.ErrorUp > 0);

        if (pEdge.Dx >= 0)
        {
            nAbsDx = pEdge.Dx;
            nAbsErrorUp = pEdge.ErrorUp;
        }
        else
        {
            //
            // Dx < 0, so negate (dx, errorUp)
            //
            // Note that since errorUp > 0, we know -errorUp < 0 and that
            // we need to add errorDown to get an errorUp >= 0 which
            // also means substracting one from dx.
            //

            nAbsDx = -pEdge.Dx - 1;
            nAbsErrorUp = -pEdge.ErrorUp + pEdge.ErrorDown;
        }

        //
        // Compute the bound of nSubpixelAdvanceY*|1/m|
        //
        // Note that the +1 below is included to bound any left over errorUp that we are dropping here.
        //

        nSubpixelDeltaUpperBound = nSubpixelYAdvance*nAbsDx + (nSubpixelYAdvance*nAbsErrorUp)/pEdge.ErrorDown + 1;
    }

    return nSubpixelDeltaUpperBound;
}

//-------------------------------------------------------------------------
//
//  Function:   ComputeDistanceLowerBound
//
//  Synopsis:
//     Compute some value that is <= distance between
//     (pEdgeLeft->X, pEdgeLeft->Error) and (pEdgeRight->X, pEdgeRight->Error)
//
//-------------------------------------------------------------------------
fn
ComputeDistanceLowerBound(
    pEdgeLeft: &CEdge, // Left edge containing the position for the distance computation
    pEdgeRight: &CEdge // Right edge containing the position for the distance computation
    ) -> INT
{
    //
    // Note: In these comments, error1 and error2 are theoretical. The actual Error members
    // are biased by -1.
    //
    // distance = (x2 + error2/errorDown2) - (x1 + error1/errorDown1)
    //          = x2 - x1 + error2/errorDown2 - error1/errorDown1
    //          >= x2 - x1 + error2/errorDown2   , since error1 < 0
    //          >= x2 - x1 - 1                   , since error2 < 0
    //          = pEdgeRight->X - pEdgeLeft->X - 1
    //
    // In the special case where error2/errorDown2 >= error1/errorDown1, we
    // can get a tigher bound of:
    //
    //          pEdgeRight->X - pEdgeLeft->X
    //
    // This case occurs often in thin strokes, so we check for it here.
    //

    assert!(pEdgeLeft.Error.get()  < 0);
    assert!(pEdgeRight.Error.get() < 0);
    assert!(pEdgeLeft.X <= pEdgeRight.X);

    let mut nSubpixelXDistanceLowerBound: INT = pEdgeRight.X.get() - pEdgeLeft.X.get();

    //
    // If error2/errorDown2 < error1/errorDown1, we need to subtract one from the bound.
    // Note that error's are actually baised by -1, we so we have to add one before
    // we do the comparison.
    //

    if (IsFractionLessThan(
             pEdgeRight.Error.get()+1,
             pEdgeRight.ErrorDown,
             pEdgeLeft.Error.get()+1,
             pEdgeLeft.ErrorDown
        ))
    {
            // We can't use the tighter lower bound described above, so we need to subtract one to
            // ensure we have a lower bound.

            nSubpixelXDistanceLowerBound -= 1;
    }

    return nSubpixelXDistanceLowerBound;
}
pub struct CHwRasterizer<'x, 'y, 'z> {
    m_rcClipBounds: MilPointAndSizeL,
    m_matWorldToDevice: CMILMatrix,
    m_pIGeometrySink: &'x mut CHwVertexBufferBuilder<'y, 'z>,
    m_fillMode: MilFillMode,
    /* 
DynArray<MilPoint2F> *m_prgPoints;
DynArray<BYTE>       *m_prgTypes;
MilPointAndSizeL      m_rcClipBounds;
CMILMatrix            m_matWorldToDevice;
IGeometrySink        *m_pIGeometrySink;
MilFillMode::Enum     m_fillMode;

//
// Complex scan coverage buffer
//

CCoverageBuffer m_coverageBuffer;

CD3DDeviceLevel1 * m_pDeviceNoRef;*/
    //m_coverageBuffer: CCoverageBuffer,
}

//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::ConvertSubpixelXToPixel
//
//  Synopsis:
//      Convert from our subpixel coordinate (x + error/errorDown)
//      to a floating point value.
//
//-------------------------------------------------------------------------
fn ConvertSubpixelXToPixel(
    x: INT,
    error: INT,
    rErrorDown: f32
    ) -> f32
{
    assert!(rErrorDown > f32::EPSILON);
    return ((x as f32) + (error as f32)/rErrorDown)*c_rInvShiftSize;
}

//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::ConvertSubpixelYToPixel
//
//  Synopsis:
//      Convert from our subpixel space to pixel space assuming no
//      error.
//
//-------------------------------------------------------------------------
fn ConvertSubpixelYToPixel(
    nSubpixel: i32
    ) -> f32
{
    return (nSubpixel as f32)*c_rInvShiftSize;
}

impl<'x, 'y, 'z> CHwRasterizer<'x, 'y, 'z> {
//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::RasterizePath
//
//  Synopsis:
//      Internal rasterizer fill path.  Note that this method follows the
//      same basic structure as the software rasterizer in aarasterizer.cpp.
//
//      The general algorithm used for rasterization is a vertical sweep of
//      the shape that maintains an active edge list.  The sweep is done
//      at a sub-scanline resolution and results in either:
//          1. Sub-scanlines being combined in the coverage buffer and output
//             as "complex scans".
//          2. Simple trapezoids being recognized in the active edge list
//             and output using a faster simple trapezoid path.
//
//      This method consists of the setup to the main rasterization loop
//      which includes:
//
//          1. Setup of the clip rectangle
//          2. Calling FixedPointPathEnumerate to populate our inactive
//             edge list.
//          3. Delegating to RasterizePath to execute the main loop.
//
//-------------------------------------------------------------------------
pub fn RasterizePath(
    &mut self,
    rgpt: &[POINT],
    rgTypes: &[BYTE],
    cPoints: UINT,
    pmatWorldTransform: &CMILMatrix
    ) -> HRESULT
{
    let mut hr;
    // Default is not implemented for arrays of size 40 so we need to use map
    let mut inactiveArrayStack: [CInactiveEdge; INACTIVE_LIST_NUMBER!()] = [(); INACTIVE_LIST_NUMBER!()].map(|_| Default::default());
    let mut pInactiveArray: &mut [CInactiveEdge];
    let mut pInactiveArrayAllocation: Vec<CInactiveEdge>;
    let mut edgeHead: CEdge = Default::default();
    let mut edgeTail: CEdge = Default::default();
    let pEdgeActiveList: Ref<CEdge>;
    let mut edgeStore = Arena::new();
    //edgeStore.init();
    let mut edgeContext: CInitializeEdgesContext = CInitializeEdgesContext::new(&mut edgeStore);

    edgeContext.ClipRect = None;

    edgeTail.X.set(i32::MAX);       // Terminator to active list
    edgeTail.StartY = i32::MAX;  // Terminator to inactive list

    edgeTail.EndY = i32::MIN;
    edgeHead.X.set(i32::MIN);       // Beginning of active list
    edgeContext.MaxY = i32::MIN;

    edgeHead.Next.set(Ref::new(&edgeTail));
    pEdgeActiveList = Ref::new(&mut edgeHead);
    //edgeContext.Store = &mut edgeStore;

    edgeContext.AntiAliasMode = c_antiAliasMode;
    assert!(edgeContext.AntiAliasMode != MilAntiAliasMode::None);

    // If the path contains 0 or 1 points, we can ignore it.
    if (cPoints < 2)
    {
        return S_OK;
    }

    let nPixelYClipBottom: INT = self.m_rcClipBounds.Y + self.m_rcClipBounds.Height;

    // Scale the clip bounds rectangle by 16 to account for our
    // scaling to 28.4 coordinates:

    let mut clipBounds : RECT = Default::default();
    clipBounds.left   = self.m_rcClipBounds.X * FIX4_ONE!();
    clipBounds.top    = self.m_rcClipBounds.Y * FIX4_ONE!();
    clipBounds.right  = (self.m_rcClipBounds.X + self.m_rcClipBounds.Width) * FIX4_ONE!();
    clipBounds.bottom = (self.m_rcClipBounds.Y + self.m_rcClipBounds.Height) * FIX4_ONE!();

    edgeContext.ClipRect = Some(&clipBounds);

    //////////////////////////////////////////////////////////////////////////
    // Convert all our points to 28.4 fixed point:

    let mut matrix: CMILMatrix = (*pmatWorldTransform).clone();
    AppendScaleToMatrix(&mut matrix, TOREAL!(16), TOREAL!(16));

    let coverageBuffer: CCoverageBuffer = Default::default();
    // Initialize the coverage buffer
    coverageBuffer.Initialize();

    // Enumerate the path and construct the edge table:

    hr = MIL_THR!(FixedPointPathEnumerate(
        rgpt,
        rgTypes,
        cPoints,
        &matrix,
        edgeContext.ClipRect,
        &mut edgeContext
        ));

    if (FAILED(hr))
    {
        if (hr == WGXERR_VALUEOVERFLOW)
        {
            // Draw nothing on value overflow and return
            hr = S_OK;
        }
        return hr;
    }

    let nTotalCount: UINT; nTotalCount = edgeContext.Store.len() as u32;
    if (nTotalCount == 0)
    {
        hr = S_OK;     // We're outta here (empty path or entirely clipped)
        return hr;
    }

    // At this point, there has to be at least two edges.  If there's only
    // one, it means that we didn't do the trivially rejection properly.

    assert!((nTotalCount >= 2) && (nTotalCount <= (UINT::MAX - 2)));

    pInactiveArray = &mut inactiveArrayStack[..];
    if (nTotalCount > (INACTIVE_LIST_NUMBER!() as u32 - 2))
    {
        pInactiveArrayAllocation = vec![Default::default(); nTotalCount as usize + 2];

        pInactiveArray = &mut pInactiveArrayAllocation;
    }

    // Initialize and sort the inactive array:

    let nSubpixelYCurrent = InitializeInactiveArray(
        edgeContext.Store,
        pInactiveArray,
        nTotalCount,
        Ref::new(&edgeTail)
        );

    let mut nSubpixelYBottom = edgeContext.MaxY;

    assert!(nSubpixelYBottom > 0);

    // Skip the head sentinel on the inactive array:

    pInactiveArray = &mut pInactiveArray[1..];

    //
    // Rasterize the path
    //

    // 'nPixelYClipBottom' is in screen space and needs to be converted to the
    // format we use for antialiasing.

    nSubpixelYBottom = nSubpixelYBottom.min(nPixelYClipBottom << c_nShift);

    // 'nTotalCount' should have been zero if all the edges were
    // clipped out (RasterizeEdges assumes there's at least one edge
    // to be drawn):

    assert!(nSubpixelYBottom > nSubpixelYCurrent);

    IFC!(self.RasterizeEdges(
        pEdgeActiveList,
        pInactiveArray,
        &coverageBuffer,
        nSubpixelYCurrent,
        nSubpixelYBottom
        ));

    return hr;
}

//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::new
//
//  Synopsis:
//      1. Ensure clean state
//      2. Convert path to internal format
//
//-------------------------------------------------------------------------
pub fn new(
    pIGeometrySink: &'x mut CHwVertexBufferBuilder<'y, 'z>,
    fillMode: MilFillMode,
    pmatWorldToDevice: Option<CMatrix<CoordinateSpace::Shape,CoordinateSpace::Device>>,
    clipRect: MilPointAndSizeL,
    ) -> Self
{
    //
    // PS#856364-2003/07/01-ashrafm  Remove pixel center fixup
    //
    // Incoming coordinate space uses integers at upper-left of pixel (pixel
    // center are half integers) at device level.
    //
    // Rasterizer uses the coordinate space with integers at pixel center.
    //
    // To convert from center (1/2, 1/2) to center (0, 0) we need to subtract
    // 1/2 from each coordinate in device space.
    //
    // See InitializeEdges in aarasterizer.ccp to see how we unconvert for
    // antialiased rendering.
    //

    let mut matWorldHPCToDeviceIPC = pmatWorldToDevice.unwrap_or(CMatrix::Identity());
    matWorldHPCToDeviceIPC.SetDx(matWorldHPCToDeviceIPC.GetDx() - 0.5);
    matWorldHPCToDeviceIPC.SetDy(matWorldHPCToDeviceIPC.GetDy() - 0.5);

    //
    // Set local state.
    //

    //  There's an opportunity for early clipping here
    //
    // However, since the rasterizer itself does a reasonable job of clipping some
    // cases, we don't early clip yet.

    Self {
        m_fillMode: fillMode,
        m_rcClipBounds: clipRect,
        m_pIGeometrySink: pIGeometrySink,
        m_matWorldToDevice: matWorldHPCToDeviceIPC,
    }
}

//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::SendGeometry
//
//  Synopsis:
//     Tessellate and send geometry to the pipeline
//
//-------------------------------------------------------------------------
pub fn SendGeometry(&mut self,
    points: &[POINT],
    types: &[BYTE],
    ) -> HRESULT
{
    let mut hr = S_OK;

    //
    // Rasterize the path
    //
    let count = points.len() as u32;
    IFR!(self.RasterizePath(
        points,
        types,
        count,
        &self.m_matWorldToDevice.clone(),
        ));
        /* 
    IFC!(self.RasterizePath(
        self.m_prgPoints.as_ref().unwrap().GetDataBuffer(),
        self.m_prgTypes.as_ref().unwrap().GetDataBuffer(),
        self.m_prgPoints.as_ref().unwrap().GetCount() as u32,
        &self.m_matWorldToDevice,
        self.m_fillMode
        ));*/

    //
    // It's possible that we output no triangles.  For example, if we tried to fill a
    // line instead of stroke it.  Since we have no efficient way to detect all these cases
    // up front, we simply rasterize and see if we generated anything.
    //

    if (self.m_pIGeometrySink.IsEmpty())
    {
        hr = WGXHR_EMPTYFILL;
    }

    RRETURN1!(hr, WGXHR_EMPTYFILL);
}
/*
//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::SendGeometryModifiers
//
//  Synopsis:   Send an AA color source to the pipeline.
//
//-------------------------------------------------------------------------
fn SendGeometryModifiers(&self,
    pPipelineBuilder: &mut CHwPipelineBuilder
    ) -> HRESULT
{
    let hr = S_OK;

    let pAntiAliasColorSource = None;

    self.m_pDeviceNoRef.GetColorComponentSource(
        CHwColorComponentSource::Diffuse,
        &pAntiAliasColorSource
        );

    IFC!(pPipelineBuilder.Set_AAColorSource(
        pAntiAliasColorSource
        ));

    return hr;
}*/

//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::GenerateOutputAndClearCoverage
//
//  Synopsis:
//      Collapse output and generate span data
//
//-------------------------------------------------------------------------
fn
GenerateOutputAndClearCoverage<'a>(&mut self, coverageBuffer: &'a CCoverageBuffer<'a>,
    nSubpixelY: INT
    ) -> HRESULT
{
    let hr = S_OK;
    let nPixelY = nSubpixelY >> c_nShift;

    let pIntervalSpanStart: Ref<CCoverageInterval> = coverageBuffer.m_pIntervalStart.get();

    IFC!(self.m_pIGeometrySink.AddComplexScan(nPixelY, pIntervalSpanStart));

    coverageBuffer.Reset();

    return hr;
}

//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::ComputeTrapezoidsEndScan
//
//  Synopsis:
//      This methods takes the current active edge list (and ycurrent)
//      and will determine:
//
//      1. Can we output some list of simple trapezoids for this active
//         edge list?  If the answer is no, then we simply return
//         nSubpixelYCurrent indicating this condition.
//
//      2. If we can output some set of trapezoids, then what is the
//         next ycurrent, i.e., how tall are our trapezoids.
//
//     Note that all trapezoids output for a particular active edge list
//     are all the same height.
//
//     To further understand the conditions for making this decision, it
//     is important to consider the simple trapezoid tessellation:
//
//           ___+_________________+___
//          /  +  /             \  +  \        '+' marks active edges
//         /  +  /               \  +  \
//        /  +  /                 \  +  \
//       /__+__/___________________\__+__\
//       1+1/m                         +
//
//      Note that 1+1/edge_slope is the required expand distance to ensure
//      that we cover all pixels required.
//
//      Now, we can fail to output any trapezoids under the following conditions:
//         1. The expand regions along the top edge of the trapezoid overlap.
//         2. The expand regions along the bottom edge of the trapezoid overlap
//            within the current scanline.  Note that if the bottom edges overlap
//            at some later point, we can shorten our trapezoid to remove the
//            overlapping.
//
//      The key to the algorithm at this point is to detect the above condition
//      in our active edge list and either update the returned end y position
//      or reject all together based on overlapping.
//
//-------------------------------------------------------------------------

fn ComputeTrapezoidsEndScan(&mut self,
    pEdgeCurrent: Ref<CEdge>,
    nSubpixelYCurrent: INT,
    nSubpixelYNextInactive: INT
    ) -> INT
{

    let mut nSubpixelYBottomTrapezoids;
    let mut pEdgeLeft: Ref<CEdge>;
    let mut pEdgeRight: Ref<CEdge>;

    //
    // Trapezoids should always start at scanline boundaries
    //

    assert!((nSubpixelYCurrent & c_nShiftMask) == 0);

    //
    // If we are doing a winding mode fill, check that we can ignore mode and do an
    // alternating fill in OutputTrapezoids.  This condition occurs when winding is
    // equivalent to alternating which happens if the pairwise edges have different
    // winding directions.
    //

    if (self.m_fillMode == MilFillMode::Winding)
    {
        let mut pEdge = pEdgeCurrent;
        while pEdge.EndY != INT::MIN {
            // The active edge list always has an even number of edges which we actually
            // assert in ASSERTACTIVELIST.

            assert!(pEdge.Next.get().EndY != INT::MIN);

            // If not alternating winding direction, we can't fill with alternate mode

            if (pEdge.WindingDirection == pEdge.Next.get().WindingDirection)
            {
                // Give up until we handle winding mode
                nSubpixelYBottomTrapezoids = nSubpixelYCurrent;
                return nSubpixelYBottomTrapezoids;
            }

            pEdge = pEdge.Next.get().Next.get();
        }
    }

    //
    // For each edge, we:
    //
    //    1. Set the new trapezoid bottom to the min of the current
    //       one and the edge EndY
    //
    //    2. Check if edges will intersect during trapezoidal shrink/expand
    //

    nSubpixelYBottomTrapezoids = nSubpixelYNextInactive;

    let mut pEdge = pEdgeCurrent;
    while pEdge.EndY != INT::MIN {
        //
        // Step 1
        //
        // Updated nSubpixelYBottomTrapezoids based on edge EndY.
        //
        // Since edges are clipped to the current clip rect y bounds, we also know
        // that pEdge->EndY <= nSubpixelYBottom so there is no need to check for that here.
        //

        nSubpixelYBottomTrapezoids = nSubpixelYBottomTrapezoids.min(pEdge.EndY);

        //
        // Step 2
        //
        // Check that edges will not overlap during trapezoid shrink/expand.
        //

        pEdgeLeft = pEdge;
        pEdgeRight = pEdge.Next.get();

        if (pEdgeRight.EndY != INT::MIN)
        {
            //
            //        __A__A'___________________B'_B__
            //        \  +  \                  /  +  /       '+' marks active edges
            //         \  +  \                /  +  /
            //          \  +  \              /  +  /
            //           \__+__\____________/__+__/
            //       1+1/m   C  C'         D' D
            //
            // We need to determine if position A' <= position B' and that position C' <= position D'
            // in the above diagram.  So, we need to ensure that both the distance between
            // A and B and the distance between C and D is greater than or equal to:
            //
            //    0.5 + |0.5/m1| + 0.5 + |0.5/m2|               (pixel space)
            //  = shiftsize + halfshiftsize*(|1/m1| + |1/m2|)   (subpixel space)
            //
            // So, we'll start by computing this distance.  Note that we can compute a distance
            // that is too large here since the self-intersection detection is simply used to
            // recognize trapezoid opportunities and isn't required for visual correctness.
            //

            let nSubpixelExpandDistanceUpperBound: INT =
                c_nShiftSize
                + ComputeDeltaUpperBound(&*pEdgeLeft, c_nHalfShiftSize)
                + ComputeDeltaUpperBound(&*pEdgeRight, c_nHalfShiftSize);

            //
            // Compute a top edge distance that is <= to the distance between A' and B' as follows:
            //   lowerbound(distance(A, B)) - nSubpixelExpandDistanceUpperBound
            //

            let nSubpixelXTopDistanceLowerBound: INT =
                ComputeDistanceLowerBound(&*pEdgeLeft, &*pEdgeRight) - nSubpixelExpandDistanceUpperBound;

            //
            // Check if the top edges cross
            //

            if (nSubpixelXTopDistanceLowerBound < 0)
            {
                // The top edges have crossed, so we are out of luck.  We can't
                // start a trapezoid on this scanline

                nSubpixelYBottomTrapezoids = nSubpixelYCurrent;
                return nSubpixelYBottomTrapezoids;
            }

            //
            // If the edges are converging, we need to check if they cross at
            // nSubpixelYBottomTrapezoids
            //
            //
            //  1) \       /    2) \    \       3)   /   /
            //      \     /          \   \          /  /
            //       \   /             \  \        / /
            //
            // The edges converge iff (dx1 > dx2 || (dx1 == dx2 && errorUp1/errorDown1 > errorUp2/errorDown2).
            //
            // Note that in the case where the edges do not converge, the code below will end up computing
            // the DDA at the end points and checking for intersection again.  This code doesn't rely on
            // the fact that the edges don't converge, so we can be too conservative here.
            //

            if (pEdgeLeft.Dx > pEdgeRight.Dx
                || ((pEdgeLeft.Dx == pEdgeRight.Dx)
                    && IsFractionGreaterThan(pEdgeLeft.ErrorUp, pEdgeLeft.ErrorDown, pEdgeRight.ErrorUp, pEdgeRight.ErrorDown)))
            {

                let nSubpixelYAdvance: INT =  nSubpixelYBottomTrapezoids - nSubpixelYCurrent;
                assert!(nSubpixelYAdvance > 0);

                //
                // Compute the edge position at nSubpixelYBottomTrapezoids
                //

                let mut nSubpixelXLeftAdjustedBottom = 0;
                let mut nSubpixelErrorLeftBottom = 0;
                let mut nSubpixelXRightBottom = 0;
                let mut nSubpixelErrorRightBottom = 0;

                AdvanceDDAMultipleSteps(
                    &*pEdgeLeft,
                    &*pEdgeRight,
                    nSubpixelYAdvance,
                    &mut nSubpixelXLeftAdjustedBottom,
                    &mut nSubpixelErrorLeftBottom,
                    &mut nSubpixelXRightBottom,
                    &mut nSubpixelErrorRightBottom
                    );

                //
                // Adjust the bottom left position by the expand distance for all the math
                // that follows.  Note that since we adjusted the top distance by that
                // same expand distance, this adjustment is equivalent to moving the edges
                // nSubpixelExpandDistanceUpperBound closer together.
                //

                nSubpixelXLeftAdjustedBottom += nSubpixelExpandDistanceUpperBound;

                //
                // Check if the bottom edge crosses.
                //
                // To avoid checking error1/errDown1 and error2/errDown2, we assume the
                // edges cross if nSubpixelXLeftAdjustedBottom == nSubpixelXRightBottom
                // and thus produce a result that is too conservative.
                //

                if (nSubpixelXLeftAdjustedBottom >= nSubpixelXRightBottom)
                {

                    //
                    // At this point, we have the following scenario
                    //
                    //            ____d1____
                    //            \        /   |   |
                    //              \    /     h1  |
                    //                \/       |   | nSubpixelYAdvance
                    //               /  \          |
                    //             /__d2__\        |
                    //
                    // We want to compute h1.  We know that:
                    //
                    //     h1 / nSubpixelYAdvance = d1 / (d1 + d2)
                    //     h1 = nSubpixelYAdvance * d1 / (d1 + d2)
                    //
                    // Now, if we approximate d1 with some d1' <= d1, we get
                    //
                    //     h1 = nSubpixelYAdvance * d1 / (d1 + d2)
                    //     h1 >= nSubpixelYAdvance * d1' / (d1' + d2)
                    //
                    // Similarly, if we approximate d2 with some d2' >= d2, we get
                    //
                    //     h1 >= nSubpixelYAdvance * d1' / (d1' + d2)
                    //        >= nSubpixelYAdvance * d1' / (d1' + d2')
                    //
                    // Since we are allowed to be too conservative with h1 (it can be
                    // less than the actual value), we'll construct such approximations
                    // for simplicity.
                    //
                    // Note that d1' = nSubpixelXTopDistanceLowerBound which we have already
                    // computed.
                    //
                    //      d2 = (x1 + error1/errorDown1) - (x2 + error2/errorDown2)
                    //         = x1 - x2 + error1/errorDown1 - error2/errorDown2
                    //         <= x1 - x2 - error2/errorDown2   , since error1 < 0
                    //         <= x1 - x2 + 1                   , since error2 < 0
                    //         = nSubpixelXLeftAdjustedBottom - nSubpixelXRightBottom + 1
                    //

                    let nSubpixelXBottomDistanceUpperBound: INT = nSubpixelXLeftAdjustedBottom - nSubpixelXRightBottom + 1;

                    assert!(nSubpixelXTopDistanceLowerBound >= 0);
                    assert!(nSubpixelXBottomDistanceUpperBound > 0);

                    #[cfg(debug_assertions)]
                    let nDbgPreviousSubpixelXBottomTrapezoids: INT = nSubpixelYBottomTrapezoids;


                    nSubpixelYBottomTrapezoids =
                        nSubpixelYCurrent +
                        (nSubpixelYAdvance * nSubpixelXTopDistanceLowerBound) /
                        (nSubpixelXTopDistanceLowerBound + nSubpixelXBottomDistanceUpperBound);

                    #[cfg(debug_assertions)]
                    assert!(nDbgPreviousSubpixelXBottomTrapezoids >= nSubpixelYBottomTrapezoids);

                    if (nSubpixelYBottomTrapezoids < nSubpixelYCurrent + c_nShiftSize)
                    {
                        // We no longer have a trapezoid that is at least one scanline high, so
                        // abort

                        nSubpixelYBottomTrapezoids = nSubpixelYCurrent;
                        return nSubpixelYBottomTrapezoids;
                    }
                }
            }
        }

        pEdge = pEdge.Next.get();
    }

    //
    // Snap to pixel boundary
    //

    nSubpixelYBottomTrapezoids = nSubpixelYBottomTrapezoids & (!c_nShiftMask);

    //
    // Ensure that we are never less than nSubpixelYCurrent
    //

    assert!(nSubpixelYBottomTrapezoids >= nSubpixelYCurrent);

    //
    // Return trapezoid end scan
    //

//Cleanup:
    return nSubpixelYBottomTrapezoids;
}


//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::OutputTrapezoids
//
//  Synopsis:
//      Given the current active edge list, output a list of
//      trapezoids.
//
//      _________________________
//     /     /             \     \
//    /     /               \     \
//   /     /                 \     \
//  /_____/___________________\_____\
//  1+1/m
//
// We output a trapezoid where the distance in X is 1+1/m slope on either edge.
// Note that we actually do a linear interpolation for coverage along the
// entire falloff region which comes within 12.5% error when compared to our
// 8x8 coverage output for complex scans.  What is happening here is
// that we are applying a linear approximation to the coverage function
// based on slope.  It is possible to get better linear interpolations
// by varying the expanded region, but it hasn't been necessary to apply
// these quality improvements yet.
//
//-------------------------------------------------------------------------
fn 
OutputTrapezoids(&mut self,
    pEdgeCurrent: Ref<CEdge>,
    nSubpixelYCurrent: INT, // inclusive
    nSubpixelYNext: INT     // exclusive
    ) -> HRESULT
{

    let hr = S_OK;
    let nSubpixelYAdvance: INT;
    let mut rSubpixelLeftErrorDown: f32;
    let mut rSubpixelRightErrorDown: f32;
    let mut rPixelXLeft: f32;
    let mut rPixelXRight: f32;
    let mut rSubpixelLeftInvSlope: f32;
    let mut rSubpixelLeftAbsInvSlope: f32;
    let mut rSubpixelRightInvSlope: f32;
    let mut rSubpixelRightAbsInvSlope: f32;
    let mut rPixelXLeftDelta: f32;
    let mut rPixelXRightDelta: f32;

    let mut pEdgeLeft = pEdgeCurrent;
    let mut pEdgeRight = (*pEdgeCurrent).Next.get();

    assert!((nSubpixelYCurrent & c_nShiftMask) == 0);
    assert!(pEdgeLeft.EndY != INT::MIN);
    assert!(pEdgeRight.EndY != INT::MIN);

    //
    // Compute the height our trapezoids
    //

    nSubpixelYAdvance = nSubpixelYNext - nSubpixelYCurrent;

    //
    // Output each trapezoid
    //

    loop
    {
        //
        // Compute x/error for end of trapezoid
        //

        let mut nSubpixelXLeftBottom: INT = 0;
        let mut nSubpixelErrorLeftBottom: INT = 0;
        let mut nSubpixelXRightBottom: INT = 0;
        let mut nSubpixelErrorRightBottom: INT = 0;

        AdvanceDDAMultipleSteps(
            &*pEdgeLeft,
            &*pEdgeRight,
            nSubpixelYAdvance,
            &mut nSubpixelXLeftBottom,
            &mut nSubpixelErrorLeftBottom,
            &mut nSubpixelXRightBottom,
            &mut nSubpixelErrorRightBottom
            );

        // The above computation should ensure that we are a simple
        // trapezoid at this point

        assert!(nSubpixelXLeftBottom <= nSubpixelXRightBottom);

        // We know we have a simple trapezoid now.  Now, compute the end of our current trapezoid

        assert!(nSubpixelYAdvance > 0);

        //
        // Computation of edge data
        //

        rSubpixelLeftErrorDown  = pEdgeLeft.ErrorDown as f32;
        rSubpixelRightErrorDown = pEdgeRight.ErrorDown as f32;
        rPixelXLeft  = ConvertSubpixelXToPixel(pEdgeLeft.X.get(), pEdgeLeft.Error.get(), rSubpixelLeftErrorDown);
        rPixelXRight = ConvertSubpixelXToPixel(pEdgeRight.X.get(), pEdgeRight.Error.get(), rSubpixelRightErrorDown);

        rSubpixelLeftInvSlope     = pEdgeLeft.Dx as f32 + pEdgeLeft.ErrorUp as f32/rSubpixelLeftErrorDown;
        rSubpixelLeftAbsInvSlope  = rSubpixelLeftInvSlope.abs();
        rSubpixelRightInvSlope    = pEdgeRight.Dx as f32 + pEdgeRight.ErrorUp as f32/rSubpixelRightErrorDown;
        rSubpixelRightAbsInvSlope = rSubpixelRightInvSlope.abs();

        rPixelXLeftDelta  = 0.5 + 0.5 * rSubpixelLeftAbsInvSlope;
        rPixelXRightDelta = 0.5 + 0.5 * rSubpixelRightAbsInvSlope;

        let rPixelYTop         = ConvertSubpixelYToPixel(nSubpixelYCurrent);
        let rPixelYBottom      = ConvertSubpixelYToPixel(nSubpixelYNext);

        let rPixelXBottomLeft  = ConvertSubpixelXToPixel(
                                        nSubpixelXLeftBottom,
                                        nSubpixelErrorLeftBottom,
                                        pEdgeLeft.ErrorDown as f32
                                        );

        let rPixelXBottomRight = ConvertSubpixelXToPixel(
                                        nSubpixelXRightBottom,
                                        nSubpixelErrorRightBottom,
                                        pEdgeRight.ErrorDown as f32
                                        );

        //
        // Output the trapezoid
        //

        IFC!(self.m_pIGeometrySink.AddTrapezoid(
            rPixelYTop,              // In: y coordinate of top of trapezoid
            rPixelXLeft,             // In: x coordinate for top left
            rPixelXRight,            // In: x coordinate for top right
            rPixelYBottom,           // In: y coordinate of bottom of trapezoid
            rPixelXBottomLeft,       // In: x coordinate for bottom left
            rPixelXBottomRight,      // In: x coordinate for bottom right
            rPixelXLeftDelta,        // In: trapezoid expand radius for left edge
            rPixelXRightDelta        // In: trapezoid expand radius for right edge
            ));

        //
        // Update the edge data
        //

        //  no need to do this if edges are stale

        pEdgeLeft.X.set(nSubpixelXLeftBottom);
        pEdgeLeft.Error.set(nSubpixelErrorLeftBottom);
        pEdgeRight.X.set(nSubpixelXRightBottom);
        pEdgeRight.Error.set(nSubpixelErrorRightBottom);

        //
        // Check for termination
        //

        if (pEdgeRight.Next.get().EndY == INT::MIN)
        {
            break;
        }

        //
        // Advance edge data
        //

        pEdgeLeft  = pEdgeRight.Next.get();
        pEdgeRight = pEdgeLeft.Next.get();

    }

    return hr;

}

//-------------------------------------------------------------------------
//
//  Function:   CHwRasterizer::RasterizeEdges
//
//  Synopsis:
//      Rasterize using trapezoidal AA
//
//-------------------------------------------------------------------------
fn
RasterizeEdges<'a, 'b>(&mut self,
    pEdgeActiveList: Ref<'a, CEdge<'a>>,
    mut pInactiveEdgeArray: &'a mut [CInactiveEdge<'a>],
    coverageBuffer: &'b CCoverageBuffer<'b>,
    mut nSubpixelYCurrent: INT,
    nSubpixelYBottom: INT
    ) -> HRESULT
{
    let hr: HRESULT = S_OK;
    let mut pEdgePrevious: Ref<CEdge>;
    let mut pEdgeCurrent: Ref<CEdge>;
    let mut nSubpixelYNextInactive: INT = 0;
    let mut nSubpixelYNext: INT;

    pInactiveEdgeArray = InsertNewEdges(
        pEdgeActiveList,
        nSubpixelYCurrent,
        pInactiveEdgeArray,
        &mut nSubpixelYNextInactive
        );

    while (nSubpixelYCurrent < nSubpixelYBottom)
    {
        ASSERTACTIVELIST!(pEdgeActiveList, nSubpixelYCurrent);

        //
        // Detect trapezoidal case
        //

        pEdgePrevious = pEdgeActiveList;
        pEdgeCurrent = pEdgeActiveList.Next.get();

        nSubpixelYNext = nSubpixelYCurrent;

        if (!IsTagEnabled!(tagDisableTrapezoids)
            && (nSubpixelYCurrent & c_nShiftMask) == 0
            && pEdgeCurrent.EndY != INT::MIN
            && nSubpixelYNextInactive >= nSubpixelYCurrent + c_nShiftSize
            )
        {
            // Edges are paired, so we can assert we have another one
            assert!(pEdgeCurrent.Next.get().EndY != INT::MIN);

            //
            // Given an active edge list, we compute the furthest we can go in the y direction
            // without creating self-intersection or going past the edge EndY.  Note that if we
            // can't even go one scanline, then nSubpixelYNext == nSubpixelYCurrent
            //

            nSubpixelYNext = self.ComputeTrapezoidsEndScan(Ref::new(&*pEdgeCurrent), nSubpixelYCurrent, nSubpixelYNextInactive);
            assert!(nSubpixelYNext >= nSubpixelYCurrent);

            //
            // Attempt to output a trapezoid.  If it turns out we don't have any
            // potential trapezoids, then nSubpixelYNext == nSubpixelYCurent
            // indicating that we need to fall back to complex scans.
            //

            if (nSubpixelYNext >= nSubpixelYCurrent + c_nShiftSize)
            {
                IFC!(self.OutputTrapezoids(
                    pEdgeCurrent,
                    nSubpixelYCurrent,
                    nSubpixelYNext
                    ));
            }
        }

        //
        // Rasterize simple trapezoid or a complex scanline
        //

        if (nSubpixelYNext > nSubpixelYCurrent)
        {
            // If we advance, it must be by at least one scan line

            assert!(nSubpixelYNext - nSubpixelYCurrent >= c_nShiftSize);

            // Advance nSubpixelYCurrent

            nSubpixelYCurrent = nSubpixelYNext;

            // Remove stale edges.  Note that the DDA is incremented in OutputTrapezoids.

            while (pEdgeCurrent.EndY != INT::MIN)
            {
                if (pEdgeCurrent.EndY <= nSubpixelYCurrent)
                {
                    // Unlink and advance

                    pEdgeCurrent = pEdgeCurrent.Next.get();
                    pEdgePrevious.Next.set(pEdgeCurrent);
                }
                else
                {
                    // Advance

                    pEdgePrevious = pEdgeCurrent;
                    pEdgeCurrent = pEdgeCurrent.Next.get();
                }
            }
        }
        else
        {
            //
            // Trapezoid rasterization failed, so
            //   1) Handle case with no active edges, or
            //   2) fall back to scan rasterization
            //

            if (pEdgeCurrent.EndY == INT::MIN)
            {
                nSubpixelYNext = nSubpixelYNextInactive;
            }
            else
            {
                nSubpixelYNext = nSubpixelYCurrent + 1;
                if (self.m_fillMode == MilFillMode::Alternate)
                {
                    IFC!(coverageBuffer.FillEdgesAlternating(pEdgeActiveList, nSubpixelYCurrent));
                }
                else
                {
                    IFC!(coverageBuffer.FillEdgesWinding(pEdgeActiveList, nSubpixelYCurrent));
                }
            }

            // If the next scan is done, output what's there:
            if (nSubpixelYNext > (nSubpixelYCurrent | c_nShiftMask))
            {
                IFC!(self.GenerateOutputAndClearCoverage(coverageBuffer, nSubpixelYCurrent));
            }

            // Advance nSubpixelYCurrent
            nSubpixelYCurrent = nSubpixelYNext;

            // Advance DDA and update edge list
            AdvanceDDAAndUpdateActiveEdgeList(nSubpixelYCurrent, pEdgeActiveList);
        }

        //
        // Update edge list
        //

        if (nSubpixelYCurrent == nSubpixelYNextInactive)
        {
            pInactiveEdgeArray = InsertNewEdges(
                pEdgeActiveList,
                nSubpixelYCurrent,
                pInactiveEdgeArray,
                &mut nSubpixelYNextInactive
                );
        }
    }

    //
    // Output the last scanline that has partial coverage
    //

    if ((nSubpixelYCurrent & c_nShiftMask) != 0)
    {
        IFC!(self.GenerateOutputAndClearCoverage(coverageBuffer, nSubpixelYCurrent));
    }

    RRETURN!(hr);
}

}
