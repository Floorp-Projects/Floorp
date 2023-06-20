// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#![allow(unused_parens)]

use std::cell::Cell;

use crate::aacoverage::c_nShift;
use crate::bezier::CMILBezier;
use crate::helpers::Int32x32To64;
use crate::matrix::CMILMatrix;
use crate::nullable_ref::Ref;
use crate::real::CFloatFPU;
//use crate::types::PathPointType::*;
use crate::types::*;
use typed_arena_nomut::Arena;

const S_OK: HRESULT = 0;

#[cfg(debug_assertions)]
macro_rules! EDGE_STORE_STACK_NUMBER {
    () => {
        10
    };
}
#[cfg(debug_assertions)]
macro_rules! EDGE_STORE_ALLOCATION_NUMBER { () => { 11 }; }
#[cfg(debug_assertions)]
macro_rules! INACTIVE_LIST_NUMBER { () => { 12 }; }
#[cfg(debug_assertions)]
macro_rules! ENUMERATE_BUFFER_NUMBER { () => { 15 }; }

#[cfg(not(debug_assertions))]
macro_rules! EDGE_STORE_STACK_NUMBER { () => { (1600 / std::mem::size_of::<CEdge>()) }; }
#[cfg(not(debug_assertions))]
macro_rules! EDGE_STORE_ALLOCATION_NUMBER { () => { (4032 / std::mem::size_of::<CEdge>()) as u32 }; }
#[cfg(not(debug_assertions))]
macro_rules! INACTIVE_LIST_NUMBER { () => { EDGE_STORE_STACK_NUMBER!() }; }
#[cfg(not(debug_assertions))]
macro_rules! ENUMERATE_BUFFER_NUMBER { () => { 32 }; }

macro_rules! ASSERTACTIVELIST {
    ($list: expr, $y: expr) => {
        // make sure we use y even in non debug builds
        _ = $y;
        #[cfg(debug_assertions)]
        AssertActiveList($list, $y);
    };
}
pub struct CEdge<'a> {
    pub Next: Cell<Ref<'a, CEdge<'a>>>, // Next active edge (don't check for NULL,
    //   look for tail sentinel instead)
    pub X: Cell<INT>,                // Current X location
    pub Dx: INT,               // X increment
    pub Error: Cell<INT>,            // Current DDA error
    pub ErrorUp: INT,          // Error increment
    pub ErrorDown: INT,        // Error decrement when the error rolls over
    pub StartY: INT,           // Y-row start
    pub EndY: INT,             // Y-row end
    pub WindingDirection: INT, // -1 or 1
}

impl<'a> std::default::Default for CEdge<'a> {
    fn default() -> Self {
        Self {
            Next: Cell::new(unsafe { Ref::null() }),
            X: Default::default(),
            Dx: Default::default(),
            Error: Default::default(),
            ErrorUp: Default::default(),
            ErrorDown: Default::default(),
            StartY: Default::default(),
            EndY: Default::default(),
            WindingDirection: Default::default(),
        }
    }
}

// We the inactive-array separate from the edge allocations so that
// we can more easily do in-place sorts on it:
#[derive(Clone)]
pub struct CInactiveEdge<'a> {
    Edge: Ref<'a, CEdge<'a>>, // Associated edge
    Yx: LONGLONG,     // Sorting key, StartY and X packed into an lword
}

impl<'a> Default for CInactiveEdge<'a> {
    fn default() -> Self {
        Self {
            Edge: unsafe { Ref::null() },
            Yx: Default::default(),
        }
    }
}
macro_rules! ASSERTACTIVELISTORDER {
    ($list: expr) => {
        #[cfg(debug_assertions)]
        AssertActiveListOrder($list)
    };
}

/**************************************************************************\
*
* Function Description:
*
* Advance DDA and update active edge list
*
* Created:
*
*   06/20/2003 ashrafm
*
\**************************************************************************/
pub fn AdvanceDDAAndUpdateActiveEdgeList(nSubpixelYCurrent: INT, pEdgeActiveList: Ref<CEdge>) {
        let mut outOfOrder = false;
        let mut pEdgePrevious: Ref<CEdge> = pEdgeActiveList;
        let mut pEdgeCurrent: Ref<CEdge> = pEdgeActiveList.Next.get();
        let mut prevX = pEdgePrevious.X.get();

        // Advance DDA and update edge list

        loop {
            if (pEdgeCurrent.EndY <= nSubpixelYCurrent) {
                // If we've hit the sentinel, our work here is done:

                if (pEdgeCurrent.EndY == INT::MIN) {
                    break; // ============>
                }
                // This edge is stale, remove it from the list:

                pEdgeCurrent = pEdgeCurrent.Next.get();
                pEdgePrevious.Next.set(pEdgeCurrent);
                continue; // ============>
            }

            // Advance the DDA:

            let mut x = pEdgeCurrent.X.get() + pEdgeCurrent.Dx;
            let mut error = pEdgeCurrent.Error.get() + pEdgeCurrent.ErrorUp;
            if (error >= 0) {
                error -= pEdgeCurrent.ErrorDown;
                x += 1;
            }
            pEdgeCurrent.X.set(x);
            pEdgeCurrent.Error.set(error);

            // Is this entry out-of-order with respect to the previous one?
            outOfOrder |= (prevX > x);

            // Advance:

            pEdgePrevious = pEdgeCurrent;
            pEdgeCurrent = pEdgeCurrent.Next.get();
            prevX = x;
        }

        // It turns out that having any out-of-order edges at this point
        // is extremely rare in practice, so only call the bubble-sort
        // if it's truly needed.
        //
        // NOTE: If you're looking at this code trying to fix a bug where
        //       the edges are out of order when the filler is called, do
        //       NOT simply change the code to always do the bubble-sort!
        //       Instead, figure out what caused our 'outOfOrder' logic
        //       above to get messed up.

        if (outOfOrder) {
            SortActiveEdges(pEdgeActiveList);
        }
        ASSERTACTIVELISTORDER!(pEdgeActiveList);

}

//+----------------------------------------------------------------------------
//

//
//  Description:  Code for rasterizing the fill of a path.
//
//  >>>> Note that some of this code is duplicated in hw\hwrasterizer.cpp,
//  >>>> so changes to this file may need to propagate.
//
//   pursue reduced code duplication
//

// This option may potentially increase performance for many
// paths that have edges adjacent at their top point and cover
// more than one span.  The code has been tested, but performance
// has not been thoroughly investigated.
const SORT_EDGES_INCLUDING_SLOPE: bool = false;

/////////////////////////////////////////////////////////////////////////
// The x86 C compiler insists on making a divide and modulus operation
// into two DIVs, when it can in fact be done in one.  So we use this
// macro.
//
// Note: QUOTIENT_REMAINDER implicitly takes unsigned arguments.
//
// QUOTIENT_REMAINDER_64_32 takes a 64-bit numerator and produces 32-bit
// results.

