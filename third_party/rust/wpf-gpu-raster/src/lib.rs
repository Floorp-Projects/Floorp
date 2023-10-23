/*!
Converts a 2D path into a set of vertices of a triangle strip mesh that represents the antialiased fill of that path.

```rust
    use wpf_gpu_raster::PathBuilder;
    let mut p = PathBuilder::new();
    p.move_to(10., 10.);
    p.line_to(40., 10.);
    p.line_to(40., 40.);
    let result = p.rasterize_to_tri_list(0, 0, 100, 100);
```

*/
#![allow(unused_parens)]
#![allow(overflowing_literals)]
#![allow(non_snake_case)]
#![allow(non_camel_case_types)]
#![allow(non_upper_case_globals)]
#![allow(dead_code)]
#![allow(unused_macros)]

#[macro_use]
mod fix;
#[macro_use]
mod helpers;
#[macro_use]
mod real;
mod bezier;
#[macro_use]
mod aarasterizer;
mod hwrasterizer;
mod aacoverage;
mod hwvertexbuffer;

mod types;
mod geometry_sink;
mod matrix;

mod nullable_ref;

#[cfg(feature = "c_bindings")]
pub mod c_bindings;

#[cfg(test)]
mod tri_rasterize;

use aarasterizer::CheckValidRange28_4;
use hwrasterizer::CHwRasterizer;
use hwvertexbuffer::{CHwVertexBuffer, CHwVertexBufferBuilder};
use real::CFloatFPU;
use types::{MilFillMode, PathPointTypeStart, MilPoint2F, MilPointAndSizeL, PathPointTypeLine, MilVertexFormat, MilVertexFormatAttribute, DynArray, BYTE, PathPointTypeBezier, PathPointTypeCloseSubpath, CMILSurfaceRect, POINT};

#[repr(C)]
#[derive(Clone, Debug, Default)]
pub struct OutputVertex {
    pub x: f32,
    pub y: f32,
    pub coverage: f32
}

#[repr(C)]
#[derive(Copy, Clone)]
pub enum FillMode {
    EvenOdd = 0,
    Winding = 1,
}

impl Default for FillMode {
    fn default() -> Self {
        FillMode::EvenOdd
    }
}

#[derive(Clone, Default)]
pub struct OutputPath {
    fill_mode: FillMode,
    points: Box<[POINT]>,
    types: Box<[BYTE]>,
}

impl std::hash::Hash for OutputVertex {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.x.to_bits().hash(state);
        self.y.to_bits().hash(state);
        self.coverage.to_bits().hash(state);
    }
}

pub struct PathBuilder {
    points: DynArray<POINT>,
    types: DynArray<BYTE>,
    initial_point: Option<MilPoint2F>,
    current_point: Option<MilPoint2F>,
    in_shape: bool,
    fill_mode: FillMode,
    outside_bounds: Option<CMILSurfaceRect>,
    need_inside: bool,
    valid_range: bool,
    rasterization_truncates: bool,
}

