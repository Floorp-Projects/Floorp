/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Support for [custom properties for cascading variables][custom].
//!
//! [custom]: https://drafts.csswg.org/css-variables/

use crate::applicable_declarations::CascadePriority;
use crate::media_queries::Device;
use crate::properties::{CSSWideKeyword, CustomDeclaration, CustomDeclarationValue};
use crate::properties_and_values::{
    registry::PropertyRegistration,
    value::{AllowComputationallyDependent, SpecifiedValue as SpecifiedRegisteredValue},
};
use crate::selector_map::{PrecomputedHashMap, PrecomputedHashSet, PrecomputedHasher};
use crate::stylesheets::UrlExtraData;
use crate::stylist::Stylist;
use crate::values::computed;
use crate::Atom;
use cssparser::{
    CowRcStr, Delimiter, Parser, ParserInput, SourcePosition, Token, TokenSerializationType,
};
use indexmap::IndexMap;
use selectors::parser::SelectorParseErrorKind;
use servo_arc::Arc;
use smallvec::SmallVec;
use std::borrow::Cow;
use std::cmp;
use std::collections::hash_map::Entry;
use std::fmt::{self, Write};
use std::hash::BuildHasherDefault;
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

static CHROME_ENVIRONMENT_VARIABLES: [EnvironmentVariable; 6] = [
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
    css: String,

    /// The url data of the stylesheet where this value came from.
    pub url_data: UrlExtraData,

    first_token_type: TokenSerializationType,
    last_token_type: TokenSerializationType,

    /// var() or env() references.
    references: VarOrEnvReferences,
}

// For all purposes, we want values to be considered equal if their css text is equal.
impl PartialEq for VariableValue {
    fn eq(&self, other: &Self) -> bool {
        self.css == other.css
    }
}

impl ToCss for SpecifiedValue {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_str(&self.css)
    }
}

/// A map from CSS variable names to CSS variable computed values, used for
/// resolving.
///
/// A consistent ordering is required for CSSDeclaration objects in the
/// DOM. CSSDeclarations expose property names as indexed properties, which
/// need to be stable. So we keep an array of property names which order is
/// determined on the order that they are added to the name-value map.
///
/// The variable values are guaranteed to not have references to other
/// properties.
pub type CustomPropertiesMap =
    IndexMap<Name, Arc<VariableValue>, BuildHasherDefault<PrecomputedHasher>>;

// IndexMap equality doesn't consider ordering, which we have to account for.
// Also, for the same reason, IndexMap equality comparisons are slower than needed.
//
// See https://github.com/bluss/indexmap/issues/153
fn maps_equal(l: Option<&CustomPropertiesMap>, r: Option<&CustomPropertiesMap>) -> bool {
    let (l, r) = match (l, r) {
        (Some(l), Some(r)) => (l, r),
        (None, None) => return true,
        _ => return false,
    };
    if std::ptr::eq(l, r) {
        return true;
    }
    if l.len() != r.len() {
        return false;
    }
    l.iter()
        .zip(r.iter())
        .all(|((k1, v1), (k2, v2))| k1 == k2 && v1 == v2)
}

/// A pair of separate CustomPropertiesMaps, split between custom properties
/// that have the inherit flag set and those with the flag unset.
#[repr(C)]
#[derive(Clone, Debug, Default)]
pub struct ComputedCustomProperties {
    /// Map for custom properties with inherit flag set, including non-registered
    /// ones. Defined as ref-counted for cheap copy of inherited values.
    pub inherited: Option<Arc<CustomPropertiesMap>>,
    /// Map for custom properties with inherit flag unset. Defined as ref-counted
    /// for cheap copy of initial values.
    pub non_inherited: Option<Arc<CustomPropertiesMap>>,
}

impl PartialEq for ComputedCustomProperties {
    fn eq(&self, other: &Self) -> bool {
        self.inherited_equal(other) && self.non_inherited_equal(other)
    }
}

impl ComputedCustomProperties {
    /// Return whether the inherited and non_inherited maps are none.
    pub fn is_empty(&self) -> bool {
        self.inherited.is_none() && self.non_inherited.is_none()
    }

    /// Return the name and value of the property at specified index, if any.
    pub fn property_at(&self, index: usize) -> Option<(&Name, &Arc<VariableValue>)> {
        // Just expose the custom property items from
        // custom_properties.inherited, followed by custom property items from
        // custom_properties.non_inherited.
        // TODO(bug 1855629): In which order should we expose these properties?
        match (&self.inherited, &self.non_inherited) {
            (Some(p1), Some(p2)) => p1
                .get_index(index)
                .or_else(|| p2.get_index(index - p1.len())),
            (Some(p1), None) => p1.get_index(index),
            (None, Some(p2)) => p2.get_index(index),
            (None, None) => None,
        }
    }

