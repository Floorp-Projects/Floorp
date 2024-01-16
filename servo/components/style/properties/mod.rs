/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Supported CSS properties and the cascade.

pub mod cascade;
pub mod declaration_block;

pub use self::cascade::*;
pub use self::declaration_block::*;
pub use self::generated::*;
/// The CSS properties supported by the style system.
/// Generated from the properties.mako.rs template by build.rs
#[macro_use]
#[allow(unsafe_code)]
#[deny(missing_docs)]
pub mod generated {
    include!(concat!(env!("OUT_DIR"), "/properties.rs"));

    #[cfg(feature = "gecko")]
    #[allow(unsafe_code, missing_docs)]
    pub mod gecko {
        include!(concat!(env!("OUT_DIR"), "/gecko_properties.rs"));
    }
}

use crate::custom_properties::{self, ComputedCustomProperties};
#[cfg(feature = "gecko")]
use crate::gecko_bindings::structs::{nsCSSPropertyID, AnimatedPropertyID, RefPtr};
use crate::logical_geometry::WritingMode;
use crate::parser::ParserContext;
use crate::str::CssString;
use crate::stylesheets::Origin;
use crate::stylist::Stylist;
use crate::values::{computed, serialize_atom_name};
use arrayvec::{ArrayVec, Drain as ArrayVecDrain};
use cssparser::{Parser, ParserInput};
use fxhash::FxHashMap;
use servo_arc::Arc;
use std::{
    borrow::Cow,
    fmt::{self, Write},
    mem,
};
use style_traits::{
    CssWriter, KeywordsCollectFn, ParseError, ParsingMode, SpecifiedValueInfo, ToCss,
};

bitflags! {
    /// A set of flags for properties.
    #[derive(Clone, Copy)]
    pub struct PropertyFlags: u16 {
        /// This longhand property applies to ::first-letter.
        const APPLIES_TO_FIRST_LETTER = 1 << 1;
        /// This longhand property applies to ::first-line.
        const APPLIES_TO_FIRST_LINE = 1 << 2;
        /// This longhand property applies to ::placeholder.
        const APPLIES_TO_PLACEHOLDER = 1 << 3;
        ///  This longhand property applies to ::cue.
        const APPLIES_TO_CUE = 1 << 4;
        /// This longhand property applies to ::marker.
        const APPLIES_TO_MARKER = 1 << 5;
        /// This property is a legacy shorthand.
        ///
        /// https://drafts.csswg.org/css-cascade/#legacy-shorthand
        const IS_LEGACY_SHORTHAND = 1 << 6;

        /* The following flags are currently not used in Rust code, they
         * only need to be listed in corresponding properties so that
         * they can be checked in the C++ side via ServoCSSPropList.h. */

        /// This property can be animated on the compositor.
        const CAN_ANIMATE_ON_COMPOSITOR = 0;
        /// This shorthand property is accessible from getComputedStyle.
        const SHORTHAND_IN_GETCS = 0;
        /// See data.py's documentation about the affects_flags.
        const AFFECTS_LAYOUT = 0;
        #[allow(missing_docs)]
        const AFFECTS_OVERFLOW = 0;
        #[allow(missing_docs)]
        const AFFECTS_PAINT = 0;
    }
}

/// An enum to represent a CSS Wide keyword.
#[derive(Clone, Copy, Debug, Eq, MallocSizeOf, PartialEq, SpecifiedValueInfo, ToCss, ToShmem)]
pub enum CSSWideKeyword {
    /// The `initial` keyword.
    Initial,
    /// The `inherit` keyword.
    Inherit,
    /// The `unset` keyword.
    Unset,
    /// The `revert` keyword.
    Revert,
    /// The `revert-layer` keyword.
    RevertLayer,
}

impl CSSWideKeyword {
    /// Returns the string representation of the keyword.
    pub fn to_str(&self) -> &'static str {
        match *self {
            CSSWideKeyword::Initial => "initial",
            CSSWideKeyword::Inherit => "inherit",
            CSSWideKeyword::Unset => "unset",
            CSSWideKeyword::Revert => "revert",
            CSSWideKeyword::RevertLayer => "revert-layer",
        }
    }
}

impl CSSWideKeyword {
    /// Parses a CSS wide keyword from a CSS identifier.
    pub fn from_ident(ident: &str) -> Result<Self, ()> {
        Ok(match_ignore_ascii_case! { ident,
            "initial" => CSSWideKeyword::Initial,
            "inherit" => CSSWideKeyword::Inherit,
            "unset" => CSSWideKeyword::Unset,
            "revert" => CSSWideKeyword::Revert,
            "revert-layer" => CSSWideKeyword::RevertLayer,
            _ => return Err(()),
        })
    }

    /// Parses a CSS wide keyword completely.
    pub fn parse(input: &mut Parser) -> Result<Self, ()> {
        let keyword = {
            let ident = input.expect_ident().map_err(|_| ())?;
            Self::from_ident(ident)?
        };
        input.expect_exhausted().map_err(|_| ())?;
        Ok(keyword)
    }
}

/// A declaration using a CSS-wide keyword.
#[derive(Clone, PartialEq, ToCss, ToShmem, MallocSizeOf)]
pub struct WideKeywordDeclaration {
    #[css(skip)]
    id: LonghandId,
    /// The CSS-wide keyword.
    pub keyword: CSSWideKeyword,
}

/// An unparsed declaration that contains `var()` functions.
#[derive(Clone, PartialEq, ToCss, ToShmem, MallocSizeOf)]
pub struct VariableDeclaration {
    /// The id of the property this declaration represents.
    #[css(skip)]
    id: LonghandId,
    /// The unparsed value of the variable.
    #[ignore_malloc_size_of = "Arc"]
    pub value: Arc<UnparsedValue>,
}

/// A custom property declaration value is either an unparsed value or a CSS
/// wide-keyword.
#[derive(Clone, PartialEq, ToCss, ToShmem)]
pub enum CustomDeclarationValue {
    /// A value.
    Value(Arc<custom_properties::SpecifiedValue>),
    /// A wide keyword.
    CSSWideKeyword(CSSWideKeyword),
}

