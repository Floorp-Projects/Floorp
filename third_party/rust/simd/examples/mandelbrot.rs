#![feature(iterator_step_by, test)]

extern crate test;
extern crate simd;
use simd::{f32x4, u32x4};
use std::io::prelude::*;

#[inline(never)]
fn mandelbrot_naive(c_x: f32, c_y: f32, max_iter: u32) -> u32 {
    let mut x = c_x;
    let mut y = c_y;
    let mut count = 0;
    while count < max_iter {
        let xy = x * y;
        let xx = x * x;
        let yy = y * y;
        let sum = xx + yy;
        if sum > 4.0 {
            break
        }
        count += 1;
        x = xx - yy + c_x;
        y = xy * 2.0 + c_y;
    }
    count
}

#[inline(never)]
fn mandelbrot_vector(c_x: f32x4, c_y: f32x4, max_iter: u32) -> u32x4 {
    let mut x = c_x;
    let mut y = c_y;

    let mut count = u32x4::splat(0);
    for _ in 0..max_iter as usize {
        let xy = x * y;
        let xx = x * x;
        let yy = y * y;
        let sum = xx + yy;
        let mask = sum.lt(f32x4::splat(4.0));

        if !mask.any() { break }
        count = count + mask.to_i().select(u32x4::splat(1),
                                           u32x4::splat(0));

        x = xx - yy + c_x;
        y = xy + xy + c_y;
    }
    count
}

const COLOURS: &'static [(f32, f32, f32)] = &[(0.0, 7.0, 100.0),
                                              (32.0, 107.0, 203.0),
                                              (237.0, 255.0, 255.0),
                                              (255.0, 170.0, 0.0),
                                              (0.0, 2.0, 0.0)];
const SCALE: f32 = 12.0;
const LIMIT: u32 = 100;

#[inline(never)]
fn output_one(buf: &mut [u8], val: u32) {
    let (r, g, b);
    if val == LIMIT {
        r = 0;
        g = 0;
        b = 0;
    } else {
        let val = (val as f32 % SCALE) * (COLOURS.len() as f32) / SCALE;
        let left = val as usize % COLOURS.len();
        let right = (left + 1) % COLOURS.len();

        let p = val - left as f32;
        let (r1, g1, b1) = COLOURS[left];
        let (r2, g2, b2) = COLOURS[right];
        r = (r1 + (r2 - r1) * p) as u8;
        g = (g1 + (g2 - g1) * p) as u8;
        b = (b1 + (b2 - b1) * p) as u8;
    }
    buf[0] = r;
    buf[1] = g;
    buf[2] = b;
}

fn main() {
    let mut args = std::env::args();
    args.next();
    let width = args.next().unwrap().parse().unwrap();
    let height = args.next().unwrap().parse().unwrap();

    let left = -2.2;
    let right = left + 3.0;
    let top = 1.0;
    let bottom = top - 2.0;

    let width_step: f32 = (right - left) / width as f32;
    let height_step: f32 = (bottom - top) / height as f32;

    let adjust = f32x4::splat(width_step) * f32x4::new(0., 1., 2., 3.);

    println!("P6 {} {} 255", width, height);
    let mut line = vec![0; width * 3];

    if args.next().is_none() {
        for i in 0..height {
            let y = f32x4::splat(top + height_step * i as f32);
            for j in (0..width).step_by(4) {
                let x = f32x4::splat(left + width_step * j as f32) + adjust;
                let ret = mandelbrot_vector(x, y, LIMIT);
                test::black_box(ret);
                for k in 0..4 { let val = ret.extract(k as u32); output_one(&mut line[3*(j + k)..3*(j + k + 1)], val); }
            }
            ::std::io::stdout().write(&line).unwrap();
        }
    } else {
        for i in 0..height {
            let y = top + height_step * i as f32;
            for j in 0..width {
                let x = left + width_step * j as f32;
                let val = mandelbrot_naive(x, y, LIMIT);
                test::black_box(val);
                output_one(&mut line[3*j..3*(j + 1)], val);
            }
            ::std::io::stdout().write(&line).unwrap();
        }
    }
}
