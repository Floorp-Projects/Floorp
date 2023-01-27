// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.


//-----------------------------------------------------------------------------
//

//
//  Description:
//      Contains HW Vertex Buffer and Builder class implementations
//
//
//  Notes:
//        
//        +--------------------------------------+
//        |                                      |
//        |           Start Stratum              |
//     1  |                                      |
//        |                                      |
//        +--------------------------------------+
//     2  |======================================|
//        +--------------------------------------+
//        |          /   \             / \       |
//        |         /     \           /   \      |
//        |   A    /   B   \    C    /  D  \  E  |
//     3  |       /         \       /       \    |
//        |      /           \     /         \   |
//        |     /             \   /           \  |
//        |    /               \ /             \ |
//        +--------------------------------------+
//        |    \               / \             / |
//        |     \             /   \           /  |
//     4  |  F   \     G     /  H  \    I    / J |
//        |       \         /       \       /    |
//        +--------------------------------------+
//     5  |======================================|
//        +--------------------------------------+
//     6  |======================================|
//        +--------------------------------------+
//        |                                      |
//        |                                      |
//     7  |           Stop Stratum               |
//        |                                      |
//        |                                      |
//        +--------------------------------------+
//        
//  
//  Strata & complement mode.
//  
//  The anti-aliased HW rasterizer produces a series of "strata" where
//  each strata can be a complex span rendered using lines (#'s 2,5,6) or
//  a series of trapezoids (#'s 3 & 4.)  In normal mode the trapezoid
//  regions B,D,G,I are filled in.
//  
//  Complement mode complicates things.  Complex spans are relatively easy
//  because we get the whole line's worth of data at once.  Trapezoids are
//  more complex because we get B,D,G and I separately.  We handle this by
//  tracking the current stratum and finishing the last incomplete
//  trapezoid stratum when a new stratum begins.  Regions E & J finish
//  trapezoid strata.  We also need to add rectangles at the beginning and
//  end of the geometry (start and stop) to fill out the complement
//  region.
//  
//  This is implemented like so:
//  
//    1. Strata are generated from top to bottom without gaps.
//    2. Before drawing any lines or trapezoids call
//       PrepareStratum(a, b, fTrapezoid) where a & b are the extent of
//       the current stratum and fTrapezoid is true if you are drawing
//       a trapezoid.  This will take care of creating the start
//       stratum and/or finishing a trapezoid stratum if necessary.
//    3. When completely done call EndBuildingOutside() which will
//       close a pending trapezoid and/or produce the stop stratum.
//  
//-----------------------------------------------------------------------------

const FORCE_TRIANGLES: bool = true;

//+----------------------------------------------------------------------------
//
//  Constants to control when we stop waffling because the tiles are too
//  small to make a difference.
//
//  Future Consideration:  can produce an excessive number of triangles.
//   How we mitigate or handle this could be improved.  Right now we stop
//   waffling if the waffle size is less than a quarter-pixel.
//   Two big improvements that could be made are:
//    - multipacking very small textures (but note that we cannot rely
//      on prefiltering to ensure that small screen space means small texture
//      source)
//    - clipping primitives to approximately the screen size
//
//-----------------------------------------------------------------------------
//const c_rMinWaffleWidthPixels: f32 = 0.25;


const FLOAT_ZERO: f32 = 0.;
const FLOAT_ONE: f32 = 1.;

//+----------------------------------------------------------------------------
//
//  Class:     CHwVertexBuffer and CHwTVertexBuffer<class TVertex>
//
//  Synopsis:  This class accumulates geometry data for a primitive
//
//-----------------------------------------------------------------------------

use crate::{types::*, geometry_sink::IGeometrySink, aacoverage::c_nShiftSizeSquared, OutputVertex, nullable_ref::Ref};


//+----------------------------------------------------------------------------
//
//  Class:     CHwVertexBuffer::Builder
//
//  Synopsis:  Base vertex builder class
//
//  Responsibilities:
//    - Given ordered basic vertex information expand/convert/pass-thru
//      to vertex buffer  (Basic vertex information is minimal vertex
//      information sent from the caller that may or may not have been
//      passed thru a tessellator.)
//    - Choosing vertex format from a minimal required vertex format
//
//  Not responsible for:
//    - Allocating space in vertex buffer
//
//  Inputs required:
//    - Key and data to translate input basic vertex info to full vertex data
//    - Vertex info from tessellation (or other Geometry Generator)
//    - Vertex buffer to send output to
//

/*pub struct CHwVertexBufferBuilder /* : public IGeometrySink */
{
    /* 
public:

    static HRESULT Create(
        MilVertexFormat vfIn,
        MilVertexFormat vfOut,
        MilVertexFormatAttribute vfaAntiAliasScaleLocation,
        __in_ecount_opt(1) CHwPipeline *pPipeline,
        __in_ecount_opt(1) CD3DDeviceLevel1 *pDevice,
        __in_ecount(1) CBufferDispenser *pBufferDispenser,
        __deref_out_ecount(1) CHwVertexBuffer::Builder **ppVertexBufferBuilder
        );

    virtual ~Builder()
    {
#if DBG
        Assert(!m_fDbgDestroyed);
        m_fDbgDestroyed = true;
#endif DBG
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    SetConstantMapping
    //
    //  Synopsis:  Use this method to specify that the given color source for
    //             the given vertex destination is constant (won't differ per
    //             vertex)
    //
    //-------------------------------------------------------------------------

    virtual HRESULT SetConstantMapping(
        MilVertexFormatAttribute mvfaDestination,
        __in_ecount(1) const CHwConstantColorSource *pConstCS
        ) PURE;


    //+------------------------------------------------------------------------
    //
    //  Member:    FinalizeMappings
    //
    //  Synopsis:  Use this method to let builder know that all mappings have
    //             been sent
    //
    //-------------------------------------------------------------------------

    virtual HRESULT FinalizeMappings(
        ) PURE;

    //+------------------------------------------------------------------------
    //
    //  Member:    SetOutsideBounds
    //
    //  Synopsis:  Enables rendering zero-alpha geometry outside of the input
    //             shape but within the given bounding rectangle, if fNeedInside
    //             isn't true then it doesn't render geometry with full alpha.
    //
    //-------------------------------------------------------------------------
    virtual void SetOutsideBounds(
        __in_ecount_opt(1) const CMILSurfaceRect *prcBounds,
        bool fNeedInside
        ) PURE;

    //+------------------------------------------------------------------------
    //
    //  Member:    HasOutsideBounds
    //
    //  Synopsis:  Returns true if outside bounds have been set.
    //
    //-------------------------------------------------------------------------
    virtual bool HasOutsideBounds() const PURE;

    //+------------------------------------------------------------------------
    //
    //  Member:    BeginBuilding
    //
    //  Synopsis:  This method lets the builder know it should start from a
    //             clean slate
    //
    //-------------------------------------------------------------------------

    virtual HRESULT BeginBuilding(
        ) PURE;

    //+------------------------------------------------------------------------
    //
    //  Member:    EndBuilding
    //
    //  Synopsis:  Use this method to let the builder know that all of the
    //             vertex data has been sent
    //
    //-------------------------------------------------------------------------

    virtual HRESULT EndBuilding(
        __deref_opt_out_ecount(1) CHwVertexBuffer **ppVertexBuffer
        ) PURE;

    //+------------------------------------------------------------------------
    //
    //  Member:    FlushReset
    //
    //  Synopsis:  Send pending state and geometry to the device and reset
    //             the vertex buffer.
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE HRESULT FlushReset()
    {
        return FlushInternal(NULL);
    }
        
    //
    // Currently all CHwVertexBuffer::Builder are supposed to be allocated via
    // a CBufferDispenser.
    //

    DECLARE_BUFFERDISPENSER_DELETE

protected:

    Builder()
    {
        m_mvfIn = MILVFAttrNone;

#if DBG
        m_mvfDbgOut = MILVFAttrNone;
#endif

        m_mvfaAntiAliasScaleLocation = MILVFAttrNone;

        m_pPipelineNoRef = NULL;
        m_pDeviceNoRef = NULL;
        
#if DBG
        m_fDbgDestroyed = false;
#endif DBG
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    FlushInternal
    //
    //  Synopsis:  Send any pending state and geometry to the device.
    //             If the optional argument is NULL then reset the
    //             vertex buffer.
    //             If the optional argument is non-NULL AND we have
    //             not yet flushed the vertex buffer return the vertex
    //             buffer.
    //
    //-------------------------------------------------------------------------

    virtual HRESULT FlushInternal(
        __deref_opt_out_ecount_opt(1) CHwVertexBuffer **ppVertexBuffer
        ) PURE;


    CHwPipeline *m_pPipelineNoRef;
    CD3DDeviceLevel1 *m_pDeviceNoRef;

    MilVertexFormat m_mvfIn;         // Vertex fields that are pre-generated

#if DBG
    MilVertexFormat m_mvfDbgOut;     // Output format of the vertex
#endif

    MilVertexFormat m_mvfGenerated;  // Vertex fields that are dynamically
                                     // generated by this builder

    MilVertexFormatAttribute m_mvfaAntiAliasScaleLocation;  // Vertex field that
                                                            // contains PPAA
                                                            // falloff factor

#if DBG
private:

    bool m_fDbgDestroyed;     // Used to check single Release pattern

#endif DBG
*/
}*/
#[derive(Default)]
pub struct CD3DVertexXYZDUV2 {
    x: f32,
    y: f32,
    //Z: f32,
    coverage: f32,
    /*U0: f32, V0: f32,
    U1: f32, V1: f32,*/
}
pub type CHwVertexBuffer<'z> = CHwTVertexBuffer<'z, OutputVertex>;
#[derive(Default)]
pub struct CHwTVertexBuffer<'z, TVertex>
{
    //m_rgIndices: DynArray<WORD>,     // Dynamic array of indices


    //m_pBuilder: Rc<CHwTVertexBufferBuilder<TVertex>>,

