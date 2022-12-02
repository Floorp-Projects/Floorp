/* The rasterization code here is based off of piglit/tests/general/triangle-rasterization.cpp:

    /**************************************************************************
     *
     * Copyright 2012 VMware, Inc.
     * All Rights Reserved.
     *
     * Permission is hereby granted, free of charge, to any person obtaining a
     * copy of this software and associated documentation files (the
     * "Software"), to deal in the Software without restriction, including
     * without limitation the rights to use, copy, modify, merge, publish,
     * distribute, sub license, and/or sell copies of the Software, and to
     * permit persons to whom the Software is furnished to do so, subject to
     * the following conditions:
     *
     * The above copyright notice and this permission notice (including the
     * next paragraph) shall be included in all copies or substantial portions
     * of the Software.
     *
     * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
     * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
     * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
     * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
     * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
     * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
     * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
     *
     **************************************************************************/

*/

use std::ops::Index;
use crate::Vertex as OutputVertex;
#[derive(Debug)]
struct Vertex {
    x: f32,
    y: f32,
    coverage: f32
}
#[derive(Debug)]
struct Triangle {
    v: [Vertex; 3],
}

impl Index<usize> for Triangle  {
    type Output = Vertex;

    fn index(&self, index: usize) -> &Self::Output {
        &self.v[index]
    }
}

// D3D11 mandates 8 bit subpixel precision:
// https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#CoordinateSnapping
const FIXED_SHIFT: i32 = 8;
const FIXED_ONE: f32 = (1 << FIXED_SHIFT) as f32;

/* Proper rounding of float to integer */
fn iround(mut v: f32) -> i64 {
    if v > 0.0 {
        v += 0.5;
    }
    if v < 0.0 {
        v -= 0.5;
    }
    return v as i64
}

/* Based on http://devmaster.net/forums/topic/1145-advanced-rasterization */
fn rast_triangle(buffer: &mut [u8], width: usize, height: usize, tri: &Triangle) {
    let center_offset = -0.5;

    let mut coverage1 = tri[0].coverage;
    let mut coverage2 = tri[1].coverage;
    let mut coverage3 = tri[2].coverage;

    /* fixed point coordinates */
    let mut x1 = iround(FIXED_ONE * (tri[0].x + center_offset));
    let     x2 = iround(FIXED_ONE * (tri[1].x + center_offset));
    let mut x3 = iround(FIXED_ONE * (tri[2].x + center_offset));

    let mut y1 = iround(FIXED_ONE * (tri[0].y + center_offset));
    let     y2 = iround(FIXED_ONE * (tri[1].y + center_offset));
    let mut y3 = iround(FIXED_ONE * (tri[2].y + center_offset));


    /* Force correct vertex order */
    let cross = (x2 - x1) * (y3 - y2) - (y2 - y1) * (x3 - x2);
    if cross > 0 {
        std::mem::swap(&mut x1, &mut x3);
        std::mem::swap(&mut y1, &mut y3);
        // I don't understand why coverage 2 and 3 are swapped instead of 1 and 3
        std::mem::swap(&mut coverage2, &mut coverage3);
    } else {
        std::mem::swap(&mut coverage1, &mut coverage3);
    }

    /* Deltas */
    let dx12 = x1 - x2;
    let dx23 = x2 - x3;
    let dx31 = x3 - x1;

    let dy12 = y1 - y2;
    let dy23 = y2 - y3;
    let dy31 = y3 - y1;

    /* Fixed-point deltas */
    let fdx12 = dx12 << FIXED_SHIFT;
    let fdx23 = dx23 << FIXED_SHIFT;
    let fdx31 = dx31 << FIXED_SHIFT;

    let fdy12 = dy12 << FIXED_SHIFT;
    let fdy23 = dy23 << FIXED_SHIFT;
    let fdy31 = dy31 << FIXED_SHIFT;

    /* Bounding rectangle */
    let mut minx = x1.min(x2).min(x3) >> FIXED_SHIFT;
    let mut maxx = x1.max(x2).max(x3) >> FIXED_SHIFT;

    let mut miny = y1.min(y2).min(y3) >> FIXED_SHIFT;
    let mut maxy = y1.max(y2).max(y3) >> FIXED_SHIFT;

    minx = minx.max(0);
    maxx = maxx.min(width as i64 - 1);

    miny = miny.max(0);
    maxy = maxy.min(height as i64 - 1);

    /* Half-edge constants */
    let mut c1 = dy12 * x1 - dx12 * y1;
    let mut c2 = dy23 * x2 - dx23 * y2;
    let mut c3 = dy31 * x3 - dx31 * y3;

    /* Correct for top-left filling convention */
    if dy12 < 0 || (dy12 == 0 && dx12 < 0) { c1 += 1 }
    if dy23 < 0 || (dy23 == 0 && dx23 < 0) { c2 += 1 }
    if dy31 < 0 || (dy31 == 0 && dx31 < 0) { c3 += 1 }

    let mut cy1 = c1 + dx12 * (miny << FIXED_SHIFT) - dy12 * (minx << FIXED_SHIFT);
    let mut cy2 = c2 + dx23 * (miny << FIXED_SHIFT) - dy23 * (minx << FIXED_SHIFT);
    let mut cy3 = c3 + dx31 * (miny << FIXED_SHIFT) - dy31 * (minx << FIXED_SHIFT);
    //dbg!(minx, maxx, tri, cross);
    /* Perform rasterization */
    let mut buffer = &mut buffer[miny as usize * width..];
    for _y in miny..=maxy {
        let mut cx1 = cy1;
        let mut cx2 = cy2;
        let mut cx3 = cy3;

        for x in minx..=maxx {
            if cx1 > 0 && cx2 > 0 && cx3 > 0 {
                // cross is equal to 2*area of the triangle.
                // we can normalize cx by 2*area to get barycentric coords.
                let area = cross.abs() as f32;
                let bary = (cx1 as f32 / area, cx2 as f32 / area, cx3 as f32 / area);
                let coverages = coverage1 * bary.0 + coverage2 * bary.1 + coverage3 * bary.2;
                let color = (coverages * 255. + 0.5) as u8;

                buffer[x as usize] = color;
            }

            cx1 -= fdy12;
            cx2 -= fdy23;
            cx3 -= fdy31;
        }

        cy1 += fdx12;
        cy2 += fdx23;
        cy3 += fdx31;

        buffer = &mut buffer[width..];
    }
}

pub fn rasterize_to_mask(vertices: &[OutputVertex], width: u32, height: u32) -> Box<[u8]> {
    let mut mask = vec![0; (width * height) as usize];
    for n in (0..vertices.len()).step_by(3) {
        let tri =
            [&vertices[n], &vertices[n+1], &vertices[n+2]];

        let tri = Triangle { v: [
            Vertex { x: tri[0].x, y: tri[0].y, coverage: tri[0].coverage},
            Vertex { x: tri[1].x, y: tri[1].y, coverage: tri[1].coverage},
            Vertex { x: tri[2].x, y: tri[2].y, coverage: tri[2].coverage}
            ]
        };
        rast_triangle(&mut mask, width as usize, height as usize, &tri);
    }
    mask.into_boxed_slice()
}