    fn inherited_equal(&self, other: &Self) -> bool {
        maps_equal(self.inherited.as_deref(), other.inherited.as_deref())
    }

    fn non_inherited_equal(&self, other: &Self) -> bool {
        maps_equal(
            self.non_inherited.as_deref(),
            other.non_inherited.as_deref(),
        )
    }

    /// Insert a custom property in the corresponding inherited/non_inherited
    /// map, depending on whether the inherit flag is set or unset.
    fn insert(
        &mut self,
        registration: Option<&PropertyRegistration>,
        name: Name,
        value: Arc<VariableValue>,
    ) -> Option<Arc<VariableValue>> {
        self.map_mut(registration).insert(name, value)
    }

    /// Remove a custom property from the corresponding inherited/non_inherited
    /// map, depending on whether the inherit flag is set or unset.
    fn remove(
        &mut self,
        registration: Option<&PropertyRegistration>,
        name: &Name,
    ) -> Option<Arc<VariableValue>> {
        self.map_mut(registration).remove(name)
    }

    /// Shrink the capacity of the inherited maps as much as possible. An empty map is just
    /// replaced with None and dropped.
    fn shrink_to_fit(&mut self) {
        if let Some(ref mut map) = self.inherited {
            if map.is_empty() {
                self.inherited = None;
            } else if let Some(ref mut map) = Arc::get_mut(map) {
                map.shrink_to_fit();
            }
        }

        if let Some(ref mut map) = self.non_inherited {
            if map.is_empty() {
                self.non_inherited = None;
            } else if let Some(ref mut map) = Arc::get_mut(map) {
                map.shrink_to_fit();
            }
        }
    }

    fn map_mut(&mut self, registration: Option<&PropertyRegistration>) -> &mut CustomPropertiesMap {
        // TODO: If the atomic load in make_mut shows up in profiles, we could cache whether
        // the current map is unique, but that seems unlikely in practice.
        let map = if registration.map_or(true, |r| r.inherits()) {
            &mut self.inherited
        } else {
            &mut self.non_inherited
        };
        Arc::make_mut(map.get_or_insert_with(Default::default))
    }

    fn get(&self, stylist: &Stylist, name: &Name) -> Option<&Arc<VariableValue>> {
        let registration = stylist.get_custom_property_registration(&name);
        if registration.map_or(true, |r| r.inherits()) {
            self.inherited.as_ref()?.get(name)
        } else {
            self.non_inherited.as_ref()?.get(name)
        }
    }
}

/// Both specified and computed values are VariableValues, the difference is
/// whether var() functions are expanded.
pub type SpecifiedValue = VariableValue;
/// Both specified and computed values are VariableValues, the difference is
/// whether var() functions are expanded.
pub type ComputedValue = VariableValue;

/// A struct holding information about the external references to that a custom
/// property value may have.
#[derive(Clone, Debug, Default, MallocSizeOf, PartialEq, ToShmem)]
struct VarOrEnvReferences {
    custom_properties: PrecomputedHashSet<Name>,
    environment: bool,
}

impl VarOrEnvReferences {
    fn has_references(&self) -> bool {
        self.environment || !self.custom_properties.is_empty()
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
    ) -> Result<Arc<Self>, ParseError<'i>> {
        let mut references = VarOrEnvReferences::default();

        let (first_token_type, css, last_token_type) =
            parse_self_contained_declaration_value(input, Some(&mut references))?;

        let mut css = css.into_owned();
        css.shrink_to_fit();

        references.custom_properties.shrink_to_fit();

        Ok(Arc::new(VariableValue {
            css,
            url_data: url_data.clone(),
            first_token_type,
            last_token_type,
            references,
        }))
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

/// Parse the value of a non-custom property that contains `var()` references.
pub fn parse_non_custom_with_var<'i, 't>(
    input: &mut Parser<'i, 't>,
) -> Result<(TokenSerializationType, Cow<'i, str>), ParseError<'i>> {
    let (first_token_type, css, _) = parse_self_contained_declaration_value(input, None)?;
    Ok((first_token_type, css))
}

fn parse_self_contained_declaration_value<'i, 't>(
    input: &mut Parser<'i, 't>,
    references: Option<&mut VarOrEnvReferences>,
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
    references: Option<&mut VarOrEnvReferences>,
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
    mut references: Option<&mut VarOrEnvReferences>,
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
                        references.as_mut().map(|r| &mut **r),
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
                        parse_var_function(input, references.as_mut().map(|r| &mut **r))
                    })?;
                    input.reset(&args_start);
                } else if name.eq_ignore_ascii_case("env") {
                    let args_start = input.state();
                    input.parse_nested_block(|input| {
                        parse_env_function(input, references.as_mut().map(|r| &mut **r))
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
    references: Option<&mut VarOrEnvReferences>,
) -> Result<(), ParseError<'i>> {
    let name = input.expect_ident_cloned()?;
    let name = parse_name(&name).map_err(|()| {
        input.new_custom_error(SelectorParseErrorKind::UnexpectedIdent(name.clone()))
    })?;
    if input.try_parse(|input| input.expect_comma()).is_ok() {
        parse_fallback(input)?;
    }
    if let Some(refs) = references {
        refs.custom_properties.insert(Atom::from(name));
    }
    Ok(())
}

fn parse_env_function<'i, 't>(
    input: &mut Parser<'i, 't>,
    references: Option<&mut VarOrEnvReferences>,
) -> Result<(), ParseError<'i>> {
    // TODO(emilio): This should be <custom-ident> per spec, but no other
    // browser does that, see https://github.com/w3c/csswg-drafts/issues/3262.
    input.expect_ident()?;
    if input.try_parse(|input| input.expect_comma()).is_ok() {
        parse_fallback(input)?;
    }
    if let Some(references) = references {
        references.environment = true;
    }
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
    computed_context: &'a computed::Context<'b>,
}

