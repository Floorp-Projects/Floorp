/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />

<%helpers:shorthand name="outline"
                    engines="gecko servo-2013"
                    sub_properties="outline-color outline-style outline-width"
                    derive_serialize="True"
                    spec="https://drafts.csswg.org/css-ui/#propdef-outline">
    use crate::properties::longhands::{outline_color, outline_width, outline_style};
    use crate::values::specified;
    use crate::parser::Parse;

    pub fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Longhands, ParseError<'i>> {
        let _unused = context;
        let mut color = None;
        let mut style = None;
        let mut width = None;
        let mut any = false;
        loop {
            if color.is_none() {
                if let Ok(value) = input.try_parse(|i| specified::Color::parse(context, i)) {
                    color = Some(value);
                    any = true;
                    continue
                }
            }
            if style.is_none() {
                if let Ok(value) = input.try_parse(|input| outline_style::parse(context, input)) {
                    style = Some(value);
                    any = true;
                    continue
                }
            }
            if width.is_none() {
                if let Ok(value) = input.try_parse(|input| outline_width::parse(context, input)) {
                    width = Some(value);
                    any = true;
                    continue
                }
            }
            break
        }
        if any {
            Ok(expanded! {
                outline_color: unwrap_or_initial!(outline_color, color),
                outline_style: unwrap_or_initial!(outline_style, style),
                outline_width: unwrap_or_initial!(outline_width, width),
            })
        } else {
            Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
        }
    }
</%helpers:shorthand>
