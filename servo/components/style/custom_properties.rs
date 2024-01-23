/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Support for [custom properties for cascading variables][custom].
//!
//! [custom]: https://drafts.csswg.org/css-variables/

use crate::applicable_declarations::CascadePriority;
use crate::media_queries::Device;
use crate::properties::{
    CSSWideKeyword, CustomDeclaration, CustomDeclarationValue, LonghandId, LonghandIdSet,
    VariableDeclaration,
};
use crate::properties_and_values::{
    registry::PropertyRegistration,
    value::{AllowComputationallyDependent, SpecifiedValue as SpecifiedRegisteredValue},
};
use crate::custom_properties_map::CustomPropertiesMap;
use crate::selector_map::{PrecomputedHashMap, PrecomputedHashSet};
use crate::stylesheets::UrlExtraData;
use crate::stylist::Stylist;
use crate::values::computed;
use crate::values::specified::FontRelativeLength;
use crate::Atom;
use cssparser::{
    CowRcStr, Delimiter, Parser, ParserInput, SourcePosition, Token, TokenSerializationType,
};
use selectors::parser::SelectorParseErrorKind;
use servo_arc::Arc;
use smallvec::SmallVec;
use std::borrow::Cow;
use std::cmp;
use std::collections::hash_map::Entry;
use std::fmt::{self, Write};
use std::ops::{Index, IndexMut};
use style_traits::{CssWriter, ParseError, StyleParseErrorKind, ToCss};

/// The environment from which to get `env` function values.
///
/// TODO(emilio): If this becomes a bit more complex we should probably move it
/// to the `media_queries` module, or something.
#[derive(Debug, MallocSizeOf)]
pub struct CssEnvironment;

type EnvironmentEvaluator = fn(device: &Device, url_data: &UrlExtraData) -> VariableValue;

struct EnvironmentVariable {
    name: Atom,
    evaluator: EnvironmentEvaluator,
}

macro_rules! make_variable {
    ($name:expr, $evaluator:expr) => {{
        EnvironmentVariable {
            name: $name,
            evaluator: $evaluator,
        }
    }};
}

fn get_safearea_inset_top(device: &Device, url_data: &UrlExtraData) -> VariableValue {
    VariableValue::pixels(device.safe_area_insets().top, url_data)
}

fn get_safearea_inset_bottom(device: &Device, url_data: &UrlExtraData) -> VariableValue {
    VariableValue::pixels(device.safe_area_insets().bottom, url_data)
}

fn get_safearea_inset_left(device: &Device, url_data: &UrlExtraData) -> VariableValue {
    VariableValue::pixels(device.safe_area_insets().left, url_data)
}

fn get_safearea_inset_right(device: &Device, url_data: &UrlExtraData) -> VariableValue {
    VariableValue::pixels(device.safe_area_insets().right, url_data)
}

fn get_content_preferred_color_scheme(device: &Device, url_data: &UrlExtraData) -> VariableValue {
    use crate::gecko::media_features::PrefersColorScheme;
    let prefers_color_scheme = unsafe {
        crate::gecko_bindings::bindings::Gecko_MediaFeatures_PrefersColorScheme(
            device.document(),
            /* use_content = */ true,
        )
    };
    VariableValue::ident(
        match prefers_color_scheme {
            PrefersColorScheme::Light => "light",
            PrefersColorScheme::Dark => "dark",
        },
        url_data,
    )
}

fn get_scrollbar_inline_size(device: &Device, url_data: &UrlExtraData) -> VariableValue {
    VariableValue::pixels(device.scrollbar_inline_size().px(), url_data)
}

static ENVIRONMENT_VARIABLES: [EnvironmentVariable; 4] = [
    make_variable!(atom!("safe-area-inset-top"), get_safearea_inset_top),
    make_variable!(atom!("safe-area-inset-bottom"), get_safearea_inset_bottom),
    make_variable!(atom!("safe-area-inset-left"), get_safearea_inset_left),
    make_variable!(atom!("safe-area-inset-right"), get_safearea_inset_right),
];

macro_rules! lnf_int {
    ($id:ident) => {
        unsafe {
            crate::gecko_bindings::bindings::Gecko_GetLookAndFeelInt(
                crate::gecko_bindings::bindings::LookAndFeel_IntID::$id as i32,
            )
        }
    };
}

macro_rules! lnf_int_variable {
    ($atom:expr, $id:ident, $ctor:ident) => {{
        fn __eval(_: &Device, url_data: &UrlExtraData) -> VariableValue {
            VariableValue::$ctor(lnf_int!($id), url_data)
        }
        make_variable!($atom, __eval)
    }};
}

static CHROME_ENVIRONMENT_VARIABLES: [EnvironmentVariable; 7] = [
    lnf_int_variable!(
        atom!("-moz-gtk-csd-titlebar-radius"),
        TitlebarRadius,
        int_pixels
    ),
    lnf_int_variable!(
        atom!("-moz-gtk-csd-close-button-position"),
        GTKCSDCloseButtonPosition,
        integer
    ),
    lnf_int_variable!(
        atom!("-moz-gtk-csd-minimize-button-position"),
        GTKCSDMinimizeButtonPosition,
        integer
    ),
    lnf_int_variable!(
        atom!("-moz-gtk-csd-maximize-button-position"),
        GTKCSDMaximizeButtonPosition,
        integer
    ),
    lnf_int_variable!(
        atom!("-moz-overlay-scrollbar-fade-duration"),
        ScrollbarFadeDuration,
        int_ms
    ),
    make_variable!(
        atom!("-moz-content-preferred-color-scheme"),
        get_content_preferred_color_scheme
    ),
    make_variable!(atom!("scrollbar-inline-size"), get_scrollbar_inline_size),
];

impl CssEnvironment {
    #[inline]
    fn get(&self, name: &Atom, device: &Device, url_data: &UrlExtraData) -> Option<VariableValue> {
        if let Some(var) = ENVIRONMENT_VARIABLES.iter().find(|var| var.name == *name) {
            return Some((var.evaluator)(device, url_data));
        }
        if !url_data.chrome_rules_enabled() {
            return None;
        }
        let var = CHROME_ENVIRONMENT_VARIABLES
            .iter()
            .find(|var| var.name == *name)?;
        Some((var.evaluator)(device, url_data))
    }
}

/// A custom property name is just an `Atom`.
///
/// Note that this does not include the `--` prefix
pub type Name = Atom;

/// Parse a custom property name.
///
/// <https://drafts.csswg.org/css-variables/#typedef-custom-property-name>
pub fn parse_name(s: &str) -> Result<&str, ()> {
    if s.starts_with("--") && s.len() > 2 {
        Ok(&s[2..])
    } else {
        Err(())
    }
}

/// A value for a custom property is just a set of tokens.
///
/// We preserve the original CSS for serialization, and also the variable
/// references to other custom property names.
#[derive(Clone, Debug, MallocSizeOf, ToShmem)]
pub struct VariableValue {
    /// The raw CSS string.
    pub css: String,

    /// The url data of the stylesheet where this value came from.
    pub url_data: UrlExtraData,

    first_token_type: TokenSerializationType,
    last_token_type: TokenSerializationType,

    /// var(), env(), or non-custom property (e.g. through `em`) references.
    references: References,
}

trivial_to_computed_value!(VariableValue);

// For all purposes, we want values to be considered equal if their css text is equal.
impl PartialEq for VariableValue {
    fn eq(&self, other: &Self) -> bool {
        self.css == other.css
    }
}

impl Eq for VariableValue {}

impl ToCss for SpecifiedValue {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_str(&self.css)
    }
}

/// A pair of separate CustomPropertiesMaps, split between custom properties
/// that have the inherit flag set and those with the flag unset.
#[repr(C)]
#[derive(Clone, Debug, Default, PartialEq)]
pub struct ComputedCustomProperties {
    /// Map for custom properties with inherit flag set, including non-registered
    /// ones.
    pub inherited: CustomPropertiesMap,
    /// Map for custom properties with inherit flag unset.
    pub non_inherited: CustomPropertiesMap,
}

impl ComputedCustomProperties {
    /// Return whether the inherited and non_inherited maps are none.
    pub fn is_empty(&self) -> bool {
        self.inherited.is_empty() && self.non_inherited.is_empty()
    }