macro_rules! QUOTIENT_REMAINDER {
    ($ulNumerator: ident, $ulDenominator: ident, $ulQuotient: ident, $ulRemainder: ident) => {
        $ulQuotient = (($ulNumerator as ULONG) / ($ulDenominator as ULONG)) as _;
        $ulRemainder = (($ulNumerator as ULONG) % ($ulDenominator as ULONG)) as _;
    };
}

macro_rules! QUOTIENT_REMAINDER_64_32 {
    ($ulNumerator: ident, $ulDenominator: ident, $ulQuotient: ident, $ulRemainder: ident) => {
        $ulQuotient = (($ulNumerator as ULONGLONG) / (($ulDenominator as ULONG) as ULONGLONG)) as _;
        $ulRemainder =
            (($ulNumerator as ULONGLONG) % (($ulDenominator as ULONG) as ULONGLONG)) as _;
    };
}

// SWAP macro:
macro_rules! SWAP {
    ($temp: ident, $a: expr, $b: expr) => {
        $temp = $a;
        $a = $b;
        $b = $temp;
    };
}

struct CEdgeAllocation {
    Next: *mut CEdgeAllocation, // Next allocation batch (may be NULL)
    /*__field_range(<=, EDGE_STORE_ALLOCATION_NUMBER)*/ Count: UINT,
    EdgeArray: [CEdge<'static>; EDGE_STORE_STACK_NUMBER!()],
}

impl Default for CEdgeAllocation {
    fn default() -> Self {
        Self { Next: NULL(), Count: Default::default(), EdgeArray: [(); EDGE_STORE_STACK_NUMBER!()].map(|_| Default::default()) }
    }
}
/* 
pub struct CEdgeStore {
    /* __field_range(<=, UINT_MAX - 2) */ TotalCount: UINT, // Total edge count in store
    /* __field_range(<=, CurrentBuffer->Count) */
    CurrentRemaining: UINT, // How much room remains in current buffer
    CurrentBuffer: *mut CEdgeAllocation, // Current buffer
    CurrentEdge: *mut CEdge<'static>, // Current edge in current buffer
    Enumerator: *mut CEdgeAllocation, // For enumerating all the edges
    EdgeHead: CEdgeAllocation, // Our built-in allocation
}

impl Default for CEdgeStore {
    fn default() -> Self {
        Self { TotalCount: Default::default(), CurrentRemaining: Default::default(), CurrentBuffer: NULL(), CurrentEdge: NULL(), Enumerator: NULL(), EdgeHead: Default::default() }
    }
}

impl CEdgeStore {
    pub fn init(&mut self) {
        self.TotalCount = 0;
        self.CurrentBuffer = NULL();
        self.CurrentEdge = NULL();
        self.Enumerator = NULL();
        self.CurrentRemaining = EDGE_STORE_STACK_NUMBER!() as u32;

        self.EdgeHead = CEdgeAllocation {
                Count: EDGE_STORE_STACK_NUMBER!() as u32,
                // hack to work around limited Default implementation for arrays
                EdgeArray: [(); EDGE_STORE_STACK_NUMBER!()].map(|_| Default::default()),
                Next: NULL(),
            };
        self.CurrentBuffer = &mut self.EdgeHead;
        self.CurrentEdge = &mut self.EdgeHead.EdgeArray[0];
    }
}

impl Drop for CEdgeStore {
    fn drop(&mut self) {
        // Free our allocation list, skipping the head, which is not
        // dynamically allocated:

        let mut allocation: *mut CEdgeAllocation = self.EdgeHead.Next;
        while (allocation != NULL()) {
            let next = unsafe { (*allocation).Next };
            drop(unsafe { Box::from_raw(allocation) });
            allocation = next;
        }
    }
}

impl CEdgeStore {
    pub fn StartEnumeration(&mut self) -> UINT {
        unsafe {
            self.Enumerator = &mut self.EdgeHead;

            // Update the count and make sure nothing more gets added (in
            // part because this Count would have to be re-computed):

            (*self.CurrentBuffer).Count -= self.CurrentRemaining;

            // This will never overflow because NextAddBuffer always ensures that TotalCount has
            // space remaining to describe the capacity of all new buffers added to the edge list.
            self.TotalCount += (*self.CurrentBuffer).Count;

            // Prevent this from being called again, because bad things would
            // happen:

            self.CurrentBuffer = NULL();

            return self.TotalCount;
        }
    }

    fn Enumerate(
        &mut self,
        /*__deref_out_ecount(*ppEndEdge - *ppStartEdge)*/ ppStartEdge: &mut *mut CEdge,
        /* __deref_out_ecount(0) */ ppEndEdge: &mut *mut CEdge,
    ) -> bool {
        /* 
        unsafe {
            let enumerator: *mut CEdgeAllocation = self.Enumerator;

            // Might return startEdge == endEdge:

            *ppStartEdge = &mut (*enumerator).EdgeArray[0];
            *ppEndEdge = (*ppStartEdge).offset((*enumerator).Count as isize);

            self.Enumerator = (*enumerator).Next;
            return (self.Enumerator != NULL());
        }*/
        return true;
    }

    fn StartAddBuffer(
        &self,
        /*__deref_out_ecount(*puRemaining)*/ ppCurrentEdge: &mut *mut CEdge,
        /* __deref_out_range(==, (this->CurrentRemaining)) */ puRemaining: &mut UINT,
    ) {
        panic!()
        // *ppCurrentEdge = self.CurrentEdge;
        // *puRemaining = self.CurrentRemaining;
    }

    fn EndAddBuffer(
        &mut self,
        /*__in_ecount(remaining) */ pCurrentEdge: *mut CEdge,
        /* __range(0, (this->CurrentBuffer->Count)) */ remaining: UINT,
    ) {
        panic!();
        //self.CurrentEdge = pCurrentEdge;
        //self.CurrentRemaining = remaining;
    }

    // Disable instrumentation checks within all methods of this class
    //SET_MILINSTRUMENTATION_FLAGS(MILINSTRUMENTATIONFLAGS_DONOTHING);
}

