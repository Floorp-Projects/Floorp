#![feature(cfg_target_feature)]
#![allow(non_snake_case)]

extern crate simd;

#[cfg(target_feature = "sse2")]
use simd::x86::sse2::f64x2;
#[cfg(target_arch = "aarch64")]
use simd::aarch64::neon::f64x2;

fn A(i: usize, j: usize) -> f64 {
    ((i + j) * (i + j + 1) / 2 + i + 1) as f64
}

fn dot(x: &[f64], y: &[f64]) -> f64 {
    x.iter().zip(y).map(|(&x, &y)| x * y).fold(0.0, |a, b| a + b)
}

fn mult_Av(v: &[f64], out: &mut [f64]) {
    assert!(v.len() == out.len());
    assert!(v.len() % 2 == 0);

    for i in 0..v.len() {
        let mut sum = f64x2::splat(0.0);

        let mut j = 0;
        while j < v.len() {
            let b = f64x2::load(v, j);
            let a = f64x2::new(A(i, j), A(i, j + 1));
            sum = sum + b / a;
            j += 2
        }
        out[i] = sum.extract(0) + sum.extract(1);
    }
}

fn mult_Atv(v: &[f64], out: &mut [f64]) {
    assert!(v.len() == out.len());
    assert!(v.len() % 2 == 0);

    for i in 0..v.len() {
        let mut sum = f64x2::splat(0.0);

        let mut j = 0;
        while j < v.len() {
            let b = f64x2::load(v, j);
            let a = f64x2::new(A(j, i), A(j + 1, i));
            sum = sum + b / a;
            j += 2
        }
        out[i] = sum.extract(0) + sum.extract(1);
    }
}

fn mult_AtAv(v: &[f64], out: &mut [f64], tmp: &mut [f64]) {
    mult_Av(v, tmp);
    mult_Atv(tmp, out);
}

fn main() {
    let mut n: usize = std::env::args().nth(1).expect("need one arg").parse().unwrap();
    if n % 2 == 1 { n += 1 }

    let mut u = vec![1.0; n];
    let mut v = u.clone();
    let mut tmp = u.clone();

    for _ in 0..10 {
        mult_AtAv(&u, &mut v, &mut tmp);
        mult_AtAv(&v, &mut u, &mut tmp);
    }

    println!("{:.9}", (dot(&u, &v) / dot(&v, &v)).sqrt());
}