impl PathBuilder {
    pub fn new() -> Self {
        Self {
            points: Vec::new(),
            types: Vec::new(),
            initial_point: None,
            current_point: None,
            in_shape: false,
            fill_mode: FillMode::EvenOdd,
            outside_bounds: None,
            need_inside: true,
            valid_range: true,
            rasterization_truncates: false,
        }
    }
    fn reset(&mut self) {
        *self = Self {
            points: std::mem::take(&mut self.points),
            types: std::mem::take(&mut self.types),
            ..Self::new()
        };
        self.points.clear();
        self.types.clear();
    }
    fn add_point(&mut self, x: f32, y: f32) {
        self.current_point = Some(MilPoint2F{X: x, Y: y});
        // Transform from pixel corner at 0.0 to pixel center at 0.0. Scale into 28.4 range.
        // Validate that the point before rounding is within expected bounds for the rasterizer.
        let (x, y) = ((x - 0.5) * 16.0, (y - 0.5) * 16.0);
        self.valid_range = self.valid_range && CheckValidRange28_4(x, y);
        self.points.push(POINT {
            x: CFloatFPU::Round(x),
            y: CFloatFPU::Round(y),
        });
    }
    pub fn line_to(&mut self, x: f32, y: f32) {
        if let Some(initial_point) = self.initial_point {
            if !self.in_shape {
                self.types.push(PathPointTypeStart);
                self.add_point(initial_point.X, initial_point.Y);
                self.in_shape = true;
            }
            self.types.push(PathPointTypeLine);
            self.add_point(x, y);
        } else {
            self.initial_point = Some(MilPoint2F{X: x, Y: y})
        }
    }
    pub fn move_to(&mut self, x: f32, y: f32) {
        self.in_shape = false;
        self.initial_point = Some(MilPoint2F{X: x, Y: y});
        self.current_point = self.initial_point;
    }
    pub fn curve_to(&mut self, c1x: f32, c1y: f32, c2x: f32, c2y: f32, x: f32, y: f32) {
        let initial_point = match self.initial_point {
            Some(initial_point) => initial_point,
            None => MilPoint2F{X:c1x, Y:c1y}
        };
        if !self.in_shape {
            self.types.push(PathPointTypeStart);
            self.add_point(initial_point.X, initial_point.Y);
            self.initial_point = Some(initial_point);
            self.in_shape = true;
        }
        self.types.push(PathPointTypeBezier);
        self.add_point(c1x, c1y);
        self.add_point(c2x, c2y);
        self.add_point(x, y);
    }
    pub fn quad_to(&mut self, cx: f32, cy: f32, x: f32, y: f32) {
        // For now we just implement quad_to on top of curve_to.
        // Long term we probably want to support quad curves
        // directly.
        let c0 = match self.current_point {
            Some(current_point) => current_point,
            None => MilPoint2F{X:cx, Y:cy}
        };

        let c1x = c0.X + (2./3.) * (cx - c0.X);
        let c1y = c0.Y + (2./3.) * (cy - c0.Y);

        let c2x = x + (2./3.) * (cx - x);
        let c2y = y + (2./3.) * (cy - y);

        self.curve_to(c1x, c1y, c2x, c2y, x, y);
    }
    pub fn close(&mut self) {
        if self.in_shape {
          // Only close the path if we are inside a shape. Otherwise, the point
          // should be safe to elide.
          if let Some(last) = self.types.last_mut() {
              *last |= PathPointTypeCloseSubpath;
          }
          self.in_shape = false;
        }
        // Close must install a new initial point that is the same as the
        // initial point of the just-closed sub-path. Thus, just leave the
        // initial point unchanged.
        self.current_point = self.initial_point;
    }
    pub fn set_fill_mode(&mut self, fill_mode: FillMode) {
        self.fill_mode = fill_mode;
    }
    /// Enables rendering geometry for areas outside the shape but
    /// within the bounds.  These areas will be created with
    /// zero alpha.
    ///
    /// This is useful for creating geometry for other blend modes.
    /// For example:
    /// - `IN(dest, geometry)` can be done with `outside_bounds` and `need_inside = false`
    /// - `IN(dest, geometry, alpha)` can be done with `outside_bounds` and `need_inside = true`
    ///
    /// Note: trapezoidal areas won't be clipped to outside_bounds
    pub fn set_outside_bounds(&mut self, outside_bounds: Option<(i32, i32, i32, i32)>, need_inside: bool) {
        self.outside_bounds = outside_bounds.map(|r| CMILSurfaceRect { left: r.0, top: r.1, right: r.2, bottom: r.3 });
        self.need_inside = need_inside;
    }

    /// Set this to true if post vertex shader coordinates are converted to fixed point
    /// via truncation. This has been observed with OpenGL on AMD GPUs on macOS. 
    pub fn set_rasterization_truncates(&mut self, rasterization_truncates: bool) {
        self.rasterization_truncates = rasterization_truncates;
    }

    /// Note: trapezoidal areas won't necessarily be clipped to the clip rect
    pub fn rasterize_to_tri_list(&self, clip_x: i32, clip_y: i32, clip_width: i32, clip_height: i32) -> Box<[OutputVertex]> {
        if !self.valid_range {
            // If any of the points are outside of valid 28.4 range, then just return an empty triangle list.
            return Box::new([]);
        }
        let (x, y, width, height, need_outside) = if let Some(CMILSurfaceRect { left, top, right, bottom }) = self.outside_bounds {
            let x0 = clip_x.max(left);
            let y0 = clip_y.max(top);
            let x1 = (clip_x + clip_width).min(right);
            let y1 = (clip_y + clip_height).min(bottom);
            (x0, y0, x1 - x0, y1 - y0, true)
        } else {
            (clip_x, clip_y, clip_width, clip_height, false)
        };
        rasterize_to_tri_list(self.fill_mode, &self.types, &self.points, x, y, width, height, self.need_inside, need_outside, self.rasterization_truncates, None)
            .flush_output()
    }

    pub fn get_path(&mut self) -> Option<OutputPath> {
        if self.valid_range && !self.points.is_empty() && !self.types.is_empty() {
            Some(OutputPath {
                fill_mode: self.fill_mode,
                points: Box::from(self.points.as_slice()),
                types: Box::from(self.types.as_slice()),
            })
        } else {
            None
        }
    }
}

