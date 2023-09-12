/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use crate::{ColorParser, PredefinedColorSpace, Color, RgbaLegacy};
use cssparser::{Parser, ParserInput};
use serde_json::{self, json, Value};

fn almost_equals(a: &Value, b: &Value) -> bool {
    match (a, b) {
        (&Value::Number(ref a), &Value::Number(ref b)) => {
            let a = a.as_f64().unwrap();
            let b = b.as_f64().unwrap();
            (a - b).abs() <= a.abs() * 1e-6
        }

        (&Value::Bool(a), &Value::Bool(b)) => a == b,
        (&Value::String(ref a), &Value::String(ref b)) => a == b,
        (&Value::Array(ref a), &Value::Array(ref b)) => {
            a.len() == b.len()
                && a.iter()
                    .zip(b.iter())
                    .all(|(ref a, ref b)| almost_equals(*a, *b))
        }
        (&Value::Object(_), &Value::Object(_)) => panic!("Not implemented"),
        (&Value::Null, &Value::Null) => true,
        _ => false,
    }
}

fn assert_json_eq(results: Value, expected: Value, message: &str) {
    if !almost_equals(&results, &expected) {
        println!(
            "{}",
            ::difference::Changeset::new(
                &serde_json::to_string_pretty(&results).unwrap(),
                &serde_json::to_string_pretty(&expected).unwrap(),
                "\n",
            )
        );
        panic!("{}", message)
    }
}


fn run_raw_json_tests<F: Fn(Value, Value) -> ()>(json_data: &str, run: F) {
    let items = match serde_json::from_str(json_data) {
        Ok(Value::Array(items)) => items,
        other => panic!("Invalid JSON: {:?}", other),
    };
    assert!(items.len() % 2 == 0);
    let mut input = None;
    for item in items.into_iter() {
        match (&input, item) {
            (&None, json_obj) => input = Some(json_obj),
            (&Some(_), expected) => {
                let input = input.take().unwrap();
                run(input, expected)
            }
        };
    }
}

fn run_json_tests<F: Fn(&mut Parser) -> Value>(json_data: &str, parse: F) {
    run_raw_json_tests(json_data, |input, expected| match input {
        Value::String(input) => {
            let mut parse_input = ParserInput::new(&input);
            let result = parse(&mut Parser::new(&mut parse_input));
            assert_json_eq(result, expected, &input);
        }
        _ => panic!("Unexpected JSON"),
    });
}
fn run_color_tests<F: Fn(Result<Color, ()>) -> Value>(json_data: &str, to_json: F) {
    run_json_tests(json_data, |input| {
        let result: Result<_, ParseError<()>> =
            input.parse_entirely(|i| Color::parse(i).map_err(Into::into));
        to_json(result.map_err(|_| ()))
    });
}

#[test]
fn color3() {
    run_color_tests(include_str!("../src/css-parsing-tests/color3.json"), |c| {
        c.ok()
            .map(|v| v.to_css_string().to_json())
            .unwrap_or(Value::Null)
    })
}

#[cfg_attr(all(miri, feature = "skip_long_tests"), ignore)]
#[test]
fn color3_hsl() {
    run_color_tests(include_str!("../src/css-parsing-tests/color3_hsl.json"), |c| {
        c.ok()
            .map(|v| v.to_css_string().to_json())
            .unwrap_or(Value::Null)
    })
}

/// color3_keywords.json is different: R, G and B are in 0..255 rather than 0..1
#[test]
fn color3_keywords() {
    run_color_tests(
        include_str!("../src/css-parsing-tests/color3_keywords.json"),
        |c| {
            c.ok()
                .map(|v| v.to_css_string().to_json())
                .unwrap_or(Value::Null)
        },
    )
}

#[cfg_attr(all(miri, feature = "skip_long_tests"), ignore)]
#[test]
fn color4_hwb() {
    run_color_tests(include_str!("../src/css-parsing-tests/color4_hwb.json"), |c| {
        c.ok()
            .map(|v| v.to_css_string().to_json())
            .unwrap_or(Value::Null)
    })
}

