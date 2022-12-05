// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.


//------------------------------------------------------------------------------
//

use std::cell::Cell;

use typed_arena_nomut::Arena;

//
//  Description:
//      Coverage buffer implementation
#[cfg(debug_assertions)]
use crate::aarasterizer::AssertActiveList;
use crate::aarasterizer::CEdge;
use crate::nullable_ref::Ref;
use crate::types::*;
//struct CEdge;
//struct CInactiveEdge;

//-------------------------------------------------------------------------
//
// TrapezoidalAA only supports 8x8 mode, so the shifts/masks are all
// constants.  Also, since we must be symmetrical, x and y shifts are
// merged into one shift unlike the implementation in aarasterizer.
//
//-------------------------------------------------------------------------

pub const c_nShift: INT = 3; 
pub const c_nShiftSize: INT = 8; 
pub const c_nShiftSizeSquared: INT = c_nShiftSize * c_nShiftSize; 
pub const c_nHalfShiftSize: INT = 4; 
pub const c_nShiftMask: INT = 7; 
//pub const c_rShiftSize: f32 = 8.0;
//pub const c_rHalfShiftSize: f32 = 4.0;
pub const c_rInvShiftSize: f32 = 1.0/8.0;
pub const c_antiAliasMode: MilAntiAliasMode = MilAntiAliasMode::EightByEight;

//
// Interval coverage descriptor for our antialiased filler
//

pub struct CCoverageInterval<'a>
{
    pub m_pNext: Cell<Ref<'a, CCoverageInterval<'a>>>, // m_pNext interval (look for sentinel, not NULL)
    pub m_nPixelX: Cell<INT>,              // Interval's left edge (m_pNext->X is the right edge)
    pub m_nCoverage: Cell<INT>,            // Pixel coverage for interval
}

impl<'a> Default for CCoverageInterval<'a> {
    fn default() -> Self {
        Self { m_pNext: Cell::new(unsafe { Ref::null() } ), m_nPixelX: Default::default(), m_nCoverage: Default::default() }
    }
}

// Define our on-stack storage use.  The 'free' versions are nicely tuned
// to avoid allocations in most common scenarios, while at the same time
// not chewing up toooo much stack space.  
//
// We make the debug versions small so that we hit the 'grow' cases more
// frequently, for better testing:

#[cfg(debug_assertions)]
    // Must be at least 6 now: 4 for the "minus4" logic in hwrasterizer.*, and then 
    // 1 each for the head and tail sentinels (since their allocation doesn't use Grow).
    const INTERVAL_BUFFER_NUMBER: usize = 8;        
#[cfg(not(debug_assertions))]
    const INTERVAL_BUFFER_NUMBER: usize = 32;


//
// Allocator structure for the antialiased fill interval data
//

