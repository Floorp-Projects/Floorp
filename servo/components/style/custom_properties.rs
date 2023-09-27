/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Support for [custom properties for cascading variables][custom].
//!
//! [custom]: https://drafts.csswg.org/css-variables/

use crate::applicable_declarations::CascadePriority;
use crate::media_queries::Device;
use crate::properties::{CSSWideKeyword, CustomDeclaration, CustomDeclarationValue};
use crate::properties_and_values::value::ComputedValue as ComputedRegisteredValue;
use crate::selector_map::{PrecomputedHashMap, PrecomputedHashSet, PrecomputedHasher};
use crate::stylist::Stylist;
use crate::Atom;
use cssparser::{
    CowRcStr, Delimiter, Parser, ParserInput, SourcePosition, Token, TokenSerializationType,
};
use indexmap::IndexMap;
use selectors::parser::SelectorParseErrorKind;
use servo_arc::{Arc, UniqueArc};
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

type EnvironmentEvaluator = fn(device: &Device) -> VariableValue;

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

fn get_safearea_inset_top(device: &Device) -> VariableValue {
    VariableValue::pixels(device.safe_area_insets().top)
}

fn get_safearea_inset_bottom(device: &Device) -> VariableValue {
    VariableValue::pixels(device.safe_area_insets().bottom)
}

fn get_safearea_inset_left(device: &Device) -> VariableValue {
    VariableValue::pixels(device.safe_area_insets().left)
}

fn get_safearea_inset_right(device: &Device) -> VariableValue {
    VariableValue::pixels(device.safe_area_insets().right)
}

fn get_content_preferred_color_scheme(device: &Device) -> VariableValue {
    use crate::gecko::media_features::PrefersColorScheme;
    let prefers_color_scheme = unsafe {
        crate::gecko_bindings::bindings::Gecko_MediaFeatures_PrefersColorScheme(
            device.document(),
            /* use_content = */ true,
        )
    };
    VariableValue::ident(match prefers_color_scheme {
        PrefersColorScheme::Light => "light",
        PrefersColorScheme::Dark => "dark",
    })
}