/// A custom property declaration with the property name and the declared value.
#[derive(Clone, PartialEq, ToCss, ToShmem, MallocSizeOf)]
pub struct CustomDeclaration {
    /// The name of the custom property.
    #[css(skip)]
    pub name: custom_properties::Name,
    /// The value of the custom property.
    #[ignore_malloc_size_of = "Arc"]
    pub value: CustomDeclarationValue,
}

impl fmt::Debug for PropertyDeclaration {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.id().to_css(&mut CssWriter::new(f))?;
        f.write_str(": ")?;

        // Because PropertyDeclaration::to_css requires CssStringWriter, we can't write
        // it directly to f, and need to allocate an intermediate string. This is
        // fine for debug-only code.
        let mut s = CssString::new();
        self.to_css(&mut s)?;
        write!(f, "{}", s)
    }
}

/// A longhand or shorthand property.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, ToComputedValue, ToResolvedValue, ToShmem, MallocSizeOf)]
#[repr(C)]
pub struct NonCustomPropertyId(u16);

impl ToCss for NonCustomPropertyId {
    #[inline]
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_str(self.name())
    }
}

impl NonCustomPropertyId {
    /// Returns the underlying index, used for use counter.
    pub fn bit(self) -> usize {
        self.0 as usize
    }

    /// Convert a `NonCustomPropertyId` into a `nsCSSPropertyID`.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn to_nscsspropertyid(self) -> nsCSSPropertyID {
        // unsafe: guaranteed by static_assert_nscsspropertyid.
        unsafe { mem::transmute(self.0 as i32) }
    }

    /// Convert an `nsCSSPropertyID` into a `NonCustomPropertyId`.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn from_nscsspropertyid(prop: nsCSSPropertyID) -> Option<Self> {
        let prop = prop as i32;
        if prop < 0 || prop >= property_counts::NON_CUSTOM as i32 {
            return None;
        }
        // guaranteed by static_assert_nscsspropertyid above.
        Some(NonCustomPropertyId(prop as u16))
    }

    /// Resolves the alias of a given property if needed.
    pub fn unaliased(self) -> Self {
        let Some(alias_id) = self.as_alias() else {
            return self;
        };
        alias_id.aliased_property()
    }

    /// Turns this `NonCustomPropertyId` into a `PropertyId`.
    #[inline]
    pub fn to_property_id(self) -> PropertyId {
        PropertyId::NonCustom(self)
    }

    /// Returns a longhand id, if this property is one.
    #[inline]
    pub fn as_longhand(self) -> Option<LonghandId> {
        if self.0 < property_counts::LONGHANDS as u16 {
            return Some(unsafe { mem::transmute(self.0 as u16) });
        }
        None
    }

    /// Returns a shorthand id, if this property is one.
    #[inline]
    pub fn as_shorthand(self) -> Option<ShorthandId> {
        if self.0 >= property_counts::LONGHANDS as u16 &&
            self.0 < property_counts::LONGHANDS_AND_SHORTHANDS as u16
        {
            return Some(unsafe { mem::transmute(self.0 - (property_counts::LONGHANDS as u16)) });
        }
        None
    }

    /// Returns an alias id, if this property is one.
    #[inline]
    pub fn as_alias(self) -> Option<AliasId> {
        debug_assert!((self.0 as usize) < property_counts::NON_CUSTOM);
        if self.0 >= property_counts::LONGHANDS_AND_SHORTHANDS as u16 {
            return Some(unsafe {
                mem::transmute(self.0 - (property_counts::LONGHANDS_AND_SHORTHANDS as u16))
            });
        }
        None
    }

    /// Returns either a longhand or a shorthand, resolving aliases.
    #[inline]
    pub fn longhand_or_shorthand(self) -> Result<LonghandId, ShorthandId> {
        let id = self.unaliased();
        match id.as_longhand() {
            Some(lh) => Ok(lh),
            None => Err(id.as_shorthand().unwrap()),
        }
    }

    /// Converts a longhand id into a non-custom property id.
    #[inline]
    pub const fn from_longhand(id: LonghandId) -> Self {
        Self(id as u16)
    }

    /// Converts a shorthand id into a non-custom property id.
    #[inline]
    pub const fn from_shorthand(id: ShorthandId) -> Self {
        Self((id as u16) + (property_counts::LONGHANDS as u16))
    }

    /// Converts an alias id into a non-custom property id.
    #[inline]
    pub const fn from_alias(id: AliasId) -> Self {
        Self((id as u16) + (property_counts::LONGHANDS_AND_SHORTHANDS as u16))
    }
}

impl From<LonghandId> for NonCustomPropertyId {
    #[inline]
    fn from(id: LonghandId) -> Self {
        Self::from_longhand(id)
    }
}

impl From<ShorthandId> for NonCustomPropertyId {
    #[inline]
    fn from(id: ShorthandId) -> Self {
        Self::from_shorthand(id)
    }
}

impl From<AliasId> for NonCustomPropertyId {
    #[inline]
    fn from(id: AliasId) -> Self {
        Self::from_alias(id)
    }
}

/// Representation of a CSS property, that is, either a longhand, a shorthand, or a custom
/// property.
#[derive(Clone, Eq, PartialEq, Debug)]
pub enum PropertyId {
    /// An alias for a shorthand property.
    NonCustom(NonCustomPropertyId),
    /// A custom property.
    Custom(custom_properties::Name),
}

impl ToCss for PropertyId {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        match *self {
            PropertyId::NonCustom(id) => dest.write_str(id.name()),
            PropertyId::Custom(ref name) => {
                dest.write_str("--")?;
                serialize_atom_name(name, dest)
            },
        }
    }
}

impl PropertyId {
    /// Return the longhand id that this property id represents.
    #[inline]
    pub fn longhand_id(&self) -> Option<LonghandId> {
        self.non_custom_non_alias_id()?.as_longhand()
    }

    /// Returns true if this property is one of the animatable properties.
    pub fn is_animatable(&self) -> bool {
        match self {
            Self::NonCustom(id) => id.is_animatable(),
            Self::Custom(..) => true,
        }
    }

    /// Returns true if this property is one of the transitionable properties.
    pub fn is_transitionable(&self) -> bool {
        match self {
            Self::NonCustom(id) => id.is_transitionable(),
            Self::Custom(..) => true,
        }
    }

