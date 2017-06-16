/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fmt;
use std::f32::consts::PI;

use super::{Token, Parser, ToCss, ParseError, BasicParseError};
use tokenizer::NumericValue;

#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};

/// A color with red, green, blue, and alpha components, in a byte each.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct RGBA {
    /// The red component.
    pub red: u8,
    /// The green component.
    pub green: u8,
    /// The blue component.
    pub blue: u8,
    /// The alpha component.
    pub alpha: u8,
}

impl RGBA {
    /// Constructs a new RGBA value from float components. It expects the red,
    /// green, blue and alpha channels in that order, and all values will be
    /// clamped to the 0.0 ... 1.0 range.
    #[inline]
    pub fn from_floats(red: f32, green: f32, blue: f32, alpha: f32) -> Self {
        Self::new(
            clamp_unit_f32(red),
            clamp_unit_f32(green),
            clamp_unit_f32(blue),
            clamp_unit_f32(alpha),
        )
    }

    /// Returns a transparent color.
    #[inline]
    pub fn transparent() -> Self {
        Self::new(0, 0, 0, 0)
    }

    /// Same thing, but with `u8` values instead of floats in the 0 to 1 range.
    #[inline]
    pub fn new(red: u8, green: u8, blue: u8, alpha: u8) -> Self {
        RGBA { red: red, green: green, blue: blue, alpha: alpha }
    }

    /// Returns the red channel in a floating point number form, from 0 to 1.
    #[inline]
    pub fn red_f32(&self) -> f32 {
        self.red as f32 / 255.0
    }

    /// Returns the green channel in a floating point number form, from 0 to 1.
    #[inline]
    pub fn green_f32(&self) -> f32 {
        self.green as f32 / 255.0
    }

    /// Returns the blue channel in a floating point number form, from 0 to 1.
    #[inline]
    pub fn blue_f32(&self) -> f32 {
        self.blue as f32 / 255.0
    }

    /// Returns the alpha channel in a floating point number form, from 0 to 1.
    #[inline]
    pub fn alpha_f32(&self) -> f32 {
        self.alpha as f32 / 255.0
    }
}

#[cfg(feature = "serde")]
impl Serialize for RGBA {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: Serializer
    {
        (self.red, self.green, self.blue, self.alpha).serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for RGBA {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: Deserializer<'de>
    {
        let (r, g, b, a) = try!(Deserialize::deserialize(deserializer));
        Ok(RGBA::new(r, g, b, a))
    }
}

#[cfg(feature = "heapsize")]
known_heap_size!(0, RGBA);

impl ToCss for RGBA {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result
        where W: fmt::Write,
    {
        // Try first with two decimal places, then with three.
        let mut rounded_alpha = (self.alpha_f32() * 100.).round() / 100.;
        if clamp_unit_f32(rounded_alpha) != self.alpha {
            rounded_alpha = (self.alpha_f32() * 1000.).round() / 1000.;
        }

        if self.alpha == 255 {
            write!(dest, "rgb({}, {}, {})", self.red, self.green, self.blue)
        } else {
            write!(dest, "rgba({}, {}, {}, {})",
                   self.red, self.green, self.blue, rounded_alpha)
        }
    }
}

/// A <color> value.
#[derive(Clone, Copy, PartialEq, Debug)]
pub enum Color {
    /// The 'currentColor' keyword
    CurrentColor,
    /// Everything else gets converted to RGBA during parsing
    RGBA(RGBA),
}

#[cfg(feature = "heapsize")]
known_heap_size!(0, Color);

impl ToCss for Color {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result where W: fmt::Write {
        match *self {
            Color::CurrentColor => dest.write_str("currentColor"),
            Color::RGBA(ref rgba) => rgba.to_css(dest),
        }
    }
}

impl Color {
    /// Parse a <color> value, per CSS Color Module Level 3.
    ///
    /// FIXME(#2) Deprecated CSS2 System Colors are not supported yet.
    pub fn parse<'i, 't>(input: &mut Parser<'i, 't>) -> Result<Color, BasicParseError<'i>> {
        let token = try!(input.next());
        match token {
            Token::Hash(ref value) | Token::IDHash(ref value) => {
                Color::parse_hash(value.as_bytes())
            },
            Token::Ident(ref value) => parse_color_keyword(&*value),
            Token::Function(ref name) => {
                return input.parse_nested_block(|arguments| {
                    parse_color_function(&*name, arguments)
                        .map_err(|e| ParseError::Basic(e))
                }).map_err(ParseError::<()>::basic);
            }
            _ => Err(())
        }.map_err(|()| BasicParseError::UnexpectedToken(token))
    }

