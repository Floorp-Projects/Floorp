/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />

<%helpers:shorthand
    name="text-emphasis"
    engines="gecko"
    sub_properties="text-emphasis-style text-emphasis-color"
    derive_serialize="True"
    spec="https://drafts.csswg.org/css-text-decor-3/#text-emphasis-property"
>
    use crate::properties::longhands::{text_emphasis_color, text_emphasis_style};

    pub fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Longhands, ParseError<'i>> {
        let mut color = None;
        let mut style = None;

        loop {
            if color.is_none() {
                if let Ok(value) = input.try_parse(|input| text_emphasis_color::parse(context, input)) {
                    color = Some(value);
                    continue
                }
            }
            if style.is_none() {
                if let Ok(value) = input.try_parse(|input| text_emphasis_style::parse(context, input)) {
                    style = Some(value);
                    continue
                }
            }
            break
        }
        if color.is_some() || style.is_some() {
            Ok(expanded! {
                text_emphasis_color: unwrap_or_initial!(text_emphasis_color, color),
                text_emphasis_style: unwrap_or_initial!(text_emphasis_style, style),
            })
        } else {
            Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
        }
    }
</%helpers:shorthand>

<%helpers:shorthand
    name="text-wrap"
    engines="gecko"
    sub_properties="text-wrap-mode text-wrap-style"
    spec="https://www.w3.org/TR/css-text-4/#text-wrap"
>
    use crate::properties::longhands::{text_wrap_mode, text_wrap_style};

    pub fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Longhands, ParseError<'i>> {
        let mut mode = None;
        let mut style = None;

        loop {
            if mode.is_none() {
                if let Ok(value) = input.try_parse(|input| text_wrap_mode::parse(context, input)) {
                    mode = Some(value);
                    continue
                }
            }
            if style.is_none() {
                if let Ok(value) = input.try_parse(|input| text_wrap_style::parse(context, input)) {
                    style = Some(value);
                    continue
                }
            }
            break
        }
        if mode.is_some() || style.is_some() {
            Ok(expanded! {
                text_wrap_mode: unwrap_or_initial!(text_wrap_mode, mode),
                text_wrap_style: unwrap_or_initial!(text_wrap_style, style),
            })
        } else {
            Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
        }
    }

    impl<'a> ToCss for LonghandsToSerialize<'a>  {
        fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: fmt::Write {
            use text_wrap_mode::computed_value::T as Mode;
            use text_wrap_style::computed_value::T as Style;

            if matches!(self.text_wrap_style, None | Some(&Style::Auto)) {
                return self.text_wrap_mode.to_css(dest);
            }

            if *self.text_wrap_mode != Mode::Wrap {
                self.text_wrap_mode.to_css(dest)?;
                dest.write_char(' ')?;
            }

            self.text_wrap_style.to_css(dest)
        }
    }
</%helpers:shorthand>

<%helpers:shorthand
    name="white-space"
    engines="gecko"
    sub_properties="text-wrap-mode white-space-collapse"
    spec="https://www.w3.org/TR/css-text-4/#white-space-property"
>
    use crate::properties::longhands::{text_wrap_mode, white_space_collapse};

    pub fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Longhands, ParseError<'i>> {
        use white_space_collapse::computed_value::T as Collapse;
        use text_wrap_mode::computed_value::T as Wrap;

        fn parse_special_shorthands<'i, 't>(input: &mut Parser<'i, 't>) -> Result<Longhands, ParseError<'i>> {
            let (mode, collapse) = try_match_ident_ignore_ascii_case! { input,
                "normal" => (Wrap::Wrap, Collapse::Collapse),
                "pre" => (Wrap::Nowrap, Collapse::Preserve),
                "pre-wrap" => (Wrap::Wrap, Collapse::Preserve),
                "pre-line" => (Wrap::Wrap, Collapse::PreserveBreaks),
                // TODO: deprecate/remove -moz-pre-space; the white-space-collapse: preserve-spaces value
                // should serve this purpose?
                "-moz-pre-space" => (Wrap::Wrap, Collapse::PreserveSpaces),
            };
            Ok(expanded! {
                text_wrap_mode: mode,
                white_space_collapse: collapse,
            })
        }

        if let Ok(result) = input.try_parse(parse_special_shorthands) {
            return Ok(result);
        }

        let mut wrap = None;
        let mut collapse = None;

        loop {
            if wrap.is_none() {
                if let Ok(value) = input.try_parse(|input| text_wrap_mode::parse(context, input)) {
                    wrap = Some(value);
                    continue
                }
            }
            if collapse.is_none() {
                if let Ok(value) = input.try_parse(|input| white_space_collapse::parse(context, input)) {
                    collapse = Some(value);
                    continue
                }
            }
            break
        }

        if wrap.is_some() || collapse.is_some() {
            Ok(expanded! {
                text_wrap_mode: unwrap_or_initial!(text_wrap_mode, wrap),
                white_space_collapse: unwrap_or_initial!(white_space_collapse, collapse),
            })
        } else {
            Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
        }
    }

    impl<'a> ToCss for LonghandsToSerialize<'a>  {
        fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: fmt::Write {
            use white_space_collapse::computed_value::T as Collapse;
            use text_wrap_mode::computed_value::T as Wrap;

            match *self.text_wrap_mode {
                Wrap::Wrap => {
                    match *self.white_space_collapse {
                        Collapse::Collapse => return dest.write_str("normal"),
                        Collapse::Preserve => return dest.write_str("pre-wrap"),
                        Collapse::PreserveBreaks => return dest.write_str("pre-line"),
                        Collapse::PreserveSpaces => return dest.write_str("-moz-pre-space"),
                        _ => (),
                    }
                },
                Wrap::Nowrap => {
                    if let Collapse::Preserve = *self.white_space_collapse {
                        return dest.write_str("pre");
                    }
                },
            }

            let mut has_value = false;
            if *self.white_space_collapse != Collapse::Collapse {
                self.white_space_collapse.to_css(dest)?;
                has_value = true;
            }

            if *self.text_wrap_mode != Wrap::Wrap {
                if has_value {
                    dest.write_char(' ')?;
                }
                self.text_wrap_mode.to_css(dest)?;
            }

            Ok(())
        }
    }
</%helpers:shorthand>

// CSS Compatibility
// https://compat.spec.whatwg.org/
<%helpers:shorthand name="-webkit-text-stroke"
                    engines="gecko"
                    sub_properties="-webkit-text-stroke-width
                                    -webkit-text-stroke-color"
                    derive_serialize="True"
                    spec="https://compat.spec.whatwg.org/#the-webkit-text-stroke">
    use crate::properties::longhands::{_webkit_text_stroke_color, _webkit_text_stroke_width};

    pub fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Longhands, ParseError<'i>> {
        let mut color = None;
        let mut width = None;
        loop {
            if color.is_none() {
                if let Ok(value) = input.try_parse(|input| _webkit_text_stroke_color::parse(context, input)) {
                    color = Some(value);
                    continue
                }
            }

            if width.is_none() {
                if let Ok(value) = input.try_parse(|input| _webkit_text_stroke_width::parse(context, input)) {
                    width = Some(value);
                    continue
                }
            }
            break
        }

        if color.is_some() || width.is_some() {
            Ok(expanded! {
                _webkit_text_stroke_color: unwrap_or_initial!(_webkit_text_stroke_color, color),
                _webkit_text_stroke_width: unwrap_or_initial!(_webkit_text_stroke_width, width),
            })
        } else {
            Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
        }
    }
</%helpers:shorthand>