    /// Returns a given property from the given name, _regardless of whether it is enabled or
    /// not_, or Err(()) for unknown properties.
    ///
    /// Do not use for non-testing purposes.
    pub fn parse_unchecked_for_testing(name: &str) -> Result<Self, ()> {
        Self::parse_unchecked(name, None)
    }

    /// Parses a property name, and returns an error if it's unknown or isn't enabled for all
    /// content.
    #[inline]
    pub fn parse_enabled_for_all_content(name: &str) -> Result<Self, ()> {
        let id = Self::parse_unchecked(name, None)?;

        if !id.enabled_for_all_content() {
            return Err(());
        }

        Ok(id)
    }

    /// Parses a property name, and returns an error if it's unknown or isn't allowed in this
    /// context.
    #[inline]
    pub fn parse(name: &str, context: &ParserContext) -> Result<Self, ()> {
        let id = Self::parse_unchecked(name, context.use_counters)?;
        if !id.allowed_in(context) {
            return Err(());
        }
        Ok(id)
    }

    /// Parses a property name, and returns an error if it's unknown or isn't allowed in this
    /// context, ignoring the rule_type checks.
    ///
    /// This is useful for parsing stuff from CSS values, for example.
    #[inline]
    pub fn parse_ignoring_rule_type(name: &str, context: &ParserContext) -> Result<Self, ()> {
        let id = Self::parse_unchecked(name, None)?;
        if !id.allowed_in_ignoring_rule_type(context) {
            return Err(());
        }
        Ok(id)
    }

    /// Returns a property id from Gecko's nsCSSPropertyID.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn from_nscsspropertyid(id: nsCSSPropertyID) -> Option<Self> {
        Some(NonCustomPropertyId::from_nscsspropertyid(id)?.to_property_id())
    }

    /// Returns a property id from Gecko's AnimatedPropertyID.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn from_gecko_animated_property_id(property: &AnimatedPropertyID) -> Option<Self> {
        Some(
            if property.mID == nsCSSPropertyID::eCSSPropertyExtra_variable {
                debug_assert!(!property.mCustomName.mRawPtr.is_null());
                Self::Custom(unsafe { crate::Atom::from_raw(property.mCustomName.mRawPtr) })
            } else {
                Self::NonCustom(NonCustomPropertyId::from_nscsspropertyid(property.mID)?)
            },
        )
    }

    /// Returns true if the property is a shorthand or shorthand alias.
    #[inline]
    pub fn is_shorthand(&self) -> bool {
        self.as_shorthand().is_ok()
    }

    /// Given this property id, get it either as a shorthand or as a
    /// `PropertyDeclarationId`.
    pub fn as_shorthand(&self) -> Result<ShorthandId, PropertyDeclarationId> {
        match *self {
            Self::NonCustom(id) => match id.longhand_or_shorthand() {
                Ok(lh) => Err(PropertyDeclarationId::Longhand(lh)),
                Err(sh) => Ok(sh),
            },
            Self::Custom(ref name) => Err(PropertyDeclarationId::Custom(name)),
        }
    }

    /// Returns the `NonCustomPropertyId` corresponding to this property id.
    pub fn non_custom_id(&self) -> Option<NonCustomPropertyId> {
        match *self {
            Self::Custom(_) => None,
            Self::NonCustom(id) => Some(id),
        }
    }

    /// Returns non-alias NonCustomPropertyId corresponding to this
    /// property id.
    fn non_custom_non_alias_id(&self) -> Option<NonCustomPropertyId> {
        self.non_custom_id().map(NonCustomPropertyId::unaliased)
    }

    /// Whether the property is enabled for all content regardless of the
    /// stylesheet it was declared on (that is, in practice only checks prefs).
    #[inline]
    pub fn enabled_for_all_content(&self) -> bool {
        let id = match self.non_custom_id() {
            // Custom properties are allowed everywhere
            None => return true,
            Some(id) => id,
        };

        id.enabled_for_all_content()
    }

    /// Converts this PropertyId in nsCSSPropertyID, resolving aliases to the
    /// resolved property, and returning eCSSPropertyExtra_variable for custom
    /// properties.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn to_nscsspropertyid_resolving_aliases(&self) -> nsCSSPropertyID {
        match self.non_custom_non_alias_id() {
            Some(id) => id.to_nscsspropertyid(),
            None => nsCSSPropertyID::eCSSPropertyExtra_variable,
        }
    }

    fn allowed_in(&self, context: &ParserContext) -> bool {
        let id = match self.non_custom_id() {
            // Custom properties are allowed everywhere
            None => return true,
            Some(id) => id,
        };
        id.allowed_in(context)
    }

    #[inline]
    fn allowed_in_ignoring_rule_type(&self, context: &ParserContext) -> bool {
        let id = match self.non_custom_id() {
            // Custom properties are allowed everywhere
            None => return true,
            Some(id) => id,
        };
        id.allowed_in_ignoring_rule_type(context)
    }

    /// Whether the property supports the given CSS type.
    /// `ty` should a bitflags of constants in style_traits::CssType.
    pub fn supports_type(&self, ty: u8) -> bool {
        let id = self.non_custom_non_alias_id();
        id.map_or(0, |id| id.supported_types()) & ty != 0
    }

    /// Collect supported starting word of values of this property.
    ///
    /// See style_traits::SpecifiedValueInfo::collect_completion_keywords for more
    /// details.
    pub fn collect_property_completion_keywords(&self, f: KeywordsCollectFn) {
        if let Some(id) = self.non_custom_non_alias_id() {
            id.collect_property_completion_keywords(f);
        }
        CSSWideKeyword::collect_completion_keywords(f);
    }
}

impl ToCss for LonghandId {
    #[inline]
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_str(self.name())
    }
}

impl fmt::Debug for LonghandId {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str(self.name())
    }
}