    /* 
#if DBG
public:
    
    CHwTVertexBuffer()
    {
        m_fDbgNonLineSegmentTriangleStrip = false;
    }
#endif

protected:

    //+------------------------------------------------------------------------
    //
    //  Member:    Reset
    //
    //  Synopsis:  Mark the beginning of a new list of vertices; the existing
    //             list is discarded
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE void Reset(
        __in_ecount(1) Builder *pVBB
        )
    {
#if DBG
        m_fDbgNonLineSegmentTriangleStrip = false;
#endif
        m_rgIndices.SetCount(0);
        m_rgVerticesTriList.SetCount(0);
        m_rgVerticesTriStrip.SetCount(0);
        m_rgVerticesLineList.SetCount(0);
        m_rgVerticesNonIndexedTriList.SetCount(0);

        m_pBuilder = pVBB;
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    AddNonIndexedTriListVertices
    //
    //  Synopsis:  Reserve space for consecutive vertices and return start
    //             index
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE HRESULT AddNonIndexedTriListVertices(
        UINT uCount,
        __deref_ecount(uCount) TVertex **ppVertices
        );

    //+------------------------------------------------------------------------
    //
    //  Member:    AddTriListVertices
    //
    //  Synopsis:  Reserve space for consecutive vertices and return start
    //             index
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE HRESULT AddTriListVertices(
        UINT uDelta,
        __deref_ecount(uDelta) TVertex **ppVertices,
        __out_ecount(1) WORD *pwIndexStart
        );

    //+------------------------------------------------------------------------
    //
    //  Member:    AddTriStripVertices
    //
    //  Synopsis:  Reserve space for consecutive vertices and return start
    //             index
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE HRESULT AddTriStripVertices(
        UINT uCount,
        __deref_ecount(uCount) TVertex **ppVertices
        );

    //+------------------------------------------------------------------------
    //
    //  Member:    AddLineListVertices
    //
    //  Synopsis:  Reserve space for consecutive vertices and return start
    //             index
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE HRESULT AddLineListVertices(
        UINT uCount,
        __deref_ecount(uCount) TVertex **ppVertices
        );

public:

    //+------------------------------------------------------------------------
    //
    //  Member:    AddLine implements ILineSink<PointXYA>
    //
    //  Synopsis:  Add a line given two points with x, y, & alpha.
    //
    //-------------------------------------------------------------------------
    HRESULT AddLine(
        __in_ecount(1) const PointXYA &v0,
        __in_ecount(1) const PointXYA &v1
        );

    //+------------------------------------------------------------------------
    //
    //  Member:    AddTriangle implements ITriangleSink<PointXYA>
    //
    //  Synopsis:  Add a triangle given three points with x, y, & alpha.
    //
    //-------------------------------------------------------------------------

    HRESULT AddTriangle(
        __in_ecount(1) const PointXYA &v0,
        __in_ecount(1) const PointXYA &v1,
        __in_ecount(1) const PointXYA &v2
        );

    // Re-introduce parent AddTriangle(WORD,WORD,WORD) into this scope.
    using CHwVertexBuffer::AddTriangle;
    
    //+------------------------------------------------------------------------
    //
    //  Member:    AddLineAsTriangleStrip
    //
    //  Synopsis:  Add a horizontal line using a trinagle strip
    //
    //-------------------------------------------------------------------------
    HRESULT AddLineAsTriangleStrip(
        __in_ecount(1) const TVertex *pBegin, // Begin
        __in_ecount(1) const TVertex *pEnd    // End
        );

    //+------------------------------------------------------------------------
    //
    //  Member:    SendVertexFormat
    //
    //  Synopsis:  Send contained vertex format to device
    //
    //-------------------------------------------------------------------------

    HRESULT SendVertexFormat(
        __inout_ecount(1) CD3DDeviceLevel1 *pDevice
        ) const;

    //+------------------------------------------------------------------------
    //
    //  Member:    DrawPrimitive
    //
    //  Synopsis:  Send the geometry data to the device and execute rendering
    //
    //-------------------------------------------------------------------------

    HRESULT DrawPrimitive(
        __inout_ecount(1) CD3DDeviceLevel1 *pDevice
        ) const;

protected:
    //+------------------------------------------------------------------------
    //
    //  Member:    GetNumTriListVertices
    //
    //  Synopsis:  Return current number of vertices
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE DWORD GetNumTriListVertices() const
    {
        return m_rgVerticesTriList.GetCount();
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    GetTriListVertices
    //
    //  Synopsis:  Return pointer to beginning of vertex list and their count
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE void GetTriListVertices(
        __deref_out_ecount_full(*puNumVertices) TVertex **ppVertices,
        __out_ecount(1) UINT * puNumVertices
        )
    {
        *ppVertices = m_rgVerticesTriList.GetDataBuffer();
        *puNumVertices = m_rgVerticesTriList.GetCount();
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    GetNumNonIndexedTriListVertices
    //
    //  Synopsis:  Return current number of vertices
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE DWORD GetNumNonIndexedTriListVertices() const
    {
        return m_rgVerticesNonIndexedTriList.GetCount();
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    GetNonIndexedTriListVertices
    //
    //  Synopsis:  Return pointer to beginning of vertex list and their count
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE void GetNonIndexedTriListVertices(
        __deref_out_ecount_full(*puNumVertices) TVertex **ppVertices,
        __out_ecount(1) UINT * puNumVertices
        )
    {
        *ppVertices = m_rgVerticesNonIndexedTriList.GetDataBuffer();
        *puNumVertices = m_rgVerticesNonIndexedTriList.GetCount();
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    GetNumTriStripVertices
    //
    //  Synopsis:  Return current number of vertices
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE DWORD GetNumTriStripVertices() const
    {
        return m_rgVerticesTriStrip.GetCount();
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    GetTriStripVertices
    //
    //  Synopsis:  Return pointer to beginning of vertex list and their count
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE void GetTriStripVertices(
        __deref_out_ecount_full(*puNumVertices) TVertex **ppVertices,
        __out_ecount(1) UINT *puNumVertices
        )
    {
        *ppVertices = m_rgVerticesTriStrip.GetDataBuffer();
        *puNumVertices = m_rgVerticesTriStrip.GetCount();
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    GetNumLineListVertices
    //
    //  Synopsis:  Return current number of vertices
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE DWORD GetNumLineListVertices() const
    {
        return m_rgVerticesLineList.GetCount();
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    GetLineListVertices
    //
    //  Synopsis:  Return pointer to beginning of vertex list and their count
    //
    //-------------------------------------------------------------------------

    MIL_FORCEINLINE void GetLineListVertices(
        __deref_out_ecount_full(*puNumVertices) TVertex **ppVertices,
        __out_ecount(1) UINT * puNumVertices
        )
    {
        *ppVertices = m_rgVerticesLineList.GetDataBuffer();
        *puNumVertices = m_rgVerticesLineList.GetCount();
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    GetLineListVertices
    //
    //  Synopsis:  Return pointer to beginning of vertex list
    //
    //-------------------------------------------------------------------------



*/

    // Dynamic array of vertices for which all allocations are zeroed.
    // XXX: the zero has been removed
    //m_rgVerticesTriList: DynArray<TVertex>,             // Indexed triangle list vertices
    //m_rgVerticesNonIndexedTriList: DynArray<TVertex>,   // Non-indexed triangle list vertices
    m_rgVerticesTriList: DynArray<TVertex>,            // Triangle strip vertices
    //m_rgVerticesLineList: DynArray<TVertex>,            // Linelist vertices

    m_rgVerticesBuffer: Option<&'z mut [TVertex]>,
    m_rgVerticesBufferOffset: usize,

    #[cfg(debug_assertions)]
    // In debug make a note if we add a triangle strip that doesn't have 6 vertices
    // so that we can ensure that we only waffle 6-vertex tri strips.
    m_fDbgNonLineSegmentTriangleStrip: bool,
    subpixel_bias: f32,
}

impl<'z, TVertex: Default> CHwTVertexBuffer<'z, TVertex> {
    pub fn new(rasterization_truncates: bool, output_buffer: Option<&'z mut [TVertex]>) -> Self {
        CHwTVertexBuffer::<TVertex> {
            subpixel_bias: if rasterization_truncates {
                // 1/512 is 0.5 of a subpixel when using 8 bits of subpixel precision.
                1./512.
            } else {
                0.
            },
            m_rgVerticesBuffer: output_buffer,
            m_rgVerticesBufferOffset: 0,
            ..Default::default()
        }
    }

    pub fn flush_output(&mut self) -> Box<[TVertex]> {
        std::mem::take(&mut self.m_rgVerticesTriList).into_boxed_slice()
    }

    pub fn get_output_buffer_size(&self) -> Option<usize> {
        if self.m_rgVerticesBuffer.is_some() {
            Some(self.m_rgVerticesBufferOffset)
        } else {
            None
        }
    }
}

//+----------------------------------------------------------------------------
//
//  Class:     CHwTVertexMappings<class TVertex>
//
//  Synopsis:  Helper class that knows how to populate a vertex from the
//             incoming basic per vertex data, like just X and Y
//
//-----------------------------------------------------------------------------
#[derive(Default)]
struct CHwTVertexMappings<TVertex>
{/* 
public:

    CHwTVertexMappings();

    void SetPositionTransform(
        __in_ecount(1) const MILMatrix3x2 &matPositionTransform
        );

    HRESULT SetConstantMapping(
        MilVertexFormatAttribute mvfaDestination,
        __in_ecount(1) const CHwConstantColorSource *pConstCS
        );

    void PointToUV(
        __in_ecount(1) const MilPoint2F &ptIn,
        __bound UINT uIndex,
        __out_ecount(1) TVertex *pvOut
        );
    
    MIL_FORCEINLINE bool AreWaffling() const
    {
        return false;
    }

private:
    static const size_t s_numOfVertexTextureCoords
        = NUM_OF_VERTEX_TEXTURE_COORDS(TVertex);
public:

    MilVertexFormat m_mvfMapped;

    MilColorF m_colorStatic;

    MILMatrix3x2 m_matPos2DTransform;

    MILMatrix3x2 m_rgmatPointToUV[s_numOfVertexTextureCoords];
    CMilPointAndSizeF m_rgSubrect[s_numOfVertexTextureCoords];
    WaffleModeFlags m_rgWaffleMode[s_numOfVertexTextureCoords];

*/
    m_vStatic: TVertex,
    subpixel_bias: f32,
}

impl<TVertex> CHwTVertexBuffer<'_, TVertex> {
    pub fn Reset(&mut self,
        /*pVBB: &mut CHwTVertexBufferBuilder<TVertex>*/
        )
    {
        #[cfg(debug_assertions)]
        {
            self.m_fDbgNonLineSegmentTriangleStrip = false;
        }

        //self.m_rgIndices.SetCount(0);
        //self.m_rgVerticesTriList.SetCount(0);
        self.m_rgVerticesTriList.SetCount(0);
        self.m_rgVerticesBufferOffset = 0;
        //self.m_rgVerticesLineList.SetCount(0);
        //self.m_rgVerticesNonIndexedTriList.SetCount(0);

        //self.m_pBuilder = pVBB;
    }

    fn IsEmpty(&self) -> bool
    {
        return true
             //  && (self.m_rgIndices.GetCount() == 0)
            //&& (self.m_rgVerticesLineList.GetCount() == 0)
            && (self.m_rgVerticesTriList.GetCount() == 0)
            && self.m_rgVerticesBufferOffset == 0
            //&& (self.m_rgVerticesNonIndexedTriList.GetCount() == 0);
    }

}

//+----------------------------------------------------------------------------
//
//  Class:     CHwTVertexBuffer<class TVertex>::Builder
//
//  Synopsis:  Implements CHwVertexBuffer::Builder for a particular vertex
//             format
//
//-----------------------------------------------------------------------------

pub struct CHwTVertexBufferBuilder<'y, 'z, TVertex>
{
    m_mvfIn: MilVertexFormat,         // Vertex fields that are pre-generated

    #[cfg(debug_assertions)]
    m_mvfDbgOut: MilVertexFormat,     // Output format of the vertex
    
    m_mvfGenerated: MilVertexFormat,  // Vertex fields that are dyn

    m_mvfaAntiAliasScaleLocation: MilVertexFormatAttribute,  // Vertex field that
                                                             // contains PPAA
                                                             // falloff factor