    /// Parse a color hash, without the leading '#' character.
    #[inline]
    pub fn parse_hash(value: &[u8]) -> Result<Self, ()> {
        match value.len() {
            8 => Ok(rgba(
                try!(from_hex(value[0])) * 16 + try!(from_hex(value[1])),
                try!(from_hex(value[2])) * 16 + try!(from_hex(value[3])),
                try!(from_hex(value[4])) * 16 + try!(from_hex(value[5])),
                try!(from_hex(value[6])) * 16 + try!(from_hex(value[7]))),
            ),
            6 => Ok(rgb(
                try!(from_hex(value[0])) * 16 + try!(from_hex(value[1])),
                try!(from_hex(value[2])) * 16 + try!(from_hex(value[3])),
                try!(from_hex(value[4])) * 16 + try!(from_hex(value[5]))),
            ),
            4 => Ok(rgba(
                try!(from_hex(value[0])) * 17,
                try!(from_hex(value[1])) * 17,
                try!(from_hex(value[2])) * 17,
                try!(from_hex(value[3])) * 17),
            ),
            3 => Ok(rgb(
                try!(from_hex(value[0])) * 17,
                try!(from_hex(value[1])) * 17,
                try!(from_hex(value[2])) * 17),
            ),
            _ => Err(())
        }
    }

}


#[inline]
fn rgb(red: u8, green: u8, blue: u8) -> Color {
    rgba(red, green, blue, 255)
}

#[inline]
fn rgba(red: u8, green: u8, blue: u8, alpha: u8) -> Color {
    Color::RGBA(RGBA::new(red, green, blue, alpha))
}


/// Return the named color with the given name.
///
/// Matching is case-insensitive in the ASCII range.
/// CSS escaping (if relevant) should be resolved before calling this function.
/// (For example, the value of an `Ident` token is fine.)
#[inline]
pub fn parse_color_keyword(ident: &str) -> Result<Color, ()> {
    macro_rules! rgb {
        ($red: expr, $green: expr, $blue: expr) => {
            Color::RGBA(RGBA {
                red: $red,
                green: $green,
                blue: $blue,
                alpha: 255,
            })
        }
    }
    ascii_case_insensitive_phf_map! {
        keyword -> Color = {
            "black" => rgb!(0, 0, 0),
            "silver" => rgb!(192, 192, 192),
            "gray" => rgb!(128, 128, 128),
            "white" => rgb!(255, 255, 255),
            "maroon" => rgb!(128, 0, 0),
            "red" => rgb!(255, 0, 0),
            "purple" => rgb!(128, 0, 128),
            "fuchsia" => rgb!(255, 0, 255),
            "green" => rgb!(0, 128, 0),
            "lime" => rgb!(0, 255, 0),
            "olive" => rgb!(128, 128, 0),
            "yellow" => rgb!(255, 255, 0),
            "navy" => rgb!(0, 0, 128),
            "blue" => rgb!(0, 0, 255),
            "teal" => rgb!(0, 128, 128),
            "aqua" => rgb!(0, 255, 255),

            "aliceblue" => rgb!(240, 248, 255),
            "antiquewhite" => rgb!(250, 235, 215),
            "aquamarine" => rgb!(127, 255, 212),
            "azure" => rgb!(240, 255, 255),
            "beige" => rgb!(245, 245, 220),
            "bisque" => rgb!(255, 228, 196),
            "blanchedalmond" => rgb!(255, 235, 205),
            "blueviolet" => rgb!(138, 43, 226),
            "brown" => rgb!(165, 42, 42),
            "burlywood" => rgb!(222, 184, 135),
            "cadetblue" => rgb!(95, 158, 160),
            "chartreuse" => rgb!(127, 255, 0),
            "chocolate" => rgb!(210, 105, 30),
            "coral" => rgb!(255, 127, 80),
            "cornflowerblue" => rgb!(100, 149, 237),
            "cornsilk" => rgb!(255, 248, 220),
            "crimson" => rgb!(220, 20, 60),
            "cyan" => rgb!(0, 255, 255),
            "darkblue" => rgb!(0, 0, 139),
            "darkcyan" => rgb!(0, 139, 139),
            "darkgoldenrod" => rgb!(184, 134, 11),
            "darkgray" => rgb!(169, 169, 169),
            "darkgreen" => rgb!(0, 100, 0),
            "darkgrey" => rgb!(169, 169, 169),
            "darkkhaki" => rgb!(189, 183, 107),
            "darkmagenta" => rgb!(139, 0, 139),
            "darkolivegreen" => rgb!(85, 107, 47),
            "darkorange" => rgb!(255, 140, 0),
            "darkorchid" => rgb!(153, 50, 204),
            "darkred" => rgb!(139, 0, 0),
            "darksalmon" => rgb!(233, 150, 122),
            "darkseagreen" => rgb!(143, 188, 143),
            "darkslateblue" => rgb!(72, 61, 139),
            "darkslategray" => rgb!(47, 79, 79),
            "darkslategrey" => rgb!(47, 79, 79),
            "darkturquoise" => rgb!(0, 206, 209),
            "darkviolet" => rgb!(148, 0, 211),
            "deeppink" => rgb!(255, 20, 147),
            "deepskyblue" => rgb!(0, 191, 255),
            "dimgray" => rgb!(105, 105, 105),
            "dimgrey" => rgb!(105, 105, 105),
            "dodgerblue" => rgb!(30, 144, 255),
            "firebrick" => rgb!(178, 34, 34),
            "floralwhite" => rgb!(255, 250, 240),
            "forestgreen" => rgb!(34, 139, 34),
            "gainsboro" => rgb!(220, 220, 220),
            "ghostwhite" => rgb!(248, 248, 255),
            "gold" => rgb!(255, 215, 0),
            "goldenrod" => rgb!(218, 165, 32),
            "greenyellow" => rgb!(173, 255, 47),
            "grey" => rgb!(128, 128, 128),
            "honeydew" => rgb!(240, 255, 240),
            "hotpink" => rgb!(255, 105, 180),
            "indianred" => rgb!(205, 92, 92),
            "indigo" => rgb!(75, 0, 130),
            "ivory" => rgb!(255, 255, 240),
            "khaki" => rgb!(240, 230, 140),
            "lavender" => rgb!(230, 230, 250),
            "lavenderblush" => rgb!(255, 240, 245),
            "lawngreen" => rgb!(124, 252, 0),
            "lemonchiffon" => rgb!(255, 250, 205),
            "lightblue" => rgb!(173, 216, 230),
            "lightcoral" => rgb!(240, 128, 128),
            "lightcyan" => rgb!(224, 255, 255),
            "lightgoldenrodyellow" => rgb!(250, 250, 210),
            "lightgray" => rgb!(211, 211, 211),
            "lightgreen" => rgb!(144, 238, 144),
            "lightgrey" => rgb!(211, 211, 211),
            "lightpink" => rgb!(255, 182, 193),
            "lightsalmon" => rgb!(255, 160, 122),
            "lightseagreen" => rgb!(32, 178, 170),
            "lightskyblue" => rgb!(135, 206, 250),
            "lightslategray" => rgb!(119, 136, 153),
            "lightslategrey" => rgb!(119, 136, 153),
            "lightsteelblue" => rgb!(176, 196, 222),
            "lightyellow" => rgb!(255, 255, 224),
            "limegreen" => rgb!(50, 205, 50),
            "linen" => rgb!(250, 240, 230),
            "magenta" => rgb!(255, 0, 255),
            "mediumaquamarine" => rgb!(102, 205, 170),
            "mediumblue" => rgb!(0, 0, 205),
            "mediumorchid" => rgb!(186, 85, 211),
            "mediumpurple" => rgb!(147, 112, 219),
            "mediumseagreen" => rgb!(60, 179, 113),
            "mediumslateblue" => rgb!(123, 104, 238),
            "mediumspringgreen" => rgb!(0, 250, 154),
            "mediumturquoise" => rgb!(72, 209, 204),
            "mediumvioletred" => rgb!(199, 21, 133),
            "midnightblue" => rgb!(25, 25, 112),
            "mintcream" => rgb!(245, 255, 250),
            "mistyrose" => rgb!(255, 228, 225),
            "moccasin" => rgb!(255, 228, 181),
            "navajowhite" => rgb!(255, 222, 173),
            "oldlace" => rgb!(253, 245, 230),
            "olivedrab" => rgb!(107, 142, 35),
            "orange" => rgb!(255, 165, 0),
            "orangered" => rgb!(255, 69, 0),
            "orchid" => rgb!(218, 112, 214),
            "palegoldenrod" => rgb!(238, 232, 170),
            "palegreen" => rgb!(152, 251, 152),
            "paleturquoise" => rgb!(175, 238, 238),
            "palevioletred" => rgb!(219, 112, 147),
            "papayawhip" => rgb!(255, 239, 213),
            "peachpuff" => rgb!(255, 218, 185),
            "peru" => rgb!(205, 133, 63),
            "pink" => rgb!(255, 192, 203),
            "plum" => rgb!(221, 160, 221),
            "powderblue" => rgb!(176, 224, 230),
            "rebeccapurple" => rgb!(102, 51, 153),
            "rosybrown" => rgb!(188, 143, 143),
            "royalblue" => rgb!(65, 105, 225),
            "saddlebrown" => rgb!(139, 69, 19),
            "salmon" => rgb!(250, 128, 114),
            "sandybrown" => rgb!(244, 164, 96),
            "seagreen" => rgb!(46, 139, 87),
            "seashell" => rgb!(255, 245, 238),
            "sienna" => rgb!(160, 82, 45),
            "skyblue" => rgb!(135, 206, 235),
            "slateblue" => rgb!(106, 90, 205),
            "slategray" => rgb!(112, 128, 144),
            "slategrey" => rgb!(112, 128, 144),
            "snow" => rgb!(255, 250, 250),
            "springgreen" => rgb!(0, 255, 127),
            "steelblue" => rgb!(70, 130, 180),
            "tan" => rgb!(210, 180, 140),
            "thistle" => rgb!(216, 191, 216),
            "tomato" => rgb!(255, 99, 71),
            "turquoise" => rgb!(64, 224, 208),
            "violet" => rgb!(238, 130, 238),
            "wheat" => rgb!(245, 222, 179),
            "whitesmoke" => rgb!(245, 245, 245),
            "yellowgreen" => rgb!(154, 205, 50),

            "transparent" => Color::RGBA(RGBA { red: 0, green: 0, blue: 0, alpha: 0 }),
            "currentcolor" => Color::CurrentColor,
        }
    }
    keyword(ident).cloned().ok_or(())
}


#[inline]
fn from_hex(c: u8) -> Result<u8, ()> {
    match c {
        b'0' ... b'9' => Ok(c - b'0'),
        b'a' ... b'f' => Ok(c - b'a' + 10),
        b'A' ... b'F' => Ok(c - b'A' + 10),
        _ => Err(())
    }
}

fn clamp_unit_f32(val: f32) -> u8 {
    // Whilst scaling by 256 and flooring would provide
    // an equal distribution of integers to percentage inputs,
    // this is not what Gecko does so we instead multiply by 255
    // and round (adding 0.5 and flooring is equivalent to rounding)
    //
    // Chrome does something similar for the alpha value, but not
    // the rgb values.
    //
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1340484
    //
    // Clamping to 256 and rounding after would let 1.0 map to 256, and
    // `256.0_f32 as u8` is undefined behavior:
    //
    // https://github.com/rust-lang/rust/issues/10184
    clamp_floor_256_f32(val * 255.)
}

fn clamp_floor_256_f32(val: f32) -> u8 {
    val.round().max(0.).min(255.) as u8
}

#[inline]
fn parse_color_function<'i, 't>(name: &str, arguments: &mut Parser<'i, 't>) -> Result<Color, BasicParseError<'i>> {
    let (red, green, blue, uses_commas) = match_ignore_ascii_case! { name,
        "rgb" | "rgba" => parse_rgb_components_rgb(arguments)?,
        "hsl" | "hsla" => parse_rgb_components_hsl(arguments)?,
        _ => return Err(BasicParseError::UnexpectedToken(Token::Ident(name.to_owned().into()))),
    };