/**************************************************************************\
*
* Function Description:
*
*   The edge initializer is out of room in its current 'store' buffer;
*   get it a new one.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

impl CEdgeStore {
    fn NextAddBuffer(
        &mut self,
        /*__deref_out_ecount(*puRemaining)*/ ppCurrentEdge: &mut *mut CEdge,
        puRemaining: &mut UINT,
    ) -> HRESULT {
        panic!()
        /* 
        unsafe {
            let hr = S_OK;

            let mut cNewTotalCount: u32 = 0;

            // The caller has completely filled up this chunk:

            assert!(*puRemaining == 0);

            // Check to make sure that "TotalCount" will be able to represent the current capacity
            cNewTotalCount = self.TotalCount + (*self.CurrentBuffer).Count;

            if (cNewTotalCount < self.TotalCount) {
                return WINCODEC_ERR_VALUEOVERFLOW;
            }

            // And that it can represent the new capacity as well, with at least 2 to spare.
            // This "magic" 2 comes from the fact that the usage pattern of this class has callers
            // needing to allocate space for TotalCount + 2 edges.
            if (cNewTotalCount + ((EDGE_STORE_ALLOCATION_NUMBER!() + 2) as UINT) < cNewTotalCount) {
                return WINCODEC_ERR_VALUEOVERFLOW;
            }

            // We have to grow our data structure by adding a new buffer
            // and adding it to the list:

            let newBuffer: *mut CEdgeAllocation = Box::into_raw(Box::<CEdgeAllocation>::new(Default::default()));/*static_cast<CEdgeAllocation*>
                                                            (GpMalloc(Mt(MAARasterizerEdge),
                                                                      sizeof(CEdgeAllocation) +
                                                                      sizeof(CEdge) * (EDGE_STORE_ALLOCATION_NUMBER
                                                                                      - EDGE_STORE_STACK_NUMBER)));*/
            IFCOOM!(newBuffer);

            (*newBuffer).Next = NULL();
            (*newBuffer).Count = EDGE_STORE_STACK_NUMBER!() as u32;//EDGE_STORE_ALLOCATION_NUMBER!() as u32;

            self.TotalCount = cNewTotalCount;

            (*self.CurrentBuffer).Next = newBuffer;
            self.CurrentBuffer = newBuffer;

            self.CurrentEdge = &mut (*newBuffer).EdgeArray[0];
            *ppCurrentEdge = panic!();//self.CurrentEdge;
            self.CurrentRemaining = EDGE_STORE_STACK_NUMBER!() as u32;//EDGE_STORE_ALLOCATION_NUMBER!();
            *puRemaining = EDGE_STORE_STACK_NUMBER!() as u32; //EDGE_STORE_ALLOCATION_NUMBER!();

            return hr;
        }*/
    }
}
*/
/**************************************************************************\
*
* Function Description:
*
*   Some debug code for verifying the state of the active edge list.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

pub fn AssertActiveList(mut list: Ref<CEdge>, yCurrent: INT) -> bool {

    let mut b = true;
    let mut activeCount = 0;

    assert!((*list).X.get() == INT::MIN);
    b &= ((*list).X.get() == INT::MIN);

    // Skip the head sentinel:

    list = (*list).Next.get();

    while ((*list).X.get() != INT::MAX) {
        assert!((*list).X.get() != INT::MIN);
        b &= ((*list).X.get() != INT::MIN);

        assert!((*list).X <= (*(*list).Next.get()).X);
        b &= ((*list).X <= (*(*list).Next.get()).X);

        assert!(((*list).StartY <= yCurrent) && (yCurrent < (*list).EndY));
        b &= (((*list).StartY <= yCurrent) && (yCurrent < (*list).EndY));

        activeCount += 1;
        list = (*list).Next.get();
    }

    assert!((*list).X.get() == INT::MAX);
    b &= ((*list).X.get() == INT::MAX);

    // There should always be a multiple of 2 edges in the active list.
    //
    // NOTE: If you hit this assert, do NOT simply comment it out!
    //       It usually means that all the edges didn't get initialized
    //       properly.  For every scan-line, there has to be a left edge
    //       and a right edge (or a multiple thereof).  So if you give
    //       even a single bad edge to the edge initializer (or you miss
    //       one), you'll probably hit this assert.

    assert!((activeCount & 1) == 0);
    b &= ((activeCount & 1) == 0);

    return (b);

}

/**************************************************************************\
*
* Function Description:
*
*   Some debug code for verifying the state of the active edge list.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

fn AssertActiveListOrder(mut list:  Ref<CEdge>) {

    assert!((*list).X.get() == INT::MIN);

    // Skip the head sentinel:

    list = (*list).Next.get();

    while ((*list).X.get() != INT::MAX) {
        assert!((*list).X.get() != INT::MIN);
        assert!((*list).X <= (*(*list).Next.get()).X);

        list = (*list).Next.get();
    }

    assert!((*list).X.get() == INT::MAX);
}

/**************************************************************************\
*
* Function Description:
*
*   Clip the edge vertically.
*
*   We've pulled this routine out-of-line from InitializeEdges mainly
*   because it needs to call inline Asm, and when there is in-line
*   Asm in a routine the compiler generally does a much less efficient
*   job optimizing the whole routine.  InitializeEdges is rather
*   performance critical, so we avoid polluting the whole routine
*   by having this functionality out-of-line.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/
fn ClipEdge(edgeBuffer: &mut CEdge, yClipTopInteger: INT, dMOriginal: INT) {
    let mut xDelta: INT;
    let mut error: INT;

    // Cases where bigNumerator will exceed 32-bits in precision
    // will be rare, but could happen, and we can't fall over
    // in those cases.

    let dN: INT = edgeBuffer.ErrorDown;
    let mut bigNumerator: LONGLONG = Int32x32To64(dMOriginal, yClipTopInteger - edgeBuffer.StartY)
        + (edgeBuffer.Error.get() + dN) as LONGLONG;
    if (bigNumerator >= 0) {
        QUOTIENT_REMAINDER_64_32!(bigNumerator, dN, xDelta, error);
    } else {
        bigNumerator = -bigNumerator;
        QUOTIENT_REMAINDER_64_32!(bigNumerator, dN, xDelta, error);

        xDelta = -xDelta;
        if (error != 0) {
            xDelta -= 1;
            error = dN - error;
        }
    }

    // Update the edge data structure with the results:

    edgeBuffer.StartY = yClipTopInteger;
    edgeBuffer.X.set(edgeBuffer.X.get() + xDelta);
    edgeBuffer.Error.set(error - dN); // Renormalize error
}

pub fn CheckValidRange28_4(x: f32, y: f32) -> bool {
    //
    // We want coordinates in the 28.4 range in the end.  The matrix we get
    // as input includes the scale by 16 to get to 28.4, so we want to
    // ensure that we are in integer range.  Assuming a sign bit and
    // five bits for the rasterizer working range, we want coordinates in the
    // -2^26 to 2^26.
    //
    // Note that the 5-bit requirement comes from the
    // implementation of InitializeEdges.
    // (See line with "error -= dN * (16 - (xStart & 15))")
    //
    // Anti-aliasing uses another c_nShift bits, so we get a
    // desired range of -2^(26-c_nShift) to 2^(26-c_nShift)
    //
    let rPixelCoordinateMax = (1 << (26 - c_nShift)) as f32;
    let rPixelCoordinateMin = -rPixelCoordinateMax;
    return x <= rPixelCoordinateMax && x >= rPixelCoordinateMin
            && y <= rPixelCoordinateMax && y >= rPixelCoordinateMin;
}

//+-----------------------------------------------------------------------------
//
//  Function:  TransformRasterizerPointsTo28_4
//
//  Synopsis:
//      Transform rasterizer points to 28.4.  If overflow occurs, return that
//      information.
//
//------------------------------------------------------------------------------
fn TransformRasterizerPointsTo28_4(
    pmat: &CMILMatrix,
    // Transform to take us to 28.4
    mut pPtsSource: &[MilPoint2F],
    // Source points
    mut cPoints: UINT,
    // Count of points
    mut pPtsDest: &mut [POINT], // Destination points
) -> HRESULT {
    let hr = S_OK;

    debug_assert!(cPoints > 0);

    while {
        //
        // Transform coordinates
        //

        let rPixelX =
            (pmat.GetM11() * pPtsSource[0].X) + (pmat.GetM21() * pPtsSource[0].Y) + pmat.GetDx();
        let rPixelY =
            (pmat.GetM12() * pPtsSource[0].X) + (pmat.GetM22() * pPtsSource[0].Y) + pmat.GetDy();

        //
        // Check for NaNs or overflow
        //

        if !CheckValidRange28_4(rPixelX, rPixelY) {
            return WGXERR_BADNUMBER;
        }

        //
        // Assign coordinates
        //

        pPtsDest[0].x = CFloatFPU::Round(rPixelX);
        pPtsDest[0].y = CFloatFPU::Round(rPixelY);

        pPtsDest = &mut pPtsDest[1..];
        pPtsSource = &pPtsSource[1..];
        cPoints -= 1;
        cPoints != 0
    } {}

    return hr;
}

pub fn AppendScaleToMatrix(pmat: &mut CMILMatrix, scaleX: REAL, scaleY: REAL) {
    pmat.SetM11(pmat.GetM11() * scaleX);
    pmat.SetM21(pmat.GetM21() * scaleX);
    pmat.SetM12(pmat.GetM12() * scaleY);
    pmat.SetM22(pmat.GetM22() * scaleY);
    pmat.SetDx(pmat.GetDx() * scaleX);
    pmat.SetDy(pmat.GetDy() * scaleY);
}

/**************************************************************************\
*
* Function Description:
*
*   Add edges to the edge list.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

pub struct CInitializeEdgesContext<'a> {
    pub MaxY: INT, // Maximum 'y' found, should be INT_MIN on
    //   first call to 'InitializeEdges'
    pub ClipRect: Option<&'a RECT>, // Bounding clip rectangle in 28.4 format
    pub Store: &'a Arena<CEdge<'a>>,  // Where to stick the edges
    pub AntiAliasMode: MilAntiAliasMode,
}

impl<'a> CInitializeEdgesContext<'a> {
    pub fn new(store: &'a Arena<CEdge<'a>>) -> Self {
        CInitializeEdgesContext { MaxY: Default::default(), ClipRect: Default::default(), Store: store, AntiAliasMode: MilAntiAliasMode::None }
    }
}

fn InitializeEdges(
    pEdgeContext: &mut CInitializeEdgesContext,
    /*__inout_ecount(vertexCount)*/
    mut pointArray: &mut [POINT], // Points to a 28.4 array of size 'vertexCount'
    //   Note that we may modify the contents!
    /*__in_range(>=, 2)*/ vertexCount: UINT,
) -> HRESULT {
    // Disable instrumentation checks for this function
    //SET_MILINSTRUMENTATION_FLAGS(MILINSTRUMENTATIONFLAGS_DONOTHING);

    let hr = S_OK;

    let mut xStart;
    let mut yStart;
    let mut yStartInteger;
    let mut yEndInteger;
    let mut dMOriginal;
    let mut dM: i32;
    let mut dN: i32;
    let mut dX: i32;
    let mut errorUp: i32;
    let mut quotient: i32;
    let mut remainder: i32;
    let mut error: i32;
    let mut windingDirection;
    //let mut edgeBuffer: *mut CEdge = NULL();
    let bufferCount: UINT = 0;
    let mut yClipTopInteger;
    let mut yClipTop;
    let mut yClipBottom;
    let mut xClipLeft;
    let mut xClipRight;

    let mut yMax = pEdgeContext.MaxY;
    let _store = &mut pEdgeContext.Store;
    let clipRect = pEdgeContext.ClipRect;

    let mut edgeCount = vertexCount - 1;
    assert!(edgeCount >= 1);

    if let Some(clipRect) = clipRect {
        yClipTopInteger = clipRect.top >> 4;
        yClipTop = clipRect.top;
        yClipBottom = clipRect.bottom;
        xClipLeft = clipRect.left;
        xClipRight = clipRect.right;

        assert!(yClipBottom > 0);
        assert!(yClipTop <= yClipBottom);
    } else {
        yClipBottom = 0;
        yClipTopInteger = INT::MIN >> c_nShift;

        // These 3 values are only used when clipRect is non-NULL
        yClipTop = 0;
        xClipLeft = 0;
        xClipRight = 0;
    }

    if (pEdgeContext.AntiAliasMode != MilAntiAliasMode::None) {
        // If antialiasing, apply the supersampling scaling here before we
        // calculate the DDAs.  We do this here and not in the Matrix
        // transform we give to FixedPointPathEnumerate mainly so that the
        // Bezier flattener can continue to operate in its optimal 28.4
        // format.
        //
        // PS#856364-2003/07/01-JasonHa  Remove pixel center fixup
        //
        // We also apply a half-pixel offset here so that the antialiasing
        // code can assume that the pixel centers are at half-pixel
        // coordinates, not on the integer coordinates.

        let mut point = &mut *pointArray;
        let mut i = vertexCount;

        while {
            point[0].x = (point[0].x + 8) << c_nShift;
            point[0].y = (point[0].y + 8) << c_nShift;
            point = &mut point[1..];
            i -= 1;
            i != 0
        } {}

        yClipTopInteger <<= c_nShift;
        yClipTop <<= c_nShift;
        yClipBottom <<= c_nShift;
        xClipLeft <<= c_nShift;
        xClipRight <<= c_nShift;
    }

    // Make 'yClipBottom' inclusive by subtracting off one pixel
    // (keeping in mind that we're in 28.4 device space):

    yClipBottom -= 16;

    // Warm up the store where we keep the edge data:

    //store.StartAddBuffer(&mut edgeBuffer, &mut bufferCount);

    'outer: loop { loop { 
        // Handle trivial rejection:

        if (yClipBottom >= 0) {
            // Throw out any edges that are above or below the clipping.
            // This has to be a precise check, because we assume later
            // on that every edge intersects in the vertical dimension
            // with the clip rectangle.  That asssumption is made in two
            // places:
            //
            // 1.  When we sort the edges, we assume either zero edges,
            //     or two or more.
            // 2.  When we start the DDAs, we assume either zero edges,
            //     or that there's at least one scan of DDAs to output.
            //
            // Plus, of course, it's less efficient if we let things
            // through.
            //
            // Note that 'yClipBottom' is inclusive:

            let clipHigh = ((pointArray[0]).y <= yClipTop) && ((pointArray[1]).y <= yClipTop);

            let clipLow = ((pointArray[0]).y > yClipBottom) && ((pointArray[1]).y > yClipBottom);

            #[cfg(debug_assertions)]
            {
                let (mut yRectTop, mut yRectBottom, y0, y1, yTop, yBottom);

                // Getting the trivial rejection code right is tricky.
                // So on checked builds let's verify that we're doing it
                // correctly, using a different approach:

                let mut clipped = false;
                if let Some(clipRect) = clipRect {
                    yRectTop = clipRect.top >> 4;
                    yRectBottom = clipRect.bottom >> 4;
                    if (pEdgeContext.AntiAliasMode != MilAntiAliasMode::None) {
                        yRectTop <<= c_nShift;
                        yRectBottom <<= c_nShift;
                    }
                    y0 = ((pointArray[0]).y + 15) >> 4;
                    y1 = ((pointArray[1]).y + 15) >> 4;
                    yTop = y0.min(y1);
                    yBottom = y0.max(y1);

                    clipped = ((yTop >= yRectBottom) || (yBottom <= yRectTop));
                }

                assert!(clipped == (clipHigh || clipLow));
            }

            if (clipHigh || clipLow) {
                break; // ======================>
            }

            if (edgeCount > 1) {
                // Here we'll collapse two edges down to one if both are
                // to the left or to the right of the clipping rectangle.

                if (((pointArray[0]).x < xClipLeft)
                    && ((pointArray[1]).x < xClipLeft)
                    && ((pointArray[2]).x < xClipLeft))
                {
                    // Note this is one reason why 'pointArray' can't be 'const':

                    pointArray[1] = pointArray[0];

                    break; // ======================>
                }

                if (((pointArray[0]).x > xClipRight)
                    && ((pointArray[1]).x > xClipRight)
                    && ((pointArray[2]).x > xClipRight))
                {
                    // Note this is one reason why 'pointArray' can't be 'const':

                    pointArray[1] = pointArray[0];

                    break; // ======================>
                }
            }
        }

        dM = (pointArray[1]).x - (pointArray[0]).x;
        dN = (pointArray[1]).y - (pointArray[0]).y;

        if (dN >= 0) {
            // The vector points downward:

            xStart = (pointArray[0]).x;
            yStart = (pointArray[0]).y;

            yStartInteger = (yStart + 15) >> 4;
            yEndInteger = ((pointArray[1]).y + 15) >> 4;

            windingDirection = 1;
        } else {
            // The vector points upward, so we have to essentially
            // 'swap' the end points:

            dN = -dN;
            dM = -dM;

            xStart = (pointArray[1]).x;
            yStart = (pointArray[1]).y;

            yStartInteger = (yStart + 15) >> 4;
            yEndInteger = ((pointArray[0]).y + 15) >> 4;

            windingDirection = -1;
        }

        // The edgeBuffer must span an integer y-value in order to be
        // added to the edgeBuffer list.  This serves to get rid of
        // horizontal edges, which cause trouble for our divides.

        if (yEndInteger > yStartInteger) {
            yMax = yMax.max(yEndInteger);

            dMOriginal = dM;
            if (dM < 0) {
                dM = -dM;
                if (dM < dN)
                // Can't be '<='
                {
                    dX = -1;
                    errorUp = dN - dM;
                } else {
                    QUOTIENT_REMAINDER!(dM, dN, quotient, remainder);

                    dX = -quotient;
                    errorUp = remainder;
                    if (remainder > 0) {
                        dX = -quotient - 1;
                        errorUp = dN - remainder;
                    }
                }
            } else {
                if (dM < dN) {
                    dX = 0;
                    errorUp = dM;
                } else {
                    QUOTIENT_REMAINDER!(dM, dN, quotient, remainder);

                    dX = quotient;
                    errorUp = remainder;
                }
            }

            error = -1; // Error is initially zero (add dN - 1 for
                        //   the ceiling, but subtract off dN so that
                        //   we can check the sign instead of comparing
                        //   to dN)

            if ((yStart & 15) != 0) {
                // Advance to the next integer y coordinate

                let mut i = 16 - (yStart & 15);
                while i != 0 {
                    xStart += dX;
                    error += errorUp;
                    if (error >= 0)
                    {
                        error -= dN;
                        xStart += 1;
                    }
                    i -= 1;
                }
            }

            if ((xStart & 15) != 0) {
                error -= dN * (16 - (xStart & 15));
                xStart += 15; // We'll want the ceiling in just a bit...
            }

            xStart >>= 4;
            error >>= 4;

            if (bufferCount == 0) {
                //IFC!(store.NextAddBuffer(&mut edgeBuffer, &mut bufferCount));
            }

            let mut edge = CEdge {
                Next: Cell::new(unsafe { Ref::null() } ),
                X: Cell::new(xStart),
                Dx: dX,
                Error: Cell::new(error),
                ErrorUp: errorUp,
                ErrorDown: dN,
                WindingDirection: windingDirection,
                StartY: yStartInteger,
                EndY: yEndInteger,// Exclusive of end
            };

            assert!(error < 0);

            // Here we handle the case where the edge starts above the
            // clipping rectangle, and we need to jump down in the 'y'
            // direction to the first unclipped scan-line.
            //
            // Consequently, we advance the DDA here:

            if (yClipTopInteger > yStartInteger) {
                assert!(edge.EndY  > yClipTopInteger);

                ClipEdge(&mut edge, yClipTopInteger, dMOriginal);
            }

            // Advance to handle the next edge:

            //edgeBuffer = unsafe { edgeBuffer.offset(1) };
            pEdgeContext.Store.alloc(edge);
            //bufferCount -= 1;
        }
        break;
    }
    pointArray = &mut pointArray[1..];
    edgeCount -= 1;
    if edgeCount == 0 {
        break 'outer;
    }
    }

    // We're done with this batch.  Let the store know how many edges
    // we ended up with:

    //store.EndAddBuffer(edgeBuffer, bufferCount);

    pEdgeContext.MaxY = yMax;

    return hr;
}