fn get_scrollbar_inline_size(device: &Device) -> VariableValue {
    VariableValue::pixels(device.scrollbar_inline_size().px())
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
        fn __eval(_: &Device) -> VariableValue {
            VariableValue::$ctor(lnf_int!($id))
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
    fn get(&self, name: &Atom, device: &Device) -> Option<VariableValue> {
        if let Some(var) = ENVIRONMENT_VARIABLES.iter().find(|var| var.name == *name) {
            return Some((var.evaluator)(device));
        }
        if !device.chrome_rules_enabled_for_document() {
            return None;
        }
        let var = CHROME_ENVIRONMENT_VARIABLES
            .iter()
            .find(|var| var.name == *name)?;
        Some((var.evaluator)(device))
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
#[derive(Clone, Debug, MallocSizeOf, PartialEq, ToShmem)]
pub struct VariableValue {
    css: String,

    first_token_type: TokenSerializationType,
    last_token_type: TokenSerializationType,

    /// var() or env() references.
    references: VarOrEnvReferences,
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

/// A pair of separate CustomPropertiesMaps, split between custom properties
/// that have the inherit flag set and those with the flag unset.
#[repr(C)]
#[derive(Clone, Debug, Default)]
pub struct ComputedCustomProperties {
    /// Map for custom properties with inherit flag set, including classical CSS
    /// variables. Defined as ref-counted for cheap copy.
    pub inherited: Option<Arc<CustomPropertiesMap>>,
    /// Map for custom properties with inherit flag unset.
    pub non_inherited: Option<Box<CustomPropertiesMap>>,
}

impl ComputedCustomProperties {
    /// Return whether the inherited and non_inherited maps are none.
    pub fn is_empty(&self) -> bool {
        self.inherited.is_none() && self.non_inherited.is_none()
    }

    fn read(&self) -> ReadOnlyCustomProperties {
        ReadOnlyCustomProperties {
            inherited: self.inherited.as_deref(),
            non_inherited: self.non_inherited.as_deref(),
        }
    }
}

impl PartialEq for ComputedCustomProperties {
    fn eq(&self, other: &Self) -> bool {
        // IndexMap equality doesn't consider ordering, which we have to account for.
        // Also, for the same reason, IndexMap equality comparisons are slower than needed.
        //
        // See https://github.com/bluss/indexmap/issues/153
        // TODO(bug 1840478): Handle non-inherited properties.
        match (&self.inherited, &other.inherited) {
            (Some(l), Some(r)) => {
                l.len() == r.len() &&
                    l.iter()
                        .zip(r.iter())
                        .all(|((k1, v1), (k2, v2))| k1 == k2 && v1 == v2)
            },
            (None, None) => true,
            _ => false,
        }
    }
}

/// A mutable CustomPropertiesMapPair, using UniqueArc for inherited properties.
#[derive(Default)]
struct MutableCustomProperties {
    inherited: Option<UniqueArc<CustomPropertiesMap>>,
    non_inherited: Option<Box<CustomPropertiesMap>>,
}

impl MutableCustomProperties {
    /// Insert a custom property in the corresponding inherited/non_inherited
    /// map, depending on whether the inherit flag is set or unset.
    fn insert(
        &mut self,
        name: Name,
        value: Arc<VariableValue>,
    ) -> Option<Arc<VariableValue>> {
        // TODO(bug 1840478): Handle non-inherited properties.
        let map = self
            .inherited
            .get_or_insert_with(|| UniqueArc::new(CustomPropertiesMap::default()));
        map.insert(name, value)
    }

    /// Remove a custom property from the corresponding inherited/non_inherited
    /// map, depending on whether the inherit flag is set or unset.
    fn remove(&mut self, name: &Name) -> Option<Arc<VariableValue>> {
        // TODO(bug 1840478): Handle non-inherited properties.
        self.inherited.as_mut()?.remove(name)
    }

    /// Shrink the capacity of the inherited/non_inherited maps as much as
    /// possible.
    fn shrink_to_fit(&mut self) {
        if let Some(ref mut map) = self.inherited {
            map.shrink_to_fit();
        }
        if let Some(ref mut map) = self.non_inherited {
            map.shrink_to_fit();
        }
    }

    fn read(&self) -> ReadOnlyCustomProperties {
        ReadOnlyCustomProperties {
            inherited: self.inherited.as_deref(),
            non_inherited: self.non_inherited.as_deref(),
        }
    }
}

#[derive(Copy, Clone, Default)]
struct ReadOnlyCustomProperties<'a> {
    inherited: Option<&'a CustomPropertiesMap>,
    non_inherited: Option<&'a CustomPropertiesMap>,
}

impl<'a> ReadOnlyCustomProperties<'a> {
    fn get(&self, name: &Name) -> Option<&Arc<VariableValue>> {
        // TODO(bug 1840478): Handle non-inherited properties.
        self.inherited?.get(name)
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
    fn empty() -> Self {
        Self {
            css: String::new(),
            last_token_type: TokenSerializationType::nothing(),
            first_token_type: TokenSerializationType::nothing(),
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
    pub fn parse<'i, 't>(input: &mut Parser<'i, 't>) -> Result<Arc<Self>, ParseError<'i>> {
        let mut references = VarOrEnvReferences::default();

        let (first_token_type, css, last_token_type) =
            parse_self_contained_declaration_value(input, Some(&mut references))?;

        let mut css = css.into_owned();
        css.shrink_to_fit();

        references.custom_properties.shrink_to_fit();

        Ok(Arc::new(VariableValue {
            css,
            first_token_type,
            last_token_type,
            references,
        }))
    }

    /// Create VariableValue from an int.
    fn integer(number: i32) -> Self {
        Self::from_token(Token::Number {
            has_sign: false,
            value: number as f32,
            int_value: Some(number),
        })
    }

    /// Create VariableValue from an int.
    fn ident(ident: &'static str) -> Self {
        Self::from_token(Token::Ident(ident.into()))
    }

    /// Create VariableValue from a float amount of CSS pixels.
    fn pixels(number: f32) -> Self {
        // FIXME (https://github.com/servo/rust-cssparser/issues/266):
        // No way to get TokenSerializationType::Dimension without creating
        // Token object.
        Self::from_token(Token::Dimension {
            has_sign: false,
            value: number,
            int_value: None,
            unit: CowRcStr::from("px"),
        })
    }

    /// Create VariableValue from an integer amount of CSS pixels.
    fn int_pixels(number: i32) -> Self {
        Self::from_token(Token::Dimension {
            has_sign: false,
            value: number as f32,
            int_value: Some(number),
            unit: CowRcStr::from("px"),
        })
    }

    fn from_token(token: Token) -> Self {
        let token_type = token.serialization_type();
        let mut css = token.to_css_string();
        css.shrink_to_fit();

        VariableValue {
            css,
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
            return Ok((
                TokenSerializationType::nothing(),
                TokenSerializationType::nothing(),
            ));
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
    custom_properties: ReadOnlyCustomProperties,
    stylist: &Stylist,
) -> Result<ComputedValue, ParseError<'i>> {
    debug_assert!(custom_properties.non_inherited.is_none());
    input.skip_whitespace();
    let after_comma = input.state();
    let first_token_type = input
        .next_including_whitespace_and_comments()
        .ok()
        .map_or_else(TokenSerializationType::nothing, |t| t.serialization_type());
    input.reset(&after_comma);
    let mut position = (after_comma.position(), first_token_type);

    let mut fallback = ComputedValue::empty();
    let last_token_type = substitute_block(
        input,
        &mut position,
        &mut fallback,
        custom_properties,
        stylist,
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
pub struct CustomPropertiesBuilder<'a> {
    seen: PrecomputedHashSet<&'a Name>,
    may_have_cycles: bool,
    custom_properties: MutableCustomProperties,
    inherited: &'a ComputedCustomProperties,
    reverted: PrecomputedHashMap<&'a Name, (CascadePriority, bool)>,
    stylist: &'a Stylist,
}

impl<'a> CustomPropertiesBuilder<'a> {
    /// Create a new builder, inheriting from a given custom properties map.
    pub fn new(inherited: &'a ComputedCustomProperties, stylist: &'a Stylist) -> Self {
        Self {
            seen: PrecomputedHashSet::default(),
            reverted: Default::default(),
            may_have_cycles: false,
            custom_properties: MutableCustomProperties::default(),
            inherited,
            stylist,
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

        // TODO(bug 1840478): Handle non-inherited properties.
        if self.custom_properties.inherited.is_none() {
            self.custom_properties.inherited = Some(match &self.inherited.inherited {
                Some(inherited) => UniqueArc::new((**inherited).clone()),
                None => UniqueArc::new(CustomPropertiesMap::default()),
            });
        }

        let map = &mut self.custom_properties;
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
                            self.inherited,
                            self.stylist,
                        );
                        return;
                    }
                    if let Some(registration) = self.stylist.get_custom_property_registration(&name)
                    {
                        let mut input = ParserInput::new(&unparsed_value.css);
                        let mut input = Parser::new(&mut input);
                        if ComputedRegisteredValue::compute(&mut input, registration).is_err() {
                            map.remove(name);
                            return;
                        }
                    }
                }
                map.insert(name.clone(), Arc::clone(unparsed_value));
            },
            CustomDeclarationValue::CSSWideKeyword(keyword) => match keyword {
                CSSWideKeyword::RevertLayer | CSSWideKeyword::Revert => {
                    let origin_revert = keyword == CSSWideKeyword::Revert;
                    self.seen.remove(name);
                    self.reverted.insert(name, (priority, origin_revert));
                },
                CSSWideKeyword::Initial => {
                    map.remove(name);
                },
                // handled in value_may_affect_style
                CSSWideKeyword::Unset | CSSWideKeyword::Inherit => unreachable!(),
            },
        }
    }

    fn value_may_affect_style(&self, name: &Name, value: &CustomDeclarationValue) -> bool {
        match *value {
            CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Unset) |
            CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Inherit) => {
                // Custom properties are inherited by default. So
                // explicit 'inherit' or 'unset' means we can just use
                // any existing value in the inherited CustomPropertiesMap.
                return false;
            },
            _ => {},
        }

        // TODO(bug 1840478): Handle non-inherited properties.
        let existing_value = self
            .custom_properties
            .inherited
            .as_ref()
            .and_then(|m| m.get(name))
            .or_else(|| self.inherited.inherited.as_ref().and_then(|m| m.get(name)));

        match (existing_value, value) {
            (None, &CustomDeclarationValue::CSSWideKeyword(CSSWideKeyword::Initial)) => {
                // The initial value of a custom property is the same as it
                // not existing in the map.
                return false;
            },
            (Some(existing_value), &CustomDeclarationValue::Value(ref value)) => {
                // Don't bother overwriting an existing inherited value with
                // the same specified value.
                if existing_value == value {
                    return false;
                }
            },
            _ => {},
        }

        true
    }

    fn inherited_properties_match(&self, custom_properties: &MutableCustomProperties) -> bool {
        // TODO(bug 1840478): Handle non-inherited properties.
        let (inherited, map) = match (&self.inherited.inherited, &custom_properties.inherited) {
            (Some(inherited), Some(map)) => (inherited, map),
            (None, None) => return true,
            _ => return false,
        };
        if inherited.len() != map.len() {
            return false;
        }
        for name in self.seen.iter() {
            if inherited.get(*name) != map.get(*name) {
                return false;
            }
        }
        true
    }

    /// Returns the final map of applicable custom properties.
    ///
    /// If there was any specified property, we've created a new map and now we
    /// need to remove any potential cycles, and wrap it in an arc.
    ///
    /// Otherwise, just use the inherited custom properties map.
    pub fn build(mut self) -> ComputedCustomProperties {
        // TODO(bug 1840478): Handle non-inherited properties.
        debug_assert!(self.custom_properties.non_inherited.is_none());
        debug_assert!(self.inherited.non_inherited.is_none());
        if self.custom_properties.inherited.is_none() {
            return self.inherited.clone();
        }

        if self.may_have_cycles {
            substitute_all(
                &mut self.custom_properties,
                self.inherited,
                &self.seen,
                self.stylist,
            );
        }

        // Some pages apply a lot of redundant custom properties, see e.g.
        // bug 1758974 comment 5. Try to detect the case where the values
        // haven't really changed, and save some memory by reusing the inherited
        // map in that case.
        if self.inherited_properties_match(&self.custom_properties) {
            return self.inherited.clone();
        }

        self.custom_properties.shrink_to_fit();
        ComputedCustomProperties {
            inherited: self
                .custom_properties
                .inherited
                .take()
                .map(|m| m.shareable()),
            non_inherited: self.custom_properties.non_inherited.take(),
        }
    }
}

/// Resolve all custom properties to either substituted, invalid, or unset
/// (meaning we should use the inherited value).
///
/// It does cycle dependencies removal at the same time as substitution.
fn substitute_all(
    custom_properties_map: &mut MutableCustomProperties,
    inherited: &ComputedCustomProperties,
    seen: &PrecomputedHashSet<&Name>,
    stylist: &Stylist,
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
    struct Context<'a> {
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
        map: &'a mut MutableCustomProperties,
        /// The inherited custom properties to handle wide keywords.
        inherited: &'a ComputedCustomProperties,
        /// The stylist is used to get registered properties, and to resolve the environment to
        /// substitute `env()` variables.
        stylist: &'a Stylist,
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
    fn traverse<'a>(name: &Name, context: &mut Context<'a>) -> Option<usize> {
        // Some shortcut checks.
        let (name, value) = {
            let props = context.map.read();
            let value = props.get(name)?;

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
            context.map.remove(&var_name);
            in_loop = true;
        }
        if in_loop {
            // This variable is in loop. Resolve to invalid.
            context.map.remove(&name);
            return None;
        }

        // Now we have shown that this variable is not in a loop, and all of its dependencies
        // should have been resolved. We can perform substitution now.
        substitute_references_in_value_and_apply(
            &name,
            &value,
            &mut context.map,
            context.inherited,
            context.stylist,
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
            inherited,
            stylist,
        };
        traverse(name, &mut context);
    }
}