#[cfg_attr(all(miri, feature = "skip_long_tests"), ignore)]
#[test]
fn color4_lab_lch_oklab_oklch() {
    run_color_tests(
        include_str!("../src/css-parsing-tests/color4_lab_lch_oklab_oklch.json"),
        |c| {
            c.ok()
                .map(|v| v.to_css_string().to_json())
                .unwrap_or(Value::Null)
        },
    )
}

#[test]
fn color4_color_function() {
    run_color_tests(
        include_str!("../src/css-parsing-tests/color4_color_function.json"),
        |c| {
            c.ok()
                .map(|v| v.to_css_string().to_json())
                .unwrap_or(Value::Null)
        },
    )
}

macro_rules! parse_single_color {
    ($i:expr) => {{
        let input = $i;
        let mut input = ParserInput::new(input);
        let mut input = Parser::new(&mut input);
        Color::parse(&mut input).map_err(Into::<ParseError<()>>::into)
    }};
}

#[test]
fn color4_invalid_color_space() {
    let result = parse_single_color!("color(invalid 1 1 1)");
    assert!(result.is_err());
}

#[test]
fn serialize_current_color() {
    let c = Color::CurrentColor;
    assert!(c.to_css_string() == "currentcolor");
}

#[test]
fn serialize_rgb_full_alpha() {
    let c = Color::Rgba(RgbaLegacy::new(255, 230, 204, 1.0));
    assert_eq!(c.to_css_string(), "rgb(255, 230, 204)");
}

#[test]
fn serialize_rgba() {
    let c = Color::Rgba(RgbaLegacy::new(26, 51, 77, 0.125));
    assert_eq!(c.to_css_string(), "rgba(26, 51, 77, 0.125)");
}

#[test]
fn serialize_rgba_two_digit_float_if_roundtrips() {
    let c = Color::Rgba(RgbaLegacy::from_floats(0., 0., 0., 0.5));
    assert_eq!(c.to_css_string(), "rgba(0, 0, 0, 0.5)");
}

trait ToJson {
    fn to_json(&self) -> Value;
}

impl<T> ToJson for T
where
    T: Clone,
    Value: From<T>,
{
    fn to_json(&self) -> Value {
        Value::from(self.clone())
    }
}

impl ToJson for Color {
    fn to_json(&self) -> Value {
        match *self {
            Color::CurrentColor => "currentcolor".to_json(),
            Color::Rgba(ref rgba) => {
                json!([rgba.red, rgba.green, rgba.blue, rgba.alpha])
            }
            Color::Hsl(ref c) => json!([c.hue, c.saturation, c.lightness, c.alpha]),
            Color::Hwb(ref c) => json!([c.hue, c.whiteness, c.blackness, c.alpha]),
            Color::Lab(ref c) => json!([c.lightness, c.a, c.b, c.alpha]),
            Color::Lch(ref c) => json!([c.lightness, c.chroma, c.hue, c.alpha]),
            Color::Oklab(ref c) => json!([c.lightness, c.a, c.b, c.alpha]),
            Color::Oklch(ref c) => json!([c.lightness, c.chroma, c.hue, c.alpha]),
            Color::ColorFunction(ref c) => {
                json!([c.color_space.as_str(), c.c1, c.c2, c.c3, c.alpha])
            }
        }
    }
}