    /// Return the name and value of the property at specified index, if any.
    pub fn property_at(&self, index: usize) -> Option<(&Name, &Option<Arc<VariableValue>>)> {
        // Just expose the custom property items from custom_properties.inherited, followed
        // by custom property items from custom_properties.non_inherited.
        self.inherited.get_index(index)
            .or_else(|| self.non_inherited.get_index(index - self.inherited.len()))
    }

    /// Insert a custom property in the corresponding inherited/non_inherited
    /// map, depending on whether the inherit flag is set or unset.
    fn insert(
        &mut self,
        registration: Option<&PropertyRegistration>,
        name: &Name,
        value: Arc<VariableValue>,
    ) {
        self.map_mut(registration).insert(name, value)
    }

    /// Remove a custom property from the corresponding inherited/non_inherited
    /// map, depending on whether the inherit flag is set or unset.
    fn remove(
        &mut self,
        registration: Option<&PropertyRegistration>,
        name: &Name,
    ) {
        self.map_mut(registration).remove(name);
    }

    /// Shrink the capacity of the inherited maps as much as possible.
    fn shrink_to_fit(&mut self) {
        self.inherited.shrink_to_fit();
        self.non_inherited.shrink_to_fit();
    }

    fn map_mut(&mut self, registration: Option<&PropertyRegistration>) -> &mut CustomPropertiesMap {
        if registration.map_or(true, |r| r.inherits()) {
            &mut self.inherited
        } else {
            &mut self.non_inherited
        }
    }

    fn get(&self, stylist: &Stylist, name: &Name) -> Option<&Arc<VariableValue>> {
        let registration = stylist.get_custom_property_registration(&name);
        if registration.map_or(true, |r| r.inherits()) {
            self.inherited.get(name)
        } else {
            self.non_inherited.get(name)
        }
    }
}

/// Both specified and computed values are VariableValues, the difference is
/// whether var() functions are expanded.
pub type SpecifiedValue = VariableValue;
/// Both specified and computed values are VariableValues, the difference is
/// whether var() functions are expanded.
pub type ComputedValue = VariableValue;

/// Set of flags to non-custom references this custom property makes.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, MallocSizeOf, ToShmem)]
struct NonCustomReferences(u8);

bitflags! {
    impl NonCustomReferences: u8 {
        /// At least one custom property depends on font-relative units.
        const FONT_UNITS = 1 << 0;
        /// At least one custom property depends on root element's font-relative units.
        const ROOT_FONT_UNITS = 1 << 1;
        /// At least one custom property depends on line height units.
        const LH_UNITS = 1 << 2;
        /// At least one custom property depends on root element's line height units.
        const ROOT_LH_UNITS = 1 << 3;
        /// All dependencies not depending on the root element.
        const NON_ROOT_DEPENDENCIES = Self::FONT_UNITS.bits() | Self::LH_UNITS.bits();
        /// All dependencies depending on the root element.
        const ROOT_DEPENDENCIES = Self::ROOT_FONT_UNITS.bits() | Self::ROOT_LH_UNITS.bits();
    }
}

impl NonCustomReferences {
    fn for_each<F>(&self, mut f: F)
    where
        F: FnMut(SingleNonCustomReference),
    {
        for (_, r) in self.iter_names() {
            let single = match r {
                Self::FONT_UNITS => SingleNonCustomReference::FontUnits,
                Self::ROOT_FONT_UNITS => SingleNonCustomReference::RootFontUnits,
                Self::LH_UNITS => SingleNonCustomReference::LhUnits,
                Self::ROOT_LH_UNITS => SingleNonCustomReference::RootLhUnits,
                _ => unreachable!("Unexpected single bit value"),
            };
            f(single);
        }
    }

    fn from_unit(value: &CowRcStr) -> Self {
        // For registered properties, any reference to font-relative dimensions
        // make it dependent on font-related properties.
        // TODO(dshin): When we unit algebra gets implemented and handled -
        // Is it valid to say that `calc(1em / 2em * 3px)` triggers this?
        if value.eq_ignore_ascii_case(FontRelativeLength::LH) {
            return Self::FONT_UNITS | Self::LH_UNITS;
        }
        if value.eq_ignore_ascii_case(FontRelativeLength::EM) ||
            value.eq_ignore_ascii_case(FontRelativeLength::EX) ||
            value.eq_ignore_ascii_case(FontRelativeLength::CAP) ||
            value.eq_ignore_ascii_case(FontRelativeLength::CH) ||
            value.eq_ignore_ascii_case(FontRelativeLength::IC)
        {
            return Self::FONT_UNITS;
        }
        if value.eq_ignore_ascii_case(FontRelativeLength::RLH) {
            return Self::ROOT_FONT_UNITS | Self::ROOT_LH_UNITS;
        }
        if value.eq_ignore_ascii_case(FontRelativeLength::REM) {
            return Self::ROOT_FONT_UNITS;
        }
        Self::empty()
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum SingleNonCustomReference {
    FontUnits = 0,
    RootFontUnits,
    LhUnits,
    RootLhUnits,
}

struct NonCustomReferenceMap<T>([Option<T>; 4]);

impl<T> Default for NonCustomReferenceMap<T> {
    fn default() -> Self {
        NonCustomReferenceMap(Default::default())
    }
}

impl<T> Index<SingleNonCustomReference> for NonCustomReferenceMap<T> {
    type Output = Option<T>;

    fn index(&self, reference: SingleNonCustomReference) -> &Self::Output {
        &self.0[reference as usize]
    }
}

impl<T> IndexMut<SingleNonCustomReference> for NonCustomReferenceMap<T> {
    fn index_mut(&mut self, reference: SingleNonCustomReference) -> &mut Self::Output {
        &mut self.0[reference as usize]
    }
}

/// Whether to defer resolving custom properties referencing font relative units.
#[derive(Clone, Copy, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum DeferFontRelativeCustomPropertyResolution {
    Yes,
    No,
}


/// A struct holding information about the external references to that a custom
/// property value may have.
#[derive(Clone, Debug, Default, MallocSizeOf, PartialEq, ToShmem)]
struct References {
    custom_properties: PrecomputedHashSet<Name>,
    environment: bool,
    non_custom_references: NonCustomReferences,
}

impl References {
    fn has_references(&self) -> bool {
        self.environment || !self.custom_properties.is_empty()
    }

    fn get_non_custom_dependencies(&self, is_root_element: bool) -> NonCustomReferences {
        let mask = NonCustomReferences::NON_ROOT_DEPENDENCIES;
        let mask = if is_root_element {
            mask | NonCustomReferences::ROOT_DEPENDENCIES
        } else {
            mask
        };

        self.non_custom_references & mask
    }
}

impl VariableValue {
    fn empty(url_data: &UrlExtraData) -> Self {
        Self {
            css: String::new(),
            last_token_type: Default::default(),
            first_token_type: Default::default(),
            url_data: url_data.clone(),
            references: Default::default(),
        }
    }

    /// Create a new custom property without parsing if the CSS is known to be valid and contain no
    /// references.
    pub fn new(
        css: String,
        url_data: &UrlExtraData,
        first_token_type: TokenSerializationType,
        last_token_type: TokenSerializationType,
    ) -> Self {
        Self {
            css,
            url_data: url_data.clone(),
            first_token_type,
            last_token_type,
            references: Default::default(),
        }
    }

    fn push<'i>(
        &mut self,
        input: &Parser<'i, '_>,
        css: &str,
        css_first_token_type: TokenSerializationType,
        css_last_token_type: TokenSerializationType,
    ) -> Result<(), ParseError<'i>> {
        /// Prevent values from getting terribly big since you can use custom
        /// properties exponentially.
        ///
        /// This number (2MB) is somewhat arbitrary, but silly enough that no
        /// reasonable page should hit it. We could limit by number of total
        /// substitutions, but that was very easy to work around in practice
        /// (just choose a larger initial value and boom).
        const MAX_VALUE_LENGTH_IN_BYTES: usize = 2 * 1024 * 1024;

