use crate::aacoverage::CCoverageInterval;
use crate::nullable_ref::Ref;
use crate::types::*;

pub trait IGeometrySink
{
    //
    // Aliased geometry output
    //
/*
    virtual HRESULT AddVertex(
        __in_ecount(1) const MilPoint2F &ptPosition,
            // In: Vertex coordinates
        __out_ecount(1) WORD *pidxOut
            // Out: Index of vertex
        ) PURE;

    virtual HRESULT AddIndexedVertices(
        UINT cVertices,
            // In: number of vertices
        __in_bcount(cVertices*uVertexStride) const void *pVertexBuffer,
            // In: vertex buffer containing the vertices
        UINT uVertexStride,
            // In: size of each vertex
        MilVertexFormat mvfFormat,
            // In: format of each vertex
        UINT cIndices,
            // In: Number of indices
        __in_ecount(cIndices) const UINT *puIndexBuffer
            // In: index buffer                                                             
        ) PURE;

    virtual void SetTransformMapping(
        __in_ecount(1) const MILMatrix3x2 &mat2DTransform
        ) PURE;

    virtual HRESULT AddTriangle(
        DWORD idx1,
            // In: Index of triangle's first vertex
        DWORD idx2,
            // In: Index of triangle's second vertex
        DWORD idx3
            // In: Index of triangle's third vertex
        ) PURE;

    //
    // Trapezoidal AA geometry output
    //
*/
    fn AddComplexScan(&mut self,
        nPixelY: INT,
            // In: y coordinate in pixel space
            pIntervalSpanStart: Ref<CCoverageInterval>
            // In: coverage segments
        ) -> HRESULT;
    
    fn AddTrapezoid(
        &mut self,
        rYMin: f32,
            // In: y coordinate of top of trapezoid
        rXLeftYMin: f32,
            // In: x coordinate for top left
        rXRightYMin: f32,
            // In: x coordinate for top right
        rYMax: f32,
            // In: y coordinate of bottom of trapezoid
        rXLeftYMax: f32,
            // In: x coordinate for bottom left
        rXRightYMax: f32,
            // In: x coordinate for bottom right
        rXDeltaLeft: f32,
            // In: trapezoid expand radius
        rXDeltaRight: f32
            // In: trapezoid expand radius
        ) -> HRESULT;

    fn IsEmpty(&self) -> bool;
    /*
    virtual HRESULT AddParallelogram(
        __in_ecount(4) const MilPoint2F *rgPosition
        ) PURE;
    
    //
    // Query sink status
    //

    // Some geometry generators don't actually know if they have output
    // any triangles, so they need to get this information from the geometry sink.

    virtual BOOL IsEmpty() PURE;
*/
}