/**************************************************************************\
*
* Function Description:
*
*   Does complete parameter checking on the 'types' array of a path.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/
fn ValidatePathTypes(typesArray: &[BYTE], mut count: INT) -> bool {
    let mut types = typesArray;

    if (count == 0) {
        return (true);
    }

    loop {
        // The first point in every subpath has to be an unadorned
        // 'start' point:

        if ((types[0] & PathPointTypePathTypeMask) != PathPointTypeStart) {
            TraceTag!((tagMILWarning, "Bad subpath start"));
            return (false);
        }

        // Advance to the first point after the 'start' point:
        count -= 1;
        if (count == 0) {
            TraceTag!((tagMILWarning, "Path ended after start-path"));
            return (false);
        }

        if ((types[1] & PathPointTypePathTypeMask) == PathPointTypeStart) {
            TraceTag!((tagMILWarning, "Can't have a start followed by a start!"));
            return (false);
        }

        // Process runs of lines and Bezier curves:

        loop {
            match (types[1] & PathPointTypePathTypeMask) {
                PathPointTypeLine => {
                    types = &types[1..];
                    count -= 1;
                    if (count == 0) {
                        return (true);
                    }
                }

                PathPointTypeBezier => {
                    if (count < 3) {
                        TraceTag!((
                            tagMILWarning,
                            "Path ended before multiple of 3 Bezier points"
                        ));
                        return (false);
                    }

                    if ((types[1] & PathPointTypePathTypeMask) != PathPointTypeBezier) {
                        TraceTag!((tagMILWarning, "Bad subpath start"));
                        return (false);
                    }

                    types = &types[1..];
                    count -= 3;
                    if (count == 0) {
                        return (true);
                    }
                }

                _ => {
                    TraceTag!((tagMILWarning, "Illegal type"));
                    return (false);
                }
            }

            // A close-subpath marker or a start-subpath marker marks the
            // end of a subpath:
            if !(!((types[0] & PathPointTypeCloseSubpath) != 0)
                && ((types[1] & PathPointTypePathTypeMask) != PathPointTypeStart)) {
                    types = &types[1..];
                    break;
                }
        }
    }
}

/**************************************************************************\
*
* Function Description:
*
*   Some debug code for verifying the path.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/
macro_rules! ASSERTPATH {
    ($types: expr, $points: expr) => {
        #[cfg(debug_assertions)]
        AssertPath($types, $points)
    };
}
fn AssertPath(rgTypes: &[BYTE], cPoints: UINT) {
    // Make sure that the 'types' array is well-formed, otherwise we
    // may fall over in the FixedPointPathEnumerate function.
    //
    // NOTE: If you hit this assert, DO NOT SIMPLY COMMENT THIS Assert OUT!
    //
    //       Instead, fix the ValidatePathTypes code if it's letting through
    //       valid paths, or (more likely) fix the code that's letting bogus
    //       paths through.  The FixedPointPathEnumerate routine has some
    //       subtle assumptions that require the path to be perfectly valid!
    //
    //       No internal code should be producing invalid paths, and all
    //       paths created by the application must be parameter checked!
    assert!(ValidatePathTypes(rgTypes, cPoints as INT));
}

//+----------------------------------------------------------------------------
//
//  Member:
//      FixedPointPathEnumerate
//
//  Synopsis:
//
//      Enumerate the path.
//
//      NOTE: The 'enumerateFunction' function is allowed to modify the
//            contents of our call-back buffer!  (This is mainly done to allow
//            'InitializeEdges' to be simpler for some clipping trivial
//            rejection cases.)
//
//      NOTICE-2006/03/22-milesc  This function was initially built to be a
//      general path enumeration function. However, we were only using it for
//      one specific purpose... for Initializing edges of a path to be filled.
//      In doing security work, I simplified this function to just do edge
//      initialization. The name is therefore now overly general. I have kept
//      the name to be a reminder that this function has been written to be
//      more general than would otherwise be evident.
//

pub fn FixedPointPathEnumerate(
    rgpt: &[POINT],
    rgTypes: &[BYTE],
    cPoints: UINT,
    _matrix: &CMILMatrix,
    clipRect: Option<&RECT>, // In scaled 28.4 format
    enumerateContext: &mut CInitializeEdgesContext,
) -> HRESULT {
    let hr = S_OK;
    let mut bufferStart: [POINT; ENUMERATE_BUFFER_NUMBER!()] = [(); ENUMERATE_BUFFER_NUMBER!()].map(|_| Default::default());
    let mut bezierBuffer: [POINT; 4] = Default::default();
    let mut buffer: &mut [POINT];
    let mut bufferSize: usize;
    let mut startFigure: [POINT; 1] = Default::default();
    // The current point offset in rgpt
    let mut iPoint: usize;
    // The current type offset in rgTypes
    let mut iType: usize;
    let mut runSize: usize;
    let mut thisCount: usize;
    let mut isMore: bool = false;
    let mut xLast: INT;
    let mut yLast: INT;

    ASSERTPATH!(rgTypes, cPoints);

    // Every valid subpath has at least two vertices in it, hence the
    // check of 'cPoints - 1':

    iPoint = 0;
    iType = 0;

    assert!(cPoints > 1);
    while (iPoint < cPoints as usize - 1) {
        assert!((rgTypes[iType] & PathPointTypePathTypeMask) == PathPointTypeStart);
        assert!((rgTypes[iType + 1] & PathPointTypePathTypeMask) != PathPointTypeStart);

        // Add the start point to the beginning of the batch, and
        // remember it for handling the close figure:

        startFigure[0] = rgpt[iPoint];

        bufferStart[0].x = startFigure[0].x;
        bufferStart[0].y = startFigure[0].y;
        let bufferStartPtr = bufferStart.as_ptr();
        buffer = &mut bufferStart[1..];
        bufferSize = ENUMERATE_BUFFER_NUMBER!() - 1;

        // We need to enter our loop with 'iType' pointing one past
        // the start figure:

        iPoint += 1;
        iType += 1;

        while {
            // Try finding a run of lines:

            if ((rgTypes[iType] & PathPointTypePathTypeMask) == PathPointTypeLine) {
                runSize = 1;

                while ((iPoint + runSize < cPoints as usize)
                    && ((rgTypes[iType + runSize] & PathPointTypePathTypeMask) == PathPointTypeLine))
                {
                    runSize += 1;
                }

                // Okay, we've found a run of lines.  Break it up into our
                // buffer size:

                loop {
                    thisCount = bufferSize.min(runSize);

                    buffer[0 .. thisCount].copy_from_slice(&rgpt[iPoint .. iPoint + thisCount]);

                    __analysis_assume!(
                        buffer + bufferSize == bufferStart + ENUMERATE_BUFFER_NUMBER
                    );
                    assert!(buffer.as_ptr().wrapping_offset(bufferSize as isize) == bufferStartPtr.wrapping_offset(ENUMERATE_BUFFER_NUMBER!()) );

                    iPoint += thisCount;
                    iType += thisCount;
                    buffer = &mut buffer[thisCount..];
                    runSize -= thisCount;
                    bufferSize -= thisCount;

                    if (bufferSize > 0) {
                        break;
                    }

                    xLast = bufferStart[ENUMERATE_BUFFER_NUMBER!() - 1].x;
                    yLast = bufferStart[ENUMERATE_BUFFER_NUMBER!() - 1].y;
                    IFR!(InitializeEdges(
                        enumerateContext,
                        &mut bufferStart,
                        ENUMERATE_BUFFER_NUMBER!()
                    ));

                    // Continue the last vertex as the first in the new batch:

                    bufferStart[0].x = xLast;
                    bufferStart[0].y = yLast;
                    buffer = &mut bufferStart[1..];
                    bufferSize = ENUMERATE_BUFFER_NUMBER!() - 1;
                    if !(runSize != 0) {
                        break;
                    }
                }
            } else {
                assert!(iPoint + 3 <= cPoints as usize);
                assert!((rgTypes[iType] & PathPointTypePathTypeMask) == PathPointTypeBezier);

                bezierBuffer.copy_from_slice(&rgpt[(iPoint - 1) .. iPoint + 3]);

                // Prepare for the next iteration:

                iPoint += 3;
                iType += 1;

                // Process the Bezier:

                let mut bezier = CMILBezier::new(&bezierBuffer, clipRect);
                loop {
                    thisCount = bezier.Flatten(buffer, &mut isMore) as usize;

                    __analysis_assume!(
                        buffer + bufferSize == bufferStart + ENUMERATE_BUFFER_NUMBER!()
                    );
                    assert!(buffer.as_ptr().wrapping_offset(bufferSize as isize) == bufferStartPtr.wrapping_offset(ENUMERATE_BUFFER_NUMBER!()));

                    buffer = &mut buffer[thisCount..];
                    bufferSize -= thisCount;

                    if (bufferSize > 0) {
                        break;
                    }

                    xLast = bufferStart[ENUMERATE_BUFFER_NUMBER!() - 1].x;
                    yLast = bufferStart[ENUMERATE_BUFFER_NUMBER!() - 1].y;
                    IFR!(InitializeEdges(
                        enumerateContext,
                        &mut bufferStart,
                        ENUMERATE_BUFFER_NUMBER!()
                    ));

                    // Continue the last vertex as the first in the new batch:

                    bufferStart[0].x = xLast;
                    bufferStart[0].y = yLast;
                    buffer = &mut bufferStart[1..];
                    bufferSize = ENUMERATE_BUFFER_NUMBER!() - 1;
                    if !isMore {
                        break;
                    }
                }
            }

            ((iPoint < cPoints as usize)
                && ((rgTypes[iType] & PathPointTypePathTypeMask) != PathPointTypeStart))
        } {}

        // Okay, the subpath is done.  But we still have to handle the
        // 'close figure' (which is implicit for a fill):
        // Add the close-figure point:

        buffer[0].x = startFigure[0].x;
        buffer[0].y = startFigure[0].y;
        bufferSize -= 1;

        // We have to flush anything we might have in the batch, unless
        // there's only one vertex in there!  (The latter case may happen
        // for the stroke case with no close figure if we just flushed a
        // batch.)
        // If we're flattening, we must call the one additional time to
        // correctly handle closing the subpath, even if there is only
        // one entry in the batch. The flattening callback handles the
        // one point case and closes the subpath properly without adding
        // extraneous points.

        let verticesInBatch = ENUMERATE_BUFFER_NUMBER!() - bufferSize;
        if (verticesInBatch > 1) {
            IFR!(InitializeEdges(
                enumerateContext,
                &mut bufferStart,
                (verticesInBatch) as UINT
            ));
        }
    }

    return hr;
}

/**************************************************************************\
*
* Function Description:
*
*   We want to sort in the inactive list; the primary key is 'y', and
*   the secondary key is 'x'.  This routine creates a single LONGLONG
*   key that represents both.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

fn YX(x: INT, y: INT, p: &mut LONGLONG) {
    // Bias 'x' by INT_MAX so that it's effectively unsigned:
    /*
    reinterpret_cast<LARGE_INTEGER*>(p)->HighPart = y;
    reinterpret_cast<LARGE_INTEGER*>(p)->LowPart = x + INT_MAX;
    */
    *p = (((y as u64) << 32) | (((x as i64 + i32::MAX as i64) as u64) & 0xffffffff)) as i64;
}