        if self.css.len() + css.len() > MAX_VALUE_LENGTH_IN_BYTES {
            return Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError));
        }

        // This happens e.g. between two subsequent var() functions:
        // `var(--a)var(--b)`.
        //
        // In that case, css_*_token_type is nonsensical.
        if css.is_empty() {
            return Ok(());
        }

        self.first_token_type.set_if_nothing(css_first_token_type);
        // If self.first_token_type was nothing,
        // self.last_token_type is also nothing and this will be false:
        if self
            .last_token_type
            .needs_separator_when_before(css_first_token_type)
        {
            self.css.push_str("/**/")
        }
        self.css.push_str(css);
        self.last_token_type = css_last_token_type;
        Ok(())
    }

    fn push_from<'i>(
        &mut self,
        input: &Parser<'i, '_>,
        position: (SourcePosition, TokenSerializationType),
        last_token_type: TokenSerializationType,
    ) -> Result<(), ParseError<'i>> {
        self.push(
            input,
            input.slice_from(position.0),
            position.1,
            last_token_type,
        )
    }

    fn push_variable<'i>(
        &mut self,
        input: &Parser<'i, '_>,
        variable: &ComputedValue,
    ) -> Result<(), ParseError<'i>> {
        debug_assert!(!variable.has_references(), "{}", variable.css);
        self.push(
            input,
            &variable.css,
            variable.first_token_type,
            variable.last_token_type,
        )
    }

    /// Parse a custom property value.
    pub fn parse<'i, 't>(
        input: &mut Parser<'i, 't>,
        url_data: &UrlExtraData,
    ) -> Result<Self, ParseError<'i>> {
        let mut references = References::default();
        let (first_token_type, css, last_token_type) =
            parse_self_contained_declaration_value(input, &mut references)?;

        let mut css = css.into_owned();
        css.shrink_to_fit();

        references.custom_properties.shrink_to_fit();

        Ok(Self {
            css,
            url_data: url_data.clone(),
            first_token_type,
            last_token_type,
            references,
        })
    }

    /// Create VariableValue from an int.
    fn integer(number: i32, url_data: &UrlExtraData) -> Self {
        Self::from_token(
            Token::Number {
                has_sign: false,
                value: number as f32,
                int_value: Some(number),
            },
            url_data,
        )
    }

    /// Create VariableValue from an int.
    fn ident(ident: &'static str, url_data: &UrlExtraData) -> Self {
        Self::from_token(Token::Ident(ident.into()), url_data)
    }

    /// Create VariableValue from a float amount of CSS pixels.
    fn pixels(number: f32, url_data: &UrlExtraData) -> Self {
        // FIXME (https://github.com/servo/rust-cssparser/issues/266):
        // No way to get TokenSerializationType::Dimension without creating
        // Token object.
        Self::from_token(
            Token::Dimension {
                has_sign: false,
                value: number,
                int_value: None,
                unit: CowRcStr::from("px"),
            },
            url_data,
        )
    }

    /// Create VariableValue from an integer amount of milliseconds.
    fn int_ms(number: i32, url_data: &UrlExtraData) -> Self {
        Self::from_token(
            Token::Dimension {
                has_sign: false,
                value: number as f32,
                int_value: Some(number),
                unit: CowRcStr::from("ms"),
            },
            url_data,
        )
    }

    /// Create VariableValue from an integer amount of CSS pixels.
    fn int_pixels(number: i32, url_data: &UrlExtraData) -> Self {
        Self::from_token(
            Token::Dimension {
                has_sign: false,
                value: number as f32,
                int_value: Some(number),
                unit: CowRcStr::from("px"),
            },
            url_data,
        )
    }

    fn from_token(token: Token, url_data: &UrlExtraData) -> Self {
        let token_type = token.serialization_type();
        let mut css = token.to_css_string();
        css.shrink_to_fit();

        VariableValue {
            css,
            url_data: url_data.clone(),
            first_token_type: token_type,
            last_token_type: token_type,
            references: Default::default(),
        }
    }

    /// Returns the raw CSS text from this VariableValue
    pub fn css_text(&self) -> &str {
        &self.css
    }

    /// Returns whether this variable value has any reference to the environment or other
    /// variables.
    pub fn has_references(&self) -> bool {
        self.references.has_references()
    }
}

fn parse_self_contained_declaration_value<'i, 't>(
    input: &mut Parser<'i, 't>,
    references: &mut References,
) -> Result<(TokenSerializationType, Cow<'i, str>, TokenSerializationType), ParseError<'i>> {
    let start_position = input.position();
    let mut missing_closing_characters = String::new();
    let (first, last) =
        parse_declaration_value(input, references, &mut missing_closing_characters)?;
    let mut css: Cow<str> = input.slice_from(start_position).into();
    if !missing_closing_characters.is_empty() {
        // Unescaped backslash at EOF in a quoted string is ignored.
        if css.ends_with("\\") && matches!(missing_closing_characters.as_bytes()[0], b'"' | b'\'') {
            css.to_mut().pop();
        }
        css.to_mut().push_str(&missing_closing_characters);
    }
    Ok((first, css, last))
}

/// <https://drafts.csswg.org/css-syntax-3/#typedef-declaration-value>
fn parse_declaration_value<'i, 't>(
    input: &mut Parser<'i, 't>,
    references: &mut References,
    missing_closing_characters: &mut String,
) -> Result<(TokenSerializationType, TokenSerializationType), ParseError<'i>> {
    input.parse_until_before(Delimiter::Bang | Delimiter::Semicolon, |input| {
        parse_declaration_value_block(input, references, missing_closing_characters)
    })
}

/// Like parse_declaration_value, but accept `!` and `;` since they are only
/// invalid at the top level
fn parse_declaration_value_block<'i, 't>(
    input: &mut Parser<'i, 't>,
    references: &mut References,
    missing_closing_characters: &mut String,
) -> Result<(TokenSerializationType, TokenSerializationType), ParseError<'i>> {
    input.skip_whitespace();
    let mut token_start = input.position();
    let mut token = match input.next_including_whitespace_and_comments() {
        Ok(token) => token,
        Err(_) => {
            return Ok(Default::default());
        },
    };
    let first_token_type = token.serialization_type();
    loop {
        macro_rules! nested {
            () => {
                input.parse_nested_block(|input| {
                    parse_declaration_value_block(
                        input,
                        references,
                        missing_closing_characters,
                    )
                })?
            };
        }
        macro_rules! check_closed {
            ($closing:expr) => {
                if !input.slice_from(token_start).ends_with($closing) {
                    missing_closing_characters.push_str($closing)
                }
            };
        }
        let last_token_type = match *token {
            Token::Comment(_) => {
                let serialization_type = token.serialization_type();
                let token_slice = input.slice_from(token_start);
                if !token_slice.ends_with("*/") {
                    missing_closing_characters.push_str(if token_slice.ends_with('*') {
                        "/"
                    } else {
                        "*/"
                    })
                }
                serialization_type
            },
            Token::BadUrl(ref u) => {
                let e = StyleParseErrorKind::BadUrlInDeclarationValueBlock(u.clone());
                return Err(input.new_custom_error(e));
            },
            Token::BadString(ref s) => {
                let e = StyleParseErrorKind::BadStringInDeclarationValueBlock(s.clone());
                return Err(input.new_custom_error(e));
            },
            Token::CloseParenthesis => {
                let e = StyleParseErrorKind::UnbalancedCloseParenthesisInDeclarationValueBlock;
                return Err(input.new_custom_error(e));
            },
            Token::CloseSquareBracket => {
                let e = StyleParseErrorKind::UnbalancedCloseSquareBracketInDeclarationValueBlock;
                return Err(input.new_custom_error(e));
            },
            Token::CloseCurlyBracket => {
                let e = StyleParseErrorKind::UnbalancedCloseCurlyBracketInDeclarationValueBlock;
                return Err(input.new_custom_error(e));
            },
            Token::Function(ref name) => {
                if name.eq_ignore_ascii_case("var") {
                    let args_start = input.state();
                    input.parse_nested_block(|input| {
                        parse_var_function(input, references)
                    })?;
                    input.reset(&args_start);
                } else if name.eq_ignore_ascii_case("env") {
                    let args_start = input.state();
                    input.parse_nested_block(|input| {
                        parse_env_function(input, references)
                    })?;
                    input.reset(&args_start);
                }
                nested!();
                check_closed!(")");
                Token::CloseParenthesis.serialization_type()
            },
            Token::ParenthesisBlock => {
                nested!();
                check_closed!(")");
                Token::CloseParenthesis.serialization_type()
            },
            Token::CurlyBracketBlock => {
                nested!();
                check_closed!("}");
                Token::CloseCurlyBracket.serialization_type()
            },
            Token::SquareBracketBlock => {
                nested!();
                check_closed!("]");
                Token::CloseSquareBracket.serialization_type()
            },
            Token::QuotedString(_) => {
                let serialization_type = token.serialization_type();
                let token_slice = input.slice_from(token_start);
                let quote = &token_slice[..1];
                debug_assert!(matches!(quote, "\"" | "'"));
                if !(token_slice.ends_with(quote) && token_slice.len() > 1) {
                    missing_closing_characters.push_str(quote)
                }
                serialization_type
            },
            Token::Ident(ref value) |
            Token::AtKeyword(ref value) |
            Token::Hash(ref value) |
            Token::IDHash(ref value) |
            Token::UnquotedUrl(ref value) |
            Token::Dimension {
                unit: ref value, ..
            } => {
                references.non_custom_references.insert(NonCustomReferences::from_unit(value));
                let serialization_type = token.serialization_type();
                let is_unquoted_url = matches!(token, Token::UnquotedUrl(_));
                if value.ends_with("�") && input.slice_from(token_start).ends_with("\\") {
                    // Unescaped backslash at EOF in these contexts is interpreted as U+FFFD
                    // Check the value in case the final backslash was itself escaped.
                    // Serialize as escaped U+FFFD, which is also interpreted as U+FFFD.
                    // (Unescaped U+FFFD would also work, but removing the backslash is annoying.)
                    missing_closing_characters.push_str("�")
                }
                if is_unquoted_url {
                    check_closed!(")");
                }
                serialization_type
            },
            _ => token.serialization_type(),
        };

        token_start = input.position();
        token = match input.next_including_whitespace_and_comments() {
            Ok(token) => token,
            Err(..) => return Ok((first_token_type, last_token_type)),
        };
    }
}