struct CCoverageIntervalBuffer<'a>
{
    m_pNext: Cell<Option<& 'a CCoverageIntervalBuffer<'a>>>,
    m_interval: [CCoverageInterval<'a>; INTERVAL_BUFFER_NUMBER],
}

impl<'a>  Default for CCoverageIntervalBuffer<'a> {
    fn default() -> Self {
        Self { m_pNext: Cell::new(None), m_interval: Default::default() }
    }
}

//------------------------------------------------------------------------------
//
//  Class: CCoverageBuffer
//
//  Description:
//      Coverage buffer implementation that maintains coverage information
//      for one scanline.  
//
//      This implementation will maintain a linked list of intervals consisting
//      of x value in pixel space and a coverage value that applies for all pixels
//      between pInterval->X and pInterval->Next->X.
//
//      For example, if we add the following interval (assuming 8x8 anti-aliasing)
//      to the coverage buffer:
//       _____ _____ _____ _____
//      |     |     |     |     |
//      |  -------------------  |
//      |_____|_____|_____|_____|
//    (0,0) (1,0) (2,0) (3,0) (4,0)
//
//      Then we will get the following coverage buffer:
//
//     m_nPixelX: INT_MIN  |  0  |  1  |  3  |  4  | INT_MAX
//   m_nCoverage: 0        |  4  |  8  |  4  |  0  | 0xdeadbeef
//       m_pNext: -------->|---->|---->|---->|---->| NULL
//              
//------------------------------------------------------------------------------
pub struct CCoverageBuffer<'a>
{
    /*
public:
    //
    // Init/Destroy methods
    //

    VOID Initialize();
    VOID Destroy();

    //
    // Setup the buffer so that it can accept another scanline
    //

    VOID Reset();

    //
    // Add a subpixel interval to the coverage buffer
    //

    HRESULT FillEdgesAlternating(
        __in_ecount(1) const CEdge *pEdgeActiveList,
        INT nSubpixelYCurrent
        );

    HRESULT FillEdgesWinding(
        __in_ecount(1) const CEdge *pEdgeActiveList,
        INT nSubpixelYCurrent
        );

    HRESULT AddInterval(INT nSubpixelXLeft, INT nSubpixelXRight);

private:

    HRESULT Grow(
        __deref_out_ecount(1) CCoverageInterval **ppIntervalNew, 
        __deref_out_ecount(1) CCoverageInterval **ppIntervalEndMinus4
        );

public:*/
    pub m_pIntervalStart: Cell<Ref<'a, CCoverageInterval<'a>>>,           // Points to list head entry

//private:
    m_pIntervalNew: Cell<Ref<'a, CCoverageInterval<'a>>>,
    interval_new_index: Cell<usize>,

    // The Minus4 in the below variable refers to the position at which
    // we need to Grow the buffer.  The buffer is grown once before an
    // AddInterval, so the Grow has to ensure that there are enough 
    // intervals for the AddInterval worst case which is the following:
    //
    //  1     2           3     4
    //  *_____*_____ _____*_____* 
    //  |     |     |     |     |
    //  |  ---|-----------|---  |
    //  |_____|_____|_____|_____|
    //
    // Note that the *'s above mark potentional insert points in the list,
    // so we need to ensure that at least 4 intervals can be allocated.
    //

    m_pIntervalEndMinus4:  Cell<Ref<'a, CCoverageInterval<'a>>>,

    // Cache the next-to-last added interval to accelerate insertion.
    m_pIntervalLast: Cell<Ref<'a, CCoverageInterval<'a>>>,

    m_pIntervalBufferBuiltin: CCoverageIntervalBuffer<'a>,
    m_pIntervalBufferCurrent: Cell<Ref<'a, CCoverageIntervalBuffer<'a>>>,

    arena: Arena<CCoverageIntervalBuffer<'a>>
       