impl<'a, 'b: 'a> CustomPropertiesBuilder<'a, 'b> {
    /// Create a new builder, inheriting from a given custom properties map.
    pub fn new(stylist: &'a Stylist, computed_context: &'a computed::Context<'b>) -> Self {
        let inherited = computed_context.inherited_custom_properties();
        let initial_values = stylist.get_custom_property_initial_values();
        let is_root_element = computed_context.is_root_element();
        Self {
            seen: PrecomputedHashSet::default(),
            reverted: Default::default(),
            may_have_cycles: false,
            custom_properties: ComputedCustomProperties {
                inherited: if is_root_element {
                    debug_assert!(inherited.is_empty());
                    initial_values.inherited.clone()
                } else {
                    inherited.inherited.clone()
                },
                non_inherited: initial_values.non_inherited.clone(),
            },
            stylist,
            computed_context,
        }
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
                self.may_have_cycles |= has_custom_property_references;

                // If the variable value has no references and it has an environment variable here,
                // perform substitution here instead of forcing a full traversal in
                // `substitute_all` afterwards.
                if !has_custom_property_references {
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
                            self.computed_context,
                            AllowComputationallyDependent::Yes,
                        ) {
                            map.insert(custom_registration, name.clone(), value);
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
                    name.clone(),
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
                            map.insert(custom_registration, name.clone(), initial_value.clone());
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
                        .as_ref()
                        .and_then(|m| m.get(name))
                    {
                        map.insert(custom_registration, name.clone(), inherited_value.clone());
                    }
                },
                // handled in value_may_affect_style
                CSSWideKeyword::Unset => unreachable!(),
            },
        }
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
                    .as_ref()
                    .map_or(true, |m| !m.contains_key(name))
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

    /// Returns the final map of applicable custom properties.
    ///
    /// If there was any specified property or non-inherited custom property
    /// with an initial value, we've created a new map and now we
    /// need to remove any potential cycles, and wrap it in an arc.
    ///
    /// Otherwise, just use the inherited custom properties map.
    pub fn build(mut self) -> ComputedCustomProperties {
        if self.may_have_cycles {
            substitute_all(
                &mut self.custom_properties,
                &self.seen,
                self.stylist,
                self.computed_context,
            );
        }

        self.custom_properties.shrink_to_fit();

        // Some pages apply a lot of redundant custom properties, see e.g.
        // bug 1758974 comment 5. Try to detect the case where the values
        // haven't really changed, and save some memory by reusing the inherited
        // map in that case.
        let initial_values = self.stylist.get_custom_property_initial_values();
        ComputedCustomProperties {
            inherited: if self
                .computed_context
                .inherited_custom_properties()
                .inherited_equal(&self.custom_properties)
            {
                self.computed_context
                    .inherited_custom_properties()
                    .inherited
                    .clone()
            } else {
                self.custom_properties.inherited.take()
            },
            non_inherited: if initial_values.non_inherited_equal(&self.custom_properties) {
                initial_values.non_inherited.clone()
            } else {
                self.custom_properties.non_inherited.take()
            },
        }
    }
}