// Converts a path that is specified as an array of edge types, each associated with a fixed number
// of points that are serialized to the points array. Edge types are specified via PathPointType
// masks, whereas points must be supplied in 28.4 signed fixed-point format. By default, users can
// fill the inside of the path excluding the outside. It may alternatively be desirable to fill the
// outside the path out to the clip boundary, optionally keeping the inside. PathBuilder may be
// used instead as a simpler interface to this function that handles building the path arrays.
pub fn rasterize_to_tri_list<'a>(
    fill_mode: FillMode,
    types: &[BYTE],
    points: &[POINT],
    clip_x: i32,
    clip_y: i32,
    clip_width: i32,
    clip_height: i32,
    need_inside: bool,
    need_outside: bool,
    rasterization_truncates: bool,
    output_buffer: Option<&'a mut [OutputVertex]>,
) -> CHwVertexBuffer<'a> {
    let clipRect = MilPointAndSizeL {
        X: clip_x,
        Y: clip_y,
        Width: clip_width,
        Height: clip_height,
    };

    let mil_fill_mode = match fill_mode {
        FillMode::EvenOdd => MilFillMode::Alternate,
        FillMode::Winding => MilFillMode::Winding,
    };

    let m_mvfIn: MilVertexFormat = MilVertexFormatAttribute::MILVFAttrXY as MilVertexFormat;
    let m_mvfGenerated: MilVertexFormat  = MilVertexFormatAttribute::MILVFAttrNone as MilVertexFormat;
    //let mvfaAALocation  = MILVFAttrNone;
    const HWPIPELINE_ANTIALIAS_LOCATION: MilVertexFormatAttribute = MilVertexFormatAttribute::MILVFAttrDiffuse;
    let mvfaAALocation = HWPIPELINE_ANTIALIAS_LOCATION;

    let outside_bounds = if need_outside {
        Some(CMILSurfaceRect {
            left: clip_x,
            top: clip_y,
            right: clip_x + clip_width,
            bottom: clip_y + clip_height,
        })
    } else {
        None
    };

    let mut vertexBuffer = CHwVertexBuffer::new(rasterization_truncates, output_buffer);
    {
        let mut vertexBuilder = CHwVertexBufferBuilder::Create(
            m_mvfIn, m_mvfIn | m_mvfGenerated, mvfaAALocation, &mut vertexBuffer);
        vertexBuilder.SetOutsideBounds(outside_bounds.as_ref(), need_inside);
        vertexBuilder.BeginBuilding();
        {
            let mut rasterizer = CHwRasterizer::new(
                &mut vertexBuilder, mil_fill_mode, None, clipRect);
            rasterizer.SendGeometry(points, types);
        }
        vertexBuilder.EndBuilding();
    }

    vertexBuffer
}