    // Disable instrumentation checks within all methods of this class
    //SET_MILINSTRUMENTATION_FLAGS(MILINSTRUMENTATIONFLAGS_DONOTHING);
}

impl<'a> Default for CCoverageBuffer<'a> {
    fn default() -> Self {
        Self {
            m_pIntervalStart: Cell::new(unsafe { Ref::null() }),
            m_pIntervalNew: Cell::new(unsafe { Ref::null() }),
            m_pIntervalEndMinus4: Cell::new(unsafe { Ref::null() }),
            m_pIntervalLast: Cell::new(unsafe { Ref::null() }),
            m_pIntervalBufferBuiltin: Default::default(),
            m_pIntervalBufferCurrent: unsafe { Cell::new(Ref::null()) },
            arena: Arena::new(),
            interval_new_index: Cell::new(0),
        }
    }
}


//
// Inlines
//
impl<'a> CCoverageBuffer<'a> {
//-------------------------------------------------------------------------
//
//  Function:   CCoverageBuffer::AddInterval
//
//  Synopsis:   Add a subpixel resolution interval to the coverage buffer
// 
//-------------------------------------------------------------------------
pub fn AddInterval(&'a self, nSubpixelXLeft: INT, nSubpixelXRight: INT) -> HRESULT
{
    let hr: HRESULT = S_OK;
    let mut nPixelXNext: INT;
    let nPixelXLeft: INT;
    let nPixelXRight: INT;
    let nCoverageLeft: INT;  // coverage from right edge of pixel for interval start
    let nCoverageRight: INT; // coverage from left edge of pixel for interval end

    let mut pInterval = self.m_pIntervalStart.get();
    let mut pIntervalNew = self.m_pIntervalNew.get();
    let mut interval_new_index = self.interval_new_index.get();
    let mut pIntervalEndMinus4 = self.m_pIntervalEndMinus4.get();

    // Make sure we have enough room to add two intervals if
    // necessary:

    if (pIntervalNew >= pIntervalEndMinus4)
    {
        IFC!(self.Grow(&mut pIntervalNew, &mut pIntervalEndMinus4, &mut interval_new_index));
    }

    // Convert interval to pixel space so that we can insert it 
    // into the coverage buffer

    debug_assert!(nSubpixelXLeft < nSubpixelXRight);
    nPixelXLeft = nSubpixelXLeft >> c_nShift;
    nPixelXRight = nSubpixelXRight >> c_nShift; 

    // Try to resume searching from the last searched interval.
    if self.m_pIntervalLast.get().m_nPixelX.get() < nPixelXLeft {
        pInterval = self.m_pIntervalLast.get();
    }

    // Skip any intervals less than 'nPixelLeft':

    loop {
        let nextInterval = pInterval.m_pNext.get();
        nPixelXNext = nextInterval.m_nPixelX.get();
        if !(nPixelXNext < nPixelXLeft) { break }

        pInterval = nextInterval;
    }

    // Remember the found interval.
    self.m_pIntervalLast.set(pInterval);

    // Insert a new interval if necessary:

    if (nPixelXNext != nPixelXLeft)
    {
        pIntervalNew.m_nPixelX.set(nPixelXLeft);
        pIntervalNew.m_nCoverage.set(pInterval.m_nCoverage.get());

        pIntervalNew.m_pNext.set(pInterval.m_pNext.get());
        pInterval.m_pNext.set(pIntervalNew);

        pInterval = pIntervalNew;

        interval_new_index += 1;
        pIntervalNew = Ref::new(&Ref::get_ref(self.m_pIntervalBufferCurrent.get()).m_interval[interval_new_index])

    }
    else
    {
        pInterval = (*pInterval).m_pNext.get();
    }

    //
    // Compute coverage for left segment as shown by the *'s below
    //
    //  |_____|_____|_____|_
    //  |     |     |     |
    //  |  ***----------  |
    //  |_____|_____|_____|
    //

    nCoverageLeft = c_nShiftSize - (nSubpixelXLeft & c_nShiftMask);

    // If nCoverageLeft == 0, then the value of nPixelXLeft is wrong
    // and should have been equal to nPixelXLeft+1.
    debug_assert!(nCoverageLeft > 0);

    // If we have partial coverage, then ensure that we have a position
    // for the end of the pixel 

    if ((nCoverageLeft < c_nShiftSize || (nPixelXLeft == nPixelXRight))
        && nPixelXLeft + 1 != pInterval.m_pNext.get().m_nPixelX.get())
    {
        pIntervalNew.m_nPixelX.set(nPixelXLeft + 1);
        pIntervalNew.m_nCoverage.set(pInterval.m_nCoverage.get());

        pIntervalNew.m_pNext.set(pInterval.m_pNext.get());
        pInterval.m_pNext.set(pIntervalNew);

        interval_new_index += 1;
        pIntervalNew = Ref::new(&Ref::get_ref(self.m_pIntervalBufferCurrent.get()).m_interval[interval_new_index])
    }
    
    //
    // If the interval only includes one pixel, then the coverage is
    // nSubpixelXRight - nSubpixelXLeft
    //

    if (nPixelXLeft == nPixelXRight)
    {
        pInterval.m_nCoverage.set(pInterval.m_nCoverage.get() + nSubpixelXRight - nSubpixelXLeft);
        debug_assert!(pInterval.m_nCoverage.get() <= c_nShiftSize*c_nShiftSize);
        //goto Cleanup;

        //Cleanup:
        // Update the coverage buffer new interval
        self.interval_new_index.set(interval_new_index);
        self.m_pIntervalNew.set(pIntervalNew);
        return hr;
    }

    // Update coverage of current interval
    pInterval.m_nCoverage.set(pInterval.m_nCoverage.get() + nCoverageLeft);
    debug_assert!(pInterval.m_nCoverage.get() <= c_nShiftSize*c_nShiftSize);

    // Increase the coverage for any intervals between 'nPixelXLeft'
    // and 'nPixelXRight':

    loop {
        let nextInterval = pInterval.m_pNext.get();
        (nPixelXNext = nextInterval.m_nPixelX.get());
    
        if !(nPixelXNext < nPixelXRight) {
            break;
        }
        pInterval = nextInterval;
        pInterval.m_nCoverage.set(pInterval.m_nCoverage.get() + c_nShiftSize);
        debug_assert!(pInterval.m_nCoverage.get() <= c_nShiftSize*c_nShiftSize);
    }

    // Remember the found interval.
    self.m_pIntervalLast.set(pInterval);

    // Insert another new interval if necessary:

    if (nPixelXNext != nPixelXRight)
    {
        pIntervalNew.m_nPixelX.set(nPixelXRight);
        pIntervalNew.m_nCoverage.set(pInterval.m_nCoverage.get() - c_nShiftSize);

        pIntervalNew.m_pNext.set(pInterval.m_pNext.get());
        pInterval.m_pNext.set(pIntervalNew);

        pInterval = pIntervalNew;

        interval_new_index += 1;
        pIntervalNew = Ref::new(&Ref::get_ref(self.m_pIntervalBufferCurrent.get()).m_interval[interval_new_index])
    }
    else
    {
        pInterval = pInterval.m_pNext.get();
    }

    //
    // Compute coverage for right segment as shown by the *'s below
    //
    //  |_____|_____|_____|_
    //  |     |     |     |
    //  |  ---------****  |
    //  |_____|_____|_____|
    //

    nCoverageRight = nSubpixelXRight & c_nShiftMask;
    if (nCoverageRight > 0)
    {
        if (nPixelXRight + 1 != (*(*pInterval).m_pNext.get()).m_nPixelX.get())
        {
            pIntervalNew.m_nPixelX.set(nPixelXRight + 1);
            pIntervalNew.m_nCoverage.set(pInterval.m_nCoverage.get());

            pIntervalNew.m_pNext.set(pInterval.m_pNext.get());
            pInterval.m_pNext.set(pIntervalNew);

            interval_new_index += 1;
            pIntervalNew = Ref::new(&Ref::get_ref(self.m_pIntervalBufferCurrent.get()).m_interval[interval_new_index])
        }

        pInterval.m_nCoverage.set((*pInterval).m_nCoverage.get() + nCoverageRight);
        debug_assert!(pInterval.m_nCoverage.get() <= c_nShiftSize*c_nShiftSize);
    }

//Cleanup:
    // Update the coverage buffer new interval
    self.interval_new_index.set(interval_new_index);
    self.m_pIntervalNew.set(pIntervalNew);

    return hr;
}


//-------------------------------------------------------------------------
//
//  Function:   CCoverageBuffer::FillEdgesAlternating
//
//  Synopsis:   
//      Given the active edge list for the current scan, do an alternate-mode
//      antialiased fill.
//
//-------------------------------------------------------------------------
pub fn FillEdgesAlternating(&'a self,
    pEdgeActiveList: Ref<CEdge>,
    nSubpixelYCurrent: INT
    ) -> HRESULT
{

    let hr: HRESULT = S_OK;
    let mut pEdgeStart: Ref<CEdge> = (*pEdgeActiveList).Next.get();
    let mut pEdgeEnd: Ref<CEdge>;
    let mut nSubpixelXLeft: INT;
    let mut nSubpixelXRight: INT;

    ASSERTACTIVELIST!(pEdgeActiveList, nSubpixelYCurrent);

    while (pEdgeStart.X.get() != INT::MAX)
    {
        pEdgeEnd = pEdgeStart.Next.get();

        // We skip empty pairs:
        (nSubpixelXLeft = pEdgeStart.X.get());
        if (nSubpixelXLeft != pEdgeEnd.X.get())
        {
            // We now know we have a non-empty interval.  Skip any
            // empty interior pairs:

            while ({(nSubpixelXRight = pEdgeEnd.X.get()); pEdgeEnd.X == pEdgeEnd.Next.get().X})
            {
                pEdgeEnd = pEdgeEnd.Next.get().Next.get();
            }

            debug_assert!((nSubpixelXLeft < nSubpixelXRight) && (nSubpixelXRight < INT::MAX));

            IFC!(self.AddInterval(nSubpixelXLeft, nSubpixelXRight));
        }

        // Prepare for the next iteration:
        pEdgeStart = pEdgeEnd.Next.get();
    }

//Cleanup:
    return hr

}

//-------------------------------------------------------------------------
//
//  Function:   CCoverageBuffer::FillEdgesWinding
//
//  Synopsis:   
//      Given the active edge list for the current scan, do an alternate-mode
//      antialiased fill.
//
//-------------------------------------------------------------------------
pub fn FillEdgesWinding(&'a self,
    pEdgeActiveList: Ref<CEdge>,
    nSubpixelYCurrent: INT
    ) -> HRESULT
{

    let hr: HRESULT = S_OK;
    let mut pEdgeStart: Ref<CEdge> = pEdgeActiveList.Next.get();
    let mut pEdgeEnd: Ref<CEdge>;
    let mut nSubpixelXLeft: INT;
    let mut nSubpixelXRight: INT;
    let mut nWindingValue: INT;

    ASSERTACTIVELIST!(pEdgeActiveList, nSubpixelYCurrent);

    while (pEdgeStart.X.get() != INT::MAX)
    {
        pEdgeEnd = pEdgeStart.Next.get();

        nWindingValue = pEdgeStart.WindingDirection;
        while ({nWindingValue += pEdgeEnd.WindingDirection; nWindingValue != 0})
        {
            pEdgeEnd = pEdgeEnd.Next.get();
        }

        debug_assert!(pEdgeEnd.X.get() != INT::MAX);

        // We skip empty pairs:

        if ({nSubpixelXLeft = pEdgeStart.X.get(); nSubpixelXLeft != pEdgeEnd.X.get()})
        {
            // We now know we have a non-empty interval.  Skip any
            // empty interior pairs:

            while ({nSubpixelXRight = pEdgeEnd.X.get(); nSubpixelXRight == pEdgeEnd.Next.get().X.get()})
            {
                pEdgeStart = pEdgeEnd.Next.get();
                pEdgeEnd = pEdgeStart.Next.get();

                nWindingValue = pEdgeStart.WindingDirection;
                while ({nWindingValue += pEdgeEnd.WindingDirection; nWindingValue != 0})
                {
                    pEdgeEnd = pEdgeEnd.Next.get();
                }
            }

            debug_assert!((nSubpixelXLeft < nSubpixelXRight) && (nSubpixelXRight < INT::MAX));

            IFC!(self.AddInterval(nSubpixelXLeft, nSubpixelXRight));
        }

        // Prepare for the next iteration:

        pEdgeStart = pEdgeEnd.Next.get();
    } 

//Cleanup:
    return hr;//RRETURN(hr);
}

//-------------------------------------------------------------------------
//
//  Function:   CCoverageBuffer::Initialize
//
//  Synopsis:   Set the coverage buffer to a valid initial state
// 
//-------------------------------------------------------------------------
pub fn Initialize(&'a self) 
{
    self.m_pIntervalBufferBuiltin.m_interval[0].m_nPixelX.set(INT::MIN);
    self.m_pIntervalBufferBuiltin.m_interval[0].m_nCoverage.set(0);
    self.m_pIntervalBufferBuiltin.m_interval[0].m_pNext.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[1]));

    self.m_pIntervalBufferBuiltin.m_interval[1].m_nPixelX.set(INT::MAX);
    self.m_pIntervalBufferBuiltin.m_interval[1].m_nCoverage.set(0xdeadbeef);
    self.m_pIntervalBufferBuiltin.m_interval[1].m_pNext.set(unsafe { Ref::null() });

    self.m_pIntervalBufferBuiltin.m_pNext.set(None);
    self.m_pIntervalBufferCurrent.set(Ref::new(&self.m_pIntervalBufferBuiltin));

    self.m_pIntervalStart.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[0]));
    self.m_pIntervalNew.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[2]));
    self.interval_new_index.set(2);
    self.m_pIntervalEndMinus4.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[INTERVAL_BUFFER_NUMBER - 4]));
    self.m_pIntervalLast.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[1]));
}