impl LonghandId {
    /// Get the name of this longhand property.
    #[inline]
    pub fn name(&self) -> &'static str {
        NonCustomPropertyId::from(*self).name()
    }

    /// Returns whether the longhand property is inherited by default.
    #[inline]
    pub fn inherited(self) -> bool {
        !LonghandIdSet::reset().contains(self)
    }

    /// Returns true if the property is one that is ignored when document
    /// colors are disabled.
    #[inline]
    pub fn ignored_when_document_colors_disabled(self) -> bool {
        LonghandIdSet::ignored_when_colors_disabled().contains(self)
    }

    /// Returns whether this longhand is `non_custom` or is a longhand of it.
    pub fn is_or_is_longhand_of(self, non_custom: NonCustomPropertyId) -> bool {
        match non_custom.longhand_or_shorthand() {
            Ok(lh) => self == lh,
            Err(sh) => self.is_longhand_of(sh),
        }
    }

    /// Returns whether this longhand is a longhand of `shorthand`.
    pub fn is_longhand_of(self, shorthand: ShorthandId) -> bool {
        self.shorthands().any(|s| s == shorthand)
    }

    /// Returns whether this property is animatable.
    #[inline]
    pub fn is_animatable(self) -> bool {
        NonCustomPropertyId::from(self).is_animatable()
    }

    /// Returns whether this property is animatable in a discrete way.
    #[inline]
    pub fn is_discrete_animatable(self) -> bool {
        LonghandIdSet::discrete_animatable().contains(self)
    }

    /// Converts from a LonghandId to an adequate nsCSSPropertyID.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn to_nscsspropertyid(self) -> nsCSSPropertyID {
        NonCustomPropertyId::from(self).to_nscsspropertyid()
    }

    #[cfg(feature = "gecko")]
    /// Returns a longhand id from Gecko's nsCSSPropertyID.
    pub fn from_nscsspropertyid(id: nsCSSPropertyID) -> Option<Self> {
        NonCustomPropertyId::from_nscsspropertyid(id)?
            .unaliased()
            .as_longhand()
    }

    /// Return whether this property is logical.
    #[inline]
    pub fn is_logical(self) -> bool {
        LonghandIdSet::logical().contains(self)
    }
}

impl ToCss for ShorthandId {
    #[inline]
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_str(self.name())
    }
}

impl ShorthandId {
    /// Get the name for this shorthand property.
    #[inline]
    pub fn name(&self) -> &'static str {
        NonCustomPropertyId::from(*self).name()
    }

    /// Converts from a ShorthandId to an adequate nsCSSPropertyID.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn to_nscsspropertyid(self) -> nsCSSPropertyID {
        NonCustomPropertyId::from(self).to_nscsspropertyid()
    }

    /// Converts from a nsCSSPropertyID to a ShorthandId.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn from_nscsspropertyid(id: nsCSSPropertyID) -> Option<Self> {
        NonCustomPropertyId::from_nscsspropertyid(id)?
            .unaliased()
            .as_shorthand()
    }

    /// Finds and returns an appendable value for the given declarations.
    ///
    /// Returns the optional appendable value.
    pub fn get_shorthand_appendable_value<'a, 'b: 'a>(
        self,
        declarations: &'a [&'b PropertyDeclaration],
    ) -> Option<AppendableValue<'a, 'b>> {
        let first_declaration = declarations.get(0)?;
        let rest = || declarations.iter().skip(1);

        // https://drafts.csswg.org/css-variables/#variables-in-shorthands
        if let Some(css) = first_declaration.with_variables_from_shorthand(self) {
            if rest().all(|d| d.with_variables_from_shorthand(self) == Some(css)) {
                return Some(AppendableValue::Css(css));
            }
            return None;
        }

        // Check whether they are all the same CSS-wide keyword.
        if let Some(keyword) = first_declaration.get_css_wide_keyword() {
            if rest().all(|d| d.get_css_wide_keyword() == Some(keyword)) {
                return Some(AppendableValue::Css(keyword.to_str()));
            }
            return None;
        }

        if self == ShorthandId::All {
            // 'all' only supports variables and CSS wide keywords.
            return None;
        }

        // Check whether all declarations can be serialized as part of shorthand.
        if declarations
            .iter()
            .all(|d| d.may_serialize_as_part_of_shorthand())
        {
            return Some(AppendableValue::DeclarationsForShorthand(
                self,
                declarations,
            ));
        }

        None
    }

    /// Returns whether this property is a legacy shorthand.
    #[inline]
    pub fn is_legacy_shorthand(self) -> bool {
        self.flags().contains(PropertyFlags::IS_LEGACY_SHORTHAND)
    }
}

impl PropertyDeclaration {
    fn with_variables_from_shorthand(&self, shorthand: ShorthandId) -> Option<&str> {
        match *self {
            PropertyDeclaration::WithVariables(ref declaration) => {
                let s = declaration.value.from_shorthand?;
                if s != shorthand {
                    return None;
                }
                Some(&*declaration.value.variable_value.css)
            },
            _ => None,
        }
    }

    /// Returns a CSS-wide keyword declaration for a given property.
    #[inline]
    pub fn css_wide_keyword(id: LonghandId, keyword: CSSWideKeyword) -> Self {
        Self::CSSWideKeyword(WideKeywordDeclaration { id, keyword })
    }

    /// Returns a CSS-wide keyword if the declaration's value is one.
    #[inline]
    pub fn get_css_wide_keyword(&self) -> Option<CSSWideKeyword> {
        match *self {
            PropertyDeclaration::CSSWideKeyword(ref declaration) => Some(declaration.keyword),
            _ => None,
        }
    }

    /// Returns whether the declaration may be serialized as part of a shorthand.
    ///
    /// This method returns false if this declaration contains variable or has a
    /// CSS-wide keyword value, since these values cannot be serialized as part
    /// of a shorthand.
    ///
    /// Caller should check `with_variables_from_shorthand()` and whether all
    /// needed declarations has the same CSS-wide keyword first.
    ///
    /// Note that, serialization of a shorthand may still fail because of other
    /// property-specific requirement even when this method returns true for all
    /// the longhand declarations.
    pub fn may_serialize_as_part_of_shorthand(&self) -> bool {
        match *self {
            PropertyDeclaration::CSSWideKeyword(..) | PropertyDeclaration::WithVariables(..) => {
                false
            },
            PropertyDeclaration::Custom(..) => {
                unreachable!("Serializing a custom property as part of shorthand?")
            },
            _ => true,
        }
    }