    /*
public:

    static MilVertexFormat GetOutVertexFormat();

    static HRESULT Create(
        __in_ecount(1) CHwTVertexBuffer<TVertex> *pVertexBuffer,
        MilVertexFormat mvfIn,
        MilVertexFormat mvfOut,
        MilVertexFormatAttribute mvfaAntiAliasScaleLocation,
        __inout_ecount(1) CBufferDispenser *pBufferDispenser,
        __deref_out_ecount(1) typename CHwTVertexBuffer<TVertex>::Builder **ppVertexBufferBuilder
        );

    HRESULT SetConstantMapping(
        MilVertexFormatAttribute mvfaDestination,
        __in_ecount(1) const CHwConstantColorSource *pConstCS
        );

    void SetTransformMapping(
        __in_ecount(1) const MILMatrix3x2 &mat2DTransform
        );

    HRESULT FinalizeMappings(
        );

    void SetOutsideBounds(
        __in_ecount_opt(1) const CMILSurfaceRect *prcBounds,
        bool fNeedInside
        );

    bool HasOutsideBounds() const
    {
        return NeedOutsideGeometry();
    }

    HRESULT BeginBuilding(
        );

    HRESULT AddVertex(
        __in_ecount(1) const MilPoint2F &ptPosition,
            // In: Vertex coordinates
        __out_ecount(1) WORD *pIndex
            // Out: The index of the new vertex
        );

    HRESULT AddIndexedVertices(
        UINT cVertices,                                                  // In: number of vertices                                                                                            
        __in_bcount(cVertices*uVertexStride) const void *pVertexBuffer,  // In: vertex buffer containing the vertices                                                                         
        UINT uVertexStride,                                              // In: size of each vertex                                                                                           
        MilVertexFormat mvfFormat,                                       // In: format of each vertex                                                                                         
        UINT cIndices,                                                   // In: Number of indices                                                                                             
        __in_ecount(cIndices) const UINT *puIndexBuffer                  // In: index buffer                                                             
        );

    HRESULT AddTriangle(
        DWORD i1,                    // In: Index of triangle's first vertex
        DWORD i2,                    // In: Index of triangle's second vertex
        DWORD i3                     // In: Index of triangle's third vertex
        );

    HRESULT AddComplexScan(
        INT nPixelY,
            // In: y coordinate in pixel space
        __in_ecount(1) const CCoverageInterval *pIntervalSpanStart
            // In: coverage segments
        );

   HRESULT AddParallelogram(
        __in_ecount(4)  const MilPoint2F *rgPosition
        );
   
    HRESULT AddTrapezoid(
        float rPixelYTop,               // In: y coordinate of top of trapezoid
        float rPixelXTopLeft,           // In: x coordinate for top left
        float rPixelXTopRight,          // In: x coordinate for top right
        float rPixelYBottom,            // In: y coordinate of bottom of trapezoid
        float rPixelXBottomLeft,        // In: x coordinate for bottom left
        float rPixelXBottomRight,       // In: x coordinate for bottom right
        float rPixelXLeftDelta,         // In: trapezoid expand radius for left edge
        float rPixelXRightDelta         // In: trapezoid expand radius for right edge
        );

    BOOL IsEmpty();

    HRESULT EndBuilding(
        __deref_opt_out_ecount(1) CHwVertexBuffer **ppVertexBuffer
        );

    HRESULT FlushInternal(
        __deref_opt_out_ecount_opt(1) CHwVertexBuffer **ppVertexBuffer
        );
            
private:

    // Helpers that do AddTrapezoid.  Same parameters
    HRESULT AddTrapezoidStandard( float, float, float, float, float, float, float, float );
    HRESULT AddTrapezoidWaffle( float, float, float, float, float, float, float, float );


    
    HRESULT PrepareStratumSlow(
        float rStratumTop,
        float rStratumBottom,
        bool fTrapezoid,
        float rTrapezoidLeft,
        float rTrapezoidRight
        );
    
    // Wrap up building of outside geometry.
    HRESULT EndBuildingOutside();

    DECLARE_BUFFERDISPENSER_NEW(CHwTVertexBuffer<TVertex>::Builder,
                                Mt(CHwTVertexBuffer_Builder));

    Builder(
        __in_ecount(1) CHwTVertexBuffer<TVertex> *pVertexBuffer
    );

    HRESULT SetupConverter(
        MilVertexFormat mvfIn,
        MilVertexFormat mvfOut,
        MilVertexFormatAttribute mvfaAntiAliasScaleLocation
        );

    HRESULT RenderPrecomputedIndexedTriangles(
        __range(1, SHORT_MAX) UINT cVertices,
        __in_ecount(cVertices) const TVertex *rgoVertices,
        __range(1, UINT_MAX) UINT cIndices,
        __in_ecount(cIndices) const UINT *rguIndices
        );


    // Expands all vertices in the buffer.
    void ExpandVertices();
    
    // Has never been successfully used to declare a method or derived type...
/*    typedef void (CHwTVertexBuffer<TVertex>::Builder::FN_ExpandVertices)(
        UINT uCount,
        TVertex *pVertex
        );*/

    // error C2143: syntax error : missing ';' before '*'
//    typedef FN_ExpandVertices *PFN_ExpandVertices;

    typedef void (CHwTVertexBuffer<TVertex>::Builder::* PFN_ExpandVertices)(
        __range(1,UINT_MAX) UINT uCount,
        __inout_ecount_full(uCount) TVertex *rgVertices
        );

    //
    // Table of vertex expansion routines for common expansion cases:
    //  - There are entries for Z, Diffuse, and one set texture coordinates for
    //    a total of eight combinations.
    //  - Additionally there is a second set of entries for anti-aliasing
    //    falloff applied thru diffuse.
    //

    static const PFN_ExpandVertices sc_pfnExpandVerticesTable[8*2];

    MIL_FORCEINLINE
    void TransferAndOrExpandVerticesInline(
        __range(1,UINT_MAX) UINT uCount,
        __in_ecount(uCount) TVertex const * rgInputVertices,
        __out_ecount(uCount) TVertex *rgOutputVertices,
        MilVertexFormat mvfOut,
        MilVertexFormatAttribute mvfaScaleByFalloff,
        bool fInputOutputAreSameBuffer,
        bool fTransformPosition
        );

    // FN_ExpandVertices ExpandVerticesFast
    template <MilVertexFormat mvfOut, MilVertexFormatAttribute mvfaScaleByFalloff>
    void ExpandVerticesFast(
        __range(1,UINT_MAX) UINT uCount,
        __inout_ecount_full(uCount) TVertex *rgVertices
        )
    {
        TransferAndOrExpandVerticesInline(
            uCount, 
            rgVertices, 
            rgVertices, 
            mvfOut, 
            mvfaScaleByFalloff,
            true, // => fInputOutputAreSameBuffer
            false // => fTransformPosition
            );
    }

    // error C2146: syntax error : missing ';' before identifier 'ExpandVerticesGeneral'
    // error C2501: 'CHwTVertexBufferBuilder<TVertex>::FN_ExpandVertices' : missing storage-class or type specifiers
//    FN_ExpandVertices ExpandVerticesGeneral
//    typename FN_ExpandVertices ExpandVerticesGeneral
    // error C4346: 'CHwTVertexBufferBuilder<TVertex>::FN_ExpandVertices' : dependent name is not a type
//    CHwTVertexBufferBuilder<TVertex>::FN_ExpandVertices ExpandVerticesGeneral
    // Can't define methos here (unless not parameters are used).
//    typename CHwTVertexBufferBuilder<TVertex>::FN_ExpandVertices ExpandVerticesGeneral
    // FN_ExpandVertices ExpandVerticesGeneral
    void ExpandVerticesGeneral(
        __range(1,UINT_MAX) UINT uCount,
        __inout_ecount_full(uCount) TVertex *rgVertices
        )
    {
        TransferAndOrExpandVerticesInline(
            uCount, 
            rgVertices,
            rgVertices,
            m_mvfGenerated, 
            m_mvfaAntiAliasScaleLocation,
            true, // => fInputOutputAreSameBuffer
            false // => fTransformPosition
            );
    }

    void TransferAndExpandVerticesGeneral(
        __range(1,UINT_MAX) UINT uCount,
        __in_ecount(uCount) TVertex const *rgInputVertices,
        __out_ecount_full(uCount) TVertex *rgOutputVertices,
        bool fTransformPosition
        )
    {
        TransferAndOrExpandVerticesInline(
            uCount, 
            rgInputVertices,
            rgOutputVertices,
            m_mvfGenerated, 
            m_mvfaAntiAliasScaleLocation,
            false,              // => fInputOutputAreSameBuffer
            fTransformPosition  // => fTransformPosition
            );
    }

    // FN_ExpandVertices ExpandVerticesInvalid
    void ExpandVerticesInvalid(
        __range(1,UINT_MAX) UINT uCount,
        __inout_ecount_full(uCount) TVertex *rgVertices
        )
    {
        RIP("Invalid ExpandVertices routine.");
    }

    //+------------------------------------------------------------------------
    //
    //  Member:    NeedCoverageGeometry
    //
    //  Synopsis:  True if we should create geometry for a particular
    //             coverage value.
    //
    //-------------------------------------------------------------------------
    bool NeedCoverageGeometry(INT nCoverage) const;





    //+------------------------------------------------------------------------
    //
    //  Member:    ReinterpretFloatAsDWORD
    //
    //  Synopsis:  Quicky helper to convert a float to a DWORD bitwise.
    //
    //-------------------------------------------------------------------------
    static MIL_FORCEINLINE DWORD ReinterpretFloatAsDWORD(float c)
    {
        return reinterpret_cast<DWORD &>(c);
    }

private:
    MIL_FORCEINLINE bool AreWaffling() const
    {
        return m_map.AreWaffling();
    }
   
    void ViewportToPackedCoordinates(
        __range(1,UINT_MAX / uGroupSize) UINT uGroupCount,        
        __inout_ecount(uGroupCount * uGroupSize) TVertex *pVertex,
        __range(2,6) UINT uGroupSize,
        /*__range(0,NUM_OF_VERTEX_TEXTURE_COORDS(TVertex)-1)*/ __bound UINT uIndex
        );
    
    void ViewportToPackedCoordinates(
        __range(1,UINT_MAX / uGroupSize) UINT uGroupCount,
        __inout_ecount(uGroupCount * uGroupSize) TVertex *pVertex,
        __range(2,6) UINT uGroupSize
        );

    template<class TWaffler>
    __out_ecount(1) typename TWaffler::ISink *
    BuildWafflePipeline(
        __out_xcount(NUM_OF_VERTEX_TEXTURE_COORDS(TVertex) * 2) TWaffler *wafflers,
        __out_ecount(1) bool &fWafflersUsed
        ) const;

    
    template<class TWaffler>
    typename TWaffler::ISink *
    BuildWafflePipeline(
        __out_xcount(NUM_OF_VERTEX_TEXTURE_COORDS(TVertex) * 2) TWaffler *wafflers
        ) const
    {
        bool fNotUsed;
        return BuildWafflePipeline(wafflers, fNotUsed);
    }*/

    m_pVB: &'y mut CHwTVertexBuffer<'z, TVertex>,

    //m_pfnExpandVertices: PFN_ExpandVertices,  // Method for expanding vertices

    //m_rgoPrecomputedTriListVertices: *const TVertex,
    //m_cPrecomputedTriListVertices: UINT,

    //m_rguPrecomputedTriListIndices: *const UINT,
    //m_cPrecomputedTriListIndices: UINT,

    //m_map: CHwTVertexMappings<TVertex>,

    // This is true if we had to flush the pipeline as we were getting
    // geometry rather than just filling up a single vertex buffer.
    m_fHasFlushed: bool,

    // The next two members control the generation of the zero-alpha geometry
    // outside the input geometry.
    m_fNeedOutsideGeometry: bool,
    m_fNeedInsideGeometry: bool,
    m_rcOutsideBounds: CMILSurfaceRect, // Bounds for creation of outside geometry

    /* 
    // Helpful m_rcOutsideBounds casts.
    float OutsideLeft() const { return static_cast<float>(m_rcOutsideBounds.left); }
    float OutsideRight() const { return static_cast<float>(m_rcOutsideBounds.right); }
    float OutsideTop() const { return static_cast<float>(m_rcOutsideBounds.top); }
    float OutsideBottom() const { return static_cast<float>(m_rcOutsideBounds.bottom); }
    */
    // This interval (if we are doing outside) shows the location
    // of the current stratum.  It is initialized to [FLT_MAX, -FLT_MAX].
    //
    // If the current stratum is a complex span then
    // m_rCurStratumBottom is set to the bottom of the stratum and
    // m_rCurStratumTop is set to FLT_MAX.
    //
    // If the current stratum is a trapezoidal one, then
    // m_rCurStratumBottom is its bottom and m_rCurStratumTop is its
    // top.
    m_rCurStratumTop: f32,
    m_rCurStratumBottom: f32,

    // If the current stratum is a trapezoidal one, following var stores
    // right boundary of the last trapezoid handled by PrepareStratum.
    // We need it to cloze the stratus properly.
    m_rLastTrapezoidRight: f32,

    // These are needed to implement outside geometry using triangle lists
    m_rLastTrapezoidTopRight: f32,
    m_rLastTrapezoidBottomRight: f32,
}

