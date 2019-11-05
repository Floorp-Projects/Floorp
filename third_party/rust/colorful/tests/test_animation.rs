extern crate colorful;

use colorful::Color;
use colorful::Colorful;
use colorful::HSL;
use colorful::RGB;

#[test]
fn test_rainbow() {
    let s = "Hello world";
    s.rainbow_with_speed(0);
}

#[test]
fn test_neon_1() {
    let s = "Hello world";
    s.neon_with_speed(Color::Grey0, Color::Grey0, 0);
}

#[test]
fn test_neon_2() {
    let s = "Hello world";
    s.neon_with_speed(HSL::new(1.0, 1.0, 0.4), HSL::new(0.5, 1.0, 0.4), 0);
}

#[test]
fn test_neon_3() {
    let s = "Hello world";
    s.neon_with_speed(RGB::new(122, 122, 122), RGB::new(222, 222, 222), 0);
}