    let alpha = if !arguments.is_exhausted() {
        if uses_commas {
            try!(arguments.expect_comma());
        } else {
            match try!(arguments.next()) {
                Token::Delim('/') => {},
                t => return Err(BasicParseError::UnexpectedToken(t)),
            };
        };
        let token = try!(arguments.next());
        match token {
            Token::Number(NumericValue { value: v, .. }) => {
                clamp_unit_f32(v)
            }
            Token::Percentage(ref v) => {
                clamp_unit_f32(v.unit_value)
            }
            t => {
                return Err(BasicParseError::UnexpectedToken(t))
            }
        }
    } else {
        255
    };

    try!(arguments.expect_exhausted());
    Ok(rgba(red, green, blue, alpha))
}


#[inline]
fn parse_rgb_components_rgb<'i, 't>(arguments: &mut Parser<'i, 't>) -> Result<(u8, u8, u8, bool), BasicParseError<'i>> {
    let red: u8;
    let green: u8;
    let blue: u8;
    let mut uses_commas = false;

    // Either integers or percentages, but all the same type.
    // https://drafts.csswg.org/css-color/#rgb-functions
    match try!(arguments.next()) {
        Token::Number(NumericValue { value: v, .. }) => {
            red = clamp_floor_256_f32(v);
            green = clamp_floor_256_f32(match try!(arguments.next()) {
                Token::Number(NumericValue { value: v, .. }) => v,
                Token::Comma => {
                    uses_commas = true;
                    try!(arguments.expect_number())
                }
                t => return Err(BasicParseError::UnexpectedToken(t))
            });
            if uses_commas {
                try!(arguments.expect_comma());
            }
            blue = clamp_floor_256_f32(try!(arguments.expect_number()));
        }
        Token::Percentage(ref v) => {
            red = clamp_unit_f32(v.unit_value);
            green = clamp_unit_f32(match try!(arguments.next()) {
                Token::Percentage(ref v) => v.unit_value,
                Token::Comma => {
                    uses_commas = true;
                    try!(arguments.expect_percentage())
                }
                t => return Err(BasicParseError::UnexpectedToken(t))
            });
            if uses_commas {
                try!(arguments.expect_comma());
            }
            blue = clamp_unit_f32(try!(arguments.expect_percentage()));
        }
        t => return Err(BasicParseError::UnexpectedToken(t))
    };
    return Ok((red, green, blue, uses_commas));
}