    /// Returns true if this property declaration is for one of the animatable properties.
    pub fn is_animatable(&self) -> bool {
        self.id().is_animatable()
    }

    /// Returns true if this property is a custom property, false
    /// otherwise.
    pub fn is_custom(&self) -> bool {
        matches!(*self, PropertyDeclaration::Custom(..))
    }

    /// The `context` parameter controls this:
    ///
    /// <https://drafts.csswg.org/css-animations/#keyframes>
    /// > The <declaration-list> inside of <keyframe-block> accepts any CSS property
    /// > except those defined in this specification,
    /// > but does accept the `animation-play-state` property and interprets it specially.
    ///
    /// This will not actually parse Importance values, and will always set things
    /// to Importance::Normal. Parsing Importance values is the job of PropertyDeclarationParser,
    /// we only set them here so that we don't have to reallocate
    pub fn parse_into<'i, 't>(
        declarations: &mut SourcePropertyDeclaration,
        id: PropertyId,
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<(), ParseError<'i>> {
        assert!(declarations.is_empty());
        debug_assert!(id.allowed_in(context), "{:?}", id);
        input.skip_whitespace();

        let start = input.state();
        let non_custom_id = match id {
            PropertyId::Custom(property_name) => {
                let value = match input.try_parse(CSSWideKeyword::parse) {
                    Ok(keyword) => CustomDeclarationValue::CSSWideKeyword(keyword),
                    Err(()) => CustomDeclarationValue::Value(Arc::new(
                        custom_properties::VariableValue::parse(input, &context.url_data)?,
                    )),
                };
                declarations.push(PropertyDeclaration::Custom(CustomDeclaration {
                    name: property_name,
                    value,
                }));
                return Ok(());
            },
            PropertyId::NonCustom(id) => id,
        };
        match non_custom_id.longhand_or_shorthand() {
            Ok(longhand_id) => {
                let declaration = input
                    .try_parse(CSSWideKeyword::parse)
                    .map(|keyword| PropertyDeclaration::css_wide_keyword(longhand_id, keyword))
                    .or_else(|()| {
                        input.look_for_var_or_env_functions();
                        input.parse_entirely(|input| longhand_id.parse_value(context, input))
                    })
                    .or_else(|err| {
                        while let Ok(_) = input.next() {} // Look for var() after the error.
                        if !input.seen_var_or_env_functions() {
                            return Err(err);
                        }
                        input.reset(&start);
                        let variable_value =
                            custom_properties::VariableValue::parse(input, &context.url_data)?;
                        Ok(PropertyDeclaration::WithVariables(VariableDeclaration {
                            id: longhand_id,
                            value: Arc::new(UnparsedValue {
                                variable_value,
                                from_shorthand: None,
                            }),
                        }))
                    })?;
                declarations.push(declaration)
            },
            Err(shorthand_id) => {
                if let Ok(keyword) = input.try_parse(CSSWideKeyword::parse) {
                    if shorthand_id == ShorthandId::All {
                        declarations.all_shorthand = AllShorthand::CSSWideKeyword(keyword)
                    } else {
                        for longhand in shorthand_id.longhands() {
                            declarations
                                .push(PropertyDeclaration::css_wide_keyword(longhand, keyword));
                        }
                    }
                } else {
                    input.look_for_var_or_env_functions();
                    // Not using parse_entirely here: each
                    // ${shorthand.ident}::parse_into function needs to do so
                    // *before* pushing to `declarations`.
                    shorthand_id
                        .parse_into(declarations, context, input)
                        .or_else(|err| {
                            while let Ok(_) = input.next() {} // Look for var() after the error.
                            if !input.seen_var_or_env_functions() {
                                return Err(err);
                            }

                            input.reset(&start);
                            let variable_value =
                                custom_properties::VariableValue::parse(input, &context.url_data)?;
                            let unparsed = Arc::new(UnparsedValue {
                                variable_value,
                                from_shorthand: Some(shorthand_id),
                            });
                            if shorthand_id == ShorthandId::All {
                                declarations.all_shorthand = AllShorthand::WithVariables(unparsed)
                            } else {
                                for id in shorthand_id.longhands() {
                                    declarations.push(PropertyDeclaration::WithVariables(
                                        VariableDeclaration {
                                            id,
                                            value: unparsed.clone(),
                                        },
                                    ))
                                }
                            }
                            Ok(())
                        })?;
                }
            },
        }
        if let Some(use_counters) = context.use_counters {
            use_counters.non_custom_properties.record(non_custom_id);
        }
        Ok(())
    }
}

/// A PropertyDeclarationId without references, for use as a hash map key.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum OwnedPropertyDeclarationId {
    /// A longhand.
    Longhand(LonghandId),
    /// A custom property declaration.
    Custom(custom_properties::Name),
}

impl OwnedPropertyDeclarationId {
    /// Return whether this property is logical.
    #[inline]
    pub fn is_logical(&self) -> bool {
        self.as_borrowed().is_logical()
    }

    /// Returns the corresponding PropertyDeclarationId.
    #[inline]
    pub fn as_borrowed(&self) -> PropertyDeclarationId {
        match self {
            Self::Longhand(id) => PropertyDeclarationId::Longhand(*id),
            Self::Custom(name) => PropertyDeclarationId::Custom(name),
        }
    }

    /// Convert an `AnimatedPropertyID` into an `OwnedPropertyDeclarationId`.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn from_gecko_animated_property_id(property: &AnimatedPropertyID) -> Option<Self> {
        Some(
            match PropertyId::from_gecko_animated_property_id(property)? {
                PropertyId::Custom(name) => Self::Custom(name),
                PropertyId::NonCustom(id) => Self::Longhand(id.as_longhand()?),
            },
        )
    }
}

/// An identifier for a given property declaration, which can be either a
/// longhand or a custom property.
#[derive(Clone, Copy, Debug, PartialEq, MallocSizeOf)]
pub enum PropertyDeclarationId<'a> {
    /// A longhand.
    Longhand(LonghandId),
    /// A custom property declaration.
    Custom(&'a custom_properties::Name),
}