fn parse_fallback<'i, 't>(input: &mut Parser<'i, 't>) -> Result<(), ParseError<'i>> {
    // Exclude `!` and `;` at the top level
    // https://drafts.csswg.org/css-syntax/#typedef-declaration-value
    input.parse_until_before(Delimiter::Bang | Delimiter::Semicolon, |input| {
        // Skip until the end.
        while input.next_including_whitespace_and_comments().is_ok() {}
        Ok(())
    })
}

fn parse_and_substitute_fallback<'i>(
    input: &mut Parser<'i, '_>,
    custom_properties: &ComputedCustomProperties,
    url_data: &UrlExtraData,
    stylist: &Stylist,
    computed_context: &computed::Context,
) -> Result<ComputedValue, ParseError<'i>> {
    input.skip_whitespace();
    let after_comma = input.state();
    let first_token_type = input
        .next_including_whitespace_and_comments()
        .ok()
        .map_or_else(TokenSerializationType::default, |t| t.serialization_type());
    input.reset(&after_comma);
    let mut position = (after_comma.position(), first_token_type);

    let mut fallback = ComputedValue::empty(url_data);
    let last_token_type = substitute_block(
        input,
        &mut position,
        &mut fallback,
        custom_properties,
        stylist,
        computed_context,
    )?;
    fallback.push_from(input, position, last_token_type)?;
    Ok(fallback)
}

// If the var function is valid, return Ok((custom_property_name, fallback))
fn parse_var_function<'i, 't>(
    input: &mut Parser<'i, 't>,
    references: &mut References,
) -> Result<(), ParseError<'i>> {
    let name = input.expect_ident_cloned()?;
    let name = parse_name(&name).map_err(|()| {
        input.new_custom_error(SelectorParseErrorKind::UnexpectedIdent(name.clone()))
    })?;
    if input.try_parse(|input| input.expect_comma()).is_ok() {
        parse_fallback(input)?;
    }
    references.custom_properties.insert(Atom::from(name));
    Ok(())
}

fn parse_env_function<'i, 't>(
    input: &mut Parser<'i, 't>,
    references: &mut References,
) -> Result<(), ParseError<'i>> {
    // TODO(emilio): This should be <custom-ident> per spec, but no other
    // browser does that, see https://github.com/w3c/csswg-drafts/issues/3262.
    input.expect_ident()?;
    if input.try_parse(|input| input.expect_comma()).is_ok() {
        parse_fallback(input)?;
    }
    references.environment = true;
    Ok(())
}

/// A struct that takes care of encapsulating the cascade process for custom
/// properties.
pub struct CustomPropertiesBuilder<'a, 'b: 'a> {
    seen: PrecomputedHashSet<&'a Name>,
    may_have_cycles: bool,
    custom_properties: ComputedCustomProperties,
    reverted: PrecomputedHashMap<&'a Name, (CascadePriority, bool)>,
    stylist: &'a Stylist,
    computed_context: &'a mut computed::Context<'b>,
    references_from_non_custom_properties: NonCustomReferenceMap<Vec<Name>>,
}