/*
//+----------------------------------------------------------------------------
//
//  Member:    CHwVertexBuffer::AddTriangle
//
//  Synopsis:  Add a triangle using the three indices given to the list
//
impl CHwVertexBuffer {

fn AddTriangle(
    i1: WORD,         // In: Index of triangle's first vertex
    i2: WORD,         // In: Index of triangle's second vertex
    i3: WORD          // In: Index of triangle's third vertex
    ) -> HRESULT
{
    let hr: HRESULT = S_OK;

    // Asserting indices < max vertex requires a debug only pure virtual method
    // which is too much of a functionality change between retail and debug.
    //
    //
    // Assert(i1 < GetNumTriListVertices());
    // Assert(i2 < GetNumTriListVertices());
    // Assert(i3 < GetNumTriListVertices());

    WORD *pIndices;

    IFC(m_rgIndices.AddMultiple(3, &pIndices));

    pIndices[0] = i1;
    pIndices[1] = i2;
    pIndices[2] = i3;

Cleanup:
    RRETURN(hr);
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::AddTriangle
//
//  Synopsis:  Add a triangle using given three points to the list
//
//-----------------------------------------------------------------------------
template <class TVertex>
HRESULT
CHwTVertexBuffer<TVertex>::AddTriangle(
    __in_ecount(1) const PointXYA &v0,
    __in_ecount(1) const PointXYA &v1,
    __in_ecount(1) const PointXYA &v2)
{
    let hr: HRESULT = S_OK;
    
    TVertex *pVertices;
    hr = AddNonIndexedTriListVertices(3,&pVertices);

    if (hr == E_OUTOFMEMORY)
    {
        DebugBreak ();
    }
    IFC(hr);
    
    pVertices[0].ptPt.X = v0.x;
    pVertices[0].ptPt.Y = v0.y;
    pVertices[0].Diffuse = reinterpret_cast<const DWORD &>(v0.a);
    pVertices[1].ptPt.X = v1.x;
    pVertices[1].ptPt.Y = v1.y;
    pVertices[1].Diffuse = reinterpret_cast<const DWORD &>(v1.a);
    pVertices[2].ptPt.X = v2.x;
    pVertices[2].ptPt.Y = v2.y;
    pVertices[2].Diffuse = reinterpret_cast<const DWORD &>(v2.a);

Cleanup:
    RRETURN(hr);
}
*/

impl CHwVertexBuffer<'_> {
//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::AddLine
//
//  Synopsis:  Add a nominal width line using given two points to the list
//
//-----------------------------------------------------------------------------
fn AddLine(&mut self,
    v0: &PointXYA,
    v1: &PointXYA
    ) -> HRESULT
{
    type TVertex = CD3DVertexXYZDUV2;
    let hr = S_OK;

    let pVertices: &mut [TVertex];
    let mut rgScratchVertices: [TVertex; 2] = Default::default();

    assert!(!(v0.y != v1.y));
    
    let fUseTriangles = /*(v0.y < m_pBuilder->GetViewportTop() + 1) ||*/ FORCE_TRIANGLES;

    //if (fUseTriangles)
    //{
        pVertices = &mut rgScratchVertices;
    //}
    //else
    //{
        //IFC!(AddLineListVertices(2, &pVertices));
    //}
    
    pVertices[0].x = v0.x;
    pVertices[0].y = v0.y;
    pVertices[0].coverage = v0.a;
    pVertices[1].x = v1.x;
    pVertices[1].y = v1.y;
    pVertices[1].coverage = v1.a;

    if (fUseTriangles)
    {
        IFC!(self.AddLineAsTriangleList(&pVertices[0],&pVertices[1]));
    }
    
    RRETURN!(hr);
}
}
/* 
//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::AddTriListVertices
//
//  Synopsis:  Reserve space for consecutive vertices and return start index
//

template <class TVertex>
MIL_FORCEINLINE
HRESULT
CHwTVertexBuffer<TVertex>::AddTriListVertices(
    UINT uDelta,
    __deref_ecount(uDelta) TVertex **ppVertices,
    __out_ecount(1) WORD *pwIndexStart
    )
{
    HRESULT hr = S_OK;

    Assert(ppVertices);

    UINT uCount = static_cast<UINT>(m_rgVerticesTriList.GetCount());
    if (uCount > SHRT_MAX)
    {
        IFC(WGXERR_INVALIDPARAMETER);
    }
    UINT newCount;
    newCount = uDelta + uCount;

    if (newCount > SHRT_MAX)
    {
        IFC(m_pBuilder->FlushReset());
        uCount = 0;
        newCount = uDelta;
    }

    if (newCount > m_rgVerticesTriList.GetCapacity())
    {
        IFC(m_rgVerticesTriList.ReserveSpace(uDelta));
    }

    m_rgVerticesTriList.SetCount(newCount);
    *pwIndexStart = static_cast<WORD>(uCount);
    *ppVertices = &m_rgVerticesTriList[uCount];

  Cleanup:
    RRETURN(hr);
}
*/

impl<TVertex: Clone + Default> CHwTVertexBuffer<'_, TVertex> {

fn AddTriVertices(&mut self, v0: TVertex, v1: TVertex, v2: TVertex) {
    if let Some(output_buffer) = &mut self.m_rgVerticesBuffer {
        let offset = self.m_rgVerticesBufferOffset;
        if offset + 3 <= output_buffer.len() {
            output_buffer[offset] = v0;
            output_buffer[offset + 1] = v1;
            output_buffer[offset + 2] = v2;
        }
        self.m_rgVerticesBufferOffset = offset + 3;
    } else {
        self.m_rgVerticesTriList.reserve(3);
        self.m_rgVerticesTriList.push(v0);
        self.m_rgVerticesTriList.push(v1);
        self.m_rgVerticesTriList.push(v2);
    }
}

fn AddTrapezoidVertices(&mut self, v0: TVertex, v1: TVertex, v2: TVertex, v3: TVertex) {
    if let Some(output_buffer) = &mut self.m_rgVerticesBuffer {
        let offset = self.m_rgVerticesBufferOffset;
        if offset + 6 <= output_buffer.len() {
            output_buffer[offset] = v0;
            output_buffer[offset + 1] = v1.clone();
            output_buffer[offset + 2] = v2.clone();

            output_buffer[offset + 3] = v1;
            output_buffer[offset + 4] = v2;
            output_buffer[offset + 5] = v3;
        }
        self.m_rgVerticesBufferOffset = offset + 6;
    } else {
        self.m_rgVerticesTriList.reserve(6);

        self.m_rgVerticesTriList.push(v0);
        self.m_rgVerticesTriList.push(v1.clone());
        self.m_rgVerticesTriList.push(v2.clone());

        self.m_rgVerticesTriList.push(v1);
        self.m_rgVerticesTriList.push(v2);
        self.m_rgVerticesTriList.push(v3);
    }
}

fn AddedNonLineSegment(&mut self) {
    #[cfg(debug_assertions)]
    {
        self.m_fDbgNonLineSegmentTriangleStrip = true;
    }
}

}

/* 
//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::AddNonIndexedTriListVertices
//
//  Synopsis:  Reserve space for triangle list vertices.
//

template <class TVertex>
MIL_FORCEINLINE
HRESULT
CHwTVertexBuffer<TVertex>::AddNonIndexedTriListVertices(
    UINT uCount,
    __deref_ecount(uCount) TVertex **ppVertices
    )
{
    HRESULT hr = S_OK;

    UINT Count = static_cast<UINT>(m_rgVerticesNonIndexedTriList.GetCount());
    UINT newCount = Count + uCount;

    if (newCount > m_rgVerticesNonIndexedTriList.GetCapacity())
    {
        IFC(m_rgVerticesNonIndexedTriList.ReserveSpace(uCount));
    }

    m_rgVerticesNonIndexedTriList.SetCount(newCount);
    *ppVertices = &m_rgVerticesNonIndexedTriList[Count];

Cleanup:
    RRETURN(hr);
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::AddLineListVertices
//
//  Synopsis:  Reserve space for consecutive vertices
//

template <class TVertex>
MIL_FORCEINLINE
HRESULT
CHwTVertexBuffer<TVertex>::AddLineListVertices(
    UINT uCount,
    __deref_ecount(uCount) TVertex **ppVertices
    )
{
    HRESULT hr = S_OK;

    Assert(ppVertices);

    UINT Count = static_cast<UINT>(m_rgVerticesLineList.GetCount());
    UINT newCount = Count + uCount;

    if (newCount > m_rgVerticesLineList.GetCapacity())
    {
        IFC(m_rgVerticesLineList.ReserveSpace(uCount));
    }

    m_rgVerticesLineList.SetCount(newCount);
    *ppVertices = &m_rgVerticesLineList[Count];

Cleanup:
    RRETURN(hr);
}

//+----------------------------------------------------------------------------
//
//  Class:     CHwVertexBuffer::Builder
//
//-----------------------------------------------------------------------------

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::GetOutVertexFormat
//
//  Synopsis:  Return MIL vertex format covered by specific builders
//
//-----------------------------------------------------------------------------

template <>
MilVertexFormat
CHwTVertexBuffer<CD3DVertexXYZDUV2>::Builder::GetOutVertexFormat()
{
    return (MILVFAttrXYZ | MILVFAttrDiffuse | MILVFAttrUV2);
}

template <>
MilVertexFormat
CHwTVertexBuffer<CD3DVertexXYZDUV8>::Builder::GetOutVertexFormat()
{
    return (MILVFAttrXYZ | MILVFAttrDiffuse | MILVFAttrUV8);
}

template <>
MilVertexFormat
CHwTVertexBuffer<CD3DVertexXYZDUV6>::Builder::GetOutVertexFormat()
{
    return (MILVFAttrXYZ | MILVFAttrDiffuse | MILVFAttrUV6);
}

template <>
MilVertexFormat
CHwTVertexBuffer<CD3DVertexXYZNDSUV4>::Builder::GetOutVertexFormat()
{
    return (MILVFAttrXYZ |
            MILVFAttrNormal |
            MILVFAttrDiffuse |
            MILVFAttrSpecular |
            MILVFAttrUV4);
}


//+----------------------------------------------------------------------------
//
//  Member:    CHwVertexBuffer::Builder::Create
//
//  Synopsis:  Choose the appropriate final vertex format and instantiate the
//             matching vertex builder
//
*/
pub type CHwVertexBufferBuilder<'y, 'z> = CHwTVertexBufferBuilder<'y, 'z, OutputVertex>;
impl<'y, 'z> CHwVertexBufferBuilder<'y, 'z> {
pub fn Create(
     vfIn: MilVertexFormat,
     vfOut: MilVertexFormat,
     mvfaAntiAliasScaleLocation: MilVertexFormatAttribute,
    pVertexBuffer: &'y mut CHwVertexBuffer<'z>,
    /*pBufferDispenser: &CBufferDispenser*/
    ) -> CHwVertexBufferBuilder<'y, 'z>
{
    CHwVertexBufferBuilder::CreateTemplate(pVertexBuffer, vfIn, vfOut, mvfaAntiAliasScaleLocation)
    //let hr: HRESULT = S_OK;

    //assert!(ppVertexBufferBuilder);

    //*ppVertexBufferBuilder = None;
/* 
    if (!(vfOut & ~CHwTVertexBuffer<CD3DVertexXYZDUV2>::Builder::GetOutVertexFormat()))
    {
        CHwTVertexBuffer<CD3DVertexXYZDUV2> *pVB = pDevice->GetVB_XYZDUV2();
        CHwTVertexBuffer<CD3DVertexXYZDUV2>::Builder *pVBB = NULL;

        IFC(CHwTVertexBuffer<CD3DVertexXYZDUV2>::Builder::Create(
            pVB,
            vfIn,
            vfOut,
            mvfaAntiAliasScaleLocation,
            pBufferDispenser,
            &pVBB
            ));
        
        *ppVertexBufferBuilder = pVBB;
    }
    else if (!(vfOut & ~CHwTVertexBuffer<CD3DVertexXYZDUV8>::Builder::GetOutVertexFormat()))
    {
        CHwTVertexBuffer<CD3DVertexXYZDUV8> *pVB = pDevice->GetVB_XYZRHWDUV8();
        CHwTVertexBuffer<CD3DVertexXYZDUV8>::Builder *pVBB = NULL;

        IFC(CHwTVertexBuffer<CD3DVertexXYZDUV8>::Builder::Create(
            pVB,
            vfIn,
            vfOut,
            mvfaAntiAliasScaleLocation,
            pBufferDispenser,
            &pVBB
            ));

        *ppVertexBufferBuilder = pVBB;
    }
    else
    {
        // NOTE-2004/03/22-chrisra Adding another vertexbuffer type requires updating enum
        //
        // If we add another buffer builder type kMaxVertexBuilderSize enum in hwvertexbuffer.h file
        // needs to be updated to reflect possible changes to the maximum size of buffer builders.
        //
        IFC(E_NOTIMPL);
    }

    // Store the pipeline, if any, which this VBB can use to spill the vertex buffer to if it
    // overflows.
    (**ppVertexBufferBuilder).m_pPipelineNoRef = pPipeline;
    (**ppVertexBufferBuilder).m_pDeviceNoRef = pDevice;


Cleanup:
    RRETURN(hr);*/
    //hr
}
    /*fn AreWafffling(&self) -> bool {
        false
    }*/

