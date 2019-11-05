extern crate colorful;
extern crate core;

use colorful::Colorful;

#[test]
fn test_hsl_color() {
    let s = "Hello world";
    assert_eq!("\x1B[38;2;19;205;94mHello world\x1B[0m", s.hsl(0.4, 0.83, 0.44).to_string());
}