impl<'a, 'b: 'a> CustomPropertiesBuilder<'a, 'b> {
    /// Create a new builder, inheriting from a given custom properties map.
    ///
    /// We expose this publicly mostly for @keyframe blocks.
    pub fn new_with_properties(stylist: &'a Stylist, custom_properties: ComputedCustomProperties, computed_context: &'a mut computed::Context<'b>) -> Self {
        Self {
            seen: PrecomputedHashSet::default(),
            reverted: Default::default(),
            may_have_cycles: false,
            custom_properties,
            stylist,
            computed_context,
            references_from_non_custom_properties: NonCustomReferenceMap::default(),
        }
    }

    /// Create a new builder, inheriting from the right style given context.
    pub fn new(stylist: &'a Stylist, context: &'a mut computed::Context<'b>) -> Self {
        let is_root_element = context.is_root_element();

        let inherited = context.inherited_custom_properties();
        let initial_values = stylist.get_custom_property_initial_values();
        let properties = ComputedCustomProperties {
            inherited: if is_root_element {
                debug_assert!(inherited.is_empty());
                initial_values.inherited.clone()
            } else {
                inherited.inherited.clone()
            },
            non_inherited: initial_values.non_inherited.clone(),
        };

        // Reuse flags from computing registered custom properties initial values, such as
        // whether they depend on viewport units.
        context.style().add_flags(stylist.get_custom_property_initial_values_flags());
        Self::new_with_properties(stylist, properties, context)
    }

    /// Cascade a given custom property declaration.
    pub fn cascade(&mut self, declaration: &'a CustomDeclaration, priority: CascadePriority) {
        let CustomDeclaration {
            ref name,
            ref value,
        } = *declaration;

        if let Some(&(reverted_priority, is_origin_revert)) = self.reverted.get(&name) {
            if !reverted_priority.allows_when_reverted(&priority, is_origin_revert) {
                return;
            }
        }

        let was_already_present = !self.seen.insert(name);
        if was_already_present {
            return;
        }

        if !self.value_may_affect_style(name, value) {
            return;
        }

        let map = &mut self.custom_properties;
        let custom_registration = self.stylist.get_custom_property_registration(&name);
        match *value {
            CustomDeclarationValue::Value(ref unparsed_value) => {
                let has_custom_property_references =
                    !unparsed_value.references.custom_properties.is_empty();
                let registered_length_property = custom_registration.map_or(
                    false,
                    |r| r.syntax.may_reference_font_relative_length()
                );
                // Non-custom dependency is really relevant for registered custom properties
                // that require computed value of such dependencies.
                let has_non_custom_dependencies = registered_length_property &&
                    !unparsed_value
                        .references
                        .get_non_custom_dependencies(self.computed_context.is_root_element())
                        .is_empty();
                self.may_have_cycles |=
                    has_custom_property_references || has_non_custom_dependencies;

                // If the variable value has no references and it has an environment variable here,
                // perform substitution here instead of forcing a full traversal in
                // `substitute_all` afterwards.
                if !has_custom_property_references && !has_non_custom_dependencies {
                    if unparsed_value.references.environment {
                        substitute_references_in_value_and_apply(
                            name,
                            unparsed_value,
                            map,
                            self.stylist,
                            self.computed_context,
                        );
                        return;
                    }
                    if let Some(registration) = custom_registration {
                        let mut input = ParserInput::new(&unparsed_value.css);
                        let mut input = Parser::new(&mut input);
                        if let Ok(value) = SpecifiedRegisteredValue::compute(
                            &mut input,
                            registration,
                            &unparsed_value.url_data,
                            self.computed_context,
                            AllowComputationallyDependent::Yes,
                        ) {
                            map.insert(custom_registration, name, value);
                        } else {
                            let inherited = self.computed_context.inherited_custom_properties();
                            let is_root_element = self.computed_context.is_root_element();
                            handle_invalid_at_computed_value_time(
                                name,
                                map,
                                inherited,
                                self.stylist,
                                is_root_element,
                            );
                        }
                        return;
                    }
                }
                map.insert(
                    custom_registration,
                    name,
                    Arc::clone(unparsed_value),
                );
            },
            CustomDeclarationValue::CSSWideKeyword(keyword) => match keyword {
                CSSWideKeyword::RevertLayer | CSSWideKeyword::Revert => {
                    let origin_revert = keyword == CSSWideKeyword::Revert;
                    self.seen.remove(name);
                    self.reverted.insert(name, (priority, origin_revert));
                },
                CSSWideKeyword::Initial => {
                    // For non-inherited custom properties, 'initial' was handled in value_may_affect_style.
                    debug_assert!(
                        custom_registration.map_or(true, |r| r.inherits()),
                        "Should've been handled earlier"
                    );
                    map.remove(custom_registration, name);
                    if let Some(registration) = custom_registration {
                        if let Some(ref initial_value) = registration.initial_value {
                            map.insert(custom_registration, name, initial_value.clone());
                        }
                    }
                },
                CSSWideKeyword::Inherit => {
                    // For inherited custom properties, 'inherit' was handled in value_may_affect_style.
                    debug_assert!(
                        !custom_registration.map_or(true, |r| r.inherits()),
                        "Should've been handled earlier"
                    );
                    if let Some(inherited_value) = self
                        .computed_context
                        .inherited_custom_properties()
                        .non_inherited
                        .get(name)
                    {
                        map.insert(custom_registration, name, inherited_value.clone());
                    }
                },
                // handled in value_may_affect_style
                CSSWideKeyword::Unset => unreachable!(),
            },
        }
    }

    /// Note a non-custom property with variable reference that may in turn depend on that property.
    /// e.g. `font-size` depending on a custom property that may be a registered property using `em`.
    pub fn note_potentially_cyclic_non_custom_dependency(&mut self, id: LonghandId, decl: &VariableDeclaration) {
        // With unit algebra in `calc()`, references aren't limited to `font-size`.
        // For example, `--foo: 100ex; font-weight: calc(var(--foo) / 1ex);`,
        // or `--foo: 1em; zoom: calc(var(--foo) * 30px / 2em);`
        let references = match id {
            LonghandId::FontSize => {
                if self.computed_context.is_root_element() {
                    NonCustomReferences::ROOT_FONT_UNITS
                } else {
                    NonCustomReferences::FONT_UNITS
                }
            },
            LonghandId::LineHeight => {
                if self.computed_context.is_root_element() {
                    NonCustomReferences::ROOT_LH_UNITS |
                        NonCustomReferences::ROOT_FONT_UNITS
                } else {
                    NonCustomReferences::LH_UNITS | NonCustomReferences::FONT_UNITS
                }
            },
            _ => return,
        };
        let variables: &std::collections::HashSet<Atom, std::hash::BuildHasherDefault<crate::selector_map::PrecomputedHasher>> = &decl.value.variable_value.references.custom_properties;
        if variables.is_empty() {
            return;
        }

        let variables: Vec<Atom> = variables.into_iter().filter(|name| {
            let registration = match self.stylist.get_custom_property_registration(name) {
                Some(r) => r,
                None => return false,
            };
            registration.syntax.may_compute_length()
        }).map(|name| name.clone()).collect();
        references.for_each(|idx| {
            let entry = &mut self.references_from_non_custom_properties[idx];
            let was_none = entry.is_none();
            let v = entry.get_or_insert_with(|| variables.clone());
            if was_none {
                return;
            }
            v.extend(variables.clone().into_iter());
        });
    }

    fn value_may_affect_style(&self, name: &Name, value: &CustomDeclarationValue) -> bool {
        let custom_registration = self.stylist.get_custom_property_registration(&name);
        match *value {
            CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Inherit) => {
                // For inherited custom properties, explicit 'inherit' means we
                // can just use any existing value in the inherited
                // CustomPropertiesMap.
                if custom_registration.map_or(true, |r| r.inherits()) {
                    return false;
                }
            },
            CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Initial) => {
                // For non-inherited custom properties, explicit 'initial' means
                // we can just use any initial value in the registration.
                if !custom_registration.map_or(true, |r| r.inherits()) {
                    return false;
                }
            },
            CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Unset) => {
                // Explicit 'unset' means we can either just use any existing
                // value in the inherited CustomPropertiesMap or the initial
                // value in the registration.
                return false;
            },
            _ => {},
        }

        let existing_value = self.custom_properties.get(self.stylist, &name);
        match (existing_value, value) {
            (None, &CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Initial)) => {
                debug_assert!(
                    custom_registration.map_or(true, |r| r.inherits()),
                    "Should've been handled earlier"
                );
                // The initial value of a custom property without a
                // guaranteed-invalid initial value is the same as it
                // not existing in the map.
                if custom_registration.map_or(true, |r| r.initial_value.is_none()) {
                    return false;
                }
            },
            (
                Some(existing_value),
                &CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Initial),
            ) => {
                debug_assert!(
                    custom_registration.map_or(true, |r| r.inherits()),
                    "Should've been handled earlier"
                );
                // Don't bother overwriting an existing value with the initial
                // value specified in the registration.
                if let Some(registration) = custom_registration {
                    if Some(existing_value) == registration.initial_value.as_ref() {
                        return false;
                    }
                }
            },
            (Some(_), &CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Inherit)) => {
                debug_assert!(
                    !custom_registration.map_or(true, |r| r.inherits()),
                    "Should've been handled earlier"
                );
                // existing_value is the registered initial value.
                // Don't bother adding it to self.custom_properties.non_inherited
                // if the key is also absent from self.inherited.non_inherited.
                if self
                    .computed_context
                    .inherited_custom_properties()
                    .non_inherited
                    .get(name)
                    .is_none()
                {
                    return false;
                }
            },
            (Some(existing_value), &CustomDeclarationValue::Value(ref value)) => {
                // Don't bother overwriting an existing value with the same
                // specified value.
                if existing_value == value {
                    return false;
                }
            },
            _ => {},
        }

        true
    }

    /// Computes the map of applicable custom properties, as well as
    /// longhand properties that are now considered invalid-at-compute time.
    /// The result is saved into the computed context.
    ///
    /// If there was any specified property or non-inherited custom property
    /// with an initial value, we've created a new map and now we
    /// need to remove any potential cycles (And marking non-custom
    /// properties), and wrap it in an arc.
    ///
    /// Some registered custom properties may require font-related properties
    /// be resolved to resolve. If these properties are not resolved at this time,
    /// `defer` should be set to `Yes`, which will leave such custom properties,
    /// and other properties referencing them, untouched. These properties are
    /// returned separately, to be resolved by `build_deferred` to fully resolve
    /// all custom properties after all necessary non-custom properties are resolved.
    pub fn build(
        mut self,
        defer: DeferFontRelativeCustomPropertyResolution,
    ) -> Option<ComputedCustomProperties> {
        let mut deferred_custom_properties = None;
        if self.may_have_cycles {
            if defer == DeferFontRelativeCustomPropertyResolution::Yes {
                deferred_custom_properties = Some(ComputedCustomProperties::default());
            }
            let mut invalid_non_custom_properties = LonghandIdSet::default();
            substitute_all(
                &mut self.custom_properties,
                deferred_custom_properties.as_mut(),
                &mut invalid_non_custom_properties,
                &self.seen,
                &self.references_from_non_custom_properties,
                self.stylist,
                self.computed_context,
            );
            self.computed_context.builder.invalid_non_custom_properties = invalid_non_custom_properties;
        }

        self.custom_properties.shrink_to_fit();

        // Some pages apply a lot of redundant custom properties, see e.g.
        // bug 1758974 comment 5. Try to detect the case where the values
        // haven't really changed, and save some memory by reusing the inherited
        // map in that case.
        let initial_values = self.stylist.get_custom_property_initial_values();
        self.computed_context.builder.custom_properties = ComputedCustomProperties {
            inherited: if self
                .computed_context
                .inherited_custom_properties()
                .inherited == self.custom_properties.inherited
            {
                self.computed_context
                    .inherited_custom_properties()
                    .inherited
                    .clone()
            } else {
                self.custom_properties.inherited
            },
            non_inherited: if initial_values.non_inherited == self.custom_properties.non_inherited {
                initial_values.non_inherited.clone()
            } else {
                self.custom_properties.non_inherited
            },
        };

        deferred_custom_properties
    }

    /// Fully resolve all deferred custom properties, assuming that the incoming context
    /// has necessary properties resolved.
    pub fn build_deferred(
        deferred: ComputedCustomProperties,
        stylist: &Stylist,
        computed_context: &mut computed::Context,
    ) {
        if deferred.is_empty() {
            return;
        }
        // Guaranteed to not have cycles at this point.
        let substitute =
            |deferred: &CustomPropertiesMap,
             stylist: &Stylist,
             context: &computed::Context,
             custom_properties: &mut ComputedCustomProperties| {
                // Since `CustomPropertiesMap` preserves insertion order, we shouldn't
                // have to worry about resolving in a wrong order.
                for (k, v) in deferred.iter() {
                    let v = match v {
                        None => continue,
                        Some(v) => v,
                    };
                    if v.has_references() {
                        substitute_references_in_value_and_apply(
                            k,
                            v.as_ref(),
                            custom_properties,
                            stylist,
                            context,
                        );
                    } else {
                        let mut input = ParserInput::new(&v.css);
                        let mut input = Parser::new(&mut input);
                        let registration =
                            stylist.get_custom_property_registration(k)
                            .expect("No references, must be registered custom property depending on font-relative properties");
                        if let Ok(value) = SpecifiedRegisteredValue::compute(
                            &mut input,
                            registration,
                            &v.url_data,
                            context,
                            AllowComputationallyDependent::Yes,
                        ) {
                            custom_properties.insert(Some(registration), k, value);
                        } else {
                            handle_invalid_at_computed_value_time(
                                k,
                                custom_properties,
                                context.inherited_custom_properties(),
                                stylist,
                                context.is_root_element(),
                            );
                        }
                    }
                }
            };
        let mut custom_properties = std::mem::take(&mut computed_context.builder.custom_properties);
        substitute(
            &deferred.inherited,
            stylist,
            computed_context,
            &mut custom_properties,
        );
        substitute(
            &deferred.non_inherited,
            stylist,
            computed_context,
            &mut custom_properties,
        );
        computed_context.builder.custom_properties = custom_properties;
    }
}

