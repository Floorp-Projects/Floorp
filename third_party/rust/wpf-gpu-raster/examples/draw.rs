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

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
use euclid::{default::Transform2D, point2};
use wpf_gpu_raster::{PathBuilder};


use std::ops::Index;


const  WIDTH: u32  = 800;
const HEIGHT: u32  = 800;


fn over(src: u32, dst: u32) -> u32 {
    let a = src >> 24;
    let a = 255 - a;
    let mask = 0xff00ff;
    let t = (dst & mask) * a + 0x800080;
    let mut rb = (t + ((t >> 8) & mask)) >> 8;
    rb &= mask;

    rb += src & mask;

    // saturate
    rb |= 0x1000100 - ((rb >> 8) & mask);
    rb &= mask;

    let t = ((dst >> 8) & mask) * a + 0x800080;
    let mut ag = (t + ((t >> 8) & mask)) >> 8;
    ag &= mask;
    ag += (src >> 8) & mask;

    // saturate
    ag |= 0x1000100 - ((ag >> 8) & mask);
    ag &= mask;

    (ag << 8) + rb
}

pub fn alpha_mul(x: u32, a: u32) -> u32 {
    let mask = 0xFF00FF;

    let src_rb = ((x & mask) * a) >> 8;
    let src_ag = ((x >> 8) & mask) * a;

    (src_rb & mask) | (src_ag & !mask)
}

fn write_image(data: &[u32], path: &str) {
    use std::path::Path;
    use std::fs::File;
    use std::io::BufWriter;

    let mut png_data: Vec<u8> = vec![0; (WIDTH * HEIGHT * 3) as usize];
    let mut i = 0;
    for pixel in data {
        png_data[i] = ((pixel >> 16) & 0xff) as u8;
        png_data[i + 1] = ((pixel >> 8) & 0xff) as u8;
        png_data[i + 2] = ((pixel >> 0) & 0xff) as u8;
        i += 3;
    }


    let path = Path::new(path);
    let file = File::create(path).unwrap();
    let w = &mut BufWriter::new(file);

    let mut encoder = png::Encoder::new(w, WIDTH, HEIGHT); // Width is 2 pixels and height is 1.
    encoder.set_color(png::ColorType::Rgb);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header().unwrap();

    writer.write_image_data(&png_data).unwrap(); // Save
}

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
fn rast_triangle(buffer: &mut [u32], stride: usize, tri: &Triangle, color: u32) {
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
    maxx = maxx.min(WIDTH as i64 - 1);

    miny = miny.max(0);
    maxy = maxy.min(HEIGHT as i64 - 1);

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

    /* Perform rasterization */
    let mut buffer = &mut buffer[miny as usize * stride..];
    for _y in miny..=maxy {
        let mut cx1 = cy1;
        let mut cx2 = cy2;
        let mut cx3 = cy3;

        for x in minx..=maxx {
            if cx1 > 0 && cx2 > 0 && cx3 > 0 {
                // cross is equal to 2*area of the triangle.
                // we can normalize cx by 2*area to get barycentric coords.
                let area = cross.abs() as f32;
                let bary = (cx1 as f32 / area, cx2 as f32/ area, cx3 as f32 / area);

                let coverages = coverage1 * bary.0 + coverage2 * bary.1 + coverage3 * bary.2;

                let color = alpha_mul(color, (coverages * 256. + 0.5) as u32);
                buffer[x as usize] = over(color, buffer[x as usize]);
            }

            cx1 -= fdy12;
            cx2 -= fdy23;
            cx3 -= fdy31;
        }

        cy1 += fdx12;
        cy2 += fdx23;
        cy3 += fdx31;

        buffer = &mut buffer[stride..];
    }
}


fn main() {
    let opt = usvg::Options::default();

    let rtree = usvg::Tree::from_file("tiger.svg", &opt).unwrap();

    let mut image = vec![0; (WIDTH * HEIGHT) as usize];
    for _ in 0..1 {
    let mut total_vertex_count = 0;
    let mut total_time = std::time::Duration::default();
    for node in rtree.root().descendants() {
        use usvg::NodeExt;
        let t = node.transform();
        let transform = Transform2D::new(
            t.a as f32, t.b as f32,
            t.c as f32, t.d as f32,
            t.e as f32, t.f as f32,
        );


        let s = 1.;
        if let usvg::NodeKind::Path(ref usvg_path) = *node.borrow() {
            let color = match usvg_path.fill {
                Some(ref fill) => {
                    match fill.paint {
                        usvg::Paint::Color(c) => 0xff000000 | (c.red as u32) << 16 | (c.green as u32) << 8 | c.blue as u32,
                        _ => 0xff00ff00,
                    }
                }
                None => {
                    continue;
                }
            };
            let mut builder = PathBuilder::new();
            //dbg!(&usvg_path.segments);
            for segment in &usvg_path.segments {
                match *segment {
                    usvg::PathSegment::MoveTo { x, y } => {
                        let p = transform.transform_point(point2(x as f32, y as f32)) * s;
                        builder.move_to(p.x, p.y);
                    }
                    usvg::PathSegment::LineTo { x, y } => {
                        let p = transform.transform_point(point2(x as f32, y as f32)) * s;
                        builder.line_to(p.x, p.y);
                    }
                    usvg::PathSegment::CurveTo { x1, y1, x2, y2, x, y, } => {
                        let c1 = transform.transform_point(point2(x1 as f32, y1 as f32)) * s;
                        let c2 = transform.transform_point(point2(x2 as f32, y2 as f32)) * s;
                        let p = transform.transform_point(point2(x as f32, y as f32)) * s;
                        builder.curve_to(
                            c1.x, c1.y,
                            c2.x, c2.y,
                            p.x, p.y,
                        );
                    }
                    usvg::PathSegment::ClosePath => {
                        builder.close();
                    }
                }
            }
            let start = std::time::Instant::now(); 
            let result = builder.rasterize_to_tri_list(0, 0, WIDTH as i32, HEIGHT as i32);
            let end = std::time::Instant::now();
            total_time += end - start;

            println!("vertices {}", result.len());
            total_vertex_count += result.len();
            if result.len() == 0 {
                continue;
            }

            for n in (0..result.len()).step_by(3) {
                let vertices =  {
                    [&result[n], &result[n+1], &result[n+2]]
                };

                let src = color;
                let tri = Triangle { v: [
                    Vertex { x: vertices[0].x, y: vertices[0].y, coverage: vertices[0].coverage},
                    Vertex { x: vertices[1].x, y: vertices[1].y, coverage: vertices[1].coverage},
                    Vertex { x: vertices[2].x, y: vertices[2].y, coverage: vertices[2].coverage}
                    ]
                };
                rast_triangle(&mut image, WIDTH as usize, &tri, src);
            }
        }
    }

    println!("total vertex count {}, took {}ms", total_vertex_count, total_time.as_secs_f32()*1000.);
    }


    write_image(&image, "out.png");
    use std::{hash::{Hash, Hasher}, collections::hash_map::DefaultHasher};
    use crate::*;
    fn calculate_hash<T: Hash>(t: &T) -> u64 {
        let mut s = DefaultHasher::new();
        t.hash(&mut s);
        s.finish()
    }

    assert_eq!(calculate_hash(&image),
        if cfg!(debug_assertions) { 0x5973c52a1c0232f3 } else { 0xf15821a5bebc5ecf});


}