/**************************************************************************\
*
* Function Description:
*
*   Recursive function to quick-sort our inactive edge list.  Note that
*   for performance, the results are not completely sorted; an insertion
*   sort has to be run after the quicksort in order to do a lighter-weight
*   sort of the subtables.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

const QUICKSORT_THRESHOLD: isize = 8;

fn QuickSortEdges(inactive: &mut [CInactiveEdge],
    /*__inout_xcount(f - l + 1 elements)*/ f: usize,
    /*__inout_xcount(array starts at f)*/ l: usize,
) {
    let mut e: Ref<CEdge>;
    let mut y: LONGLONG;
    let mut first: LONGLONG;
    let mut second: LONGLONG;
    let mut last: LONGLONG;

    // Find the median of the first, middle, and last elements:

    let m = f + ((l - f) >> 1);

    SWAP!(y, inactive[f + 1].Yx, inactive[m].Yx);
    SWAP!(e, inactive[f + 1].Edge, inactive[m].Edge);

    if {second = inactive[f + 1].Yx; second > {last = inactive[l].Yx; last}} {
        inactive[f + 1].Yx = last;
        inactive[l].Yx = second;

        SWAP!(e, inactive[f + 1].Edge, inactive[l].Edge);
    }
    if {first = inactive[f].Yx; first} > {last = inactive[l].Yx; last} {
        inactive[f].Yx = last;
        inactive[l].Yx = first;

        SWAP!(e, inactive[f].Edge, inactive[l].Edge);
    }
    if {second = inactive[f + 1].Yx; second} > {first = inactive[f].Yx; first} {
        inactive[f + 1].Yx = first;
        inactive[f].Yx = second;

        SWAP!(e, inactive[f + 1].Edge, inactive[f].Edge);
    }

    // f->Yx is now the desired median, and (f + 1)->Yx <= f->Yx <= l->Yx

    debug_assert!((inactive[f + 1].Yx <= inactive[f].Yx) && (inactive[f].Yx <= inactive[l].Yx));

    let median = inactive[f].Yx;

    let mut i = f + 2;
    while (inactive[i].Yx < median) {
        i += 1;
    }

    let mut j = l - 1;
    while (inactive[j].Yx > median) {
        j -= 1;
    }

    while (i < j) {
        SWAP!(y, inactive[i].Yx, inactive[j].Yx);
        SWAP!(e, inactive[i].Edge, inactive[j].Edge);

        while {
            i = i + 1;
            inactive[i].Yx < median
        } {}

        while {
            j = j - 1 ;
            inactive[j].Yx > median
        } {}
    }

    SWAP!(y, inactive[f].Yx, inactive[j].Yx);
    SWAP!(e, inactive[f].Edge, inactive[j].Edge);

    let a = j - f;
    let b = l - j;

    // Use less stack space by recursing on the shorter subtable.  Also,
    // have the less-overhead insertion-sort handle small subtables.

    if (a <= b) {
        if (a > QUICKSORT_THRESHOLD as usize) {
            // 'a' is the smallest, so do it first:

            QuickSortEdges(inactive, f, j - 1);
            QuickSortEdges(inactive, j + 1, l);
        } else if (b > QUICKSORT_THRESHOLD as usize) {
            QuickSortEdges(inactive, j + 1, l);
        }
    } else {
        if (b > QUICKSORT_THRESHOLD as usize) {
            // 'b' is the smallest, so do it first:

            QuickSortEdges(inactive, j + 1 , l);
            QuickSortEdges(inactive, f, j + 1);
        } else if (a > QUICKSORT_THRESHOLD as usize) {
            QuickSortEdges(inactive, f, j -1);
        }
    }
}