/// Resolve all custom properties to either substituted, invalid, or unset
/// (meaning we should use the inherited value).
///
/// It does cycle dependencies removal at the same time as substitution.
fn substitute_all(
    custom_properties_map: &mut ComputedCustomProperties,
    seen: &PrecomputedHashSet<&Name>,
    stylist: &Stylist,
    computed_context: &computed::Context,
) {
    // The cycle dependencies removal in this function is a variant
    // of Tarjan's algorithm. It is mostly based on the pseudo-code
    // listed in
    // https://en.wikipedia.org/w/index.php?
    // title=Tarjan%27s_strongly_connected_components_algorithm&oldid=801728495

    /// Struct recording necessary information for each variable.
    #[derive(Debug)]
    struct VarInfo {
        /// The name of the variable. It will be taken to save addref
        /// when the corresponding variable is popped from the stack.
        /// This also serves as a mark for whether the variable is
        /// currently in the stack below.
        name: Option<Name>,
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
        /// Information of each variable indexed by the order index.
        var_info: SmallVec<[VarInfo; 5]>,
        /// The stack of order index of visited variables. It contains
        /// all unfinished strong connected components.
        stack: SmallVec<[usize; 5]>,
        map: &'a mut ComputedCustomProperties,
        /// The stylist is used to get registered properties, and to resolve the environment to
        /// substitute `env()` variables.
        stylist: &'a Stylist,
        /// The computed context is used to get inherited custom
        /// properties  and compute registered custom properties.
        computed_context: &'a computed::Context<'b>,
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
    fn traverse<'a, 'b>(name: &Name, context: &mut Context<'a, 'b>) -> Option<usize> {
        // Some shortcut checks.
        let (name, value) = {
            let value = context.map.get(context.stylist, name)?;

            // Nothing to resolve.
            if value.references.custom_properties.is_empty() {
                debug_assert!(
                    !value.references.environment,
                    "Should've been handled earlier"
                );
                return None;
            }

            // Whether this variable has been visited in this traversal.
            let key;
            match context.index_map.entry(name.clone()) {
                Entry::Occupied(entry) => {
                    return Some(*entry.get());
                },
                Entry::Vacant(entry) => {
                    key = entry.key().clone();
                    entry.insert(context.count);
                },
            }

            // Hold a strong reference to the value so that we don't
            // need to keep reference to context.map.
            (key, value.clone())
        };

        // Add new entry to the information table.
        let index = context.count;
        context.count += 1;
        debug_assert_eq!(index, context.var_info.len());
        context.var_info.push(VarInfo {
            name: Some(name),
            lowlink: index,
        });
        context.stack.push(index);

        let mut self_ref = false;
        let mut lowlink = index;
        for next in value.references.custom_properties.iter() {
            let next_index = match traverse(next, context) {
                Some(index) => index,
                // There is nothing to do if the next variable has been
                // fully resolved at this point.
                None => {
                    continue;
                },
            };
            let next_info = &context.var_info[next_index];
            if next_index > index {
                // The next variable has a larger index than us, so it
                // must be inserted in the recursive call above. We want
                // to get its lowlink.
                lowlink = cmp::min(lowlink, next_info.lowlink);
            } else if next_index == index {
                self_ref = true;
            } else if next_info.name.is_some() {
                // The next variable has a smaller order index and it is
                // in the stack, so we are at the same component.
                lowlink = cmp::min(lowlink, next_index);
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
                .name
                .take()
                .expect("Variable should not be poped from stack twice");
            if var_index == index {
                name = var_name;
                break;
            }
            // Anything here is in a loop which can traverse to the
            // variable we are handling, so remove it from the map, it's invalid
            // at computed-value time.
            context.map.remove(
                context.stylist.get_custom_property_registration(&var_name),
                &var_name,
            );
            in_loop = true;
        }
        if in_loop {
            // This variable is in loop. Resolve to invalid.
            context.map.remove(
                context.stylist.get_custom_property_registration(&name),
                &name,
            );
            return None;
        }

        // Now we have shown that this variable is not in a loop, and all of its dependencies
        // should have been resolved. We can perform substitution now.
        substitute_references_in_value_and_apply(
            &name,
            &value,
            &mut context.map,
            context.stylist,
            context.computed_context,
        );

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
            stack: SmallVec::new(),
            var_info: SmallVec::new(),
            map: custom_properties_map,
            stylist,
            computed_context,
        };
        traverse(name, &mut context);
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
                    custom_properties.insert(custom_registration, name.clone(), Arc::clone(value));
                    return;
                }
            } else {
                if let Some(ref initial_value) = registration.initial_value {
                    custom_properties.insert(
                        custom_registration,
                        name.clone(),
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
                                name.clone(),
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
                                name.clone(),
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
                    computed_context,
                    AllowComputationallyDependent::Yes,
                ) {
                    custom_properties.insert(custom_registration, name.clone(), value);
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
        custom_properties.insert(custom_registration, name.clone(), Arc::new(computed_value));
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
    input: &'i str,
    first_token_type: TokenSerializationType,
    custom_properties: &ComputedCustomProperties,
    url_data: &UrlExtraData,
    stylist: &Stylist,
    computed_context: &computed::Context,
) -> Result<String, ParseError<'i>> {
    let mut substituted = ComputedValue::empty(url_data);
    let mut input = ParserInput::new(input);
    let mut input = Parser::new(&mut input);
    let mut position = (input.position(), first_token_type);
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