//-------------------------------------------------------------------------
//
//  Function:   CCoverageBuffer::Destroy
//
//  Synopsis:   Free all allocated buffers
// 
//-------------------------------------------------------------------------
pub fn Destroy(&mut self)
{
    // Free the linked-list of allocations (skipping 'm_pIntervalBufferBuiltin',
    // which is built into the class):


}


//-------------------------------------------------------------------------
//
//  Function:   CCoverageBuffer::Reset
//
//  Synopsis:   Reset the coverage buffer
// 
//-------------------------------------------------------------------------
pub fn Reset(&'a self)
{
    // Reset our coverage structure.  Point the head back to the tail,
    // and reset where the next new entry will be placed:

    self.m_pIntervalBufferBuiltin.m_interval[0].m_pNext.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[1]));

    self.m_pIntervalBufferCurrent.set(Ref::new(&self.m_pIntervalBufferBuiltin));
    self.m_pIntervalNew.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[2]));
    self.interval_new_index.set(2);
    self.m_pIntervalEndMinus4.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[INTERVAL_BUFFER_NUMBER - 4]));
    self.m_pIntervalLast.set(Ref::new(&self.m_pIntervalBufferBuiltin.m_interval[1]));
}

//-------------------------------------------------------------------------
//
//  Function:   CCoverageBuffer::Grow
//
//  Synopsis:   
//      Grow our interval buffer.
//
//-------------------------------------------------------------------------
fn Grow(&'a self,
    ppIntervalNew: &mut Ref<'a, CCoverageInterval<'a>>, 
    ppIntervalEndMinus4: &mut Ref<'a, CCoverageInterval<'a>>,
    interval_new_index: &mut usize
    ) -> HRESULT
{
    let hr: HRESULT = S_OK;
    let pIntervalBufferNew = (*self.m_pIntervalBufferCurrent.get()).m_pNext.get();

    let pIntervalBufferNew = pIntervalBufferNew.unwrap_or_else(||
    {
        let pIntervalBufferNew = self.arena.alloc(Default::default());

        (*pIntervalBufferNew).m_pNext.set(None);
        (*self.m_pIntervalBufferCurrent.get()).m_pNext.set(Some(pIntervalBufferNew));
        pIntervalBufferNew
    });

    self.m_pIntervalBufferCurrent.set(Ref::new(pIntervalBufferNew));

    self.m_pIntervalNew.set(Ref::new(&(*pIntervalBufferNew).m_interval[2]));
    self.interval_new_index.set(2);
    self.m_pIntervalEndMinus4.set(Ref::new(&(*pIntervalBufferNew).m_interval[INTERVAL_BUFFER_NUMBER - 4]));

    *ppIntervalNew = self.m_pIntervalNew.get();
    *ppIntervalEndMinus4 = self.m_pIntervalEndMinus4.get();
    *interval_new_index = 2;

    return hr;
}

}
/* 
impl<'a> Drop for CCoverageBuffer<'a> {
    fn drop(&mut self) {
        self.Destroy();
    }
}*/