/// Resolve all custom properties to either substituted, invalid, or unset
/// (meaning we should use the inherited value).
///
/// It does cycle dependencies removal at the same time as substitution.
fn substitute_all(
    custom_properties_map: &mut ComputedCustomProperties,
    mut deferred_properties_map: Option<&mut ComputedCustomProperties>,
    invalid_non_custom_properties: &mut LonghandIdSet,
    seen: &PrecomputedHashSet<&Name>,
    references_from_non_custom_properties: &NonCustomReferenceMap<Vec<Name>>,
    stylist: &Stylist,
    computed_context: &computed::Context,
) {
    // The cycle dependencies removal in this function is a variant
    // of Tarjan's algorithm. It is mostly based on the pseudo-code
    // listed in
    // https://en.wikipedia.org/w/index.php?
    // title=Tarjan%27s_strongly_connected_components_algorithm&oldid=801728495

    #[derive(Clone, Eq, PartialEq, Debug)]
    enum VarType {
        Custom(Name),
        NonCustom(SingleNonCustomReference),
    }

    /// Struct recording necessary information for each variable.
    #[derive(Debug)]
    struct VarInfo {
        /// The name of the variable. It will be taken to save addref
        /// when the corresponding variable is popped from the stack.
        /// This also serves as a mark for whether the variable is
        /// currently in the stack below.
        var: Option<VarType>,
        /// If the variable is in a dependency cycle, lowlink represents
        /// a smaller index which corresponds to a variable in the same
        /// strong connected component, which is known to be accessible
        /// from this variable. It is not necessarily the root, though.
        lowlink: usize,
    }
    /// Context struct for traversing the variable graph, so that we can
    /// avoid referencing all the fields multiple times.
    struct Context<'a, 'b: 'a> {
        /// Number of variables visited. This is used as the order index
        /// when we visit a new unresolved variable.
        count: usize,
        /// The map from custom property name to its order index.
        index_map: PrecomputedHashMap<Name, usize>,
        /// Mapping from a non-custom dependency to its order index.
        non_custom_index_map: NonCustomReferenceMap<usize>,
        /// Information of each variable indexed by the order index.
        var_info: SmallVec<[VarInfo; 5]>,
        /// The stack of order index of visited variables. It contains
        /// all unfinished strong connected components.
        stack: SmallVec<[usize; 5]>,
        /// References to non-custom properties in this strongly connected component.
        non_custom_references: NonCustomReferences,
        map: &'a mut ComputedCustomProperties,
        /// The stylist is used to get registered properties, and to resolve the environment to
        /// substitute `env()` variables.
        stylist: &'a Stylist,
        /// The computed context is used to get inherited custom
        /// properties  and compute registered custom properties.
        computed_context: &'a computed::Context<'b>,
        /// Longhand IDs that became invalid due to dependency cycle(s).
        invalid_non_custom_properties: &'a mut LonghandIdSet,
        /// Properties that cannot yet be substituted.
        deferred_properties: Option<&'a mut ComputedCustomProperties>,
    }

    /// This function combines the traversal for cycle removal and value
    /// substitution. It returns either a signal None if this variable
    /// has been fully resolved (to either having no reference or being
    /// marked invalid), or the order index for the given name.
    ///
    /// When it returns, the variable corresponds to the name would be
    /// in one of the following states:
    /// * It is still in context.stack, which means it is part of an
    ///   potentially incomplete dependency circle.
    /// * It has been removed from the map.  It can be either that the
    ///   substitution failed, or it is inside a dependency circle.
    ///   When this function removes a variable from the map because
    ///   of dependency circle, it would put all variables in the same
    ///   strong connected component to the set together.
    /// * It doesn't have any reference, because either this variable
    ///   doesn't have reference at all in specified value, or it has
    ///   been completely resolved.
    /// * There is no such variable at all.
    fn traverse<'a, 'b>(
        var: VarType,
        non_custom_references: &NonCustomReferenceMap<Vec<Name>>,
        context: &mut Context<'a, 'b>,
    ) -> Option<usize> {
        // Some shortcut checks.
        let (value, should_substitute) = match var {
            VarType::Custom(ref name) => {
                let value = context.map.get(context.stylist, name)?;

                let non_custom_references = value
                    .references
                    .get_non_custom_dependencies(context.computed_context.is_root_element());
                let has_custom_property_reference = !value.references.custom_properties.is_empty();
                // Nothing to resolve.
                if !has_custom_property_reference && non_custom_references.is_empty() {
                    debug_assert!(
                        !value.references.environment,
                        "Should've been handled earlier"
                    );
                    return None;
                }

                // Has this variable been visited?
                match context.index_map.entry(name.clone()) {
                    Entry::Occupied(entry) => {
                        return Some(*entry.get());
                    },
                    Entry::Vacant(entry) => {
                        entry.insert(context.count);
                    },
                }
                context.non_custom_references |= value.as_ref().references.non_custom_references;

                // Hold a strong reference to the value so that we don't
                // need to keep reference to context.map.
                (Some(value.clone()), has_custom_property_reference)
            },
            VarType::NonCustom(ref non_custom) => {
                let entry = &mut context.non_custom_index_map[*non_custom];
                if let Some(v) = entry {
                    return Some(*v);
                }
                *entry = Some(context.count);
                (None, false)
            },
        };

        // Add new entry to the information table.
        let index = context.count;
        context.count += 1;
        debug_assert_eq!(index, context.var_info.len());
        context.var_info.push(VarInfo {
            var: Some(var.clone()),
            lowlink: index,
        });
        context.stack.push(index);

        let mut self_ref = false;
        let mut lowlink = index;
        let visit_link =
            |var: VarType, context: &mut Context, lowlink: &mut usize, self_ref: &mut bool| {
                let next_index = match traverse(var, non_custom_references, context) {
                    Some(index) => index,
                    // There is nothing to do if the next variable has been
                    // fully resolved at this point.
                    None => {
                        return;
                    },
                };
                let next_info = &context.var_info[next_index];
                if next_index > index {
                    // The next variable has a larger index than us, so it
                    // must be inserted in the recursive call above. We want
                    // to get its lowlink.
                    *lowlink = cmp::min(*lowlink, next_info.lowlink);
                } else if next_index == index {
                    *self_ref = true;
                } else if next_info.var.is_some() {
                    // The next variable has a smaller order index and it is
                    // in the stack, so we are at the same component.
                    *lowlink = cmp::min(*lowlink, next_index);
                }
            };
        if let Some(ref v) = value.as_ref() {
            debug_assert!(
                matches!(var, VarType::Custom(_)),
                "Non-custom property has references?"
            );

            // Visit other custom properties...
            for next in v.references.custom_properties.iter() {
                visit_link(
                    VarType::Custom(next.clone()),
                    context,
                    &mut lowlink,
                    &mut self_ref,
                );
            }

            // ... Then non-custom properties.
            v.references.non_custom_references.for_each(|r| {
                visit_link(VarType::NonCustom(r), context, &mut lowlink, &mut self_ref);
            });
        } else if let VarType::NonCustom(non_custom) = var {
            let entry = &non_custom_references[non_custom];
            if let Some(deps) = entry.as_ref() {
                for d in deps {
                    // Visit any reference from this non-custom property to custom properties.
                    visit_link(
                        VarType::Custom(d.clone()),
                        context,
                        &mut lowlink,
                        &mut self_ref,
                    );
                }
            }
        }

        context.var_info[index].lowlink = lowlink;
        if lowlink != index {
            // This variable is in a loop, but it is not the root of
            // this strong connected component. We simply return for
            // now, and the root would remove it from the map.
            //
            // This cannot be removed from the map here, because
            // otherwise the shortcut check at the beginning of this
            // function would return the wrong value.
            return Some(index);
        }

        // This is the root of a strong-connected component.
        let mut in_loop = self_ref;
        let name;

        let handle_variable_in_loop = |name: &Name, context: &mut Context<'a, 'b>| {
            if context
                .non_custom_references
                .intersects(NonCustomReferences::FONT_UNITS | NonCustomReferences::ROOT_FONT_UNITS)
            {
                context
                    .invalid_non_custom_properties
                    .insert(LonghandId::FontSize);
            }
            if context.non_custom_references.intersects(
                NonCustomReferences::LH_UNITS |
                    NonCustomReferences::ROOT_LH_UNITS,
            ) {
                context
                    .invalid_non_custom_properties
                    .insert(LonghandId::LineHeight);
            }
            // This variable is in loop. Resolve to invalid.
            handle_invalid_at_computed_value_time(
                name,
                context.map,
                context.computed_context.inherited_custom_properties(),
                context.stylist,
                context.computed_context.is_root_element(),
            );
        };
        loop {
            let var_index = context
                .stack
                .pop()
                .expect("The current variable should still be in stack");
            let var_info = &mut context.var_info[var_index];
            // We should never visit the variable again, so it's safe
            // to take the name away, so that we don't do additional
            // reference count.
            let var_name = var_info
                .var
                .take()
                .expect("Variable should not be poped from stack twice");
            if var_index == index {
                name = match var_name {
                    VarType::Custom(name) => name,
                    // At the root of this component, and it's a non-custom
                    // reference - we have nothing to substitute, so
                    // it's effectively resolved.
                    VarType::NonCustom(..) => return None,
                };
                break;
            }
            if let VarType::Custom(name) = var_name {
                // Anything here is in a loop which can traverse to the
                // variable we are handling, so it's invalid at
                // computed-value time.
                handle_variable_in_loop(&name, context);
            }
            in_loop = true;
        }
        // We've gotten to the root of this strongly connected component, so clear
        // whether or not it involved non-custom references.
        // It's fine to track it like this, because non-custom properties currently
        // being tracked can only participate in any loop only once.
        if in_loop {
            handle_variable_in_loop(&name, context);
            context.non_custom_references = NonCustomReferences::default();
            return None;
        }

        if let Some(ref v) = value.as_ref() {
            let registration = context.stylist.get_custom_property_registration(&name);
            let registered_length_property = registration.map_or(
                false,
                |r| r.syntax.may_reference_font_relative_length()
            );
            let mut defer = false;
            if !context.non_custom_references.is_empty() && registered_length_property {
                if let Some(deferred) = &mut context.deferred_properties {
                    // This property directly depends on a non-custom property, defer resolving it.
                    deferred.insert(registration, &name, (*v).clone());
                    context.map.remove(registration, &name);
                    defer = true;
                }
            }
            if should_substitute && !defer {
                for e in v.references.custom_properties.iter() {
                    if let Some(deferred) = &mut context.deferred_properties {
                        if deferred.get(context.stylist, e).is_some() {
                            // This property depends on a custom property that depends on a non-custom property, defer.
                            deferred.insert(registration, &name, (*v).clone());
                            context.map.remove(registration, &name);
                            defer = true;
                            break;
                        }
                    }
                }
                if !defer {
                    substitute_references_in_value_and_apply(
                        &name,
                        v,
                        &mut context.map,
                        context.stylist,
                        context.computed_context,
                    );
                }
            }
        }
        context.non_custom_references = NonCustomReferences::default();

        // All resolved, so return the signal value.
        None
    }

    // Note that `seen` doesn't contain names inherited from our parent, but
    // those can't have variable references (since we inherit the computed
    // variables) so we don't want to spend cycles traversing them anyway.
    for name in seen {
        let mut context = Context {
            count: 0,
            index_map: PrecomputedHashMap::default(),
            non_custom_index_map: NonCustomReferenceMap::default(),
            stack: SmallVec::new(),
            var_info: SmallVec::new(),
            map: custom_properties_map,
            non_custom_references: NonCustomReferences::default(),
            stylist,
            computed_context,
            invalid_non_custom_properties,
            deferred_properties: deferred_properties_map.as_deref_mut(),
        };
        traverse(
            VarType::Custom((*name).clone()),
            references_from_non_custom_properties,
            &mut context,
        );
    }
}