/// Replace `var()` and `env()` functions in a pre-existing variable value.
fn substitute_references_in_value_and_apply(
    name: &Name,
    value: &VariableValue,
    custom_properties: &mut MutableCustomProperties,
    inherited: &ComputedCustomProperties,
    stylist: &Stylist,
) {
    debug_assert!(value.has_references());

    let mut computed_value = ComputedValue::empty();

    {
        let mut input = ParserInput::new(&value.css);
        let mut input = Parser::new(&mut input);
        let mut position = (input.position(), value.first_token_type);

        let last_token_type = substitute_block(
            &mut input,
            &mut position,
            &mut computed_value,
            custom_properties.read(),
            stylist,
        );

        let last_token_type = match last_token_type {
            Ok(t) => t,
            Err(..) => {
                // Invalid at computed value time.
                custom_properties.remove(name);
                return;
            },
        };

        if computed_value
            .push_from(&input, position, last_token_type)
            .is_err()
        {
            custom_properties.remove(name);
            return;
        }
    }

    let should_insert = {
        let mut input = ParserInput::new(&computed_value.css);
        let mut input = Parser::new(&mut input);

        // If variable fallback results in a wide keyword, deal with it now.
        if let Ok(kw) = input.try_parse(CSSWideKeyword::parse) {
            match kw {
                CSSWideKeyword::Initial => {
                    custom_properties.remove(name);
                },
                CSSWideKeyword::Revert |
                CSSWideKeyword::RevertLayer |
                CSSWideKeyword::Inherit |
                CSSWideKeyword::Unset => {
                    // TODO: It's unclear what this should do for revert / revert-layer, see
                    // https://github.com/w3c/csswg-drafts/issues/9131. For now treating as unset
                    // seems fine?
                    // TODO(bug 1840478): Handle non-inherited properties.
                    match inherited.inherited.as_ref().and_then(|map| map.get(name)) {
                        Some(value) => {
                            custom_properties.insert(name.clone(), Arc::clone(value));
                        },
                        None => {
                            custom_properties.remove(name);
                        },
                    };
                },
            }
            false
        } else {
            if let Some(registration) = stylist.get_custom_property_registration(&name) {
                if ComputedRegisteredValue::compute(&mut input, registration).is_err() {
                    custom_properties.remove(name);
                    return;
                }
            }
            true
        }
    };
    if should_insert {
        computed_value.css.shrink_to_fit();
        custom_properties.insert(name.clone(), Arc::new(computed_value));
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
    custom_properties: ReadOnlyCustomProperties,
    stylist: &Stylist,
) -> Result<TokenSerializationType, ParseError<'i>> {
    debug_assert!(custom_properties.non_inherited.is_none());
    let mut last_token_type = TokenSerializationType::nothing();
    let mut set_position_at_next_iteration = false;
    loop {
        let before_this_token = input.position();
        let next = input.next_including_whitespace_and_comments();
        if set_position_at_next_iteration {
            *position = (
                before_this_token,
                match next {
                    Ok(token) => token.serialization_type(),
                    Err(_) => TokenSerializationType::nothing(),
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
                        if let Some(v) = device.environment().get(&name, device) {
                            env_value = v;
                            Some(&env_value)
                        } else {
                            None
                        }
                    } else {
                        // TODO(bug 1840478): Handle non-inherited properties.
                        registration = stylist.get_custom_property_registration(&name);
                        custom_properties.get(&name).map(|v| &**v)
                    };

                    if let Some(v) = value {
                        last_token_type = v.last_token_type;

                        if let Some(registration) = registration {
                            if input.try_parse(|input| input.expect_comma()).is_ok() {
                                let fallback = parse_and_substitute_fallback(
                                    input,
                                    custom_properties,
                                    stylist,
                                )?;
                                let mut fallback_input = ParserInput::new(&fallback.css);
                                let mut fallback_input = Parser::new(&mut fallback_input);
                                let compute_result = ComputedRegisteredValue::compute(
                                    &mut fallback_input,
                                    registration,
                                );
                                if compute_result.is_err() {
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
                        let fallback =
                            parse_and_substitute_fallback(input, custom_properties, stylist)?;
                        last_token_type = fallback.last_token_type;

                        if let Some(registration) = registration {
                            let mut fallback_input = ParserInput::new(&fallback.css);
                            let mut fallback_input = Parser::new(&mut fallback_input);
                            let compute_result =
                                ComputedRegisteredValue::compute(&mut fallback_input, registration);
                            if compute_result.is_err() {
                                return Err(
                                    input.new_custom_error(StyleParseErrorKind::UnspecifiedError)
                                );
                            }
                        }
                        partial_computed_value.push_variable(&input, &fallback)?;
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
    stylist: &Stylist,
) -> Result<String, ParseError<'i>> {
    let mut substituted = ComputedValue::empty();
    let mut input = ParserInput::new(input);
    let mut input = Parser::new(&mut input);
    let mut position = (input.position(), first_token_type);
    let last_token_type = substitute_block(
        &mut input,
        &mut position,
        &mut substituted,
        custom_properties.read(),
        stylist,
    )?;
    substituted.push_from(&input, position, last_token_type)?;
    Ok(substituted.css)
}