        // Helpful m_rcOutsideBounds casts.
        fn OutsideLeft(&self) -> f32  { return self.m_rcOutsideBounds.left as f32; }
        fn OutsideRight(&self) -> f32 { return self.m_rcOutsideBounds.right as f32; }
        fn OutsideTop(&self) -> f32 { return self.m_rcOutsideBounds.top as f32; }
        fn OutsideBottom(&self) -> f32 { return self.m_rcOutsideBounds.bottom as f32; }
}

//+----------------------------------------------------------------------------
//
//  Class:     THwTVertexMappings<class TVertex>
//
//-----------------------------------------------------------------------------

//+----------------------------------------------------------------------------
//
//  Member:    THwTVertexMappings<TVertex>::THwTVertexMappings
//
//  Synopsis:  ctor
//
//-----------------------------------------------------------------------------
/* 
template <class TVertex>
CHwTVertexMappings<TVertex>::CHwTVertexMappings()
 :
    m_mvfMapped(MILVFAttrNone)
{
    for (int i = 0; i < ARRAY_SIZE(m_rgWaffleMode); ++i)
    {
        m_rgWaffleMode[i] = WaffleModeNone;
    }

    m_matPos2DTransform.SetIdentity();
}


//+----------------------------------------------------------------------------
//
//  Member:    THwTVertexMappings<TVertex>::SetPositionTransform
//
//  Synopsis:  Sets the position transform that needs to be applied.
//
//-----------------------------------------------------------------------------
template <class TVertex>
void 
CHwTVertexMappings<TVertex>::SetPositionTransform(
    __in_ecount(1) const MILMatrix3x2 &matPositionTransform
    )
{
    m_matPos2DTransform = matPositionTransform;
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexMappings<TVertex>::SetConstantMapping
//
//  Synopsis:  Remember the static color for the given vertex field
//

template <class TVertex>
HRESULT
CHwTVertexMappings<TVertex>::SetConstantMapping(
    MilVertexFormatAttribute mvfaLocation,
    __in_ecount(1) const CHwConstantColorSource *pConstCS
    )
{
    HRESULT hr = S_OK;

    Assert(!(m_mvfMapped & mvfaLocation));
    pConstCS->GetColor(m_colorStatic);
    m_mvfMapped |= mvfaLocation;    // Remember this field has been mapped

    RRETURN(hr);
}

//+----------------------------------------------------------------------------
//
//  Function:  GetMILVFAttributeOfTextureCoord
//
//  Synopsis:  Compute MilVertexFormatAttribute for a texture coordinate index
//

MIL_FORCEINLINE
MilVertexFormat
GetMILVFAttributeOfTextureCoord(
    DWORD dwCoordIndex
    )
{
    return MILVFAttrUV1 << dwCoordIndex;
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexMappings<TVertex>::PointToUV
//
//  Synopsis:  Helper function to populate the texture coordinates at the given
//             index using the given point
//

template <class TVertex>
MIL_FORCEINLINE void
CHwTVertexMappings<TVertex>::PointToUV(
    __in_ecount(1) const MilPoint2F &ptIn,
    __bound UINT uIndex,
    __out_ecount(1) TVertex *pvOut
    )
{
    m_rgmatPointToUV[uIndex].TransformPoint(
        &pvOut->ptTx[uIndex],
        ptIn.X,
        ptIn.Y
        );
}





//+----------------------------------------------------------------------------
//
//  Class:     CHwTVertexBuffer<TVertex>::Builder
//
//-----------------------------------------------------------------------------


*/