impl<'a> ToCss for PropertyDeclarationId<'a> {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        match *self {
            PropertyDeclarationId::Longhand(id) => dest.write_str(id.name()),
            PropertyDeclarationId::Custom(name) => {
                dest.write_str("--")?;
                serialize_atom_name(name, dest)
            },
        }
    }
}

impl<'a> PropertyDeclarationId<'a> {
    /// Returns PropertyFlags for given property.
    #[inline(always)]
    pub fn flags(&self) -> PropertyFlags {
        match self {
            Self::Longhand(id) => id.flags(),
            Self::Custom(_) => PropertyFlags::empty(),
        }
    }

    /// Convert to an OwnedPropertyDeclarationId.
    pub fn to_owned(&self) -> OwnedPropertyDeclarationId {
        match self {
            PropertyDeclarationId::Longhand(id) => OwnedPropertyDeclarationId::Longhand(*id),
            PropertyDeclarationId::Custom(name) => {
                OwnedPropertyDeclarationId::Custom((*name).clone())
            },
        }
    }

    /// Whether a given declaration id is either the same as `other`, or a
    /// longhand of it.
    pub fn is_or_is_longhand_of(&self, other: &PropertyId) -> bool {
        match *self {
            PropertyDeclarationId::Longhand(id) => match *other {
                PropertyId::NonCustom(non_custom_id) => id.is_or_is_longhand_of(non_custom_id),
                PropertyId::Custom(_) => false,
            },
            PropertyDeclarationId::Custom(name) => {
                matches!(*other, PropertyId::Custom(ref other_name) if name == other_name)
            },
        }
    }

    /// Whether a given declaration id is a longhand belonging to this
    /// shorthand.
    pub fn is_longhand_of(&self, shorthand: ShorthandId) -> bool {
        match *self {
            PropertyDeclarationId::Longhand(ref id) => id.is_longhand_of(shorthand),
            _ => false,
        }
    }

    /// Returns the name of the property without CSS escaping.
    pub fn name(&self) -> Cow<'static, str> {
        match *self {
            PropertyDeclarationId::Longhand(id) => id.name().into(),
            PropertyDeclarationId::Custom(name) => {
                let mut s = String::new();
                write!(&mut s, "--{}", name).unwrap();
                s.into()
            },
        }
    }

    /// Returns longhand id if it is, None otherwise.
    #[inline]
    pub fn as_longhand(&self) -> Option<LonghandId> {
        match *self {
            PropertyDeclarationId::Longhand(id) => Some(id),
            _ => None,
        }
    }

    /// Return whether this property is logical.
    #[inline]
    pub fn is_logical(&self) -> bool {
        match self {
            PropertyDeclarationId::Longhand(id) => id.is_logical(),
            PropertyDeclarationId::Custom(_) => false,
        }
    }

    /// If this is a logical property, return the corresponding physical one in
    /// the given writing mode.
    ///
    /// Otherwise, return unchanged.
    #[inline]
    pub fn to_physical(&self, wm: WritingMode) -> Self {
        match self {
            Self::Longhand(id) => Self::Longhand(id.to_physical(wm)),
            Self::Custom(_) => self.clone(),
        }
    }

    /// Returns whether this property is animatable.
    #[inline]
    pub fn is_animatable(&self) -> bool {
        match self {
            Self::Longhand(id) => id.is_animatable(),
            Self::Custom(_) => true,
        }
    }

    /// Returns whether this property is animatable in a discrete way.
    #[inline]
    pub fn is_discrete_animatable(&self) -> bool {
        match self {
            Self::Longhand(longhand) => longhand.is_discrete_animatable(),
            // TODO(bug 1846516): Refine this?
            Self::Custom(_) => true,
        }
    }

    /// Converts from a to an adequate nsCSSPropertyID, returning
    /// eCSSPropertyExtra_variable for custom properties.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn to_nscsspropertyid(self) -> nsCSSPropertyID {
        match self {
            PropertyDeclarationId::Longhand(id) => id.to_nscsspropertyid(),
            PropertyDeclarationId::Custom(_) => nsCSSPropertyID::eCSSPropertyExtra_variable,
        }
    }

    /// Convert a `PropertyDeclarationId` into an `AnimatedPropertyID`
    /// Note that the rust AnimatedPropertyID doesn't implement Drop, so owned controls whether the
    /// custom name should be addrefed or not.
    ///
    /// FIXME(emilio, bug 1870107): This is a bit error-prone. We should consider using cbindgen to
    /// generate the property id representation or so.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn to_gecko_animated_property_id(&self, owned: bool) -> AnimatedPropertyID {
        match self {
            Self::Longhand(id) => AnimatedPropertyID {
                mID: id.to_nscsspropertyid(),
                mCustomName: RefPtr::null(),
            },
            Self::Custom(name) => {
                let mut property_id = AnimatedPropertyID {
                    mID: nsCSSPropertyID::eCSSPropertyExtra_variable,
                    mCustomName: RefPtr::null(),
                };
                property_id.mCustomName.mRawPtr = if owned {
                    (*name).clone().into_addrefed()
                } else {
                    name.as_ptr()
                };
                property_id
            },
        }
    }
}

/// A set of all properties.
#[derive(Clone, PartialEq, Default)]
pub struct NonCustomPropertyIdSet {
    storage: [u32; ((property_counts::NON_CUSTOM as usize) - 1 + 32) / 32],
}

impl NonCustomPropertyIdSet {
    /// Creates an empty `NonCustomPropertyIdSet`.
    pub fn new() -> Self {
        Self {
            storage: Default::default(),
        }
    }

    /// Insert a non-custom-property in the set.
    #[inline]
    pub fn insert(&mut self, id: NonCustomPropertyId) {
        let bit = id.0 as usize;
        self.storage[bit / 32] |= 1 << (bit % 32);
    }

    /// Return whether the given property is in the set
    #[inline]
    pub fn contains(&self, id: NonCustomPropertyId) -> bool {
        let bit = id.0 as usize;
        (self.storage[bit / 32] & (1 << (bit % 32))) != 0
    }
}

/// A set of longhand properties
#[derive(Clone, Copy, Debug, Default, MallocSizeOf, PartialEq)]
pub struct LonghandIdSet {
    storage: [u32; ((property_counts::LONGHANDS as usize) - 1 + 32) / 32],
}