// See https://drafts.csswg.org/css-variables-2/#invalid-at-computed-value-time
fn handle_invalid_at_computed_value_time(
    name: &Name,
    custom_properties: &mut ComputedCustomProperties,
    inherited: &ComputedCustomProperties,
    stylist: &Stylist,
    is_root_element: bool,
) {
    let custom_registration = stylist.get_custom_property_registration(&name);
    if let Some(ref registration) = custom_registration {
        if !registration.syntax.is_universal() {
            // For the root element, inherited maps are empty. We should just
            // use the initial value if any, rather than removing the name.
            if registration.inherits() && !is_root_element {
                if let Some(value) = inherited.get(stylist, name) {
                    custom_properties.insert(custom_registration, name, Arc::clone(value));
                    return;
                }
            } else {
                if let Some(ref initial_value) = registration.initial_value {
                    custom_properties.insert(
                        custom_registration,
                        name,
                        Arc::clone(initial_value),
                    );
                    return;
                }
            }
        }
    }
    custom_properties.remove(custom_registration, name);
}

/// Replace `var()` and `env()` functions in a pre-existing variable value.
fn substitute_references_in_value_and_apply(
    name: &Name,
    value: &VariableValue,
    custom_properties: &mut ComputedCustomProperties,
    stylist: &Stylist,
    computed_context: &computed::Context,
) {
    debug_assert!(value.has_references());

    let inherited = computed_context.inherited_custom_properties();
    let is_root_element = computed_context.is_root_element();
    let custom_registration = stylist.get_custom_property_registration(&name);
    let mut computed_value = ComputedValue::empty(&value.url_data);

    {
        let mut input = ParserInput::new(&value.css);
        let mut input = Parser::new(&mut input);
        let mut position = (input.position(), value.first_token_type);

        let last_token_type = substitute_block(
            &mut input,
            &mut position,
            &mut computed_value,
            custom_properties,
            stylist,
            computed_context,
        );

        let last_token_type = match last_token_type {
            Ok(t) => t,
            Err(..) => {
                handle_invalid_at_computed_value_time(
                    name,
                    custom_properties,
                    inherited,
                    stylist,
                    is_root_element,
                );
                return;
            },
        };

        if computed_value
            .push_from(&input, position, last_token_type)
            .is_err()
        {
            handle_invalid_at_computed_value_time(
                name,
                custom_properties,
                inherited,
                stylist,
                is_root_element,
            );
            return;
        }
    }

    let should_insert = {
        let mut input = ParserInput::new(&computed_value.css);
        let mut input = Parser::new(&mut input);

        // If variable fallback results in a wide keyword, deal with it now.
        let inherits = custom_registration.map_or(true, |r| r.inherits());

        if let Ok(kw) = input.try_parse(CSSWideKeyword::parse) {
            // TODO: It's unclear what this should do for revert / revert-layer, see
            // https://github.com/w3c/csswg-drafts/issues/9131. For now treating as unset
            // seems fine?
            match (kw, inherits, is_root_element) {
                (CSSWideKeyword::Initial, _, _) |
                (CSSWideKeyword::Revert, false, _) |
                (CSSWideKeyword::RevertLayer, false, _) |
                (CSSWideKeyword::Unset, false, _) |
                (CSSWideKeyword::Revert, true, true) |
                (CSSWideKeyword::RevertLayer, true, true) |
                (CSSWideKeyword::Unset, true, true) |
                (CSSWideKeyword::Inherit, _, true) => {
                    custom_properties.remove(custom_registration, name);
                    if let Some(registration) = custom_registration {
                        if let Some(ref initial_value) = registration.initial_value {
                            custom_properties.insert(
                                custom_registration,
                                name,
                                Arc::clone(initial_value),
                            );
                        }
                    }
                },
                (CSSWideKeyword::Revert, true, false) |
                (CSSWideKeyword::RevertLayer, true, false) |
                (CSSWideKeyword::Inherit, _, false) |
                (CSSWideKeyword::Unset, true, false) => {
                    match inherited.get(stylist, name) {
                        Some(value) => {
                            custom_properties.insert(
                                custom_registration,
                                name,
                                Arc::clone(value),
                            );
                        },
                        None => {
                            custom_properties.remove(custom_registration, name);
                        },
                    };
                },
            }
            false
        } else {
            if let Some(registration) = custom_registration {
                if let Ok(value) = SpecifiedRegisteredValue::compute(
                    &mut input,
                    registration,
                    &computed_value.url_data,
                    computed_context,
                    AllowComputationallyDependent::Yes,
                ) {
                    custom_properties.insert(custom_registration, name, value);
                } else {
                    handle_invalid_at_computed_value_time(
                        name,
                        custom_properties,
                        inherited,
                        stylist,
                        is_root_element,
                    );
                }
                return;
            }
            true
        }
    };
    if should_insert {
        computed_value.css.shrink_to_fit();
        custom_properties.insert(custom_registration, name, Arc::new(computed_value));
    }
}