#[inline]
fn parse_rgb_components_hsl<'i, 't>(arguments: &mut Parser<'i, 't>) -> Result<(u8, u8, u8, bool), BasicParseError<'i>> {
    let mut uses_commas = false;
    // Hue given as an angle
    // https://drafts.csswg.org/css-values/#angles
    let token = try!(arguments.next());
    let hue_degrees = match token {
        Token::Number(NumericValue { value: v, .. }) => Ok(v),
        Token::Dimension(NumericValue { value: v, .. }, ref unit) => {
            match_ignore_ascii_case! { &*unit,
                "deg" => Ok(v),
                "grad" => Ok(v * 360. / 400.),
                "rad" => Ok(v * 360. / (2. * PI)),
                "turn" => Ok(v * 360.),
                _ => Err(()),
            }
        }
        t => return Err(BasicParseError::UnexpectedToken(t))
    };
    let hue_degrees = try!(hue_degrees.map_err(|()| BasicParseError::UnexpectedToken(token)));
    // Subtract an integer before rounding, to avoid some rounding errors:
    let hue_normalized_degrees = hue_degrees - 360. * (hue_degrees / 360.).floor();
    let hue = hue_normalized_degrees / 360.;

    // Saturation and lightness are clamped to 0% ... 100%
    // https://drafts.csswg.org/css-color/#the-hsl-notation
    let saturation = match try!(arguments.next()) {
        Token::Percentage(ref v) => v.unit_value,
        Token::Comma => {
            uses_commas = true;
            try!(arguments.expect_percentage())
        }
        t => return Err(BasicParseError::UnexpectedToken(t))
    };
    let saturation = saturation.max(0.).min(1.);

    if uses_commas {
        try!(arguments.expect_comma());
    }

    let lightness = try!(arguments.expect_percentage());
    let lightness = lightness.max(0.).min(1.);

    // https://drafts.csswg.org/css-color/#hsl-color
    // except with h pre-multiplied by 3, to avoid some rounding errors.
    fn hue_to_rgb(m1: f32, m2: f32, mut h3: f32) -> f32 {
        if h3 < 0. { h3 += 3. }
        if h3 > 3. { h3 -= 3. }

        if h3 * 2. < 1. { m1 + (m2 - m1) * h3 * 2. }
        else if h3 * 2. < 3. { m2 }
        else if h3 < 2. { m1 + (m2 - m1) * (2. - h3) * 2. }
        else { m1 }
    }
    let m2 = if lightness <= 0.5 { lightness * (saturation + 1.) }
             else { lightness + saturation - lightness * saturation };
    let m1 = lightness * 2. - m2;
    let hue_times_3 = hue * 3.;
    let red = clamp_unit_f32(hue_to_rgb(m1, m2, hue_times_3 + 1.));
    let green = clamp_unit_f32(hue_to_rgb(m1, m2, hue_times_3));
    let blue = clamp_unit_f32(hue_to_rgb(m1, m2, hue_times_3 - 1.));
    return Ok((red, green, blue, uses_commas));
}