#[cfg(test)]
mod tests {
    use std::{hash::{Hash, Hasher}, collections::hash_map::DefaultHasher};
    use crate::{*, tri_rasterize::rasterize_to_mask};
    fn calculate_hash<T: Hash>(t: &T) -> u64 {
        let mut s = DefaultHasher::new();
        t.hash(&mut s);
        s.finish()
    }
    #[test]
    fn basic() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(10., 30.);
        p.line_to(30., 30.);
        p.line_to(30., 10.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 18);
        //assert_eq!(dbg!(calculate_hash(&result)), 0x5851570566450135);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0xfbb7c3932059e240);
    }

    #[test]
    fn simple() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(40., 10.);
        p.line_to(40., 40.);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        //assert_eq!(dbg!(calculate_hash(&result)), 0x81a9af7769f88e68);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x6d1595533d40ef92);
    }

    #[test]
    fn rust() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(40., 10.);
        p.line_to(40., 40.);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        //assert_eq!(dbg!(calculate_hash(&result)), 0x81a9af7769f88e68);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x6d1595533d40ef92);
    }

    #[test]
    fn fill_mode() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(40., 10.);
        p.line_to(40., 40.);
        p.line_to(10., 40.);
        p.close();
        p.move_to(15., 15.);
        p.line_to(35., 15.);
        p.line_to(35., 35.);
        p.line_to(15., 35.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        //assert_eq!(dbg!(calculate_hash(&result)), 0xb34344234f2f75a8);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0xc7bf999c56ccfc34);

        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(40., 10.);
        p.line_to(40., 40.);
        p.line_to(10., 40.);
        p.close();
        p.move_to(15., 15.);
        p.line_to(35., 15.);
        p.line_to(35., 35.);
        p.line_to(15., 35.);
        p.close();
        p.set_fill_mode(FillMode::Winding);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        //assert_eq!(dbg!(calculate_hash(&result)), 0xee4ecd8a738fc42c);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0xfafad659db9a2efd);

    }

    #[test]
    fn range() {
        // test for a start point out of range
        let mut p = PathBuilder::new();
        p.curve_to(8.872974e16, 0., 0., 0., 0., 0.);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 0);

        // test for a subsequent point out of range
        let mut p = PathBuilder::new();
        p.curve_to(0., 0., 8.872974e16, 0., 0., 0.);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 0);
    }

    #[test]
    fn multiple_starts() {
        let mut p = PathBuilder::new();
        p.line_to(10., 10.);
        p.move_to(0., 0.);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 0);
    }

    #[test]
    fn path_closing() {
        let mut p = PathBuilder::new();
        p.curve_to(0., 0., 0., 0., 0., 32.0);
        p.close();
        p.curve_to(0., 0., 0., 0., 0., 32.0);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 0);
    }

    #[test]
    fn curve() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.curve_to(40., 10., 40., 10., 40., 40.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0xa92aae8dba7b8cd4);
        assert_eq!(dbg!(calculate_hash(&result)), 0x8dbc4d23f9bba38d);
    }

    #[test]
    fn partial_coverage_last_line() {
        let mut p = PathBuilder::new();

        p.move_to(10., 10.);
        p.line_to(40., 10.);
        p.line_to(40., 39.6);
        p.line_to(10., 39.6);

        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 21);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0xfa200c3bae144952);
        assert_eq!(dbg!(calculate_hash(&result)), 0xf90cb6afaadfb559);
    }

    #[test]
    fn delta_upper_bound() {
        let mut p = PathBuilder::new();
        p.move_to(-122.3 + 200.,84.285);
        p.curve_to(-122.3 + 200., 84.285, -122.2 + 200.,86.179, -123.03 + 200., 86.16);
        p.curve_to(-123.85 + 200., 86.141, -140.3 + 200., 38.066, -160.83 + 200., 40.309);
        p.curve_to(-160.83 + 200., 40.309, -143.05 + 200., 32.956,  -122.3 + 200., 84.285);
        p.close();

        let result = p.rasterize_to_tri_list(0, 0, 400, 400);
        assert_eq!(result.len(), 429);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x5e82d98fdb47a796);
        assert_eq!(dbg!(calculate_hash(&result)), 0x52d52992e249587a);
    }


    #[test]
    fn self_intersect() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(40., 10.);
        p.line_to(10., 40.);
        p.line_to(40., 40.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x49ecc769e1d4ec01);
        assert_eq!(dbg!(calculate_hash(&result)), 0xf10babef5c619d19);
    }

    #[test]
    fn grid() {
        let mut p = PathBuilder::new();

        for i in 0..200 {
            let offset = i as f32 * 1.3;
            p.move_to(0. + offset, -8.);
            p.line_to(0.5 + offset, -8.);
            p.line_to(0.5 + offset, 40.);
            p.line_to(0. + offset, 40.);
            p.close();
        }
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 12000);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x5a7df39d9e9292f0);
    }

    #[test]
    fn outside() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(40., 10.);
        p.line_to(10., 40.);
        p.line_to(40., 40.);
        p.close();
        p.set_outside_bounds(Some((0, 0, 50, 50)), false);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x59403ddbb7e1d09a);
        assert_eq!(dbg!(calculate_hash(&result)), 0x805fd385e47e6f2);

        // ensure that adjusting the outside bounds changes the results
        p.set_outside_bounds(Some((5, 5, 50, 50)), false);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x59403ddbb7e1d09a);
        assert_eq!(dbg!(calculate_hash(&result)), 0xcec2ed688999c966);
    }

    #[test]
    fn outside_inside() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(40., 10.);
        p.line_to(10., 40.);
        p.line_to(40., 40.);
        p.close();
        p.set_outside_bounds(Some((0, 0, 50, 50)), true);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x49ecc769e1d4ec01);
        assert_eq!(dbg!(calculate_hash(&result)), 0xaf76b42a5244d1ec);
    }

    #[test]
    fn outside_clipped() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.);
        p.line_to(10., 40.);
        p.line_to(90., 40.);
        p.line_to(40., 10.);
        p.close();
        p.set_outside_bounds(Some((0, 0, 50, 50)), false);
        let result = p.rasterize_to_tri_list(0, 0, 50, 50);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x3d2a08f5d0bac999);
        assert_eq!(dbg!(calculate_hash(&result)), 0xbd42b934ab52be39);
    }

    #[test]
    fn clip_edge() {
        let mut p = PathBuilder::new();
        // tests the bigNumerator < 0 case of aarasterizer::ClipEdge
        p.curve_to(-24., -10., -300., 119., 0.0, 0.0);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        // The edge merging only happens between points inside the enumerate buffer. This means
        // that the vertex output can depend on the size of the enumerate buffer because there
        // the number of edges and positions of vertices will change depending on edge merging.
        if ENUMERATE_BUFFER_NUMBER!() == 32 {
            assert_eq!(result.len(), 111);
        } else {
            assert_eq!(result.len(), 171);
        }
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x50b887b09a4c16e);
    }

    #[test]
    fn enum_buffer_num() {
        let mut p = PathBuilder::new();
        p.curve_to(0.0, 0.0, 0.0, 12.0, 0.0, 44.919434);
        p.line_to(64.0, 36.0 );
        p.line_to(0.0, 80.0,);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 300);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x659cc742f16b42f2);
    }

    #[test]
    fn fill_alternating_empty_interior_pairs() {
        let mut p = PathBuilder::new();
        p.line_to( 0., 2. );
        p.curve_to(0.0, 0.0,1., 6., 0.0, 0.0);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 9);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x726606a662fe46a0);
    }

    #[test]
    fn fill_winding_empty_interior_pairs() {
        let mut p = PathBuilder::new();
        p.curve_to(45., 61., 0.09, 0., 0., 0.);
        p.curve_to(45., 61., 0.09, 0., 0., 0.);
        p.curve_to(0., 0., 0., 38., 0.09, 15.);
        p.set_fill_mode(FillMode::Winding);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 462);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x651ea4ade5543087);
    }

    #[test]
    fn empty_fill() {
        let mut p = PathBuilder::new();
        p.move_to(0., 0.);
        p.line_to(10., 100.);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 0);
    }

    #[test]
    fn rasterize_line() {
        let mut p = PathBuilder::new();
        p.move_to(1., 1.);
        p.line_to(2., 1.);
        p.line_to(2., 2.);
        p.line_to(1., 2.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        let mask = rasterize_to_mask(&result, 3, 3);
        assert_eq!(&mask[..], &[0,   0, 0,
                                0, 255, 0,
                                0,   0, 0][..]);
    }

    #[test]
    fn triangle() {
        let mut p = PathBuilder::new();
        p.move_to(1., 10.);
        p.line_to(100., 13.);
        p.line_to(1., 16.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x4757b0c5a19b02f0);
    }

    #[test]
    fn single_pixel() {
        let mut p = PathBuilder::new();
        p.move_to(1.5, 1.5);
        p.line_to(2., 1.5);
        p.line_to(2., 2.);
        p.line_to(1.5, 2.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(result.len(), 3);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 4, 4)), 0x9f481fe5588e341c);
    }

    #[test]
    fn traps_outside_bounds() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.0);
        p.line_to(30., 10.);
        p.line_to(50., 20.);
        p.line_to(30., 30.);
        p.line_to(10., 30.);
        p.close();
        // The generated trapezoids are not necessarily clipped to the outside bounds rect
        // and in this case the outside bounds geometry ends up drawing on top of the
        // edge geometry which could be considered a bug.
        p.set_outside_bounds(Some((0, 0, 50, 30)), true);
        let result = p.rasterize_to_tri_list(0, 0, 100, 100);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 100, 100)), 0x6514e3d79d641f09);

    }

    #[test]
    fn quad_to() {
        let mut p = PathBuilder::new();
        p.move_to(10., 10.0);
        p.quad_to(30., 10., 30., 30.);
        p.quad_to(10., 30., 30., 30.);
        p.quad_to(60., 30., 60., 10.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 70, 40);
        assert_eq!(result.len(), 279);
        assert_eq!(calculate_hash(&rasterize_to_mask(&result, 70, 40)), 0xbd2eec3cfe9bd30b);
    }

    #[test]
    fn close_after_move_to() {
        let mut p = PathBuilder::new();
        p.move_to(10., 0.);
        p.close();
        p.move_to(0., 0.);
        p.line_to(0., 10.);
        p.line_to(10., 10.);
        p.move_to(10., 0.);
        p.close();
        let result = p.rasterize_to_tri_list(0, 0, 20, 20);
        assert_eq!(result.len(), 27);
        assert_eq!(dbg!(calculate_hash(&result)), 0xecfdf5bdfa25a1dd);
    }
}
