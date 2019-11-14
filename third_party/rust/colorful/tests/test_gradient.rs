extern crate colorful;

use colorful::Color;
use colorful::Colorful;

#[test]
fn test_gradient_color() {
    let s = "Hello world";
    assert_eq!("\u{1b}[38;2;255;0;0mH\u{1b}[38;2;255;6;0me\u{1b}[38;2;255;13;0ml\u{1b}[38;2;255;19;0ml\u{1b}[38;2;255;26;0mo\u{1b}[38;2;255;32;0m \u{1b}[38;2;255;38;0mw\u{1b}[38;2;255;45;0mo\u{1b}[38;2;255;51;0mr\u{1b}[38;2;255;57;0ml\u{1b}[38;2;255;64;0md\u{1b}[0m".to_owned(), s.gradient(Color::Red).to_string());
}

#[test]
fn test_gradient_multiple_lines() {
    let s = "a\nb";
    assert_eq!("\u{1b}[38;2;255;0;0ma\u{1b}[0m\n\u{1b}[38;2;255;0;0mb\u{1b}[0m".to_owned(), s.gradient(Color::Red).to_string());
}