to_shmem::impl_trivial_to_shmem!(LonghandIdSet);

impl LonghandIdSet {
    /// Return an empty LonghandIdSet.
    #[inline]
    pub fn new() -> Self {
        Self {
            storage: Default::default(),
        }
    }

    /// Iterate over the current longhand id set.
    pub fn iter(&self) -> LonghandIdSetIterator {
        LonghandIdSetIterator {
            longhands: self,
            cur: 0,
        }
    }

    /// Returns whether this set contains at least every longhand that `other`
    /// also contains.
    pub fn contains_all(&self, other: &Self) -> bool {
        for (self_cell, other_cell) in self.storage.iter().zip(other.storage.iter()) {
            if (*self_cell & *other_cell) != *other_cell {
                return false;
            }
        }
        true
    }

    /// Returns whether this set contains any longhand that `other` also contains.
    pub fn contains_any(&self, other: &Self) -> bool {
        for (self_cell, other_cell) in self.storage.iter().zip(other.storage.iter()) {
            if (*self_cell & *other_cell) != 0 {
                return true;
            }
        }
        false
    }

    /// Remove all the given properties from the set.
    #[inline]
    pub fn remove_all(&mut self, other: &Self) {
        for (self_cell, other_cell) in self.storage.iter_mut().zip(other.storage.iter()) {
            *self_cell &= !*other_cell;
        }
    }

    /// Return whether the given property is in the set
    #[inline]
    pub fn contains(&self, id: LonghandId) -> bool {
        let bit = id as usize;
        (self.storage[bit / 32] & (1 << (bit % 32))) != 0
    }

    /// Return whether this set contains any reset longhand.
    #[inline]
    pub fn contains_any_reset(&self) -> bool {
        self.contains_any(Self::reset())
    }

    /// Add the given property to the set
    #[inline]
    pub fn insert(&mut self, id: LonghandId) {
        let bit = id as usize;
        self.storage[bit / 32] |= 1 << (bit % 32);
    }

    /// Remove the given property from the set
    #[inline]
    pub fn remove(&mut self, id: LonghandId) {
        let bit = id as usize;
        self.storage[bit / 32] &= !(1 << (bit % 32));
    }

    /// Clear all bits
    #[inline]
    pub fn clear(&mut self) {
        for cell in &mut self.storage {
            *cell = 0
        }
    }

    /// Returns whether the set is empty.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.storage.iter().all(|c| *c == 0)
    }
}

/// An iterator over a set of longhand ids.
pub struct LonghandIdSetIterator<'a> {
    longhands: &'a LonghandIdSet,
    cur: usize,
}

impl<'a> Iterator for LonghandIdSetIterator<'a> {
    type Item = LonghandId;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if self.cur >= property_counts::LONGHANDS {
                return None;
            }

            let id: LonghandId = unsafe { mem::transmute(self.cur as u16) };
            self.cur += 1;

            if self.longhands.contains(id) {
                return Some(id);
            }
        }
    }
}

/// An ArrayVec of subproperties, contains space for the longest shorthand except all.
pub type SubpropertiesVec<T> = ArrayVec<T, { property_counts::MAX_SHORTHAND_EXPANDED }>;

/// A stack-allocated vector of `PropertyDeclaration`
/// large enough to parse one CSS `key: value` declaration.
/// (Shorthands expand to multiple `PropertyDeclaration`s.)
#[derive(Default)]
pub struct SourcePropertyDeclaration {
    /// The storage for the actual declarations (except for all).
    pub declarations: SubpropertiesVec<PropertyDeclaration>,
    /// Stored separately to keep SubpropertiesVec smaller.
    pub all_shorthand: AllShorthand,
}

// This is huge, but we allocate it on the stack and then never move it,
// we only pass `&mut SourcePropertyDeclaration` references around.
size_of_test!(SourcePropertyDeclaration, 632);

impl SourcePropertyDeclaration {
    /// Create one with a single PropertyDeclaration.
    #[inline]
    pub fn with_one(decl: PropertyDeclaration) -> Self {
        let mut result = Self::default();
        result.declarations.push(decl);
        result
    }

    /// Similar to Vec::drain: leaves this empty when the return value is dropped.
    pub fn drain(&mut self) -> SourcePropertyDeclarationDrain {
        SourcePropertyDeclarationDrain {
            declarations: self.declarations.drain(..),
            all_shorthand: mem::replace(&mut self.all_shorthand, AllShorthand::NotSet),
        }
    }

    /// Reset to initial state
    pub fn clear(&mut self) {
        self.declarations.clear();
        self.all_shorthand = AllShorthand::NotSet;
    }

    /// Whether we're empty.
    pub fn is_empty(&self) -> bool {
        self.declarations.is_empty() && matches!(self.all_shorthand, AllShorthand::NotSet)
    }

    /// Push a single declaration.
    pub fn push(&mut self, declaration: PropertyDeclaration) {
        let _result = self.declarations.try_push(declaration);
        debug_assert!(_result.is_ok());
    }
}

/// Return type of SourcePropertyDeclaration::drain
pub struct SourcePropertyDeclarationDrain<'a> {
    /// A drain over the non-all declarations.
    pub declarations:
        ArrayVecDrain<'a, PropertyDeclaration, { property_counts::MAX_SHORTHAND_EXPANDED }>,
    /// The all shorthand that was set.
    pub all_shorthand: AllShorthand,
}

/// An unparsed property value that contains `var()` functions.
#[derive(Debug, Eq, PartialEq, ToShmem)]
pub struct UnparsedValue {
    /// The variable value, references and so on.
    pub(super) variable_value: custom_properties::VariableValue,
    /// The shorthand this came from.
    from_shorthand: Option<ShorthandId>,
}

impl ToCss for UnparsedValue {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        // https://drafts.csswg.org/css-variables/#variables-in-shorthands
        if self.from_shorthand.is_none() {
            self.variable_value.to_css(dest)?;
        }
        Ok(())
    }
}

/// A simple cache for properties that come from a shorthand and have variable
/// references.
///
/// This cache works because of the fact that you can't have competing values
/// for a given longhand coming from the same shorthand (but note that this is
/// why the shorthand needs to be part of the cache key).
pub type ShorthandsWithPropertyReferencesCache =
    FxHashMap<(ShorthandId, LonghandId), PropertyDeclaration>;