/**************************************************************************\
*
* Function Description:
*
*   Do a sort of the inactive table using an insertion-sort.  Expects
*   large tables to have already been sorted via quick-sort.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

fn InsertionSortEdges(
    /* __inout_xcount(count forward & -1 back)*/ mut inactive: &mut [CInactiveEdge],
    mut count: INT,
) {
    let mut e: Ref<CEdge>;
    let mut y: LONGLONG;
    let mut yPrevious: LONGLONG;

    assert!(inactive[0].Yx == i64::MIN);
    assert!(count >= 2);
    //inactive = &mut inactive[1..];

    let mut indx = 2; // Skip first entry (by definition it's already in order!)
    count -= 1;

    while {
        let mut p = indx;

        // Copy the current stuff to temporary variables to make a hole:

        e = (inactive[indx]).Edge;
        y = (inactive[indx]).Yx;

        // Shift everything one slot to the right (effectively moving
        // the hole one position to the left):

        while (y < {yPrevious = inactive[p-1].Yx; yPrevious}) {
            inactive[p].Yx = yPrevious;
            inactive[p].Edge = inactive[p-1].Edge;
            p -= 1;
        }

        // Drop the temporary stuff into the final hole:

        inactive[p].Yx = y;
        inactive[p].Edge = e;

        // The quicksort should have ensured that we don't have to move
        // any entry terribly far:

        assert!((indx - p) <= QUICKSORT_THRESHOLD as usize);

        indx += 1;
        count -= 1;
        count != 0
    } {}
}