impl<'y, 'z, TVertex: Default> CHwTVertexBufferBuilder<'y, 'z, TVertex> {

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::Create
//
//  Synopsis:  Instantiate a specific type of vertex builder
//

fn CreateTemplate(
     pVertexBuffer: &'y mut CHwTVertexBuffer<'z, TVertex>,
     mvfIn: MilVertexFormat,
     mvfOut: MilVertexFormat,
     mvfaAntiAliasScaleLocation: MilVertexFormatAttribute,
     /*pBufferDispenser: __inout_ecount(1) CBufferDispenser *,*/
    ) -> Self
{



    let mut pVertexBufferBuilder = CHwTVertexBufferBuilder::<TVertex>::new(pVertexBuffer);

    IFC!(pVertexBufferBuilder.SetupConverter(
        mvfIn,
        mvfOut,
        mvfaAntiAliasScaleLocation
        ));

    return pVertexBufferBuilder;
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::Builder
//
//  Synopsis:  ctor
//
//-----------------------------------------------------------------------------

fn new(pVertexBuffer: &'y mut CHwTVertexBuffer<'z, TVertex>) -> Self
{
    Self {
    m_pVB: pVertexBuffer,


    //m_rgoPrecomputedTriListVertices: NULL(),
    //m_cPrecomputedTriListVertices: 0,

    //m_rguPrecomputedTriListIndices: NULL(),
    //m_cPrecomputedTriListIndices: 0,

    // These two track the Y extent of the shape this builder is producing.
    m_rCurStratumTop: f32::MAX,
    m_rCurStratumBottom:  -f32::MAX,
    m_fNeedOutsideGeometry: false,
    m_fNeedInsideGeometry: true,

    m_rLastTrapezoidRight: -f32::MAX,
    m_rLastTrapezoidTopRight: -f32::MAX,
    m_rLastTrapezoidBottomRight: -f32::MAX,

    m_fHasFlushed: false,
    //m_map: Default::default(),
    m_rcOutsideBounds: Default::default(),
        #[cfg(debug_assertions)]
        m_mvfDbgOut: MilVertexFormatAttribute::MILVFAttrNone as MilVertexFormat,
        m_mvfIn: MilVertexFormatAttribute::MILVFAttrNone as MilVertexFormat,
        m_mvfGenerated: MilVertexFormatAttribute::MILVFAttrNone  as MilVertexFormat,
        m_mvfaAntiAliasScaleLocation: MilVertexFormatAttribute::MILVFAttrNone,
    }
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::SetupConverter
//
//  Synopsis:  Choose the appropriate conversion method
//

fn SetupConverter(&mut self,
     mvfIn: MilVertexFormat,
     mvfOut: MilVertexFormat,
     mvfaAntiAliasScaleLocation: MilVertexFormatAttribute,
     ) -> HRESULT
{
    let hr = S_OK;

    self.m_mvfIn = mvfIn;

    #[cfg(debug_assertions)]
    {
    self.m_mvfDbgOut = mvfOut;
    }

    self.m_mvfGenerated = mvfOut & !self.m_mvfIn;
    self.m_mvfaAntiAliasScaleLocation = mvfaAntiAliasScaleLocation;

    assert!((self.m_mvfGenerated & MilVertexFormatAttribute::MILVFAttrXY as MilVertexFormat) == 0);

    RRETURN!(hr);
}
}
/* 

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::SetTransformMapping
//
//  Synopsis:  Delegate mapping sets to CHwTVertexMappings
//
//-----------------------------------------------------------------------------

template <class TVertex>
void
CHwTVertexBuffer<TVertex>::Builder::SetTransformMapping(
    __in_ecount(1) const MILMatrix3x2 &mat2DPositionTransform
    )
{
    m_map.SetPositionTransform(mat2DPositionTransform);
}
                                                                    
template <class TVertex>
HRESULT
CHwTVertexBuffer<TVertex>::Builder::SetConstantMapping(
    MilVertexFormatAttribute mvfaLocation,
    __in_ecount(1) const CHwConstantColorSource *pConstCS
    )
{
    HRESULT hr = S_OK;

    IFC(m_map.SetConstantMapping(mvfaLocation, pConstCS));

Cleanup:
    RRETURN(hr);
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::FinalizeMappings
//
//  Synopsis:  Complete setup of vertex mappings
//

template <class TVertex>
HRESULT
CHwTVertexBuffer<TVertex>::Builder::FinalizeMappings(
    )
{
    HRESULT hr = S_OK;

    //
    // Set default Z if required.
    //

    if (m_mvfGenerated & MILVFAttrZ)
    {
        if (!(m_map.m_mvfMapped & MILVFAttrZ))
        {
            m_map.m_vStatic.Z = 0.5f;
        }
    }

    //
    // If AA falloff is not going to scale the diffuse color and it is
    // generated then see if the color is constant such that we can do any
    // complex conversions just once here instead of in every iteration of the
    // expansion loop.  If AA falloff is going to scale the diffuse color then
    // we can still optimize for the falloff = 1.0 case by precomputing that
    // color now and checking for 1.0 during generation.  Such a precomputation
    // has shown significant to performance.
    //

    if (m_mvfGenerated & MILVFAttrDiffuse)
    {
        if (m_map.m_mvfMapped & MILVFAttrDiffuse)
        {

            // Assumes diffuse color is constant
            m_map.m_vStatic.Diffuse =
                Convert_MilColorF_scRGB_To_Premultiplied_MilColorB_sRGB(&m_map.m_colorStatic);
        }
        else
        {
            // Set default Diffuse value: White
            m_map.m_vStatic.Diffuse = MIL_COLOR(0xFF,0xFF,0xFF,0xFF);
        }
    }

    RRETURN(hr);
}*/
impl<TVertex> CHwTVertexBufferBuilder<'_, '_, TVertex> {

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::SetOutsideBounds
//
//
//  Synopsis:  Enables rendering geometry for areas outside the shape but
//             within the bounds.  These areas will be created with
//             zero alpha.
//

pub fn SetOutsideBounds(&mut self,
    prcOutsideBounds: Option<&CMILSurfaceRect>,
    fNeedInside: bool,
    )
{
    // Waffling and outside bounds is not currently implemented.  It's
    // not difficult to do but currently there is no need.
    //assert!(!(self.AreWaffling() && self.prcOutsideBounds));

    if let Some(prcOutsideBounds) = prcOutsideBounds
    {
        self.m_rcOutsideBounds = prcOutsideBounds.clone();
        self.m_fNeedOutsideGeometry = true;
        self.m_fNeedInsideGeometry = fNeedInside;
    }
    else
    {
        self.m_fNeedOutsideGeometry = false;
        self.m_fNeedInsideGeometry = true;
    }
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::BeginBuilding
//
//  Synopsis:  Prepare for a new primitive by resetting the vertex buffer
//
pub fn BeginBuilding(&mut self,
    ) -> HRESULT
{
    
    let hr: HRESULT = S_OK;

    self.m_fHasFlushed = false;
    self.m_pVB.Reset(/*self*/);

    RRETURN!(hr);
}
}
impl IGeometrySink for CHwVertexBufferBuilder<'_, '_> {

    fn AddTrapezoid(&mut self,
        rPixelYTop: f32,              // In: y coordinate of top of trapezoid
        rPixelXTopLeft: f32,          // In: x coordinate for top left
        rPixelXTopRight: f32,         // In: x coordinate for top right
        rPixelYBottom: f32,           // In: y coordinate of bottom of trapezoid
        rPixelXBottomLeft: f32,       // In: x coordinate for bottom left
        rPixelXBottomRight: f32,      // In: x coordinate for bottom right
        rPixelXLeftDelta: f32,        // In: trapezoid expand radius for left edge
        rPixelXRightDelta: f32        // In: trapezoid expand radius for right edge
        ) -> HRESULT
    {
        let hr = S_OK;
    
        if (/*self.AreWaffling()*/ false)
        {
            /*IFC(AddTrapezoidWaffle(
                    rPixelYTop,
                    rPixelXTopLeft,
                    rPixelXTopRight,
                    rPixelYBottom,
                    rPixelXBottomLeft,
                    rPixelXBottomRight,
                    rPixelXLeftDelta,
                    rPixelXRightDelta));*/
        }
        else
        {
            IFC!(self.AddTrapezoidStandard(
                    rPixelYTop,
                    rPixelXTopLeft,
                    rPixelXTopRight,
                    rPixelYBottom,
                    rPixelXBottomLeft,
                    rPixelXBottomRight,
                    rPixelXLeftDelta,
                    rPixelXRightDelta));
        }
    
    //Cleanup:
        RRETURN!(hr);
    }
    

    fn IsEmpty(&self) -> bool {
        self.m_pVB.IsEmpty()
    }

/* 

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddVertex
//
//  Synopsis:  Add a vertex to the vertex buffer
//
//             Remember just the given vertex information now and convert later
//             in a single, more optimal pass.
//

template <class TVertex>
HRESULT
CHwTVertexBuffer<TVertex>::Builder::AddVertex(
    __in_ecount(1) const MilPoint2F &ptPosition,
        // Vertex coordinates
    __out_ecount(1) WORD *pIndex
        // The index of the new vertex
    )
{
    HRESULT hr = S_OK;

    Assert(!NeedOutsideGeometry());
    Assert(m_mvfIn == MILVFAttrXY);

    TVertex *pVertex;

    IFC(m_pVB->AddTriListVertices(1, &pVertex, pIndex));

    pVertex->ptPt = ptPosition;

    //  store coverage as a DWORD instead of float

    pVertex->Diffuse = FLOAT_ONE;

Cleanup:
    RRETURN(hr);
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddIndexedVertices, IGeometrySink
//
//  Synopsis:  Add a fully computed, indexed vertex to the vertex buffer
//

template <class TVertex>
HRESULT
CHwTVertexBuffer<TVertex>::Builder::AddIndexedVertices(
    UINT cVertices,
        // In: number of vertices                                                       
    __in_bcount(cVertices*uVertexStride) const void *pVertexBufferNoRef,
        // In: vertex buffer containing the vertices                                    
    UINT uVertexStride,
        // In: size of each vertex                                                      
    MilVertexFormat mvfFormat,
        // In: format of each vertex                                                    
    UINT cIndices,
        // In: Number of indices                                                        
    __in_ecount(cIndices) const UINT *puIndexBuffer
        // In: index buffer                                                             
    )
{
    Assert(m_mvfIn & (MILVFAttrXYZ | MILVFAttrDiffuse | MILVFAttrUV2));
    Assert(mvfFormat == (MILVFAttrXYZ | MILVFAttrDiffuse | MILVFAttrUV2));

    Assert(uVertexStride == sizeof(TVertex));

    m_rgoPrecomputedTriListVertices = reinterpret_cast<const TVertex *>(pVertexBufferNoRef);
    m_cPrecomputedTriListVertices = cVertices;

    m_rguPrecomputedTriListIndices = puIndexBuffer;
    m_cPrecomputedTriListIndices = cIndices;

    return S_OK;
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddTriangle
//
//  Synopsis:  Add a triangle to the vertex buffer
//

template <class TVertex>
HRESULT
CHwTVertexBuffer<TVertex>::Builder::AddTriangle(
    DWORD i1,                    // In: Index of triangle's first vertex
    DWORD i2,                    // In: Index of triangle's second vertex
    DWORD i3                     // In: Index of triangle's third vertex
    )
{
    HRESULT hr = S_OK;

    Assert(!NeedOutsideGeometry());

    if (AreWaffling())
    {
        TVertex *pVertex;
        UINT uNumVertices;
        m_pVB->GetTriListVertices(&pVertex, &uNumVertices);

        Assert(i1 < uNumVertices);
        Assert(i2 < uNumVertices);
        Assert(i3 < uNumVertices);

        PointXYA rgPoints[3];
        rgPoints[0].x = pVertex[i1].ptPt.X;
        rgPoints[0].y = pVertex[i1].ptPt.Y;
        rgPoints[0].a = 1;
        rgPoints[1].x = pVertex[i2].ptPt.X;
        rgPoints[1].y = pVertex[i2].ptPt.Y;
        rgPoints[1].a = 1;
        rgPoints[2].x = pVertex[i3].ptPt.X;
        rgPoints[2].y = pVertex[i3].ptPt.Y;
        rgPoints[2].a = 1;
        
        TriangleWaffler<PointXYA> wafflers[NUM_OF_VERTEX_TEXTURE_COORDS(TVertex) * 2];
        TriangleWaffler<PointXYA>::ISink *pWaffleSinkNoRef = BuildWafflePipeline(wafflers);
        IFC(pWaffleSinkNoRef->AddTriangle(rgPoints[0], rgPoints[1], rgPoints[2]));
    }
    else
    {
        IFC(m_pVB->AddTriangle(
                static_cast<WORD>(i1),
                static_cast<WORD>(i2),
                static_cast<WORD>(i3)
                ));
    }
    
Cleanup:
    RRETURN(hr);
}
*/


//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddComplexScan
//
//  Synopsis:  Add a coverage span to the vertex buffer
//
//-----------------------------------------------------------------------------
    fn AddComplexScan(&mut self,
        nPixelY: INT,
            // In: y coordinate in pixel space
            mut pIntervalSpanStart: Ref<crate::aacoverage::CCoverageInterval>
            // In: coverage segments
        ) -> HRESULT {

    let hr: HRESULT = S_OK;
    //let pVertex: *mut CD3DVertexXYZDUV2 = NULL();

    IFC!(self.PrepareStratum((nPixelY) as f32,
                  (nPixelY+1) as f32, 
                  false, /* Not a trapezoid. */ 
                  0., 0.,
                0., 0., 0., 0.));

    let rPixelY: f32;
    rPixelY = (nPixelY) as f32 + 0.5;

    //LineWaffler<PointXYA> wafflers[NUM_OF_VERTEX_TEXTURE_COORDS(TVertex) * 2];

    // Use sink for waffling & the first line fix up (aka the complicated cases.)
    //ILineSink<PointXYA> *pLineSink = NULL;
    let mut pLineSink = None;

    /*if (self.AreWaffling())
    {
        bool fWafflersUsed;
        pLineSink = BuildWafflePipeline(wafflers, OUT fWafflersUsed);
        if (!fWafflersUsed)
        {
            pLineSink = NULL;
        }
    }*/
    
    // Use triangles instead of lines, for lines too close to the top of the viewport
    // because lines are clipped (before rasterization) against a viewport that only
    // includes half of the top pixel row.  Waffling will take care of this separately.
    if (/*pLineSink.is_none() && rPixelY < self.GetViewportTop() + 1 ||*/ FORCE_TRIANGLES)
    {
        pLineSink = Some(&mut self.m_pVB);
    }

    //
    // Output all segments if creating outside geometry, otherwise only output segments
    // with non-zero coverage.
    //

    if (pLineSink.is_none())
    {
        /* 
        UINT nSegmentCount = 0;

        for (const CCoverageInterval *pIntervalSpanTemp = pIntervalSpanStart;
             pIntervalSpanTemp->m_nPixelX != INT_MAX;
             pIntervalSpanTemp = pIntervalSpanTemp->m_pNext
             )
        {
            if (NeedCoverageGeometry(pIntervalSpanTemp->m_nCoverage))
            {
                ++nSegmentCount;
            }
        }

        //
        // Add vertices
        //
        if (nSegmentCount)
        {
            IFC(m_pVB->AddLineListVertices(nSegmentCount*2, &pVertex));
        }*/
    }

    //
    // Having allocated space (if not using sink), now let's actually output the vertices.
    //

    while ((*pIntervalSpanStart).m_nPixelX.get() != INT::MAX)
    {
        assert!(!(*pIntervalSpanStart).m_pNext.get().is_null());

        //
        // Output line list segments
        //
        // Note that line segments light pixels by going through the the
        // "diamond" interior of a pixel.  While we could accomplish this
        // by going from left edge to right edge of pixel, D3D10 uses the
        // convention that the LASTPIXEL is never lit.  We respect that now
        // by setting D3DRS_LASTPIXEL to FALSE and use line segments that
        // start in center of first pixel and end in center of one pixel
        // beyond last.
        //
        // Since our top left corner is integer, we add 0.5 to get to the
        // pixel center.
        //
        if (self.NeedCoverageGeometry((*pIntervalSpanStart).m_nCoverage.get()))
        {
            let rCoverage: f32 = ((*pIntervalSpanStart).m_nCoverage.get() as f32)/(c_nShiftSizeSquared as f32);
            
            let mut iBegin: LONG = (*pIntervalSpanStart).m_nPixelX.get();
            let mut iEnd: LONG = (*(*pIntervalSpanStart).m_pNext.get()).m_nPixelX.get();
            if (self.NeedOutsideGeometry())
            {
                // Intersect the interval with the outside bounds to create
                // start and stop lines.  The scan begins (ends) with an
                // interval starting (ending) at -inf (+inf).

                // The given geometry is not guaranteed to be within m_rcOutsideBounds but
                // the additional inner min and max (in that order) produce empty spans
                // for intervals not intersecting m_rcOutsideBounds.
                //
                // We could cull here but that should really be done by the geometry
                // generator.

                iBegin = iBegin.max(iEnd.min(self.m_rcOutsideBounds.left));
                iEnd = iEnd.min(iBegin.max(self.m_rcOutsideBounds.right));
            }
            let rPixelXBegin: f32= (iBegin as f32) + 0.5;
            let rPixelXEnd: f32 = (iEnd as f32) + 0.5;

            //
            // Output line (linelist or tristrip) for a pixel
            //

            //if let Some(pLineSink) = pLineSink 
            {
                let mut v0: PointXYA = Default::default(); let mut v1: PointXYA = Default::default();
                v0.x = rPixelXBegin;
                v0.y = rPixelY;
                v0.a = rCoverage;

                v1.x = rPixelXEnd;
                v1.y = rPixelY;
                v1.a = rCoverage;

                IFC!(self.m_pVB.AddLine(&v0,&v1));
            }
            //else
            {
                /* 
                let dwDiffuse = ReinterpretFloatAsDWORD(rCoverage);

                pVertex[0].ptPt.X = rPixelXBegin;
                pVertex[0].ptPt.Y = rPixelY;
                pVertex[0].Diffuse = dwDiffuse;

                pVertex[1].ptPt.X = rPixelXEnd;
                pVertex[1].ptPt.Y = rPixelY;
                pVertex[1].Diffuse = dwDiffuse;

                // Advance output vertex pointer
                pVertex += 2;*/
            }
        }

        //
        // Advance coverage buffer
        //

        pIntervalSpanStart = (*pIntervalSpanStart).m_pNext.get();
    }


//Cleanup:
    RRETURN!(hr);

}
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddLineAsTriangleList
//
//  Synopsis:  Adds a horizontal line as a triangle list to work around
//             issue in D3D9 where horizontal lines with y = 0 may not render.
//
//              Line clipping in D3D9
//             This behavior will change in D3D10 and this work-around will no
//             longer be needed.  (Pixel center conventions will also change.)
//              
//-----------------------------------------------------------------------------
impl CHwVertexBuffer<'_> {
    fn AddLineAsTriangleList(&mut self,
    pBegin: &CD3DVertexXYZDUV2, // Begin
    pEnd: &CD3DVertexXYZDUV2    // End
    ) -> HRESULT
{
    let hr = S_OK;

    // Collect pertinent data from vertices.
    debug_assert!(pBegin.y == pEnd.y);
    debug_assert!(pBegin.coverage == pEnd.coverage);

    // Offset begin and end X left by 0.5 because the line starts on the first
    // pixel center and ends on the center of the pixel after the line segment.
    let x0 = pBegin.x - 0.5;
    let x1 = pEnd.x - 0.5;
    let y = pBegin.y;
    let dwDiffuse = pBegin.coverage;

    //
    // Add the vertices
    //

    // OpenGL doesn't specify how vertex positions are converted to fixed point prior to rasterization. On macOS, with AMD GPUs,
    // the GPU appears to truncate to fixed point instead of rounding. This behaviour is controlled by PA_SU_VTX_CNTL
    // register. To handle this we'll add a 1./512. subpixel bias to the center vertex to cause the coordinates to round instead
    // of truncate.
    //
    // D3D11 requires the fixed point integer result to be within 0.6ULP which implicitly disallows the truncate behaviour above.
    // This means that D2D doesn't need to deal with this problem.
    let subpixel_bias = self.subpixel_bias;


    // Use a single triangle to cover the entire line
    self.AddTriVertices(
        OutputVertex{ x: x0, y: y - 0.5, coverage: dwDiffuse },
        OutputVertex{ x: x0, y: y + 0.5, coverage: dwDiffuse },
        OutputVertex{ x: x1, y: y + subpixel_bias, coverage: dwDiffuse },
    );

    self.AddedNonLineSegment();

  //Cleanup:
    RRETURN!(hr);
}
}

/* 
//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddParallelogram
//
//  Synopsis:  This function adds the coordinates of a parallelogram to the vertex strip buffer. 
//
//  Parameter: rgPosition contains four coordinates of the parallelogram. Coordinates should have 
//              a winding order
//
//-----------------------------------------------------------------------------
template <class TVertex>
HRESULT
CHwTVertexBuffer<TVertex>::Builder::AddParallelogram(
        __in_ecount(4)  const MilPoint2F *rgPosition
        )
{
    HRESULT hr = S_OK;

    if (AreWaffling())
    {
        PointXYA rgPoints[4];
        for (int i = 0; i < 4; ++i)
        {
            rgPoints[i].x = rgPosition[i].X;
            rgPoints[i].y = rgPosition[i].Y;
            rgPoints[i].a = 1;
        }
        TriangleWaffler<PointXYA> wafflers[NUM_OF_VERTEX_TEXTURE_COORDS(TVertex) * 2];
        TriangleWaffler<PointXYA>::ISink *pWaffleSinkNoRef = BuildWafflePipeline(wafflers);
        IFC(pWaffleSinkNoRef->AddTriangle(rgPoints[0], rgPoints[1], rgPoints[3]));
        IFC(pWaffleSinkNoRef->AddTriangle(rgPoints[3], rgPoints[1], rgPoints[2]));
    }
    else
    {
        TVertex *pVertex;
  
        //
        // Add the vertices
        //

        IFC(m_pVB->AddTriStripVertices(6, &pVertex));

        //
        // Duplicate the first vertex. This creates 2 degenerate triangles: one connecting
        // the previous rect to this one and another between vertices 0 and 1.
        //

        pVertex[0].ptPt = rgPosition[0];
        pVertex[0].Diffuse = FLOAT_ONE;

        pVertex[1].ptPt = rgPosition[0];
        pVertex[1].Diffuse = FLOAT_ONE;
    
        pVertex[2].ptPt = rgPosition[1];
        pVertex[2].Diffuse = FLOAT_ONE;

        pVertex[3].ptPt = rgPosition[3];
        pVertex[3].Diffuse = FLOAT_ONE;

        pVertex[4].ptPt = rgPosition[2];
        pVertex[4].Diffuse = FLOAT_ONE;

        //
        // Duplicate the last vertex. This creates 2 degenerate triangles: one
        // between vertices 4 and 5 and one connecting this Rect to the
        // next one.
        //

        pVertex[5].ptPt = rgPosition[2];
        pVertex[5].Diffuse = FLOAT_ONE;
    }
        
  Cleanup:
    RRETURN(hr);
}
    
//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::BuildWafflePipeline<TWaffler>
//
//  Synopsis:  Builds a pipeline of wafflers into the provided array of wafflers.
//             And returns a pointer (not to be deleted) to the input sink
//             of the waffle pipeline.
//             the final result is sinked int m_pVB.
//
//-----------------------------------------------------------------------------

template<class TVertex>
template<class TWaffler>
__out_ecount(1) typename TWaffler::ISink *
CHwTVertexBuffer<TVertex>::Builder::BuildWafflePipeline(
        __out_xcount(NUM_OF_VERTEX_TEXTURE_COORDS(TVertex) * 2) TWaffler *wafflers,
        __out_ecount(1) bool &fWafflersUsed
    ) const
{
    UINT count = 0;

    for (int i = 0; i < NUM_OF_VERTEX_TEXTURE_COORDS(TVertex); ++i)
    {
        if (m_map.m_rgWaffleMode[i] != 0)
        {
            const MILMatrix3x2 &pMatWaffle = m_map.m_rgmatPointToUV[i];

            // Each column ([a,b,c] transpose) of this matrix specifies a waffler that
            // partitions the plane into regions between the lines:
            //                    ax + by + c = k
            // for every integer k.
            //
            // If this partition width is substantially less than a pixel we have
            // serious problems with waffling generating too many triangles for
            // doubtful visual effect so we don't perform a waffling with width less
            // than c_rMinWaffleWidthPixels.  So we need to know the width of the partition
            // regions:
            //
            // Changing c just translates the partition so let's assume c = 0.
            // The line ax + by = 0 goes through the origin and the line ax + by
            // = 1 is adjacent to it in the partition.  The distance between
            // these lines is also the distance from ax + by = 1 to the origin.
            // Using Lagrange multipliers we can determine that this distance
            // is
            //                     1/sqrt(a*a+b*b).
            // We want to avoid waffling if this is less than c_rMinWaffleWidthPixels
            // or equivalently:
            //   1/sqrt(a*a+b*b) < c_rMinWaffleWidthPixels
            //     sqrt(a*a+b*b) > 1/c_rMinWaffleWidthPixels
            //          a*a+b*b  > 1/(c_rMinWaffleWidthPixels*c_rMinWaffleWidthPixels)
            //          

            const float c_rMaxWaffleMagnitude = 1/(c_rMinWaffleWidthPixels*c_rMinWaffleWidthPixels);
            
            float mag0 = pMatWaffle.m_00*pMatWaffle.m_00+pMatWaffle.m_10*pMatWaffle.m_10;
            if (mag0 < c_rMaxWaffleMagnitude)
            {
                wafflers[count].Set(pMatWaffle.m_00, pMatWaffle.m_10, pMatWaffle.m_20, wafflers+count+1);
                ++count;
            }

            float mag1 = pMatWaffle.m_01*pMatWaffle.m_01+pMatWaffle.m_11*pMatWaffle.m_11;
            if (mag1 < c_rMaxWaffleMagnitude)
            {
                wafflers[count].Set(pMatWaffle.m_01, pMatWaffle.m_11, pMatWaffle.m_21, wafflers+count+1);
                ++count;
            }
        }
    }

    if (count)
    {
        fWafflersUsed = true;
        // As the last step in the chain we send the triangles to our vertex buffer.
        wafflers[count-1].SetSink(m_pVB);
        return &wafflers[0];
    }
    else
    {
        fWafflersUsed = false;
        // If we built no wafflers then sink straight into the vertex buffer.
        return m_pVB;
    }
}


//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::IsEmpty
//
//  Synopsis:  Does our VB have any triangles/lines?
//
//-----------------------------------------------------------------------------
template <class TVertex>
BOOL
CHwTVertexBuffer<TVertex>::Builder::IsEmpty()
{
    return m_pVB->IsEmpty();
}
*/
//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddTrapezoid
//
//  Synopsis:  Add a trapezoid to the vertex buffer
//
//
//      left edge       right edge
//      ___+_________________+___      <<< top edge
//     /  +  /             \  +  \
//    /  +  /               \  +  \
//   /  +  /                 \  +  \
//  /__+__/___________________\__+__\  <<< bottom edge
//    + ^^                        +
//      delta
//
impl CHwVertexBufferBuilder<'_, '_> {

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddTrapezoidStandard
//
//  Synopsis:  See AddTrapezoid.  This doesn't do waffling & uses tri strips.
//

fn AddTrapezoidStandard(&mut self,
    rPixelYTop: f32,              // In: y coordinate of top of trapezoid
    rPixelXTopLeft: f32,          // In: x coordinate for top left
    rPixelXTopRight: f32,         // In: x coordinate for top right
    rPixelYBottom: f32,           // In: y coordinate of bottom of trapezoid
    rPixelXBottomLeft: f32,       // In: x coordinate for bottom left
    rPixelXBottomRight: f32,      // In: x coordinate for bottom right
    rPixelXLeftDelta: f32,        // In: trapezoid expand radius for left edge
    rPixelXRightDelta: f32        // In: trapezoid expand radius for right edge
    ) -> HRESULT
{
    type TVertex = CD3DVertexXYZDUV2;
    let hr = S_OK;
    //TVertex *pVertex;

    IFC!(self.PrepareStratum(
        rPixelYTop,
        rPixelYBottom,
        true, /* Trapezoid */
        rPixelXTopLeft.min(rPixelXBottomLeft),
        rPixelXTopRight.max(rPixelXBottomRight),
        rPixelXTopLeft - rPixelXLeftDelta, rPixelXBottomLeft - rPixelXLeftDelta,
        rPixelXTopRight + rPixelXRightDelta, rPixelXBottomRight + rPixelXRightDelta
        ));
    
    //
    // Add the vertices
    //

	let fNeedOutsideGeometry: bool; let fNeedInsideGeometry: bool;
    fNeedOutsideGeometry = self.NeedOutsideGeometry();
    fNeedInsideGeometry = self.NeedInsideGeometry();

    //
    // Fill in the vertices
    //

    self.m_pVB.AddTrapezoidVertices(
        OutputVertex{
            x: rPixelXTopLeft - rPixelXLeftDelta,
            y: rPixelYTop,
            coverage: FLOAT_ZERO,
        },
        OutputVertex{
            x: rPixelXBottomLeft - rPixelXLeftDelta,
            y: rPixelYBottom,
            coverage: FLOAT_ZERO,
        },
        OutputVertex{
            x: rPixelXTopLeft + rPixelXLeftDelta,
            y: rPixelYTop,
            coverage: FLOAT_ONE,
        },
        OutputVertex{
            x: rPixelXBottomLeft + rPixelXLeftDelta,
            y: rPixelYBottom,
            coverage: FLOAT_ONE,
        }
    );


    if (fNeedInsideGeometry)
    {
        self.m_pVB.AddTrapezoidVertices(
            OutputVertex{
                x: rPixelXTopLeft + rPixelXLeftDelta,
                y: rPixelYTop,
                coverage: FLOAT_ONE,
            },
            OutputVertex{
                x: rPixelXBottomLeft + rPixelXLeftDelta,
                y: rPixelYBottom,
                coverage: FLOAT_ONE,
            },
            OutputVertex{
                x: rPixelXTopRight - rPixelXRightDelta,
                y: rPixelYTop,
                coverage: FLOAT_ONE,
            },
            OutputVertex{
                x: rPixelXBottomRight - rPixelXRightDelta,
                y: rPixelYBottom,
                coverage: FLOAT_ONE,
            }
        );
    }

    self.m_pVB.AddTrapezoidVertices(
        OutputVertex{
            x: rPixelXTopRight - rPixelXRightDelta,
            y: rPixelYTop,
            coverage: FLOAT_ONE,
        },
        OutputVertex{
            x: rPixelXBottomRight - rPixelXRightDelta,
            y: rPixelYBottom,
            coverage: FLOAT_ONE,
        },
        OutputVertex{
            x: rPixelXTopRight + rPixelXRightDelta,
            y: rPixelYTop,
            coverage: FLOAT_ZERO,
        },
        OutputVertex{
            x: rPixelXBottomRight + rPixelXRightDelta,
            y: rPixelYBottom,
            coverage: FLOAT_ZERO,
        }
    );

    if (!fNeedOutsideGeometry)
    {
        //
        // Duplicate the last vertex. This creates 2 degenerate triangles: one
        // between vertices 8 and 9 and one connecting this trapezoid to the
        // next one.
        //

        //pVertex.push(OutputVertex{
        //  x: rPixelXBottomRight + rPixelXRightDelta,
        //  y: rPixelYBottom,
        //  coverage: FLOAT_ZERO,
        //});
    }

    self.m_pVB.AddedNonLineSegment();

//Cleanup:
    RRETURN!(hr);
}
}
/* 
//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::AddTrapezoidWaffle
//
//  Synopsis:  See AddTrapezoid.  This adds a waffled trapezoid.
//
//-----------------------------------------------------------------------------
template <class TVertex>
HRESULT
CHwTVertexBuffer<TVertex>::Builder::AddTrapezoidWaffle(
    float rPixelYTop,              // In: y coordinate of top of trapezoid
    float rPixelXTopLeft,          // In: x coordinate for top left
    float rPixelXTopRight,         // In: x coordinate for top right
    float rPixelYBottom,           // In: y coordinate of bottom of trapezoid
    float rPixelXBottomLeft,       // In: x coordinate for bottom left
    float rPixelXBottomRight,      // In: x coordinate for bottom right
    float rPixelXLeftDelta,        // In: trapezoid expand radius for left edge
    float rPixelXRightDelta        // In: trapezoid expand radius for right edge
    )
{
    HRESULT hr = S_OK;

    // We have 2 (u & v) wafflers per texture coordinate that need waffling.
    TriangleWaffler<PointXYA> wafflers[NUM_OF_VERTEX_TEXTURE_COORDS(TVertex) * 2];
    bool fWafflersUsed = false;

    TriangleWaffler<PointXYA>::ISink *pWaffleSinkNoRef = BuildWafflePipeline(wafflers, OUT fWafflersUsed);

    PointXYA vertices[8];

    //
    // Fill in the strip vertices
    //

    // Nonstandard coverage mapping and waffling are not supported at the same time.
    Assert(!NeedOutsideGeometry());

    vertices[0].x = rPixelXTopLeft - rPixelXLeftDelta;
    vertices[0].y = rPixelYTop;
    vertices[0].a = 0;

    vertices[1].x = rPixelXBottomLeft - rPixelXLeftDelta;
    vertices[1].y = rPixelYBottom;
    vertices[1].a = 0;

    vertices[2].x = rPixelXTopLeft + rPixelXLeftDelta;
    vertices[2].y = rPixelYTop;
    vertices[2].a = 1;

    vertices[3].x = rPixelXBottomLeft + rPixelXLeftDelta;
    vertices[3].y = rPixelYBottom;
    vertices[3].a = 1;

    vertices[4].x = rPixelXTopRight - rPixelXRightDelta;
    vertices[4].y = rPixelYTop;
    vertices[4].a = 1;

    vertices[5].x = rPixelXBottomRight - rPixelXRightDelta;
    vertices[5].y = rPixelYBottom;
    vertices[5].a = 1;

    vertices[6].x = rPixelXTopRight + rPixelXRightDelta;
    vertices[6].y = rPixelYTop;
    vertices[6].a = 0;

    vertices[7].x = rPixelXBottomRight + rPixelXRightDelta;
    vertices[7].y = rPixelYBottom;
    vertices[7].a = 0;

    // Send the triangles in the strip through the waffle pipeline.
    for (int i = 0; i < 6; ++i)
    {
        IFC(pWaffleSinkNoRef->AddTriangle(vertices[i+1], vertices[i], vertices[i+2]));
    }

Cleanup:
    RRETURN(hr);
}
*/
impl CHwVertexBufferBuilder<'_, '_> {

    //+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::NeedCoverageGeometry
//
//  Synopsis:  Returns true if the coverage value needs to be rendered
//             based on NeedInsideGeometry() and NeedOutsideGeometry()
//
//             Two cases where we don't need to generate geometry:
//              1. NeedInsideGeometry is false, and coverage is c_nShiftSizeSquared.
//              2. NeedOutsideGeometry is false and coverage is 0
//
//-----------------------------------------------------------------------------
fn NeedCoverageGeometry(&self,
    nCoverage: INT
    ) -> bool
{
    return    (self.NeedInsideGeometry()  || nCoverage != c_nShiftSizeSquared)
           && (self.NeedOutsideGeometry() || nCoverage != 0);
}

    //+------------------------------------------------------------------------
    //
    //  Member:    NeedOutsideGeometry
    //
    //  Synopsis:  True if we should create geometry with zero alpha for
    //             areas outside the input geometry but within a given
    //             bounding box.
    //
    //-------------------------------------------------------------------------
    fn NeedOutsideGeometry(&self) -> bool
    {
        return self.m_fNeedOutsideGeometry;
    }

        //+------------------------------------------------------------------------
    //
    //  Member:    NeedInsideGeometry
    //
    //  Synopsis:  True if we should create geometry for areas completely
    //             withing the input geometry (i.e. alpha 1.)  Should only
    //             be false if NeedOutsideGeometry is true.
    //
    //-------------------------------------------------------------------------
    fn NeedInsideGeometry(&self) -> bool
    {
        assert!(self.m_fNeedOutsideGeometry || self.m_fNeedInsideGeometry);
        return self.m_fNeedInsideGeometry;
    }



    // Helpers that handle extra shapes in trapezoid mode.
    fn PrepareStratum(&mut self,
        rStratumTop: f32,
        rStratumBottom: f32,
        fTrapezoid: bool,
        rTrapezoidLeft: f32,
        rTrapezoidRight: f32,
        rTrapezoidTopLeft: f32, // = 0
        rTrapezoidBottomLeft: f32, // = 0
        rTrapezoidTopRight: f32, // = 0
        rTrapezoidBottomRight: f32, // = 0

        ) -> HRESULT
    {
        return if self.NeedOutsideGeometry() {
            self.PrepareStratumSlow(
                rStratumTop,
                rStratumBottom,
                fTrapezoid,
                rTrapezoidLeft,
                rTrapezoidRight,
                rTrapezoidTopLeft,
                rTrapezoidBottomLeft,
                rTrapezoidTopRight,
                rTrapezoidBottomRight
                )
     } else { S_OK };
    }

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::PrepareStratumSlow
//
//  Synopsis:  Call before producing a new stratum (complex span or trapezoid.)
//             Handles several tasks:
//               1. Producing between top of complement geometry & the 1st
//                  stratum or when a gap between strata occurs (because
//                  the geometry is not closed and has horizontal gaps.)
//                  Passing in FLT_MAX for rStratumTop and rStratumBottom
//                  Fills the gap between the last stratum and the bottom
//                  of the outside.
//               2. Begins and/or ends the triangle strip corresponding to
//                  a trapezoid row.
//               3. Updates status vars m_rCurStratumTop & m_rCurStratumBottom
//
//  Note:      Call PrepareStratum which inlines the check for NeedOutsideGeometry()
//             If NeedOutsideGeometry is false PrepareStratum() does nothing.
//             This (slow) version asserts NeedOutsideGeometry()
//
//-----------------------------------------------------------------------------
fn PrepareStratumSlow(&mut self,
    rStratumTop: f32,
    rStratumBottom: f32,
    fTrapezoid: bool,
    rTrapezoidLeft: f32,
    rTrapezoidRight: f32,
    rTrapezoidTopLeft: f32,
    rTrapezoidBottomLeft: f32,
    rTrapezoidTopRight: f32,
    rTrapezoidBottomRight: f32,
    ) -> HRESULT
{
    type TVertex = OutputVertex;
    let hr: HRESULT = S_OK;
    
    assert!(!(rStratumTop > rStratumBottom));
    assert!(self.NeedOutsideGeometry());

    // There's only once case where a stratum can go "backwards"
    // and that's when we're done building & calling from
    // EndBuildingOutside
        
    let fEndBuildingOutside: f32 = (rStratumBottom == self.OutsideBottom() &&
                                rStratumTop == self.OutsideBottom()) as i32 as f32;

    if (fEndBuildingOutside == 1.)
    {
        assert!(!fTrapezoid);
    }
    else
    {
        assert!(!(rStratumBottom < self.m_rCurStratumBottom));
    }
    
    if (   fEndBuildingOutside == 1.
        || rStratumBottom != self.m_rCurStratumBottom)
    {
        
        // New stratum starting now.  Two things to do
        //  1. Close out current trapezoid stratum if necessary.
        //  2. Begin new trapezoid stratum if necessary.
        
        if (self.m_rCurStratumTop != f32::MAX)
        {
            // we do not clip trapezoids so RIGHT boundary
            // of the stratus can be outside of m_rcOutsideBounds.
            
            let rOutsideRight: f32 = self.OutsideRight().max(self.m_rLastTrapezoidRight);

            // End current trapezoid stratum.

            self.m_pVB.AddTrapezoidVertices(
                OutputVertex{
                    x: self.m_rLastTrapezoidTopRight,
                    y: self.m_rCurStratumTop,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: self.m_rLastTrapezoidBottomRight,
                    y: self.m_rCurStratumBottom,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: rOutsideRight,
                    y: self.m_rCurStratumTop,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: rOutsideRight,
                    y: self.m_rCurStratumBottom,
                    coverage: FLOAT_ZERO,
                }
            );
        }
        // Compute the gap between where the last stratum ended and where
        // this one begins.
        let flGap: f32 = rStratumTop - self.m_rCurStratumBottom;

        if (flGap > 0.)
        {
            // The "special" case of a gap at the beginning is caught here
            // using the sentinel initial value of m_rCurStratumBottom.

            let flRectTop: f32 = if self.m_rCurStratumBottom == -f32::MAX {
                              self.OutsideTop() } else {
                              self.m_rCurStratumBottom };
            let flRectBot: f32  = (rStratumTop as f32);

            // Produce rectangular for any horizontal intervals in the
            // outside bounds that have no generated geometry.
            assert!(self.m_rCurStratumBottom != -f32::MAX || self.m_rCurStratumTop == f32::MAX);

            let outside_left = self.OutsideLeft();
            let outside_right = self.OutsideRight();
            
            // Duplicate first vertex.
            self.m_pVB.AddTrapezoidVertices(
                OutputVertex{
                    x: outside_left,
                    y: flRectTop,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: outside_left,
                    y: flRectBot,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: outside_right,
                    y: flRectTop,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: outside_right,
                    y: flRectBot,
                    coverage: FLOAT_ZERO,
                }
            );
        }

        if (fTrapezoid)
        {

            // we do not clip trapezoids so left boundary
            // of the stratus can be outside of m_rcOutsideBounds.
            
            let rOutsideLeft: f32 = self.OutsideLeft().min(rTrapezoidLeft);

            // Begin new trapezoid stratum.

            self.m_pVB.AddTrapezoidVertices(
                OutputVertex{
                    x: rOutsideLeft,
                    y: rStratumTop,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: rOutsideLeft,
                    y: rStratumBottom,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: rTrapezoidTopLeft,
                    y: rStratumTop,
                    coverage: FLOAT_ZERO,
                },
                OutputVertex{
                    x: rTrapezoidBottomLeft,
                    y: rStratumBottom,
                    coverage: FLOAT_ZERO,
                }
            );
        }
    }
    
    if (fTrapezoid)
    {
        self.m_rLastTrapezoidTopRight = rTrapezoidTopRight;
        self.m_rLastTrapezoidBottomRight = rTrapezoidBottomRight;
        self.m_rLastTrapezoidRight = rTrapezoidRight;
    }

    self.m_rCurStratumTop = if fTrapezoid { rStratumTop } else { f32::MAX };
    self.m_rCurStratumBottom = rStratumBottom;

    RRETURN!(hr);
}
 
//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::EndBuildingOutside
//
//  Synopsis:  Finish creating outside geometry.
//             1. If no geometry was created then just fill bounds.
//             2. Otherwise:
//                 A. End last trapezoid row
//                 B. Produce stop stratum
// 
//-----------------------------------------------------------------------------
fn EndBuildingOutside(&mut self) -> HRESULT
{
    return self.PrepareStratum(
        self.OutsideBottom(),
        self.OutsideBottom(),
        false, /* Not a trapezoid. */
        0., 0.,
        0., 0.,
        0., 0.,
        );
}

//+----------------------------------------------------------------------------
//
//  Member:    CHwTVertexBuffer<TVertex>::Builder::EndBuilding
//
//  Synopsis:  Expand all vertices to the full required format and return
//             vertex buffer.
//
//-----------------------------------------------------------------------------
pub fn EndBuilding(&mut self) -> HRESULT
{
    let hr = S_OK;

    IFC!(self.EndBuildingOutside());
    
//Cleanup:
    RRETURN!(hr);
}

}
