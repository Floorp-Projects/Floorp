use core::colors::Colorado;
use core::style::Style;
use HSL;


pub mod colors;
pub mod symbols;
pub mod style;
pub mod color_string;
pub mod rgb;
pub mod hsl;

pub trait StrMarker {
    fn to_str(&self) -> String;
    fn get_fg_color(&self) -> Option<Colorado> { None }
    fn get_bg_color(&self) -> Option<Colorado> { None }
    fn get_style(&self) -> Option<Vec<Style>> { None }
}

impl<'a> StrMarker for &'a str {
    fn to_str(&self) -> String {
        String::from(*self)
    }
}

impl StrMarker for String {
    fn to_str(&self) -> String {
        self.clone()
    }
}

/// `ColorInterface` is for basic trait for `Colorful`, `RGB`, `HSL` and `Color` implement this trait.
pub trait ColorInterface: Clone {
    fn to_color_str(&self) -> String;
    fn to_hsl(&self) -> HSL;
}