/**************************************************************************\
*
* Function Description:
*
*   Assert the state of the inactive array.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/
macro_rules! ASSERTINACTIVEARRAY {
    ($inactive: expr, $count: expr) => {
        #[cfg(debug_assertions)]
        AssertInactiveArray($inactive, $count);
    };
}
fn AssertInactiveArray(
    /*__in_ecount(count)*/
    mut inactive: &[CInactiveEdge], // Annotation should allow the -1 element
    mut count: INT,
) {
    // Verify the head:

    /*#if !ANALYSIS*/
    // #if needed because prefast don't know that the -1 element is avaliable
    assert!(inactive[0].Yx == i64::MIN);
    /*#endif*/
    assert!(inactive[1].Yx != i64::MIN);

    while {
        let mut yx: LONGLONG = 0;
        YX((*inactive[1].Edge).X.get(), (*inactive[1].Edge).StartY, &mut yx);

        assert!(inactive[1].Yx == yx);
        /*#if !ANALYSIS*/
        // #if needed because tools don't know that the -1 element is avaliable
        assert!(inactive[1].Yx >= inactive[0].Yx);
        /*#endif*/
        inactive = &inactive[1..];
        count -= 1;
        count != 0
    } {}

    // Verify that the tail is setup appropriately:

    assert!((*inactive[1].Edge).StartY == INT::MAX);
}