#[test]
fn generic_parser() {
    #[derive(Debug, PartialEq)]
    enum OutputType {
        CurrentColor,
        Rgba(u8, u8, u8, f32),
        Hsl(Option<f32>, Option<f32>, Option<f32>, Option<f32>),
        Hwb(Option<f32>, Option<f32>, Option<f32>, Option<f32>),
        Lab(Option<f32>, Option<f32>, Option<f32>, Option<f32>),
        Lch(Option<f32>, Option<f32>, Option<f32>, Option<f32>),
        Oklab(Option<f32>, Option<f32>, Option<f32>, Option<f32>),
        Oklch(Option<f32>, Option<f32>, Option<f32>, Option<f32>),
        ColorFunction(
            PredefinedColorSpace,
            Option<f32>,
            Option<f32>,
            Option<f32>,
            Option<f32>,
        ),
    }

    impl FromParsedColor for OutputType {
        fn from_current_color() -> Self {
            OutputType::CurrentColor
        }

        fn from_rgba(red: u8, green: u8, blue: u8, alpha: f32) -> Self {
            OutputType::Rgba(red, green, blue, alpha)
        }

        fn from_hsl(
            hue: Option<f32>,
            saturation: Option<f32>,
            lightness: Option<f32>,
            alpha: Option<f32>,
        ) -> Self {
            OutputType::Hsl(hue, saturation, lightness, alpha)
        }

        fn from_hwb(
            hue: Option<f32>,
            blackness: Option<f32>,
            whiteness: Option<f32>,
            alpha: Option<f32>,
        ) -> Self {
            OutputType::Hwb(hue, blackness, whiteness, alpha)
        }

        fn from_lab(
            lightness: Option<f32>,
            a: Option<f32>,
            b: Option<f32>,
            alpha: Option<f32>,
        ) -> Self {
            OutputType::Lab(lightness, a, b, alpha)
        }

        fn from_lch(
            lightness: Option<f32>,
            chroma: Option<f32>,
            hue: Option<f32>,
            alpha: Option<f32>,
        ) -> Self {
            OutputType::Lch(lightness, chroma, hue, alpha)
        }

        fn from_oklab(
            lightness: Option<f32>,
            a: Option<f32>,
            b: Option<f32>,
            alpha: Option<f32>,
        ) -> Self {
            OutputType::Oklab(lightness, a, b, alpha)
        }

        fn from_oklch(
            lightness: Option<f32>,
            chroma: Option<f32>,
            hue: Option<f32>,
            alpha: Option<f32>,
        ) -> Self {
            OutputType::Oklch(lightness, chroma, hue, alpha)
        }

        fn from_color_function(
            color_space: PredefinedColorSpace,
            c1: Option<f32>,
            c2: Option<f32>,
            c3: Option<f32>,
            alpha: Option<f32>,
        ) -> Self {
            OutputType::ColorFunction(color_space, c1, c2, c3, alpha)
        }
    }

    struct TestColorParser;
    impl<'i> ColorParser<'i> for TestColorParser {
        type Output = OutputType;
        type Error = ();
    }

    #[rustfmt::skip]
    const TESTS: &[(&str, OutputType)] = &[
        ("currentColor",                OutputType::CurrentColor),
        ("rgb(1, 2, 3)",                OutputType::Rgba(1, 2, 3, 1.0)),
        ("rgba(1, 2, 3, 0.4)",          OutputType::Rgba(1, 2, 3, 0.4)),
        ("rgb(none none none / none)",  OutputType::Rgba(0, 0, 0, 0.0)),
        ("rgb(1 none 3 / none)",        OutputType::Rgba(1, 0, 3, 0.0)),

        ("hsla(45deg, 20%, 30%, 0.4)",  OutputType::Hsl(Some(45.0), Some(0.2), Some(0.3), Some(0.4))),
        ("hsl(45deg none none)",        OutputType::Hsl(Some(45.0), None, None, Some(1.0))),
        ("hsl(none 10% none / none)",   OutputType::Hsl(None, Some(0.1), None, None)),
        ("hsl(120 100.0% 50.0%)",       OutputType::Hsl(Some(120.0), Some(1.0), Some(0.5), Some(1.0))),

        ("hwb(45deg 20% 30% / 0.4)",    OutputType::Hwb(Some(45.0), Some(0.2), Some(0.3), Some(0.4))),

        ("lab(100 20 30 / 0.4)",        OutputType::Lab(Some(100.0), Some(20.0), Some(30.0), Some(0.4))),
        ("lch(100 20 30 / 0.4)",        OutputType::Lch(Some(100.0), Some(20.0), Some(30.0), Some(0.4))),

        ("oklab(100 20 30 / 0.4)",      OutputType::Oklab(Some(100.0), Some(20.0), Some(30.0), Some(0.4))),
        ("oklch(100 20 30 / 0.4)",      OutputType::Oklch(Some(100.0), Some(20.0), Some(30.0), Some(0.4))),

        ("color(srgb 0.1 0.2 0.3 / 0.4)",           OutputType::ColorFunction(PredefinedColorSpace::Srgb, Some(0.1), Some(0.2), Some(0.3), Some(0.4))),
        ("color(srgb none none none)",              OutputType::ColorFunction(PredefinedColorSpace::Srgb, None, None, None, Some(1.0))),
        ("color(srgb none none none / none)",       OutputType::ColorFunction(PredefinedColorSpace::Srgb, None, None, None, None)),
        ("color(srgb-linear 0.1 0.2 0.3 / 0.4)",    OutputType::ColorFunction(PredefinedColorSpace::SrgbLinear, Some(0.1), Some(0.2), Some(0.3), Some(0.4))),
        ("color(display-p3 0.1 0.2 0.3 / 0.4)",     OutputType::ColorFunction(PredefinedColorSpace::DisplayP3, Some(0.1), Some(0.2), Some(0.3), Some(0.4))),
        ("color(a98-rgb 0.1 0.2 0.3 / 0.4)",        OutputType::ColorFunction(PredefinedColorSpace::A98Rgb, Some(0.1), Some(0.2), Some(0.3), Some(0.4))),
        ("color(prophoto-rgb 0.1 0.2 0.3 / 0.4)",   OutputType::ColorFunction(PredefinedColorSpace::ProphotoRgb, Some(0.1), Some(0.2), Some(0.3), Some(0.4))),
        ("color(rec2020 0.1 0.2 0.3 / 0.4)",        OutputType::ColorFunction(PredefinedColorSpace::Rec2020, Some(0.1), Some(0.2), Some(0.3), Some(0.4))),
        ("color(xyz-d50 0.1 0.2 0.3 / 0.4)",        OutputType::ColorFunction(PredefinedColorSpace::XyzD50, Some(0.1), Some(0.2), Some(0.3), Some(0.4))),
        ("color(xyz-d65 0.1 0.2 0.3 / 0.4)",        OutputType::ColorFunction(PredefinedColorSpace::XyzD65, Some(0.1), Some(0.2), Some(0.3), Some(0.4))),
    ];

    for (input, expected) in TESTS {
        let mut input = ParserInput::new(*input);
        let mut input = Parser::new(&mut input);

        let actual: OutputType = parse_color_with(&TestColorParser, &mut input).unwrap();
        assert_eq!(actual, *expected);
    }
}

#[test]
fn serialize_modern_components() {
    // None.
    assert_eq!(ModernComponent(&None).to_css_string(), "none".to_string());

    // Finite values.
    assert_eq!(
        ModernComponent(&Some(10.0)).to_css_string(),
        "10".to_string()
    );
    assert_eq!(
        ModernComponent(&Some(-10.0)).to_css_string(),
        "-10".to_string()
    );
    assert_eq!(ModernComponent(&Some(0.0)).to_css_string(), "0".to_string());
    assert_eq!(
        ModernComponent(&Some(-0.0)).to_css_string(),
        "0".to_string()
    );

    // Infinite values.
    assert_eq!(
        ModernComponent(&Some(f32::INFINITY)).to_css_string(),
        "calc(infinity)".to_string()
    );
    assert_eq!(
        ModernComponent(&Some(f32::NEG_INFINITY)).to_css_string(),
        "calc(-infinity)".to_string()
    );

    // NaN.
    assert_eq!(
        ModernComponent(&Some(f32::NAN)).to_css_string(),
        "calc(NaN)".to_string()
    );
}
