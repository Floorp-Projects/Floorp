/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://rust-lang.github.io/rust-clippy/master/index.html#float_cmp
// Silence, clippy!
const EPSILON: f64 = 0.0001f64;

#[derive(Debug, Clone)]
pub struct Point {
    coord_x: f64,
    coord_y: f64,
}

#[derive(Debug, Clone)]
pub struct Line {
    start: Point,
    end: Point,
}

pub fn gradient(ln: Line) -> f64 {
    let rise = ln.end.coord_y - ln.start.coord_y;
    let run = ln.end.coord_x - ln.start.coord_x;
    rise / run
}

pub fn intersection(ln1: Line, ln2: Line) -> Option<Point> {
    // TODO: yuck, should be able to take &Line as argument here
    // and have rust figure it out with a bunch of annotations...
    let g1 = gradient(ln1.clone());
    let z1 = ln1.start.coord_y - g1 * ln1.start.coord_x;
    let g2 = gradient(ln2.clone());
    let z2 = ln2.start.coord_y - g2 * ln2.start.coord_x;
    // Parallel lines do not intersect.
    if (g1 - g2).abs() < EPSILON {
        return None;
    }
    // Otherwise, they intersect at this fancy calculation that
    // I found on wikipedia.
    let x = (z2 - z1) / (g1 - g2);
    Some(Point {
        coord_x: x,
        coord_y: g1 * x + z1,
    })
}

include!(concat!(env!("OUT_DIR"), "/geometry.uniffi.rs"));