/**************************************************************************\
*
* Function Description:
*
*   Initialize and sort the inactive array.
*
* Returns:
*
*   'y' value of topmost edge.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

pub fn InitializeInactiveArray<'a>(
    pEdgeStore: &'a Arena<CEdge<'a>>,
    /*__in_ecount(count+2)*/ rgInactiveArray: &mut [CInactiveEdge<'a>],
    count: UINT,
    tailEdge: Ref<'a, CEdge<'a>> // Tail sentinel for inactive list
) -> INT {
    let rgInactiveArrayPtr = rgInactiveArray.as_mut_ptr();

    // First initialize the inactive array.  Skip the first entry,
    // which we reserve as a head sentinel for the insertion sort:

    let mut pInactiveEdge = &mut rgInactiveArray[1..];

    for e in pEdgeStore.iter() {

            pInactiveEdge[0].Edge = Ref::new(e);
            YX(e.X.get(), e.StartY, &mut pInactiveEdge[0].Yx);
            pInactiveEdge = &mut pInactiveEdge[1..];
    }

    assert!(unsafe { pInactiveEdge.as_mut_ptr().offset_from(rgInactiveArrayPtr) } as UINT == count + 1);

    // Add the tail, which is used when reading back the array.  This
    // is why we had to allocate the array as 'count + 1':

    pInactiveEdge[0].Edge = tailEdge;

    // Add the head, which is used for the insertion sort.  This is why
    // we had to allocate the array as 'count + 2':

    rgInactiveArray[0].Yx = i64::MIN;

    // Only invoke the quicksort routine if it's worth the overhead:

    if (count as isize > QUICKSORT_THRESHOLD) {
        // Quick-sort this, skipping the first and last elements,
        // which are sentinels.
        //
        // We do 'inactiveArray + count' to be inclusive of the last
        // element:

        QuickSortEdges(rgInactiveArray, 1, count as usize);
    }

    // Do a quick sort to handle the mostly sorted result:

    InsertionSortEdges(rgInactiveArray, count as i32);

    ASSERTINACTIVEARRAY!(rgInactiveArray, count as i32);

    // Return the 'y' value of the topmost edge:

    return (*rgInactiveArray[1].Edge).StartY;

}

/**************************************************************************\
*
* Function Description:
*
*   Insert edges into the active edge list.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

pub fn InsertNewEdges<'a>(
    mut pActiveList: Ref<'a, CEdge<'a>>,
    iCurrentY: INT,
    /*__deref_inout_xcount(array terminated by an edge with StartY != iCurrentY)*/
    ppInactiveEdge: &'a mut [CInactiveEdge<'a>],
    pYNextInactive: &mut INT, // will be INT_MAX when no more
) -> &'a mut [CInactiveEdge<'a>] {

    let mut inactive: &mut [CInactiveEdge] = ppInactiveEdge;

    assert!((*inactive[0].Edge).StartY == iCurrentY);

    while {
        let newActive: Ref<CEdge> = inactive[0].Edge;

        // The activeList edge list sentinel has X = INT_MAX, so this always
        // terminates:

        while ((*(*pActiveList).Next.get()).X < (*newActive).X) {
            pActiveList = (*pActiveList).Next.get();
        }

        if SORT_EDGES_INCLUDING_SLOPE {
            // The activeList edge list sentinel has Dx = INT_MAX, so this always
            // terminates:

            while (((*(*pActiveList).Next.get()).X == (*newActive).X) && ((*(*pActiveList).Next.get()).Dx < (*newActive).Dx)) {
                pActiveList = (*pActiveList).Next.get();
            }
        }

        (*newActive).Next.set((*pActiveList).Next.get());
        (*pActiveList).Next.set(newActive);

        inactive = &mut inactive[1..];
        (*(inactive[0]).Edge).StartY == iCurrentY
    } {}

    *pYNextInactive = (*(inactive[0]).Edge).StartY;
    return inactive;

}

/**************************************************************************\
*
* Function Description:
*
*   Sort the edges so that they're in ascending 'x' order.
*
*   We use a bubble-sort for this stage, because edges maintain good
*   locality and don't often switch ordering positions.
*
* Created:
*
*   03/25/2000 andrewgo
*
\**************************************************************************/

fn SortActiveEdges(list: Ref<CEdge>) {

    let mut swapOccurred: bool;
    let mut tmp: Ref<CEdge>;

    // We should never be called with an empty active edge list:

    assert!((*(*list).Next.get()).X.get() != INT::MAX);

    while {
        swapOccurred = false;

        let mut previous = list;
        let mut current = (*list).Next.get();
        let mut next = (*current).Next.get();
        let mut nextX = (*next).X.get();

        while {
            if (nextX < (*current).X.get()) {
                swapOccurred = true;

                (*previous).Next.set(next);
                (*current).Next.set((*next).Next.get());
                (*next).Next.set(current);

                SWAP!(tmp, next, current);
            }

            previous = current;
            current = next;
            next = (*next).Next.get();
            nextX = (*next).X.get();
            nextX != INT::MAX
        } {}
        swapOccurred
    } {}

}