/// Replace `var()` functions in an arbitrary bit of input.
///
/// If the variable has its initial value, the callback should return `Err(())`
/// and leave `partial_computed_value` unchanged.
///
/// Otherwise, it should push the value of the variable (with its own `var()` functions replaced)
/// to `partial_computed_value` and return `Ok(last_token_type of what was pushed)`
///
/// Return `Err(())` if `input` is invalid at computed-value time.
/// or `Ok(last_token_type that was pushed to partial_computed_value)` otherwise.
fn substitute_block<'i>(
    input: &mut Parser<'i, '_>,
    position: &mut (SourcePosition, TokenSerializationType),
    partial_computed_value: &mut ComputedValue,
    custom_properties: &ComputedCustomProperties,
    stylist: &Stylist,
    computed_context: &computed::Context,
) -> Result<TokenSerializationType, ParseError<'i>> {
    let mut last_token_type = TokenSerializationType::default();
    let mut set_position_at_next_iteration = false;
    loop {
        let before_this_token = input.position();
        let next = input.next_including_whitespace_and_comments();
        if set_position_at_next_iteration {
            *position = (
                before_this_token,
                match next {
                    Ok(token) => token.serialization_type(),
                    Err(_) => TokenSerializationType::default(),
                },
            );
            set_position_at_next_iteration = false;
        }
        let token = match next {
            Ok(token) => token,
            Err(..) => break,
        };
        match token {
            Token::Function(ref name)
                if name.eq_ignore_ascii_case("var") || name.eq_ignore_ascii_case("env") =>
            {
                let is_env = name.eq_ignore_ascii_case("env");

                partial_computed_value.push(
                    input,
                    input.slice(position.0..before_this_token),
                    position.1,
                    last_token_type,
                )?;
                input.parse_nested_block(|input| {
                    // parse_var_function() / parse_env_function() ensure neither .unwrap() will fail.
                    let name = {
                        let name = input.expect_ident().unwrap();
                        if is_env {
                            Atom::from(&**name)
                        } else {
                            Atom::from(parse_name(&name).unwrap())
                        }
                    };

                    let env_value;

                    let registration;
                    let value = if is_env {
                        registration = None;
                        let device = stylist.device();
                        if let Some(v) = device.environment().get(
                            &name,
                            device,
                            &partial_computed_value.url_data,
                        ) {
                            env_value = v;
                            Some(&env_value)
                        } else {
                            None
                        }
                    } else {
                        registration = stylist.get_custom_property_registration(&name);
                        custom_properties.get(stylist, &name).map(|v| &**v)
                    };

                    if let Some(v) = value {
                        last_token_type = v.last_token_type;

                        if let Some(registration) = registration {
                            if input.try_parse(|input| input.expect_comma()).is_ok() {
                                let fallback = parse_and_substitute_fallback(
                                    input,
                                    custom_properties,
                                    &partial_computed_value.url_data,
                                    stylist,
                                    computed_context,
                                )?;
                                let mut fallback_input = ParserInput::new(&fallback.css);
                                let mut fallback_input = Parser::new(&mut fallback_input);
                                if let Err(_) = SpecifiedRegisteredValue::compute(
                                    &mut fallback_input,
                                    registration,
                                    &partial_computed_value.url_data,
                                    computed_context,
                                    AllowComputationallyDependent::Yes,
                                ) {
                                    return Err(input
                                        .new_custom_error(StyleParseErrorKind::UnspecifiedError));
                                }
                            }
                        } else {
                            // Skip over the fallback, as `parse_nested_block` would return `Err`
                            // if we don't consume all of `input`.
                            // FIXME: Add a specialized method to cssparser to do this with less work.
                            while input.next().is_ok() {}
                        }
                        partial_computed_value.push_variable(input, v)?;
                    } else {
                        input.expect_comma()?;
                        let fallback = parse_and_substitute_fallback(
                            input,
                            custom_properties,
                            &partial_computed_value.url_data,
                            stylist,
                            computed_context,
                        )?;
                        last_token_type = fallback.last_token_type;

                        if let Some(registration) = registration {
                            let mut fallback_input = ParserInput::new(&fallback.css);
                            let mut fallback_input = Parser::new(&mut fallback_input);
                            if let Ok(fallback) = SpecifiedRegisteredValue::compute(
                                &mut fallback_input,
                                registration,
                                &fallback.url_data,
                                computed_context,
                                AllowComputationallyDependent::Yes,
                            ) {
                                partial_computed_value.push_variable(input, &fallback)?;
                            } else {
                                return Err(
                                    input.new_custom_error(StyleParseErrorKind::UnspecifiedError)
                                );
                            }
                        } else {
                            partial_computed_value.push_variable(&input, &fallback)?;
                        }
                    }
                    Ok(())
                })?;
                set_position_at_next_iteration = true
            },
            Token::Function(_) |
            Token::ParenthesisBlock |
            Token::CurlyBracketBlock |
            Token::SquareBracketBlock => {
                input.parse_nested_block(|input| {
                    substitute_block(
                        input,
                        position,
                        partial_computed_value,
                        custom_properties,
                        stylist,
                        computed_context,
                    )
                })?;
                // It's the same type for CloseCurlyBracket and CloseSquareBracket.
                last_token_type = Token::CloseParenthesis.serialization_type();
            },

            _ => last_token_type = token.serialization_type(),
        }
    }
    // FIXME: deal with things being implicitly closed at the end of the input. E.g.
    // ```html
    // <div style="--color: rgb(0,0,0">
    // <p style="background: var(--color) var(--image) top left; --image: url('a.png"></p>
    // </div>
    // ```
    Ok(last_token_type)
}

/// Replace `var()` and `env()` functions for a non-custom property.
///
/// Return `Err(())` for invalid at computed time.
pub fn substitute<'i>(
    variable_value: &'i VariableValue,
    custom_properties: &ComputedCustomProperties,
    stylist: &Stylist,
    computed_context: &computed::Context,
) -> Result<String, ParseError<'i>> {
    let mut substituted = ComputedValue::empty(&variable_value.url_data);
    let mut input = ParserInput::new(&variable_value.css);
    let mut input = Parser::new(&mut input);
    let mut position = (input.position(), variable_value.first_token_type);
    let last_token_type = substitute_block(
        &mut input,
        &mut position,
        &mut substituted,
        custom_properties,
        stylist,
        computed_context,
    )?;
    substituted.push_from(&input, position, last_token_type)?;
    Ok(substituted.css)
}