impl UnparsedValue {
    fn substitute_variables<'cache>(
        &self,
        longhand_id: LonghandId,
        custom_properties: &ComputedCustomProperties,
        stylist: &Stylist,
        computed_context: &computed::Context,
        shorthand_cache: &'cache mut ShorthandsWithPropertyReferencesCache,
    ) -> Cow<'cache, PropertyDeclaration> {
        let invalid_at_computed_value_time = || {
            let keyword = if longhand_id.inherited() {
                CSSWideKeyword::Inherit
            } else {
                CSSWideKeyword::Initial
            };
            Cow::Owned(PropertyDeclaration::css_wide_keyword(longhand_id, keyword))
        };

        if computed_context
            .builder
            .invalid_non_custom_properties
            .contains(longhand_id)
        {
            return invalid_at_computed_value_time();
        }

        if let Some(shorthand_id) = self.from_shorthand {
            let key = (shorthand_id, longhand_id);
            if shorthand_cache.contains_key(&key) {
                // FIXME: This double lookup should be avoidable, but rustc
                // doesn't like that, see:
                //
                // https://github.com/rust-lang/rust/issues/82146
                return Cow::Borrowed(&shorthand_cache[&key]);
            }
        }

        let css = match custom_properties::substitute(
            &self.variable_value,
            custom_properties,
            stylist,
            computed_context,
        ) {
            Ok(css) => css,
            Err(..) => return invalid_at_computed_value_time(),
        };

        // As of this writing, only the base URL is used for property
        // values.
        //
        // NOTE(emilio): we intentionally pase `None` as the rule type here.
        // If something starts depending on it, it's probably a bug, since
        // it'd change how values are parsed depending on whether we're in a
        // @keyframes rule or not, for example... So think twice about
        // whether you want to do this!
        //
        // FIXME(emilio): ParsingMode is slightly fishy...
        let context = ParserContext::new(
            Origin::Author,
            &self.variable_value.url_data,
            None,
            ParsingMode::DEFAULT,
            computed_context.quirks_mode,
            /* namespaces = */ Default::default(),
            None,
            None,
        );

        let mut input = ParserInput::new(&css);
        let mut input = Parser::new(&mut input);
        input.skip_whitespace();

        if let Ok(keyword) = input.try_parse(CSSWideKeyword::parse) {
            return Cow::Owned(PropertyDeclaration::css_wide_keyword(longhand_id, keyword));
        }

        let shorthand = match self.from_shorthand {
            None => {
                return match input.parse_entirely(|input| longhand_id.parse_value(&context, input))
                {
                    Ok(decl) => Cow::Owned(decl),
                    Err(..) => invalid_at_computed_value_time(),
                }
            },
            Some(shorthand) => shorthand,
        };

        let mut decls = SourcePropertyDeclaration::default();
        // parse_into takes care of doing `parse_entirely` for us.
        if shorthand
            .parse_into(&mut decls, &context, &mut input)
            .is_err()
        {
            return invalid_at_computed_value_time();
        }

        for declaration in decls.declarations.drain(..) {
            let longhand = declaration.id().as_longhand().unwrap();
            if longhand.is_logical() {
                let writing_mode = computed_context.builder.writing_mode;
                shorthand_cache.insert(
                    (shorthand, longhand.to_physical(writing_mode)),
                    declaration.clone(),
                );
            }
            shorthand_cache.insert((shorthand, longhand), declaration);
        }

        let key = (shorthand, longhand_id);
        match shorthand_cache.get(&key) {
            Some(decl) => Cow::Borrowed(decl),
            None => {
                // FIXME: We should always have the key here but it seems
                // sometimes we don't, see bug 1696409.
                #[cfg(feature = "gecko")]
                {
                    if crate::gecko_bindings::structs::GECKO_IS_NIGHTLY {
                        panic!("Expected {:?} to be in the cache but it was not!", key);
                    }
                }
                invalid_at_computed_value_time()
            },
        }
    }
}
/// A parsed all-shorthand value.
pub enum AllShorthand {
    /// Not present.
    NotSet,
    /// A CSS-wide keyword.
    CSSWideKeyword(CSSWideKeyword),
    /// An all shorthand with var() references that we can't resolve right now.
    WithVariables(Arc<UnparsedValue>),
}

impl Default for AllShorthand {
    fn default() -> Self {
        Self::NotSet
    }
}

impl AllShorthand {
    /// Iterates property declarations from the given all shorthand value.
    #[inline]
    pub fn declarations(&self) -> AllShorthandDeclarationIterator {
        AllShorthandDeclarationIterator {
            all_shorthand: self,
            longhands: ShorthandId::All.longhands(),
        }
    }
}

/// An iterator over the all shorthand's shorthand declarations.
pub struct AllShorthandDeclarationIterator<'a> {
    all_shorthand: &'a AllShorthand,
    longhands: NonCustomPropertyIterator<LonghandId>,
}

impl<'a> Iterator for AllShorthandDeclarationIterator<'a> {
    type Item = PropertyDeclaration;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        match *self.all_shorthand {
            AllShorthand::NotSet => None,
            AllShorthand::CSSWideKeyword(ref keyword) => Some(
                PropertyDeclaration::css_wide_keyword(self.longhands.next()?, *keyword),
            ),
            AllShorthand::WithVariables(ref unparsed) => {
                Some(PropertyDeclaration::WithVariables(VariableDeclaration {
                    id: self.longhands.next()?,
                    value: unparsed.clone(),
                }))
            },
        }
    }
}

/// An iterator over all the property ids that are enabled for a given
/// shorthand, if that shorthand is enabled for all content too.
pub struct NonCustomPropertyIterator<Item: 'static> {
    filter: bool,
    iter: std::slice::Iter<'static, Item>,
}

impl<Item> Iterator for NonCustomPropertyIterator<Item>
where
    Item: 'static + Copy + Into<NonCustomPropertyId>,
{
    type Item = Item;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let id = *self.iter.next()?;
            if !self.filter || id.into().enabled_for_all_content() {
                return Some(id);
            }
        }
    }
}
