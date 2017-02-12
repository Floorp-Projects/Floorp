/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fmt;

use super::{Token, Parser, ToCss};

#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};

/// A color with red, green, blue, and alpha components.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct RGBA {
    /// The red channel. Nominally in 0.0 ... 1.0.
    pub red: f32,
    /// The green channel. Nominally in 0.0 ... 1.0.
    pub green: f32,
    /// The blue channel. Nominally in 0.0 ... 1.0.
    pub blue: f32,
    /// The alpha (opacity) channel. Clamped to 0.0 ... 1.0.
    pub alpha: f32,
}

#[cfg(feature = "serde")]
impl Serialize for RGBA {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: Serializer
    {
        (self.red, self.green, self.blue, self.alpha).serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl Deserialize for RGBA {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: Deserializer
    {
        let (red, green, blue, alpha) =
            try!(Deserialize::deserialize(deserializer));
        Ok(RGBA {
            red: red,
            green: green,
            blue: blue,
            alpha: alpha,
        })
    }
}

#[cfg(feature = "heapsize")]
known_heap_size!(0, RGBA);

impl ToCss for RGBA {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result where W: fmt::Write {
        if self.alpha == 1f32 {
            write!(dest, "rgb({}, {}, {})",
                   (self.red * 255.).round(),
                   (self.green * 255.).round(),
                   (self.blue * 255.).round())
        } else {
            write!(dest, "rgba({}, {}, {}, {})",
                   (self.red * 255.).round(),
                   (self.green * 255.).round(),
                   (self.blue * 255.).round(),
                   self.alpha)
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
        match self {
            &Color::CurrentColor => dest.write_str("currentColor"),
            &Color::RGBA(rgba) => rgba.to_css(dest),
        }
    }
}

impl Color {
    /// Parse a <color> value, per CSS Color Module Level 3.
    ///
    /// FIXME(#2) Deprecated CSS2 System Colors are not supported yet.
    pub fn parse(input: &mut Parser) -> Result<Color, ()> {
        match try!(input.next()) {
            Token::Hash(value) | Token::IDHash(value) => parse_color_hash(&*value),
            Token::Ident(value) => parse_color_keyword(&*value),
            Token::Function(name) => {
                input.parse_nested_block(|arguments| {
                    parse_color_function(&*name, arguments)
                })
            }
            _ => Err(())
        }
    }
}


#[inline]
fn rgb(red: f32, green: f32, blue: f32) -> Result<Color, ()> {
    Ok(Color::RGBA(RGBA {
        red: red / 255.,
        green: green / 255.,
        blue: blue / 255.,
        alpha: 1.,
    }))
}

#[inline]
fn rgba(red: f32, green: f32, blue: f32, alpha: f32) -> Result<Color, ()> {
    Ok(Color::RGBA(RGBA {
        red: red / 255.,
        green: green / 255.,
        blue: blue / 255.,
        alpha: alpha / 255.,
    }))
}


/// Return the named color with the given name.
///
/// Matching is case-insensitive in the ASCII range.
/// CSS escaping (if relevant) should be resolved before calling this function.
/// (For example, the value of an `Ident` token is fine.)
#[inline]
pub fn parse_color_keyword(ident: &str) -> Result<Color, ()> {
    match_ignore_ascii_case! { ident,
        "black" => rgb(0., 0., 0.),
        "silver" => rgb(192., 192., 192.),
        "gray" => rgb(128., 128., 128.),
        "white" => rgb(255., 255., 255.),
        "maroon" => rgb(128., 0., 0.),
        "red" => rgb(255., 0., 0.),
        "purple" => rgb(128., 0., 128.),
        "fuchsia" => rgb(255., 0., 255.),
        "green" => rgb(0., 128., 0.),
        "lime" => rgb(0., 255., 0.),
        "olive" => rgb(128., 128., 0.),
        "yellow" => rgb(255., 255., 0.),
        "navy" => rgb(0., 0., 128.),
        "blue" => rgb(0., 0., 255.),
        "teal" => rgb(0., 128., 128.),
        "aqua" => rgb(0., 255., 255.),

        "aliceblue" => rgb(240., 248., 255.),
        "antiquewhite" => rgb(250., 235., 215.),
        "aquamarine" => rgb(127., 255., 212.),
        "azure" => rgb(240., 255., 255.),
        "beige" => rgb(245., 245., 220.),
        "bisque" => rgb(255., 228., 196.),
        "blanchedalmond" => rgb(255., 235., 205.),
        "blueviolet" => rgb(138., 43., 226.),
        "brown" => rgb(165., 42., 42.),
        "burlywood" => rgb(222., 184., 135.),
        "cadetblue" => rgb(95., 158., 160.),
        "chartreuse" => rgb(127., 255., 0.),
        "chocolate" => rgb(210., 105., 30.),
        "coral" => rgb(255., 127., 80.),
        "cornflowerblue" => rgb(100., 149., 237.),
        "cornsilk" => rgb(255., 248., 220.),
        "crimson" => rgb(220., 20., 60.),
        "cyan" => rgb(0., 255., 255.),
        "darkblue" => rgb(0., 0., 139.),
        "darkcyan" => rgb(0., 139., 139.),
        "darkgoldenrod" => rgb(184., 134., 11.),
        "darkgray" => rgb(169., 169., 169.),
        "darkgreen" => rgb(0., 100., 0.),
        "darkgrey" => rgb(169., 169., 169.),
        "darkkhaki" => rgb(189., 183., 107.),
        "darkmagenta" => rgb(139., 0., 139.),
        "darkolivegreen" => rgb(85., 107., 47.),
        "darkorange" => rgb(255., 140., 0.),
        "darkorchid" => rgb(153., 50., 204.),
        "darkred" => rgb(139., 0., 0.),
        "darksalmon" => rgb(233., 150., 122.),
        "darkseagreen" => rgb(143., 188., 143.),
        "darkslateblue" => rgb(72., 61., 139.),
        "darkslategray" => rgb(47., 79., 79.),
        "darkslategrey" => rgb(47., 79., 79.),
        "darkturquoise" => rgb(0., 206., 209.),
        "darkviolet" => rgb(148., 0., 211.),
        "deeppink" => rgb(255., 20., 147.),
        "deepskyblue" => rgb(0., 191., 255.),
        "dimgray" => rgb(105., 105., 105.),
        "dimgrey" => rgb(105., 105., 105.),
        "dodgerblue" => rgb(30., 144., 255.),
        "firebrick" => rgb(178., 34., 34.),
        "floralwhite" => rgb(255., 250., 240.),
        "forestgreen" => rgb(34., 139., 34.),
        "gainsboro" => rgb(220., 220., 220.),
        "ghostwhite" => rgb(248., 248., 255.),
        "gold" => rgb(255., 215., 0.),
        "goldenrod" => rgb(218., 165., 32.),
        "greenyellow" => rgb(173., 255., 47.),
        "grey" => rgb(128., 128., 128.),
        "honeydew" => rgb(240., 255., 240.),
        "hotpink" => rgb(255., 105., 180.),
        "indianred" => rgb(205., 92., 92.),
        "indigo" => rgb(75., 0., 130.),
        "ivory" => rgb(255., 255., 240.),
        "khaki" => rgb(240., 230., 140.),
        "lavender" => rgb(230., 230., 250.),
        "lavenderblush" => rgb(255., 240., 245.),
        "lawngreen" => rgb(124., 252., 0.),
        "lemonchiffon" => rgb(255., 250., 205.),
        "lightblue" => rgb(173., 216., 230.),
        "lightcoral" => rgb(240., 128., 128.),
        "lightcyan" => rgb(224., 255., 255.),
        "lightgoldenrodyellow" => rgb(250., 250., 210.),
        "lightgray" => rgb(211., 211., 211.),
        "lightgreen" => rgb(144., 238., 144.),
        "lightgrey" => rgb(211., 211., 211.),
        "lightpink" => rgb(255., 182., 193.),
        "lightsalmon" => rgb(255., 160., 122.),
        "lightseagreen" => rgb(32., 178., 170.),
        "lightskyblue" => rgb(135., 206., 250.),
        "lightslategray" => rgb(119., 136., 153.),
        "lightslategrey" => rgb(119., 136., 153.),
        "lightsteelblue" => rgb(176., 196., 222.),
        "lightyellow" => rgb(255., 255., 224.),
        "limegreen" => rgb(50., 205., 50.),
        "linen" => rgb(250., 240., 230.),
        "magenta" => rgb(255., 0., 255.),
        "mediumaquamarine" => rgb(102., 205., 170.),
        "mediumblue" => rgb(0., 0., 205.),
        "mediumorchid" => rgb(186., 85., 211.),
        "mediumpurple" => rgb(147., 112., 219.),
        "mediumseagreen" => rgb(60., 179., 113.),
        "mediumslateblue" => rgb(123., 104., 238.),
        "mediumspringgreen" => rgb(0., 250., 154.),
        "mediumturquoise" => rgb(72., 209., 204.),
        "mediumvioletred" => rgb(199., 21., 133.),
        "midnightblue" => rgb(25., 25., 112.),
        "mintcream" => rgb(245., 255., 250.),
        "mistyrose" => rgb(255., 228., 225.),
        "moccasin" => rgb(255., 228., 181.),
        "navajowhite" => rgb(255., 222., 173.),
        "oldlace" => rgb(253., 245., 230.),
        "olivedrab" => rgb(107., 142., 35.),
        "orange" => rgb(255., 165., 0.),
        "orangered" => rgb(255., 69., 0.),
        "orchid" => rgb(218., 112., 214.),
        "palegoldenrod" => rgb(238., 232., 170.),
        "palegreen" => rgb(152., 251., 152.),
        "paleturquoise" => rgb(175., 238., 238.),
        "palevioletred" => rgb(219., 112., 147.),
        "papayawhip" => rgb(255., 239., 213.),
        "peachpuff" => rgb(255., 218., 185.),
        "peru" => rgb(205., 133., 63.),
        "pink" => rgb(255., 192., 203.),
        "plum" => rgb(221., 160., 221.),
        "powderblue" => rgb(176., 224., 230.),
        "rebeccapurple" => rgb(102., 51., 153.),
        "rosybrown" => rgb(188., 143., 143.),
        "royalblue" => rgb(65., 105., 225.),
        "saddlebrown" => rgb(139., 69., 19.),
        "salmon" => rgb(250., 128., 114.),
        "sandybrown" => rgb(244., 164., 96.),
        "seagreen" => rgb(46., 139., 87.),
        "seashell" => rgb(255., 245., 238.),
        "sienna" => rgb(160., 82., 45.),
        "skyblue" => rgb(135., 206., 235.),
        "slateblue" => rgb(106., 90., 205.),
        "slategray" => rgb(112., 128., 144.),
        "slategrey" => rgb(112., 128., 144.),
        "snow" => rgb(255., 250., 250.),
        "springgreen" => rgb(0., 255., 127.),
        "steelblue" => rgb(70., 130., 180.),
        "tan" => rgb(210., 180., 140.),
        "thistle" => rgb(216., 191., 216.),
        "tomato" => rgb(255., 99., 71.),
        "turquoise" => rgb(64., 224., 208.),
        "violet" => rgb(238., 130., 238.),
        "wheat" => rgb(245., 222., 179.),
        "whitesmoke" => rgb(245., 245., 245.),
        "yellowgreen" => rgb(154., 205., 50.),

        "transparent" => Ok(Color::RGBA(RGBA { red: 0., green: 0., blue: 0., alpha: 0. })),
        "currentcolor" => Ok(Color::CurrentColor),
        _ => Err(())
    }
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


#[inline]
fn parse_color_hash(value: &str) -> Result<Color, ()> {
    let value = value.as_bytes();
    match value.len() {
        8 => rgba(
            (try!(from_hex(value[0])) * 16 + try!(from_hex(value[1]))) as f32,
            (try!(from_hex(value[2])) * 16 + try!(from_hex(value[3]))) as f32,
            (try!(from_hex(value[4])) * 16 + try!(from_hex(value[5]))) as f32,
            (try!(from_hex(value[6])) * 16 + try!(from_hex(value[7]))) as f32,
        ),
        6 => rgb(
            (try!(from_hex(value[0])) * 16 + try!(from_hex(value[1]))) as f32,
            (try!(from_hex(value[2])) * 16 + try!(from_hex(value[3]))) as f32,
            (try!(from_hex(value[4])) * 16 + try!(from_hex(value[5]))) as f32,
        ),
        4 => rgba(
            (try!(from_hex(value[0])) * 17) as f32,
            (try!(from_hex(value[1])) * 17) as f32,
            (try!(from_hex(value[2])) * 17) as f32,
            (try!(from_hex(value[3])) * 17) as f32,
        ),
        3 => rgb(
            (try!(from_hex(value[0])) * 17) as f32,
            (try!(from_hex(value[1])) * 17) as f32,
            (try!(from_hex(value[2])) * 17) as f32,
        ),
        _ => Err(())
    }
}


#[inline]
fn parse_color_function(name: &str, arguments: &mut Parser) -> Result<Color, ()> {
    let (is_rgb, has_alpha) = match_ignore_ascii_case! { name,
        "rgba" => (true, true),
        "rgb" => (true, false),
        "hsl" => (false, false),
        "hsla" => (false, true),
        _ => return Err(())
    };

    fn clamp(val: f32) -> f32 {
        val.max(0.).min(1.)
    }

    let mut red: f32;
    let mut green: f32;
    let mut blue: f32;
    if is_rgb {
        // Either integers or percentages, but all the same type.
        match try!(arguments.next()) {
            Token::Number(ref v) if v.int_value.is_some() => {
                red = v.value / 255.;
                try!(arguments.expect_comma());
                green = try!(arguments.expect_integer()) as f32 / 255.;
                try!(arguments.expect_comma());
                blue = try!(arguments.expect_integer()) as f32 / 255.;
            }
            Token::Percentage(ref v) => {
                red = v.unit_value;
                try!(arguments.expect_comma());
                green = try!(arguments.expect_percentage());
                try!(arguments.expect_comma());
                blue = try!(arguments.expect_percentage());
            }
            _ => return Err(())
        };
        // The spec says to clamp to the device gamut which may be wider than 0% ... 100%,
        // but moz2d doesn’t seem to have any support for this, so let’s not bother.
        // https://drafts.csswg.org/css-color/#rgb-functions
        // https://github.com/servo/rust-cssparser/issues/76
        red = clamp(red);
        green = clamp(green);
        blue = clamp(blue);
    } else {
        let hue = try!(arguments.expect_number()) / 360.;
        let hue = hue - hue.floor();
        // Saturation and lightness are clamped to 0% ... 100%
        // regardless of device gamut:
        // https://drafts.csswg.org/css-color/#the-hsl-notation
        try!(arguments.expect_comma());
        let saturation = clamp(try!(arguments.expect_percentage()));
        try!(arguments.expect_comma());
        let lightness = clamp(try!(arguments.expect_percentage()));

        // https://drafts.csswg.org/css-color/#hsl-color
        fn hue_to_rgb(m1: f32, m2: f32, mut h: f32) -> f32 {
            if h < 0. { h += 1. }
            if h > 1. { h -= 1. }

            if h * 6. < 1. { m1 + (m2 - m1) * h * 6. }
            else if h * 2. < 1. { m2 }
            else if h * 3. < 2. { m1 + (m2 - m1) * (2. / 3. - h) * 6. }
            else { m1 }
        }
        let m2 = if lightness <= 0.5 { lightness * (saturation + 1.) }
                 else { lightness + saturation - lightness * saturation };
        let m1 = lightness * 2. - m2;
        red = hue_to_rgb(m1, m2, hue + 1. / 3.);
        green = hue_to_rgb(m1, m2, hue);
        blue = hue_to_rgb(m1, m2, hue - 1. / 3.);
    }

    let alpha = if has_alpha {
        try!(arguments.expect_comma());
        clamp(try!(arguments.expect_number()))
    } else {
        1.
    };
    try!(arguments.expect_exhausted());
    Ok(Color::RGBA(RGBA { red: red, green: green, blue: blue, alpha: alpha }))
}
