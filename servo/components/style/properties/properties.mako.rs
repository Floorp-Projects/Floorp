/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// This file is a Mako template: http://www.makotemplates.org/

// Please note that valid Rust syntax may be mangled by the Mako parser.
// For example, Vec<&Foo> will be mangled as Vec&Foo>. To work around these issues, the code
// can be escaped. In the above example, Vec<<&Foo> or Vec< &Foo> achieves the desired result of Vec<&Foo>.

<%namespace name="helpers" file="/helpers.mako.rs" />

use app_units::Au;
use servo_arc::{Arc, UniqueArc};
use std::{ops, ptr};
use std::{fmt, mem};

#[cfg(feature = "servo")] use euclid::SideOffsets2D;
#[cfg(feature = "gecko")] use crate::gecko_bindings::structs::{self, nsCSSPropertyID};
#[cfg(feature = "servo")] use crate::logical_geometry::LogicalMargin;
#[cfg(feature = "servo")] use crate::computed_values;
use crate::logical_geometry::WritingMode;
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps};
use crate::computed_value_flags::*;
use cssparser::Parser;
use crate::media_queries::Device;
use crate::parser::ParserContext;
use crate::selector_parser::PseudoElement;
use crate::stylist::Stylist;
#[cfg(feature = "servo")] use servo_config::prefs;
use style_traits::{CssWriter, KeywordsCollectFn, ParseError, SpecifiedValueInfo, StyleParseErrorKind, ToCss};
use crate::stylesheets::{CssRuleType, CssRuleTypes, Origin};
use crate::logical_geometry::{LogicalAxis, LogicalCorner, LogicalSide};
use crate::use_counters::UseCounters;
use crate::rule_tree::StrongRuleNode;
use crate::str::CssStringWriter;
use crate::values::{
    computed,
    resolved,
    specified::{font::SystemFont, length::LineHeightBase},
};
use std::cell::Cell;
use super::{
    PropertyDeclarationId, PropertyId, NonCustomPropertyId,
    NonCustomPropertyIdSet, PropertyFlags, SourcePropertyDeclaration,
    LonghandIdSet, VariableDeclaration, CustomDeclaration,
    WideKeywordDeclaration, NonCustomPropertyIterator,
};

<%!
    from collections import defaultdict
    from data import Method, PropertyRestrictions, Keyword, to_rust_ident, \
                     to_camel_case, RULE_VALUES, SYSTEM_FONT_LONGHANDS, PRIORITARY_PROPERTIES
    import os.path
%>

/// Conversion with fewer impls than From/Into
pub trait MaybeBoxed<Out> {
    /// Convert
    fn maybe_boxed(self) -> Out;
}

impl<T> MaybeBoxed<T> for T {
    #[inline]
    fn maybe_boxed(self) -> T { self }
}

impl<T> MaybeBoxed<Box<T>> for T {
    #[inline]
    fn maybe_boxed(self) -> Box<T> { Box::new(self) }
}

macro_rules! expanded {
    ( $( $name: ident: $value: expr ),+ ) => {
        expanded!( $( $name: $value, )+ )
    };
    ( $( $name: ident: $value: expr, )+ ) => {
        Longhands {
            $(
                $name: MaybeBoxed::maybe_boxed($value),
            )+
        }
    }
}

/// A module with all the code for longhand properties.
#[allow(missing_docs)]
pub mod longhands {
    % for style_struct in data.style_structs:
    include!("${repr(os.path.join(OUT_DIR, 'longhands/{}.rs'.format(style_struct.name_lower)))[1:-1]}");
    % endfor
}

macro_rules! unwrap_or_initial {
    ($prop: ident) => (unwrap_or_initial!($prop, $prop));
    ($prop: ident, $expr: expr) =>
        ($expr.unwrap_or_else(|| $prop::get_initial_specified_value()));
}

/// A module with code for all the shorthand css properties, and a few
/// serialization helpers.
#[allow(missing_docs)]
pub mod shorthands {
    use cssparser::Parser;
    use crate::parser::{Parse, ParserContext};
    use style_traits::{ParseError, StyleParseErrorKind};
    use crate::values::specified;

    % for style_struct in data.style_structs:
    include!("${repr(os.path.join(OUT_DIR, 'shorthands/{}.rs'.format(style_struct.name_lower)))[1:-1]}");
    % endfor

    // We didn't define the 'all' shorthand using the regular helpers:shorthand
    // mechanism, since it causes some very large types to be generated.
    //
    // Also, make sure logical properties appear before its physical
    // counter-parts, in order to prevent bugs like:
    //
    //   https://bugzilla.mozilla.org/show_bug.cgi?id=1410028
    //
    // FIXME(emilio): Adopt the resolution from:
    //
    //   https://github.com/w3c/csswg-drafts/issues/1898
    //
    // when there is one, whatever that is.
    <%
        logical_longhands = []
        other_longhands = []

        for p in data.longhands:
            if p.name in ['direction', 'unicode-bidi']:
                continue;
            if not p.enabled_in_content() and not p.experimental(engine):
                continue;
            if "Style" not in p.rule_types_allowed_names():
                continue;
            if p.logical:
                logical_longhands.append(p.name)
            else:
                other_longhands.append(p.name)

        data.declare_shorthand(
            "all",
            logical_longhands + other_longhands,
            engines="gecko servo-2013 servo-2020",
            spec="https://drafts.csswg.org/css-cascade-3/#all-shorthand"
        )
        ALL_SHORTHAND_LEN = len(logical_longhands) + len(other_longhands);
    %>
}

<%
    from itertools import groupby

    # After this code, `data.longhands` is sorted in the following order:
    # - first all keyword variants and all variants known to be Copy,
    # - second all the other variants, such as all variants with the same field
    #   have consecutive discriminants.
    # The variable `variants` contain the same entries as `data.longhands` in
    # the same order, but must exist separately to the data source, because
    # we then need to add three additional variants `WideKeywordDeclaration`,
    # `VariableDeclaration` and `CustomDeclaration`.

    variants = []
    for property in data.longhands:
        variants.append({
            "name": property.camel_case,
            "type": property.specified_type(),
            "doc": "`" + property.name + "`",
            "copy": property.specified_is_copy(),
        })

    groups = {}
    keyfunc = lambda x: x["type"]
    sortkeys = {}
    for ty, group in groupby(sorted(variants, key=keyfunc), keyfunc):
        group = list(group)
        groups[ty] = group
        for v in group:
            if len(group) == 1:
                sortkeys[v["name"]] = (not v["copy"], 1, v["name"], "")
            else:
                sortkeys[v["name"]] = (not v["copy"], len(group), ty, v["name"])
    variants.sort(key=lambda x: sortkeys[x["name"]])

    # It is extremely important to sort the `data.longhands` array here so
    # that it is in the same order as `variants`, for `LonghandId` and
    # `PropertyDeclarationId` to coincide.
    data.longhands.sort(key=lambda x: sortkeys[x.camel_case])
%>

// WARNING: It is *really* important for the variants of `LonghandId`
// and `PropertyDeclaration` to be defined in the exact same order,
// with the exception of `CSSWideKeyword`, `WithVariables` and `Custom`,
// which don't exist in `LonghandId`.

<%
    extra_variants = [
        {
            "name": "CSSWideKeyword",
            "type": "WideKeywordDeclaration",
            "doc": "A CSS-wide keyword.",
            "copy": False,
        },
        {
            "name": "WithVariables",
            "type": "VariableDeclaration",
            "doc": "An unparsed declaration.",
            "copy": False,
        },
        {
            "name": "Custom",
            "type": "CustomDeclaration",
            "doc": "A custom property declaration.",
            "copy": False,
        },
    ]
    for v in extra_variants:
        variants.append(v)
        groups[v["type"]] = [v]
%>

/// Servo's representation for a property declaration.
#[derive(ToShmem)]
#[repr(u16)]
pub enum PropertyDeclaration {
    % for variant in variants:
    /// ${variant["doc"]}
    ${variant["name"]}(${variant["type"]}),
    % endfor
}

// There's one of these for each parsed declaration so it better be small.
size_of_test!(PropertyDeclaration, 32);

#[repr(C)]
struct PropertyDeclarationVariantRepr<T> {
    tag: u16,
    value: T
}

impl Clone for PropertyDeclaration {
    #[inline]
    fn clone(&self) -> Self {
        use self::PropertyDeclaration::*;

        <%
            [copy, others] = [list(g) for _, g in groupby(variants, key=lambda x: not x["copy"])]
        %>

        let self_tag = unsafe {
            (*(self as *const _ as *const PropertyDeclarationVariantRepr<()>)).tag
        };
        if self_tag <= LonghandId::${copy[-1]["name"]} as u16 {
            #[derive(Clone, Copy)]
            #[repr(u16)]
            enum CopyVariants {
                % for v in copy:
                _${v["name"]}(${v["type"]}),
                % endfor
            }

            unsafe {
                let mut out = mem::MaybeUninit::uninit();
                ptr::write(
                    out.as_mut_ptr() as *mut CopyVariants,
                    *(self as *const _ as *const CopyVariants),
                );
                return out.assume_init();
            }
        }

        // This function ensures that all properties not handled above
        // do not have a specified value implements Copy. If you hit
        // compile error here, you may want to add the type name into
        // Longhand.specified_is_copy in data.py.
        fn _static_assert_others_are_not_copy() {
            struct Helper<T>(T);
            trait AssertCopy { fn assert() {} }
            trait AssertNotCopy { fn assert() {} }
            impl<T: Copy> AssertCopy for Helper<T> {}
            % for ty in sorted(set(x["type"] for x in others)):
            impl AssertNotCopy for Helper<${ty}> {}
            Helper::<${ty}>::assert();
            % endfor
        }

        match *self {
            ${" |\n".join("{}(..)".format(v["name"]) for v in copy)} => {
                unsafe { debug_unreachable!() }
            }
            % for ty, vs in groupby(others, key=lambda x: x["type"]):
            <%
                vs = list(vs)
            %>
            % if len(vs) == 1:
            ${vs[0]["name"]}(ref value) => {
                ${vs[0]["name"]}(value.clone())
            }
            % else:
            ${" |\n".join("{}(ref value)".format(v["name"]) for v in vs)} => {
                unsafe {
                    let mut out = mem::MaybeUninit::uninit();
                    ptr::write(
                        out.as_mut_ptr() as *mut PropertyDeclarationVariantRepr<${ty}>,
                        PropertyDeclarationVariantRepr {
                            tag: *(self as *const _ as *const u16),
                            value: value.clone(),
                        },
                    );
                    out.assume_init()
                }
            }
            % endif
            % endfor
        }
    }
}

impl PartialEq for PropertyDeclaration {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        use self::PropertyDeclaration::*;

        unsafe {
            let this_repr =
                &*(self as *const _ as *const PropertyDeclarationVariantRepr<()>);
            let other_repr =
                &*(other as *const _ as *const PropertyDeclarationVariantRepr<()>);
            if this_repr.tag != other_repr.tag {
                return false;
            }
            match *self {
                % for ty, vs in groupby(variants, key=lambda x: x["type"]):
                ${" |\n".join("{}(ref this)".format(v["name"]) for v in vs)} => {
                    let other_repr =
                        &*(other as *const _ as *const PropertyDeclarationVariantRepr<${ty}>);
                    *this == other_repr.value
                }
                % endfor
            }
        }
    }
}

impl MallocSizeOf for PropertyDeclaration {
    #[inline]
    fn size_of(&self, ops: &mut MallocSizeOfOps) -> usize {
        use self::PropertyDeclaration::*;

        match *self {
            % for ty, vs in groupby(variants, key=lambda x: x["type"]):
            ${" | ".join("{}(ref value)".format(v["name"]) for v in vs)} => {
                value.size_of(ops)
            }
            % endfor
        }
    }
}


impl PropertyDeclaration {
    /// Returns the given value for this declaration as a particular type.
    /// It's the caller's responsibility to guarantee that the longhand id has the right specified
    /// value representation.
    pub(crate) unsafe fn unchecked_value_as<T>(&self) -> &T {
        &(*(self as *const _ as *const PropertyDeclarationVariantRepr<T>)).value
    }

    /// Dumps the property declaration before crashing.
    #[cold]
    #[cfg(debug_assertions)]
    pub(crate) fn debug_crash(&self, reason: &str) {
        panic!("{}: {:?}", reason, self);
    }
    #[cfg(not(debug_assertions))]
    #[inline(always)]
    pub(crate) fn debug_crash(&self, _reason: &str) {}

    /// Returns whether this is a variant of the Longhand(Value) type, rather
    /// than one of the special variants in extra_variants.
    fn is_longhand_value(&self) -> bool {
        match *self {
            % for v in extra_variants:
            PropertyDeclaration::${v["name"]}(..) => false,
            % endfor
            _ => true,
        }
    }

    /// Like the method on ToCss, but without the type parameter to avoid
    /// accidentally monomorphizing this large function multiple times for
    /// different writers.
    pub fn to_css(&self, dest: &mut CssStringWriter) -> fmt::Result {
        use self::PropertyDeclaration::*;

        let mut dest = CssWriter::new(dest);
        match *self {
            % for ty, vs in groupby(variants, key=lambda x: x["type"]):
            ${" | ".join("{}(ref value)".format(v["name"]) for v in vs)} => {
                value.to_css(&mut dest)
            }
            % endfor
        }
    }

    /// Returns the color value of a given property, for high-contrast-mode tweaks.
    pub(super) fn color_value(&self) -> Option<<&crate::values::specified::Color> {
        ${static_longhand_id_set("COLOR_PROPERTIES", lambda p: p.predefined_type == "Color")}
        <%
            # sanity check
            assert data.longhands_by_name["background-color"].predefined_type == "Color"

            color_specified_type = data.longhands_by_name["background-color"].specified_type()
        %>
        let id = self.id().as_longhand()?;
        if !COLOR_PROPERTIES.contains(id) || !self.is_longhand_value() {
            return None;
        }
        let repr = self as *const _ as *const PropertyDeclarationVariantRepr<${color_specified_type}>;
        Some(unsafe { &(*repr).value })
    }
}

/// A module with all the code related to animated properties.
///
/// This needs to be "included" by mako at least after all longhand modules,
/// given they populate the global data.
pub mod animated_properties {
    <%include file="/helpers/animated_properties.mako.rs" />
}

/// A module to group various interesting property counts.
pub mod property_counts {
    /// The number of (non-alias) longhand properties.
    pub const LONGHANDS: usize = ${len(data.longhands)};
    /// The number of (non-alias) shorthand properties.
    pub const SHORTHANDS: usize = ${len(data.shorthands)};
    /// The number of aliases.
    pub const ALIASES: usize = ${len(data.all_aliases())};
    /// The number of counted unknown properties.
    pub const COUNTED_UNKNOWN: usize = ${len(data.counted_unknown_properties)};
    /// The number of (non-alias) longhands and shorthands.
    pub const LONGHANDS_AND_SHORTHANDS: usize = LONGHANDS + SHORTHANDS;
    /// The number of non-custom properties.
    pub const NON_CUSTOM: usize = LONGHANDS_AND_SHORTHANDS + ALIASES;
    /// The number of prioritary properties that we have.
    pub const PRIORITARY: usize = ${len(PRIORITARY_PROPERTIES)};
    /// The max number of longhands that a shorthand other than "all" expands to.
    pub const MAX_SHORTHAND_EXPANDED: usize =
        ${max(len(s.sub_properties) for s in data.shorthands_except_all())};
    /// The max amount of longhands that the `all` shorthand will ever contain.
    pub const ALL_SHORTHAND_EXPANDED: usize = ${ALL_SHORTHAND_LEN};
    /// The number of animatable properties.
    pub const ANIMATABLE: usize = ${sum(1 for prop in data.longhands if prop.animatable)};
}

% if engine == "gecko":
#[allow(dead_code)]
unsafe fn static_assert_nscsspropertyid() {
    % for i, property in enumerate(data.longhands + data.shorthands + data.all_aliases()):
    std::mem::transmute::<[u8; ${i}], [u8; ${property.nscsspropertyid()} as usize]>([0; ${i}]); // ${property.name}
    % endfor
}
% endif

impl NonCustomPropertyId {
    /// Get the property name.
    #[inline]
    pub fn name(self) -> &'static str {
        static MAP: [&'static str; property_counts::NON_CUSTOM] = [
            % for property in data.longhands + data.shorthands + data.all_aliases():
            "${property.name}",
            % endfor
        ];
        MAP[self.0 as usize]
    }

    /// Returns whether this property is transitionable.
    #[inline]
    pub fn is_transitionable(self) -> bool {
        ${static_non_custom_property_id_set("TRANSITIONABLE", lambda p: p.transitionable)}
        TRANSITIONABLE.contains(self)
    }

    /// Returns whether this property is animatable.
    #[inline]
    pub fn is_animatable(self) -> bool {
        ${static_non_custom_property_id_set("ANIMATABLE", lambda p: p.animatable)}
        ANIMATABLE.contains(self)
    }

    /// Whether this property is enabled for all content right now.
    #[inline]
    pub(super) fn enabled_for_all_content(self) -> bool {
        ${static_non_custom_property_id_set(
            "EXPERIMENTAL",
            lambda p: p.experimental(engine)
        )}

        ${static_non_custom_property_id_set(
            "ALWAYS_ENABLED",
            lambda p: (not p.experimental(engine)) and p.enabled_in_content()
        )}

        let passes_pref_check = || {
            % if engine == "gecko":
                unsafe { structs::nsCSSProps_gPropertyEnabled[self.0 as usize] }
            % else:
                static PREF_NAME: [Option< &str>; ${
                    len(data.longhands) + len(data.shorthands) + len(data.all_aliases())
                }] = [
                    % for property in data.longhands + data.shorthands + data.all_aliases():
                        <%
                            attrs = {"servo-2013": "servo_2013_pref", "servo-2020": "servo_2020_pref"}
                            pref = getattr(property, attrs[engine])
                        %>
                        % if pref:
                            Some("${pref}"),
                        % else:
                            None,
                        % endif
                    % endfor
                ];
                let pref = match PREF_NAME[self.0 as usize] {
                    None => return true,
                    Some(pref) => pref,
                };

                prefs::pref_map().get(pref).as_bool().unwrap_or(false)
            % endif
        };

        if ALWAYS_ENABLED.contains(self) {
            return true
        }

        if EXPERIMENTAL.contains(self) && passes_pref_check() {
            return true
        }

        false
    }

    /// Returns whether a given rule allows a given property.
    #[inline]
    pub fn allowed_in_rule(self, rule_types: CssRuleTypes) -> bool {
        debug_assert!(
            rule_types.contains(CssRuleType::Keyframe) ||
            rule_types.contains(CssRuleType::Page) ||
            rule_types.contains(CssRuleType::Style),
            "Declarations are only expected inside a keyframe, page, or style rule."
        );

        static MAP: [u32; property_counts::NON_CUSTOM] = [
            % for property in data.longhands + data.shorthands + data.all_aliases():
            % for name in RULE_VALUES:
            % if property.rule_types_allowed & RULE_VALUES[name] != 0:
            CssRuleType::${name}.bit() |
            % endif
            % endfor
            0,
            % endfor
        ];
        MAP[self.0 as usize] & rule_types.bits() != 0
    }

    pub(super) fn allowed_in(self, context: &ParserContext) -> bool {
        if !self.allowed_in_rule(context.rule_types()) {
            return false;
        }

        self.allowed_in_ignoring_rule_type(context)
    }


    pub(super) fn allowed_in_ignoring_rule_type(self, context: &ParserContext) -> bool {
        // The semantics of these are kinda hard to reason about, what follows
        // is a description of the different combinations that can happen with
        // these three sets.
        //
        // Experimental properties are generally controlled by prefs, but an
        // experimental property explicitly enabled in certain context (UA or
        // chrome sheets) is always usable in the context regardless of the
        // pref value.
        //
        // Non-experimental properties are either normal properties which are
        // usable everywhere, or internal-only properties which are only usable
        // in certain context they are explicitly enabled in.
        if self.enabled_for_all_content() {
            return true;
        }

        ${static_non_custom_property_id_set(
            "ENABLED_IN_UA_SHEETS",
            lambda p: p.explicitly_enabled_in_ua_sheets()
        )}
        ${static_non_custom_property_id_set(
            "ENABLED_IN_CHROME",
            lambda p: p.explicitly_enabled_in_chrome()
        )}

        if context.stylesheet_origin == Origin::UserAgent &&
            ENABLED_IN_UA_SHEETS.contains(self)
        {
            return true
        }

        if context.chrome_rules_enabled() && ENABLED_IN_CHROME.contains(self) {
            return true
        }

        false
    }

    /// The supported types of this property. The return value should be
    /// style_traits::CssType when it can become a bitflags type.
    pub(super) fn supported_types(&self) -> u8 {
        const SUPPORTED_TYPES: [u8; ${len(data.longhands) + len(data.shorthands)}] = [
            % for prop in data.longhands:
                <${prop.specified_type()} as SpecifiedValueInfo>::SUPPORTED_TYPES,
            % endfor
            % for prop in data.shorthands:
            % if prop.name == "all":
                0, // 'all' accepts no value other than CSS-wide keywords
            % else:
                <shorthands::${prop.ident}::Longhands as SpecifiedValueInfo>::SUPPORTED_TYPES,
            % endif
            % endfor
        ];
        SUPPORTED_TYPES[self.0 as usize]
    }

    /// See PropertyId::collect_property_completion_keywords.
    pub(super) fn collect_property_completion_keywords(&self, f: KeywordsCollectFn) {
        fn do_nothing(_: KeywordsCollectFn) {}
        const COLLECT_FUNCTIONS: [fn(KeywordsCollectFn);
                                  ${len(data.longhands) + len(data.shorthands)}] = [
            % for prop in data.longhands:
                <${prop.specified_type()} as SpecifiedValueInfo>::collect_completion_keywords,
            % endfor
            % for prop in data.shorthands:
            % if prop.name == "all":
                do_nothing, // 'all' accepts no value other than CSS-wide keywords
            % else:
                <shorthands::${prop.ident}::Longhands as SpecifiedValueInfo>::
                    collect_completion_keywords,
            % endif
            % endfor
        ];
        COLLECT_FUNCTIONS[self.0 as usize](f);
    }
}

<%def name="static_non_custom_property_id_set(name, is_member)">
static ${name}: NonCustomPropertyIdSet = NonCustomPropertyIdSet {
    <%
        storage = [0] * int((len(data.longhands) + len(data.shorthands) + len(data.all_aliases()) - 1 + 32) / 32)
        for i, property in enumerate(data.longhands + data.shorthands + data.all_aliases()):
            if is_member(property):
                storage[int(i / 32)] |= 1 << (i % 32)
    %>
    storage: [${", ".join("0x%x" % word for word in storage)}]
};
</%def>

<%def name="static_longhand_id_set(name, is_member)">
static ${name}: LonghandIdSet = LonghandIdSet {
    <%
        storage = [0] * int((len(data.longhands) - 1 + 32) / 32)
        for i, property in enumerate(data.longhands):
            if is_member(property):
                storage[int(i / 32)] |= 1 << (i % 32)
    %>
    storage: [${", ".join("0x%x" % word for word in storage)}]
};
</%def>

<%
    logical_groups = defaultdict(list)
    for prop in data.longhands:
        if prop.logical_group:
            logical_groups[prop.logical_group].append(prop)

    for group, props in logical_groups.items():
        logical_count = sum(1 for p in props if p.logical)
        if logical_count * 2 != len(props):
            raise RuntimeError("Logical group {} has ".format(group) +
                               "unbalanced logical / physical properties")

    FIRST_LINE_RESTRICTIONS = PropertyRestrictions.first_line(data)
    FIRST_LETTER_RESTRICTIONS = PropertyRestrictions.first_letter(data)
    MARKER_RESTRICTIONS = PropertyRestrictions.marker(data)
    PLACEHOLDER_RESTRICTIONS = PropertyRestrictions.placeholder(data)
    CUE_RESTRICTIONS = PropertyRestrictions.cue(data)

    def restriction_flags(property):
        name = property.name
        flags = []
        if name in FIRST_LINE_RESTRICTIONS:
            flags.append("APPLIES_TO_FIRST_LINE")
        if name in FIRST_LETTER_RESTRICTIONS:
            flags.append("APPLIES_TO_FIRST_LETTER")
        if name in PLACEHOLDER_RESTRICTIONS:
            flags.append("APPLIES_TO_PLACEHOLDER")
        if name in MARKER_RESTRICTIONS:
            flags.append("APPLIES_TO_MARKER")
        if name in CUE_RESTRICTIONS:
            flags.append("APPLIES_TO_CUE")
        return flags

%>

/// A group for properties which may override each other via logical resolution.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(u8)]
pub enum LogicalGroupId {
    % for i, group in enumerate(logical_groups.keys()):
    /// ${group}
    ${to_camel_case(group)} = ${i},
    % endfor
}

impl LogicalGroupId {
    /// Return the list of physical mapped properties for a given logical group.
    fn physical_properties(self) -> &'static [LonghandId] {
        static PROPS: [[LonghandId; 4]; ${len(logical_groups)}] = [
        % for group, props in logical_groups.items():
        [
            <% physical_props = [p for p in props if p.logical][0].all_physical_mapped_properties(data) %>
            % for phys in physical_props:
            LonghandId::${phys.camel_case},
            % endfor
            % for i in range(len(physical_props), 4):
            LonghandId::${physical_props[0].camel_case},
            % endfor
        ],
        % endfor
        ];
        &PROPS[self as usize]
    }
}

/// A set of logical groups.
#[derive(Clone, Copy, Debug, Default, MallocSizeOf, PartialEq)]
pub struct LogicalGroupSet {
    storage: [u32; (${len(logical_groups)} - 1 + 32) / 32]
}

impl LogicalGroupSet {
    /// Creates an empty `NonCustomPropertyIdSet`.
    pub fn new() -> Self {
        Self {
            storage: Default::default(),
        }
    }

    /// Return whether the given group is in the set
    #[inline]
    pub fn contains(&self, g: LogicalGroupId) -> bool {
        let bit = g as usize;
        (self.storage[bit / 32] & (1 << (bit % 32))) != 0
    }

    /// Insert a group the set.
    #[inline]
    pub fn insert(&mut self, g: LogicalGroupId) {
        let bit = g as usize;
        self.storage[bit / 32] |= 1 << (bit % 32);
    }
}


#[repr(u8)]
#[derive(Copy, Clone, Debug)]
pub(crate) enum PrioritaryPropertyId {
    % for p in data.longhands:
    % if p.is_prioritary():
    ${p.camel_case},
    % endif
    % endfor
}

impl PrioritaryPropertyId {
    #[inline]
    pub fn to_longhand(self) -> LonghandId {
        static PRIORITARY_TO_LONGHAND: [LonghandId; property_counts::PRIORITARY] = [
        % for p in data.longhands:
        % if p.is_prioritary():
            LonghandId::${p.camel_case},
        % endif
        % endfor
        ];
        PRIORITARY_TO_LONGHAND[self as usize]
    }
    #[inline]
    pub fn from_longhand(l: LonghandId) -> Option<Self> {
        static LONGHAND_TO_PRIORITARY: [Option<PrioritaryPropertyId>; ${len(data.longhands)}] = [
        % for p in data.longhands:
        % if p.is_prioritary():
            Some(PrioritaryPropertyId::${p.camel_case}),
        % else:
            None,
        % endif
        % endfor
        ];
        LONGHAND_TO_PRIORITARY[l as usize]
    }
}

impl LonghandIdSet {
    /// The set of non-inherited longhands.
    #[inline]
    pub(super) fn reset() -> &'static Self {
        ${static_longhand_id_set("RESET", lambda p: not p.style_struct.inherited)}
        &RESET
    }

    #[inline]
    pub(super) fn discrete_animatable() -> &'static Self {
        ${static_longhand_id_set("DISCRETE_ANIMATABLE", lambda p: p.animation_value_type == "discrete")}
        &DISCRETE_ANIMATABLE
    }

    #[inline]
    pub(super) fn logical() -> &'static Self {
        ${static_longhand_id_set("LOGICAL", lambda p: p.logical)}
        &LOGICAL
    }

    /// Returns the set of longhands that are ignored when document colors are
    /// disabled.
    #[inline]
    pub(super) fn ignored_when_colors_disabled() -> &'static Self {
        ${static_longhand_id_set(
            "IGNORED_WHEN_COLORS_DISABLED",
            lambda p: p.ignored_when_colors_disabled
        )}
        &IGNORED_WHEN_COLORS_DISABLED
    }

    /// Only a few properties are allowed to depend on the visited state of
    /// links. When cascading visited styles, we can save time by only
    /// processing these properties.
    pub(super) fn visited_dependent() -> &'static Self {
        ${static_longhand_id_set("VISITED_DEPENDENT", lambda p: p.is_visited_dependent())}
        debug_assert!(Self::late_group().contains_all(&VISITED_DEPENDENT));
        &VISITED_DEPENDENT
    }

    #[inline]
    pub(super) fn prioritary_properties() -> &'static Self {
        ${static_longhand_id_set("PRIORITARY_PROPERTIES", lambda p: p.is_prioritary())}
        &PRIORITARY_PROPERTIES
    }

    #[inline]
    pub(super) fn late_group_only_inherited() -> &'static Self {
        ${static_longhand_id_set("LATE_GROUP_ONLY_INHERITED", lambda p: p.style_struct.inherited and not p.is_prioritary())}
        &LATE_GROUP_ONLY_INHERITED
    }

    #[inline]
    pub(super) fn late_group() -> &'static Self {
        ${static_longhand_id_set("LATE_GROUP", lambda p: not p.is_prioritary())}
        &LATE_GROUP
    }

    /// Returns the set of properties that are declared as having no effect on
    /// Gecko <scrollbar> elements or their descendant scrollbar parts.
    #[cfg(debug_assertions)]
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn has_no_effect_on_gecko_scrollbars() -> &'static Self {
        // data.py asserts that has_no_effect_on_gecko_scrollbars is True or
        // False for properties that are inherited and Gecko pref controlled,
        // and is None for all other properties.
        ${static_longhand_id_set(
            "HAS_NO_EFFECT_ON_SCROLLBARS",
            lambda p: p.has_effect_on_gecko_scrollbars is False
        )}
        &HAS_NO_EFFECT_ON_SCROLLBARS
    }

    /// Returns the set of border properties for the purpose of disabling native
    /// appearance.
    #[inline]
    pub fn border_background_properties() -> &'static Self {
        ${static_longhand_id_set(
            "BORDER_BACKGROUND_PROPERTIES",
            lambda p: (p.logical_group and p.logical_group.startswith("border")) or \
                       p.name in ["background-color", "background-image"]
        )}
        &BORDER_BACKGROUND_PROPERTIES
    }
}

/// An identifier for a given longhand property.
#[derive(Clone, Copy, Eq, Hash, MallocSizeOf, PartialEq, ToComputedValue, ToResolvedValue, ToShmem)]
#[repr(u16)]
pub enum LonghandId {
    % for i, property in enumerate(data.longhands):
        /// ${property.name}
        ${property.camel_case} = ${i},
    % endfor
}

enum LogicalMappingKind {
    Side(LogicalSide),
    Corner(LogicalCorner),
    Axis(LogicalAxis),
}

struct LogicalMappingData {
    group: LogicalGroupId,
    kind: LogicalMappingKind,
}

impl LogicalMappingData {
    fn to_physical(&self, wm: WritingMode) -> LonghandId {
        let index = match self.kind {
            LogicalMappingKind::Side(s) => s.to_physical(wm) as usize,
            LogicalMappingKind::Corner(c) => c.to_physical(wm) as usize,
            LogicalMappingKind::Axis(a) => a.to_physical(wm) as usize,
        };
        self.group.physical_properties()[index]
    }
}

impl LonghandId {
    /// Returns an iterator over all the shorthands that include this longhand.
    pub fn shorthands(self) -> NonCustomPropertyIterator<ShorthandId> {
        // first generate longhand to shorthands lookup map
        //
        // NOTE(emilio): This currently doesn't exclude the "all" shorthand. It
        // could potentially do so, which would speed up serialization
        // algorithms and what not, I guess.
        <%
            from functools import cmp_to_key
            longhand_to_shorthand_map = {}
            num_sub_properties = {}
            for shorthand in data.shorthands:
                num_sub_properties[shorthand.camel_case] = len(shorthand.sub_properties)
                for sub_property in shorthand.sub_properties:
                    if sub_property.ident not in longhand_to_shorthand_map:
                        longhand_to_shorthand_map[sub_property.ident] = []

                    longhand_to_shorthand_map[sub_property.ident].append(shorthand.camel_case)

            def cmp(a, b):
                return (a > b) - (a < b)

            def preferred_order(x, y):
                # Since we want properties in order from most subproperties to least,
                # reverse the arguments to cmp from the expected order.
                result = cmp(num_sub_properties.get(y, 0), num_sub_properties.get(x, 0))
                if result:
                    return result
                # Fall back to lexicographic comparison.
                return cmp(x, y)

            # Sort the lists of shorthand properties according to preferred order:
            # https://drafts.csswg.org/cssom/#concept-shorthands-preferred-order
            for shorthand_list in longhand_to_shorthand_map.values():
                shorthand_list.sort(key=cmp_to_key(preferred_order))
        %>

        // based on lookup results for each longhand, create result arrays
        static MAP: [&'static [ShorthandId]; property_counts::LONGHANDS] = [
        % for property in data.longhands:
            &[
                % for shorthand in longhand_to_shorthand_map.get(property.ident, []):
                    ShorthandId::${shorthand},
                % endfor
            ],
        % endfor
        ];

        NonCustomPropertyIterator {
            filter: NonCustomPropertyId::from(self).enabled_for_all_content(),
            iter: MAP[self as usize].iter(),
        }
    }

    pub(super) fn parse_value<'i, 't>(
        self,
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<PropertyDeclaration, ParseError<'i>> {
        type ParsePropertyFn = for<'i, 't> fn(
            context: &ParserContext,
            input: &mut Parser<'i, 't>,
        ) -> Result<PropertyDeclaration, ParseError<'i>>;
        static PARSE_PROPERTY: [ParsePropertyFn; ${len(data.longhands)}] = [
        % for property in data.longhands:
            longhands::${property.ident}::parse_declared,
        % endfor
        ];
        (PARSE_PROPERTY[self as usize])(context, input)
    }

    /// Return the relevant data to map a particular logical property into physical.
    fn logical_mapping_data(self) -> Option<<&'static LogicalMappingData> {
        const LOGICAL_MAPPING_DATA: [Option<LogicalMappingData>; ${len(data.longhands)}] = [
            % for prop in data.longhands:
            % if prop.logical:
            Some(LogicalMappingData {
                group: LogicalGroupId::${to_camel_case(prop.logical_group)},
                kind: ${prop.logical_mapping_kind(data)}
            }),
            % else:
            None,
            % endif
            % endfor
        ];
        LOGICAL_MAPPING_DATA[self as usize].as_ref()
    }

    /// If this is a logical property, return the corresponding physical one in the given
    /// writing mode. Otherwise, return unchanged.
    #[inline]
    pub fn to_physical(self, wm: WritingMode) -> Self {
        let Some(data) = self.logical_mapping_data() else { return self };
        data.to_physical(wm)
    }

    /// Return the logical group of this longhand property.
    pub fn logical_group(self) -> Option<LogicalGroupId> {
        const LOGICAL_GROUP_IDS: [Option<LogicalGroupId>; ${len(data.longhands)}] = [
            % for prop in data.longhands:
            % if prop.logical_group:
            Some(LogicalGroupId::${to_camel_case(prop.logical_group)}),
            % else:
            None,
            % endif
            % endfor
        ];
        LOGICAL_GROUP_IDS[self as usize]
    }

    /// Returns PropertyFlags for given longhand property.
    #[inline(always)]
    pub fn flags(self) -> PropertyFlags {
        // TODO(emilio): This can be simplified further as Rust gains more
        // constant expression support.
        const FLAGS: [u16; ${len(data.longhands)}] = [
            % for property in data.longhands:
                % for flag in property.flags + restriction_flags(property):
                    PropertyFlags::${flag}.bits() |
                % endfor
                0,
            % endfor
        ];
        PropertyFlags::from_bits_retain(FLAGS[self as usize])
    }
}

/// An identifier for a given shorthand property.
#[derive(Clone, Copy, Debug, Eq, Hash, MallocSizeOf, PartialEq, ToComputedValue, ToResolvedValue, ToShmem)]
#[repr(u16)]
pub enum ShorthandId {
    % for i, property in enumerate(data.shorthands):
        /// ${property.name}
        ${property.camel_case} = ${i},
    % endfor
}

impl ShorthandId {
    /// Get the longhand ids that form this shorthand.
    pub fn longhands(self) -> NonCustomPropertyIterator<LonghandId> {
        static MAP: [&'static [LonghandId]; property_counts::SHORTHANDS] = [
        % for property in data.shorthands:
            &[
                % for sub in property.sub_properties:
                    LonghandId::${sub.camel_case},
                % endfor
            ],
        % endfor
        ];
        NonCustomPropertyIterator {
            filter: NonCustomPropertyId::from(self).enabled_for_all_content(),
            iter: MAP[self as usize].iter(),
        }
    }

    /// Try to serialize the given declarations as this shorthand.
    ///
    /// Returns an error if writing to the stream fails, or if the declarations
    /// do not map to a shorthand.
    pub fn longhands_to_css(
        self,
        declarations: &[&PropertyDeclaration],
        dest: &mut CssStringWriter,
    ) -> fmt::Result {
        type LonghandsToCssFn = for<'a, 'b> fn(&'a [&'b PropertyDeclaration], &mut CssStringWriter) -> fmt::Result;
        fn all_to_css(_: &[&PropertyDeclaration], _: &mut CssStringWriter) -> fmt::Result {
            // No need to try to serialize the declarations as the 'all'
            // shorthand, since it only accepts CSS-wide keywords (and variable
            // references), which will be handled in
            // get_shorthand_appendable_value.
            Ok(())
        }

        static LONGHANDS_TO_CSS: [LonghandsToCssFn; ${len(data.shorthands)}] = [
            % for shorthand in data.shorthands:
            % if shorthand.ident == "all":
                all_to_css,
            % else:
                shorthands::${shorthand.ident}::to_css,
            % endif
            % endfor
        ];

        LONGHANDS_TO_CSS[self as usize](declarations, dest)
    }

    /// Returns PropertyFlags for the given shorthand property.
    #[inline]
    pub fn flags(self) -> PropertyFlags {
        const FLAGS: [u16; ${len(data.shorthands)}] = [
            % for property in data.shorthands:
                % for flag in property.flags:
                    PropertyFlags::${flag}.bits() |
                % endfor
                0,
            % endfor
        ];
        PropertyFlags::from_bits_retain(FLAGS[self as usize])
    }

    /// Returns the order in which this property appears relative to other
    /// shorthands in idl-name-sorting order.
    #[inline]
    pub fn idl_name_sort_order(self) -> u32 {
        <%
            from data import to_idl_name
            ordered = {}
            sorted_shorthands = sorted(data.shorthands, key=lambda p: to_idl_name(p.ident))
            for order, shorthand in enumerate(sorted_shorthands):
                ordered[shorthand.ident] = order
        %>
        static IDL_NAME_SORT_ORDER: [u32; ${len(data.shorthands)}] = [
            % for property in data.shorthands:
            ${ordered[property.ident]},
            % endfor
        ];
        IDL_NAME_SORT_ORDER[self as usize]
    }

    pub(super) fn parse_into<'i, 't>(
        self,
        declarations: &mut SourcePropertyDeclaration,
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<(), ParseError<'i>> {
        type ParseIntoFn = for<'i, 't> fn(
            declarations: &mut SourcePropertyDeclaration,
            context: &ParserContext,
            input: &mut Parser<'i, 't>,
        ) -> Result<(), ParseError<'i>>;

        fn parse_all<'i, 't>(
            _: &mut SourcePropertyDeclaration,
            _: &ParserContext,
            input: &mut Parser<'i, 't>
        ) -> Result<(), ParseError<'i>> {
            // 'all' accepts no value other than CSS-wide keywords
            Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
        }

        static PARSE_INTO: [ParseIntoFn; ${len(data.shorthands)}] = [
            % for shorthand in data.shorthands:
            % if shorthand.ident == "all":
            parse_all,
            % else:
            shorthands::${shorthand.ident}::parse_into,
            % endif
            % endfor
        ];

        (PARSE_INTO[self as usize])(declarations, context, input)
    }
}

/// The counted unknown property list which is used for css use counters.
///
/// FIXME: This should be just #[repr(u8)], but can't be because of ABI issues,
/// see https://bugs.llvm.org/show_bug.cgi?id=44228.
#[derive(Clone, Copy, Debug, Eq, FromPrimitive, Hash, PartialEq)]
#[repr(u32)]
pub enum CountedUnknownProperty {
    % for prop in data.counted_unknown_properties:
    /// ${prop.name}
    ${prop.camel_case},
    % endfor
}

impl CountedUnknownProperty {
    /// Parse the counted unknown property, for testing purposes only.
    pub fn parse_for_testing(property_name: &str) -> Option<Self> {
        ascii_case_insensitive_phf_map! {
            unknown_ids -> CountedUnknownProperty = {
                % for property in data.counted_unknown_properties:
                "${property.name}" => CountedUnknownProperty::${property.camel_case},
                % endfor
            }
        }
        unknown_ids::get(property_name).cloned()
    }

    /// Returns the underlying index, used for use counter.
    #[inline]
    pub fn bit(self) -> usize {
        self as usize
    }
}

impl PropertyId {
    /// Returns a given property from the given name, _regardless of whether it
    /// is enabled or not_, or Err(()) for unknown properties.
    pub(super) fn parse_unchecked(
        property_name: &str,
        use_counters: Option< &UseCounters>,
    ) -> Result<Self, ()> {
        // A special id for css use counters. ShorthandAlias is not used in the Servo build.
        // That's why we need to allow dead_code.
        pub enum StaticId {
            NonCustom(NonCustomPropertyId),
            CountedUnknown(CountedUnknownProperty),
        }
        ascii_case_insensitive_phf_map! {
            static_ids -> StaticId = {
                % for i, property in enumerate(data.longhands + data.shorthands + data.all_aliases()):
                "${property.name}" => StaticId::NonCustom(NonCustomPropertyId(${i})),
                % endfor
                % for property in data.counted_unknown_properties:
                "${property.name}" => {
                    StaticId::CountedUnknown(CountedUnknownProperty::${property.camel_case})
                },
                % endfor
            }
        }

        if let Some(id) = static_ids::get(property_name) {
            return Ok(match *id {
                StaticId::NonCustom(id) => PropertyId::NonCustom(id),
                StaticId::CountedUnknown(unknown_prop) => {
                    if let Some(counters) = use_counters {
                        counters.counted_unknown_properties.record(unknown_prop);
                    }
                    // Always return Err(()) because these aren't valid custom property names.
                    return Err(());
                }
            });
        }

        let name = crate::custom_properties::parse_name(property_name)?;
        Ok(PropertyId::Custom(crate::custom_properties::Name::from(name)))
    }
}

impl PropertyDeclaration {
    /// Given a property declaration, return the property declaration id.
    #[inline]
    pub fn id(&self) -> PropertyDeclarationId {
        match *self {
            PropertyDeclaration::Custom(ref declaration) => {
                return PropertyDeclarationId::Custom(&declaration.name)
            }
            PropertyDeclaration::CSSWideKeyword(ref declaration) => {
                return PropertyDeclarationId::Longhand(declaration.id);
            }
            PropertyDeclaration::WithVariables(ref declaration) => {
                return PropertyDeclarationId::Longhand(declaration.id);
            }
            _ => {}
        }
        // This is just fine because PropertyDeclaration and LonghandId
        // have corresponding discriminants.
        let id = unsafe { *(self as *const _ as *const LonghandId) };
        debug_assert_eq!(id, match *self {
            % for property in data.longhands:
            PropertyDeclaration::${property.camel_case}(..) => LonghandId::${property.camel_case},
            % endfor
            _ => id,
        });
        PropertyDeclarationId::Longhand(id)
    }

    /// Given a declaration, convert it into a declaration for a corresponding
    /// physical property.
    #[inline]
    pub fn to_physical(&self, wm: WritingMode) -> Self {
        match *self {
            PropertyDeclaration::WithVariables(VariableDeclaration {
                id,
                ref value,
            }) => {
                return PropertyDeclaration::WithVariables(VariableDeclaration {
                    id: id.to_physical(wm),
                    value: value.clone(),
                })
            }
            PropertyDeclaration::CSSWideKeyword(WideKeywordDeclaration {
                id,
                keyword,
            }) => {
                return PropertyDeclaration::CSSWideKeyword(WideKeywordDeclaration {
                    id: id.to_physical(wm),
                    keyword,
                })
            }
            PropertyDeclaration::Custom(..) => return self.clone(),
            % for prop in data.longhands:
            PropertyDeclaration::${prop.camel_case}(..) => {},
            % endfor
        }

        let mut ret = self.clone();

        % for prop in data.longhands:
        % for physical_property in prop.all_physical_mapped_properties(data):
        % if physical_property.specified_type() != prop.specified_type():
            <% raise "Logical property %s should share specified value with physical property %s" % \
                     (prop.name, physical_property.name) %>
        % endif
        % endfor
        % endfor

        unsafe {
            let longhand_id = *(&mut ret as *mut _ as *mut LonghandId);

            debug_assert_eq!(
                PropertyDeclarationId::Longhand(longhand_id),
                ret.id()
            );

            // This is just fine because PropertyDeclaration and LonghandId
            // have corresponding discriminants.
            *(&mut ret as *mut _ as *mut LonghandId) = longhand_id.to_physical(wm);

            debug_assert_eq!(
                PropertyDeclarationId::Longhand(longhand_id.to_physical(wm)),
                ret.id()
            );
        }

        ret
    }

    /// Returns whether or not the property is set by a system font
    pub fn get_system(&self) -> Option<SystemFont> {
        match *self {
            % if engine == "gecko":
            % for prop in SYSTEM_FONT_LONGHANDS:
                PropertyDeclaration::${to_camel_case(prop)}(ref prop) => {
                    prop.get_system()
                }
            % endfor
            % endif
            _ => None,
        }
    }
}

#[cfg(feature = "gecko")]
pub use super::gecko::style_structs;

/// The module where all the style structs are defined.
#[cfg(feature = "servo")]
pub mod style_structs {
    use fxhash::FxHasher;
    use super::longhands;
    use std::hash::{Hash, Hasher};
    use crate::logical_geometry::WritingMode;
    use crate::media_queries::Device;
    use crate::values::computed::NonNegativeLength;

    % for style_struct in data.active_style_structs():
        % if style_struct.name == "Font":
        #[derive(Clone, Debug, MallocSizeOf)]
        #[cfg_attr(feature = "servo", derive(Serialize, Deserialize))]
        % else:
        #[derive(Clone, Debug, MallocSizeOf, PartialEq)]
        % endif
        /// The ${style_struct.name} style struct.
        pub struct ${style_struct.name} {
            % for longhand in style_struct.longhands:
                % if not longhand.logical:
                    /// The ${longhand.name} computed value.
                    pub ${longhand.ident}: longhands::${longhand.ident}::computed_value::T,
                % endif
            % endfor
            % if style_struct.name == "InheritedText":
                /// The "used" text-decorations that apply to this box.
                ///
                /// FIXME(emilio): This is technically a box-tree concept, and
                /// would be nice to move away from style.
                pub text_decorations_in_effect: crate::values::computed::text::TextDecorationsInEffect,
            % endif
            % if style_struct.name == "Font":
                /// The font hash, used for font caching.
                pub hash: u64,
            % endif
            % if style_struct.name == "Box":
                /// The display value specified by the CSS stylesheets (without any style adjustments),
                /// which is needed for hypothetical layout boxes.
                pub original_display: longhands::display::computed_value::T,
            % endif
        }
        % if style_struct.name == "Font":
        impl PartialEq for Font {
            fn eq(&self, other: &Font) -> bool {
                self.hash == other.hash
                % for longhand in style_struct.longhands:
                    && self.${longhand.ident} == other.${longhand.ident}
                % endfor
            }
        }
        % endif

        impl ${style_struct.name} {
            % for longhand in style_struct.longhands:
                % if not longhand.logical:
                    % if longhand.ident == "display":
                        /// Set `display`.
                        ///
                        /// We need to keep track of the original display for hypothetical boxes,
                        /// so we need to special-case this.
                        #[allow(non_snake_case)]
                        #[inline]
                        pub fn set_display(&mut self, v: longhands::display::computed_value::T) {
                            self.display = v;
                            self.original_display = v;
                        }
                    % else:
                        /// Set ${longhand.name}.
                        #[allow(non_snake_case)]
                        #[inline]
                        pub fn set_${longhand.ident}(&mut self, v: longhands::${longhand.ident}::computed_value::T) {
                            self.${longhand.ident} = v;
                        }
                    % endif
                    % if longhand.ident == "display":
                        /// Set `display` from other struct.
                        ///
                        /// Same as `set_display` above.
                        /// Thus, we need to special-case this.
                        #[allow(non_snake_case)]
                        #[inline]
                        pub fn copy_display_from(&mut self, other: &Self) {
                            self.display = other.display.clone();
                            self.original_display = other.display.clone();
                        }
                    % else:
                        /// Set ${longhand.name} from other struct.
                        #[allow(non_snake_case)]
                        #[inline]
                        pub fn copy_${longhand.ident}_from(&mut self, other: &Self) {
                            self.${longhand.ident} = other.${longhand.ident}.clone();
                        }
                    % endif
                    /// Reset ${longhand.name} from the initial struct.
                    #[allow(non_snake_case)]
                    #[inline]
                    pub fn reset_${longhand.ident}(&mut self, other: &Self) {
                        self.copy_${longhand.ident}_from(other)
                    }

                    /// Get the computed value for ${longhand.name}.
                    #[allow(non_snake_case)]
                    #[inline]
                    pub fn clone_${longhand.ident}(&self) -> longhands::${longhand.ident}::computed_value::T {
                        self.${longhand.ident}.clone()
                    }
                % endif
                % if longhand.need_index:
                    /// If this longhand is indexed, get the number of elements.
                    #[allow(non_snake_case)]
                    pub fn ${longhand.ident}_count(&self) -> usize {
                        self.${longhand.ident}.0.len()
                    }

                    /// If this longhand is indexed, get the element at given
                    /// index.
                    #[allow(non_snake_case)]
                    pub fn ${longhand.ident}_at(&self, index: usize)
                        -> longhands::${longhand.ident}::computed_value::SingleComputedValue {
                        self.${longhand.ident}.0[index].clone()
                    }
                % endif
            % endfor
            % if style_struct.name == "Border":
                % for side in ["top", "right", "bottom", "left"]:
                    /// Whether the border-${side} property has nonzero width.
                    #[allow(non_snake_case)]
                    pub fn border_${side}_has_nonzero_width(&self) -> bool {
                        use crate::Zero;
                        !self.border_${side}_width.is_zero()
                    }
                % endfor
            % elif style_struct.name == "Font":
                /// Computes a font hash in order to be able to cache fonts
                /// effectively in GFX and layout.
                pub fn compute_font_hash(&mut self) {
                    // Corresponds to the fields in
                    // `gfx::font_template::FontTemplateDescriptor`.
                    let mut hasher: FxHasher = Default::default();
                    self.font_weight.hash(&mut hasher);
                    self.font_stretch.hash(&mut hasher);
                    self.font_style.hash(&mut hasher);
                    self.font_family.hash(&mut hasher);
                    self.hash = hasher.finish()
                }

                /// (Servo does not handle MathML, so this just calls copy_font_size_from)
                pub fn inherit_font_size_from(&mut self, parent: &Self,
                                              _: Option<NonNegativeLength>,
                                              _: &Device) {
                    self.copy_font_size_from(parent);
                }
                /// (Servo does not handle MathML, so this just calls set_font_size)
                pub fn apply_font_size(&mut self,
                                       v: longhands::font_size::computed_value::T,
                                       _: &Self,
                                       _: &Device) -> Option<NonNegativeLength> {
                    self.set_font_size(v);
                    None
                }
                /// (Servo does not handle MathML, so this does nothing)
                pub fn apply_unconstrained_font_size(&mut self, _: NonNegativeLength) {
                }

            % elif style_struct.name == "Outline":
                /// Whether the outline-width property is non-zero.
                #[inline]
                pub fn outline_has_nonzero_width(&self) -> bool {
                    use crate::Zero;
                    !self.outline_width.is_zero()
                }
            % elif style_struct.name == "Box":
                /// Sets the display property, but without touching original_display,
                /// except when the adjustment comes from root or item display fixups.
                pub fn set_adjusted_display(
                    &mut self,
                    dpy: longhands::display::computed_value::T,
                    is_item_or_root: bool
                ) {
                    self.display = dpy;
                    if is_item_or_root {
                        self.original_display = dpy;
                    }
                }
            % endif
        }

    % endfor
}

% for style_struct in data.active_style_structs():
    impl style_structs::${style_struct.name} {
        % for longhand in style_struct.longhands:
            % if longhand.need_index:
                /// Iterate over the values of ${longhand.name}.
                #[allow(non_snake_case)]
                #[inline]
                pub fn ${longhand.ident}_iter(&self) -> ${longhand.camel_case}Iter {
                    ${longhand.camel_case}Iter {
                        style_struct: self,
                        current: 0,
                        max: self.${longhand.ident}_count(),
                    }
                }

                /// Get a value mod `index` for the property ${longhand.name}.
                #[allow(non_snake_case)]
                #[inline]
                pub fn ${longhand.ident}_mod(&self, index: usize)
                    -> longhands::${longhand.ident}::computed_value::SingleComputedValue {
                    self.${longhand.ident}_at(index % self.${longhand.ident}_count())
                }

                /// Clone the computed value for the property.
                #[allow(non_snake_case)]
                #[inline]
                #[cfg(feature = "gecko")]
                pub fn clone_${longhand.ident}(
                    &self,
                ) -> longhands::${longhand.ident}::computed_value::T {
                    longhands::${longhand.ident}::computed_value::List(
                        self.${longhand.ident}_iter().collect()
                    )
                }
            % endif
        % endfor

        % if style_struct.name == "UI":
            /// Returns whether there is any animation specified with
            /// animation-name other than `none`.
            pub fn specifies_animations(&self) -> bool {
                self.animation_name_iter().any(|name| !name.is_none())
            }

            /// Returns whether there are any transitions specified.
            #[cfg(feature = "servo")]
            pub fn specifies_transitions(&self) -> bool {
                (0..self.transition_property_count()).any(|index| {
                    let combined_duration =
                        self.transition_duration_mod(index).seconds().max(0.) +
                        self.transition_delay_mod(index).seconds();
                    combined_duration > 0.
                })
            }

            /// Returns whether there is any named progress timeline specified with
            /// scroll-timeline-name other than `none`.
            pub fn specifies_scroll_timelines(&self) -> bool {
                self.scroll_timeline_name_iter().any(|name| !name.is_none())
            }

            /// Returns whether there is any named progress timeline specified with
            /// view-timeline-name other than `none`.
            pub fn specifies_view_timelines(&self) -> bool {
                self.view_timeline_name_iter().any(|name| !name.is_none())
            }

            /// Returns true if animation properties are equal between styles, but without
            /// considering keyframe data and animation-timeline.
            #[cfg(feature = "servo")]
            pub fn animations_equals(&self, other: &Self) -> bool {
                self.animation_name_iter().eq(other.animation_name_iter()) &&
                self.animation_delay_iter().eq(other.animation_delay_iter()) &&
                self.animation_direction_iter().eq(other.animation_direction_iter()) &&
                self.animation_duration_iter().eq(other.animation_duration_iter()) &&
                self.animation_fill_mode_iter().eq(other.animation_fill_mode_iter()) &&
                self.animation_iteration_count_iter().eq(other.animation_iteration_count_iter()) &&
                self.animation_play_state_iter().eq(other.animation_play_state_iter()) &&
                self.animation_timing_function_iter().eq(other.animation_timing_function_iter())
            }

        % elif style_struct.name == "Column":
            /// Whether this is a multicol style.
            #[cfg(feature = "servo")]
            pub fn is_multicol(&self) -> bool {
                !self.column_width.is_auto() || !self.column_count.is_auto()
            }
        % endif
    }

    % for longhand in style_struct.longhands:
        % if longhand.need_index:
            /// An iterator over the values of the ${longhand.name} properties.
            pub struct ${longhand.camel_case}Iter<'a> {
                style_struct: &'a style_structs::${style_struct.name},
                current: usize,
                max: usize,
            }

            impl<'a> Iterator for ${longhand.camel_case}Iter<'a> {
                type Item = longhands::${longhand.ident}::computed_value::SingleComputedValue;

                fn next(&mut self) -> Option<Self::Item> {
                    self.current += 1;
                    if self.current <= self.max {
                        Some(self.style_struct.${longhand.ident}_at(self.current - 1))
                    } else {
                        None
                    }
                }
            }
        % endif
    % endfor
% endfor


#[cfg(feature = "gecko")]
pub use super::gecko::{ComputedValues, ComputedValuesInner};

#[cfg(feature = "servo")]
#[cfg_attr(feature = "servo", derive(Clone, Debug))]
/// Actual data of ComputedValues, to match up with Gecko
pub struct ComputedValuesInner {
    % for style_struct in data.active_style_structs():
        ${style_struct.ident}: Arc<style_structs::${style_struct.name}>,
    % endfor
    custom_properties: crate::custom_properties::ComputedCustomProperties,

    /// The writing mode of this computed values struct.
    pub writing_mode: WritingMode,

    /// The effective zoom value.
    pub effective_zoom: Zoom,

    /// A set of flags we use to store misc information regarding this style.
    pub flags: ComputedValueFlags,

    /// The rule node representing the ordered list of rules matched for this
    /// node.  Can be None for default values and text nodes.  This is
    /// essentially an optimization to avoid referencing the root rule node.
    pub rules: Option<StrongRuleNode>,

    /// The element's computed values if visited, only computed if there's a
    /// relevant link for this element. A element's "relevant link" is the
    /// element being matched if it is a link or the nearest ancestor link.
    visited_style: Option<Arc<ComputedValues>>,
}

/// The struct that Servo uses to represent computed values.
///
/// This struct contains an immutable atomically-reference-counted pointer to
/// every kind of style struct.
///
/// When needed, the structs may be copied in order to get mutated.
#[cfg(feature = "servo")]
#[cfg_attr(feature = "servo", derive(Clone, Debug))]
pub struct ComputedValues {
    /// The actual computed values
    ///
    /// In Gecko the outer ComputedValues is actually a ComputedStyle, whereas
    /// ComputedValuesInner is the core set of computed values.
    ///
    /// We maintain this distinction in servo to reduce the amount of special
    /// casing.
    inner: ComputedValuesInner,

    /// The pseudo-element that we're using.
    pseudo: Option<PseudoElement>,
}

impl ComputedValues {
    /// Returns the pseudo-element that this style represents.
    #[cfg(feature = "servo")]
    pub fn pseudo(&self) -> Option<<&PseudoElement> {
        self.pseudo.as_ref()
    }

    /// Returns true if this is the style for a pseudo-element.
    #[cfg(feature = "servo")]
    pub fn is_pseudo_style(&self) -> bool {
        self.pseudo().is_some()
    }

    /// Returns whether this style's display value is equal to contents.
    pub fn is_display_contents(&self) -> bool {
        self.clone_display().is_contents()
    }

    /// Gets a reference to the rule node. Panic if no rule node exists.
    pub fn rules(&self) -> &StrongRuleNode {
        self.rules.as_ref().unwrap()
    }

    /// Returns the visited rules, if applicable.
    pub fn visited_rules(&self) -> Option<<&StrongRuleNode> {
        self.visited_style().and_then(|s| s.rules.as_ref())
    }

    /// Gets a reference to the custom properties map (if one exists).
    pub fn custom_properties(&self) -> &crate::custom_properties::ComputedCustomProperties {
        &self.custom_properties
    }

    /// Returns whether we have the same custom properties as another style.
    pub fn custom_properties_equal(&self, other: &Self) -> bool {
      self.custom_properties() == other.custom_properties()
    }

% for prop in data.longhands:
% if not prop.logical:
    /// Gets the computed value of a given property.
    #[inline(always)]
    #[allow(non_snake_case)]
    pub fn clone_${prop.ident}(
        &self,
    ) -> longhands::${prop.ident}::computed_value::T {
        self.get_${prop.style_struct.ident.strip("_")}().clone_${prop.ident}()
    }
% endif
% endfor

    /// Writes the (resolved or computed) value of the given longhand as a string in `dest`.
    ///
    /// TODO(emilio): We should move all the special resolution from
    /// nsComputedDOMStyle to ToResolvedValue instead.
    pub fn computed_or_resolved_value(
        &self,
        property_id: LonghandId,
        context: Option<<&resolved::Context>,
        dest: &mut CssStringWriter,
    ) -> fmt::Result {
        use crate::values::resolved::ToResolvedValue;
        let mut dest = CssWriter::new(dest);
        let property_id = property_id.to_physical(self.writing_mode);
        match property_id {
            % for specified_type, props in groupby(data.longhands, key=lambda x: x.specified_type()):
            <% props = list(props) %>
            ${" |\n".join("LonghandId::{}".format(p.camel_case) for p in props)} => {
                let value = match property_id {
                    % for prop in props:
                    % if not prop.logical:
                    LonghandId::${prop.camel_case} => self.clone_${prop.ident}(),
                    % endif
                    % endfor
                    _ => unsafe { debug_unreachable!() },
                };
                if let Some(c) = context {
                    value.to_resolved_value(c).to_css(&mut dest)
                } else {
                    value.to_css(&mut dest)
                }
            }
            % endfor
        }
    }

    /// Returns the given longhand's resolved value as a property declaration.
    pub fn computed_or_resolved_declaration(
        &self,
        property_id: LonghandId,
        context: Option<<&resolved::Context>,
    ) -> PropertyDeclaration {
        use crate::values::resolved::ToResolvedValue;
        use crate::values::computed::ToComputedValue;
        let physical_property_id = property_id.to_physical(self.writing_mode);
        match physical_property_id {
            % for specified_type, props in groupby(data.longhands, key=lambda x: x.specified_type()):
            <% props = list(props) %>
            ${" |\n".join("LonghandId::{}".format(p.camel_case) for p in props)} => {
                let mut computed_value = match physical_property_id {
                    % for prop in props:
                    % if not prop.logical:
                    LonghandId::${prop.camel_case} => self.clone_${prop.ident}(),
                    % endif
                    % endfor
                    _ => unsafe { debug_unreachable!() },
                };
                if let Some(c) = context {
                    let resolved = computed_value.to_resolved_value(c);
                    computed_value = ToResolvedValue::from_resolved_value(resolved);
                }
                let specified = ToComputedValue::from_computed_value(&computed_value);
                % if props[0].boxed:
                let specified = Box::new(specified);
                % endif
                % if len(props) == 1:
                PropertyDeclaration::${props[0].camel_case}(specified)
                % else:
                unsafe {
                    let mut out = mem::MaybeUninit::uninit();
                    ptr::write(
                        out.as_mut_ptr() as *mut PropertyDeclarationVariantRepr<${specified_type}>,
                        PropertyDeclarationVariantRepr {
                            tag: property_id as u16,
                            value: specified,
                        },
                    );
                    out.assume_init()
                }
                % endif
            }
            % endfor
        }
    }

    /// Resolves the currentColor keyword.
    ///
    /// Any color value from computed values (except for the 'color' property
    /// itself) should go through this method.
    ///
    /// Usage example:
    /// let top_color =
    ///   style.resolve_color(style.get_border().clone_border_top_color());
    #[inline]
    pub fn resolve_color(&self, color: computed::Color) -> crate::color::AbsoluteColor {
        let current_color = self.get_inherited_text().clone_color();
        color.resolve_to_absolute(&current_color)
    }

    /// Returns which longhand properties have different values in the two
    /// ComputedValues.
    #[cfg(feature = "gecko_debug")]
    pub fn differing_properties(&self, other: &ComputedValues) -> LonghandIdSet {
        let mut set = LonghandIdSet::new();
        % for prop in data.longhands:
        % if not prop.logical:
        if self.clone_${prop.ident}() != other.clone_${prop.ident}() {
            set.insert(LonghandId::${prop.camel_case});
        }
        % endif
        % endfor
        set
    }

    /// Create a `TransitionPropertyIterator` for this styles transition properties.
    pub fn transition_properties<'a>(
        &'a self
    ) -> animated_properties::TransitionPropertyIterator<'a> {
        animated_properties::TransitionPropertyIterator::from_style(self)
    }
}

#[cfg(feature = "servo")]
impl ComputedValues {
    /// Create a new refcounted `ComputedValues`
    pub fn new(
        pseudo: Option<<&PseudoElement>,
        custom_properties: crate::custom_properties::ComputedCustomProperties,
        writing_mode: WritingMode,
        effective_zoom: computed::Zoom,
        flags: ComputedValueFlags,
        rules: Option<StrongRuleNode>,
        visited_style: Option<Arc<ComputedValues>>,
        % for style_struct in data.active_style_structs():
        ${style_struct.ident}: Arc<style_structs::${style_struct.name}>,
        % endfor
    ) -> Arc<Self> {
        Arc::new(Self {
            inner: ComputedValuesInner {
                custom_properties,
                writing_mode,
                rules,
                visited_style,
                effective_zoom,
                flags,
            % for style_struct in data.active_style_structs():
                ${style_struct.ident},
            % endfor
            },
            pseudo: pseudo.cloned(),
        })
    }

    /// Get the initial computed values.
    pub fn initial_values() -> &'static Self { &*INITIAL_SERVO_VALUES }

    /// Serializes the computed value of this property as a string.
    pub fn computed_value_to_string(&self, property: PropertyDeclarationId) -> String {
        match property {
            PropertyDeclarationId::Longhand(id) => {
                let mut s = String::new();
                self.get_longhand_property_value(
                    id,
                    &mut CssWriter::new(&mut s)
                ).unwrap();
                s
            }
            PropertyDeclarationId::Custom(name) => {
                // FIXME(bug 1869476): This should use a stylist to determine
                // whether the name corresponds to an inherited custom property
                // and then choose the inherited/non_inherited map accordingly.
                let p = &self.custom_properties;
                let value = p
                    .inherited
                    .as_ref()
                    .and_then(|map| map.get(name))
                    .or_else(|| p.non_inherited.as_ref().and_then(|map| map.get(name)));
                value.map_or(String::new(), |value| value.to_css_string())
            }
        }
    }
}

#[cfg(feature = "servo")]
impl ops::Deref for ComputedValues {
    type Target = ComputedValuesInner;
    fn deref(&self) -> &ComputedValuesInner {
        &self.inner
    }
}

#[cfg(feature = "servo")]
impl ops::DerefMut for ComputedValues {
    fn deref_mut(&mut self) -> &mut ComputedValuesInner {
        &mut self.inner
    }
}

#[cfg(feature = "servo")]
impl ComputedValuesInner {
    /// Returns the visited style, if any.
    pub fn visited_style(&self) -> Option<<&ComputedValues> {
        self.visited_style.as_deref()
    }

    % for style_struct in data.active_style_structs():
        /// Clone the ${style_struct.name} struct.
        #[inline]
        pub fn clone_${style_struct.name_lower}(&self) -> Arc<style_structs::${style_struct.name}> {
            self.${style_struct.ident}.clone()
        }

        /// Get a immutable reference to the ${style_struct.name} struct.
        #[inline]
        pub fn get_${style_struct.name_lower}(&self) -> &style_structs::${style_struct.name} {
            &self.${style_struct.ident}
        }

        /// Get a mutable reference to the ${style_struct.name} struct.
        #[inline]
        pub fn mutate_${style_struct.name_lower}(&mut self) -> &mut style_structs::${style_struct.name} {
            Arc::make_mut(&mut self.${style_struct.ident})
        }
    % endfor

    /// Gets a reference to the rule node. Panic if no rule node exists.
    pub fn rules(&self) -> &StrongRuleNode {
        self.rules.as_ref().unwrap()
    }

    #[inline]
    /// Returns whether the "content" property for the given style is completely
    /// ineffective, and would yield an empty `::before` or `::after`
    /// pseudo-element.
    pub fn ineffective_content_property(&self) -> bool {
        use crate::values::generics::counters::Content;
        match self.get_counters().content {
            Content::Normal | Content::None => true,
            Content::Items(ref items) => items.is_empty(),
        }
    }

    /// Whether the current style or any of its ancestors is multicolumn.
    #[inline]
    pub fn can_be_fragmented(&self) -> bool {
        self.flags.contains(ComputedValueFlags::CAN_BE_FRAGMENTED)
    }

    /// Whether the current style is multicolumn.
    #[inline]
    pub fn is_multicol(&self) -> bool {
        self.get_column().is_multicol()
    }

    /// Get the logical computed inline size.
    #[inline]
    pub fn content_inline_size(&self) -> &computed::Size {
        let position_style = self.get_position();
        if self.writing_mode.is_vertical() {
            &position_style.height
        } else {
            &position_style.width
        }
    }

    /// Get the logical computed block size.
    #[inline]
    pub fn content_block_size(&self) -> &computed::Size {
        let position_style = self.get_position();
        if self.writing_mode.is_vertical() { &position_style.width } else { &position_style.height }
    }

    /// Get the logical computed min inline size.
    #[inline]
    pub fn min_inline_size(&self) -> &computed::Size {
        let position_style = self.get_position();
        if self.writing_mode.is_vertical() { &position_style.min_height } else { &position_style.min_width }
    }

    /// Get the logical computed min block size.
    #[inline]
    pub fn min_block_size(&self) -> &computed::Size {
        let position_style = self.get_position();
        if self.writing_mode.is_vertical() { &position_style.min_width } else { &position_style.min_height }
    }

    /// Get the logical computed max inline size.
    #[inline]
    pub fn max_inline_size(&self) -> &computed::MaxSize {
        let position_style = self.get_position();
        if self.writing_mode.is_vertical() { &position_style.max_height } else { &position_style.max_width }
    }

    /// Get the logical computed max block size.
    #[inline]
    pub fn max_block_size(&self) -> &computed::MaxSize {
        let position_style = self.get_position();
        if self.writing_mode.is_vertical() { &position_style.max_width } else { &position_style.max_height }
    }

    /// Get the logical computed padding for this writing mode.
    #[inline]
    pub fn logical_padding(&self) -> LogicalMargin<<&computed::LengthPercentage> {
        let padding_style = self.get_padding();
        LogicalMargin::from_physical(self.writing_mode, SideOffsets2D::new(
            &padding_style.padding_top.0,
            &padding_style.padding_right.0,
            &padding_style.padding_bottom.0,
            &padding_style.padding_left.0,
        ))
    }

    /// Get the logical border width
    #[inline]
    pub fn border_width_for_writing_mode(&self, writing_mode: WritingMode) -> LogicalMargin<Au> {
        let border_style = self.get_border();
        LogicalMargin::from_physical(writing_mode, SideOffsets2D::new(
            Au::from(border_style.border_top_width),
            Au::from(border_style.border_right_width),
            Au::from(border_style.border_bottom_width),
            Au::from(border_style.border_left_width),
        ))
    }

    /// Gets the logical computed border widths for this style.
    #[inline]
    pub fn logical_border_width(&self) -> LogicalMargin<Au> {
        self.border_width_for_writing_mode(self.writing_mode)
    }

    /// Gets the logical computed margin from this style.
    #[inline]
    pub fn logical_margin(&self) -> LogicalMargin<<&computed::LengthPercentageOrAuto> {
        let margin_style = self.get_margin();
        LogicalMargin::from_physical(self.writing_mode, SideOffsets2D::new(
            &margin_style.margin_top,
            &margin_style.margin_right,
            &margin_style.margin_bottom,
            &margin_style.margin_left,
        ))
    }

    /// Gets the logical position from this style.
    #[inline]
    pub fn logical_position(&self) -> LogicalMargin<<&computed::LengthPercentageOrAuto> {
        // FIXME(SimonSapin): should be the writing mode of the containing block, maybe?
        let position_style = self.get_position();
        LogicalMargin::from_physical(self.writing_mode, SideOffsets2D::new(
            &position_style.top,
            &position_style.right,
            &position_style.bottom,
            &position_style.left,
        ))
    }

    /// Return true if the effects force the transform style to be Flat
    pub fn overrides_transform_style(&self) -> bool {
        use crate::computed_values::mix_blend_mode::T as MixBlendMode;

        let effects = self.get_effects();
        // TODO(gw): Add clip-path, isolation, mask-image, mask-border-source when supported.
        effects.opacity < 1.0 ||
           !effects.filter.0.is_empty() ||
           !effects.clip.is_auto() ||
           effects.mix_blend_mode != MixBlendMode::Normal
    }

    /// <https://drafts.csswg.org/css-transforms/#grouping-property-values>
    pub fn get_used_transform_style(&self) -> computed_values::transform_style::T {
        use crate::computed_values::transform_style::T as TransformStyle;

        let box_ = self.get_box();

        if self.overrides_transform_style() {
            TransformStyle::Flat
        } else {
            // Return the computed value if not overridden by the above exceptions
            box_.transform_style
        }
    }

    /// Whether given this transform value, the compositor would require a
    /// layer.
    pub fn transform_requires_layer(&self) -> bool {
        use crate::values::generics::transform::TransformOperation;
        // Check if the transform matrix is 2D or 3D
        for transform in &*self.get_box().transform.0 {
            match *transform {
                TransformOperation::Perspective(..) => {
                    return true;
                }
                TransformOperation::Matrix3D(m) => {
                    // See http://dev.w3.org/csswg/css-transforms/#2d-matrix
                    if m.m31 != 0.0 || m.m32 != 0.0 ||
                       m.m13 != 0.0 || m.m23 != 0.0 ||
                       m.m43 != 0.0 || m.m14 != 0.0 ||
                       m.m24 != 0.0 || m.m34 != 0.0 ||
                       m.m33 != 1.0 || m.m44 != 1.0 {
                        return true;
                    }
                }
                TransformOperation::Translate3D(_, _, z) |
                TransformOperation::TranslateZ(z) => {
                    if z.px() != 0. {
                        return true;
                    }
                }
                _ => {}
            }
        }

        // Neither perspective nor transform present
        false
    }
}

/// A reference to a style struct of the parent, or our own style struct.
pub enum StyleStructRef<'a, T: 'static> {
    /// A borrowed struct from the parent, for example, for inheriting style.
    Borrowed(&'a T),
    /// An owned struct, that we've already mutated.
    Owned(UniqueArc<T>),
    /// Temporarily vacated, will panic if accessed
    Vacated,
}

impl<'a, T: 'a> StyleStructRef<'a, T>
where
    T: Clone,
{
    /// Ensure a mutable reference of this value exists, either cloning the
    /// borrowed value, or returning the owned one.
    pub fn mutate(&mut self) -> &mut T {
        if let StyleStructRef::Borrowed(v) = *self {
            *self = StyleStructRef::Owned(UniqueArc::new(v.clone()));
        }

        match *self {
            StyleStructRef::Owned(ref mut v) => v,
            StyleStructRef::Borrowed(..) => unreachable!(),
            StyleStructRef::Vacated => panic!("Accessed vacated style struct")
        }
    }

    /// Whether this is pointer-equal to the struct we're going to copy the
    /// value from.
    ///
    /// This is used to avoid allocations when people write stuff like `font:
    /// inherit` or such `all: initial`.
    #[inline]
    pub fn ptr_eq(&self, struct_to_copy_from: &T) -> bool {
        match *self {
            StyleStructRef::Owned(..) => false,
            StyleStructRef::Borrowed(s) => {
                s as *const T == struct_to_copy_from as *const T
            }
            StyleStructRef::Vacated => panic!("Accessed vacated style struct")
        }
    }

    /// Extract a unique Arc from this struct, vacating it.
    ///
    /// The vacated state is a transient one, please put the Arc back
    /// when done via `put()`. This function is to be used to separate
    /// the struct being mutated from the computed context
    pub fn take(&mut self) -> UniqueArc<T> {
        use std::mem::replace;
        let inner = replace(self, StyleStructRef::Vacated);

        match inner {
            StyleStructRef::Owned(arc) => arc,
            StyleStructRef::Borrowed(s) => UniqueArc::new(s.clone()),
            StyleStructRef::Vacated => panic!("Accessed vacated style struct"),
        }
    }

    /// Replace vacated ref with an arc
    pub fn put(&mut self, arc: UniqueArc<T>) {
        debug_assert!(matches!(*self, StyleStructRef::Vacated));
        *self = StyleStructRef::Owned(arc);
    }

    /// Get a mutable reference to the owned struct, or `None` if the struct
    /// hasn't been mutated.
    pub fn get_if_mutated(&mut self) -> Option<<&mut T> {
        match *self {
            StyleStructRef::Owned(ref mut v) => Some(v),
            StyleStructRef::Borrowed(..) => None,
            StyleStructRef::Vacated => panic!("Accessed vacated style struct")
        }
    }

    /// Returns an `Arc` to the internal struct, constructing one if
    /// appropriate.
    pub fn build(self) -> Arc<T> {
        match self {
            StyleStructRef::Owned(v) => v.shareable(),
            // SAFETY: We know all style structs are arc-allocated.
            StyleStructRef::Borrowed(v) => unsafe { Arc::from_raw_addrefed(v) },
            StyleStructRef::Vacated => panic!("Accessed vacated style struct")
        }
    }
}

impl<'a, T: 'a> ops::Deref for StyleStructRef<'a, T> {
    type Target = T;

    fn deref(&self) -> &T {
        match *self {
            StyleStructRef::Owned(ref v) => &**v,
            StyleStructRef::Borrowed(v) => v,
            StyleStructRef::Vacated => panic!("Accessed vacated style struct")
        }
    }
}

/// A type used to compute a struct with minimal overhead.
///
/// This allows holding references to the parent/default computed values without
/// actually cloning them, until we either build the style, or mutate the
/// inherited value.
pub struct StyleBuilder<'a> {
    /// The device we're using to compute style.
    ///
    /// This provides access to viewport unit ratios, etc.
    pub device: &'a Device,

    /// The stylist we're using to compute style except for media queries.
    /// device is used in media queries instead.
    pub stylist: Option<<&'a Stylist>,

    /// The style we're inheriting from.
    ///
    /// This is effectively
    /// `parent_style.unwrap_or(device.default_computed_values())`.
    inherited_style: &'a ComputedValues,

    /// The style we're getting reset structs from.
    reset_style: &'a ComputedValues,

    /// The rule node representing the ordered list of rules matched for this
    /// node.
    pub rules: Option<StrongRuleNode>,

    /// The computed custom properties.
    pub custom_properties: crate::custom_properties::ComputedCustomProperties,

    /// Non-custom properties that are considered invalid at compute time
    /// due to cyclic dependencies with custom properties.
    /// e.g. `--foo: 1em; font-size: var(--foo)` where `--foo` is registered.
    pub invalid_non_custom_properties: LonghandIdSet,

    /// The pseudo-element this style will represent.
    pub pseudo: Option<<&'a PseudoElement>,

    /// Whether we have mutated any reset structs since the the last time
    /// `clear_modified_reset` was called.  This is used to tell whether the
    /// `StyleAdjuster` did any work.
    modified_reset: bool,

    /// Whether this is the style for the root element.
    pub is_root_element: bool,

    /// The writing mode flags.
    ///
    /// TODO(emilio): Make private.
    pub writing_mode: WritingMode,

    /// The effective zoom.
    pub effective_zoom: computed::Zoom,

    /// Flags for the computed value.
    pub flags: Cell<ComputedValueFlags>,

    /// The element's style if visited, only computed if there's a relevant link
    /// for this element.  A element's "relevant link" is the element being
    /// matched if it is a link or the nearest ancestor link.
    pub visited_style: Option<Arc<ComputedValues>>,
    % for style_struct in data.active_style_structs():
        ${style_struct.ident}: StyleStructRef<'a, style_structs::${style_struct.name}>,
    % endfor
}

impl<'a> StyleBuilder<'a> {
    /// Trivially construct a `StyleBuilder`.
    pub fn new(
        device: &'a Device,
        stylist: Option<<&'a Stylist>,
        parent_style: Option<<&'a ComputedValues>,
        pseudo: Option<<&'a PseudoElement>,
        rules: Option<StrongRuleNode>,
        is_root_element: bool,
    ) -> Self {
        let reset_style = device.default_computed_values();
        let inherited_style = parent_style.unwrap_or(reset_style);

        let flags = inherited_style.flags.inherited();
        StyleBuilder {
            device,
            stylist,
            inherited_style,
            reset_style,
            pseudo,
            rules,
            modified_reset: false,
            is_root_element,
            custom_properties: crate::custom_properties::ComputedCustomProperties::default(),
            invalid_non_custom_properties: LonghandIdSet::default(),
            writing_mode: inherited_style.writing_mode,
            effective_zoom: inherited_style.effective_zoom,
            flags: Cell::new(flags),
            visited_style: None,
            % for style_struct in data.active_style_structs():
            % if style_struct.inherited:
            ${style_struct.ident}: StyleStructRef::Borrowed(inherited_style.get_${style_struct.name_lower}()),
            % else:
            ${style_struct.ident}: StyleStructRef::Borrowed(reset_style.get_${style_struct.name_lower}()),
            % endif
            % endfor
        }
    }

    /// NOTE(emilio): This is done so we can compute relative units with respect
    /// to the parent style, but all the early properties / writing-mode / etc
    /// are already set to the right ones on the kid.
    ///
    /// Do _not_ actually call this to construct a style, this should mostly be
    /// used for animations.
    pub fn for_animation(
        device: &'a Device,
        stylist: Option<<&'a Stylist>,
        style_to_derive_from: &'a ComputedValues,
        parent_style: Option<<&'a ComputedValues>,
    ) -> Self {
        let reset_style = device.default_computed_values();
        let inherited_style = parent_style.unwrap_or(reset_style);
        StyleBuilder {
            device,
            stylist,
            inherited_style,
            reset_style,
            pseudo: None,
            modified_reset: false,
            is_root_element: false,
            rules: None,
            custom_properties: style_to_derive_from.custom_properties().clone(),
            invalid_non_custom_properties: LonghandIdSet::default(),
            writing_mode: style_to_derive_from.writing_mode,
            effective_zoom: style_to_derive_from.effective_zoom,
            flags: Cell::new(style_to_derive_from.flags),
            visited_style: None,
            % for style_struct in data.active_style_structs():
            ${style_struct.ident}: StyleStructRef::Borrowed(
                style_to_derive_from.get_${style_struct.name_lower}()
            ),
            % endfor
        }
    }

    /// Copy the reset properties from `style`.
    pub fn copy_reset_from(&mut self, style: &'a ComputedValues) {
        % for style_struct in data.active_style_structs():
        % if not style_struct.inherited:
        self.${style_struct.ident} =
            StyleStructRef::Borrowed(style.get_${style_struct.name_lower}());
        % endif
        % endfor
    }

    % for property in data.longhands:
    % if not property.logical:
    % if not property.style_struct.inherited:
    /// Inherit `${property.ident}` from our parent style.
    #[allow(non_snake_case)]
    pub fn inherit_${property.ident}(&mut self) {
        let inherited_struct =
            self.inherited_style.get_${property.style_struct.name_lower}();

        self.modified_reset = true;
        self.add_flags(ComputedValueFlags::INHERITS_RESET_STYLE);

        % if property.ident == "content":
        self.add_flags(ComputedValueFlags::CONTENT_DEPENDS_ON_INHERITED_STYLE);
        % endif

        % if property.ident == "display":
        self.add_flags(ComputedValueFlags::DISPLAY_DEPENDS_ON_INHERITED_STYLE);
        % endif

        if self.${property.style_struct.ident}.ptr_eq(inherited_struct) {
            return;
        }

        self.${property.style_struct.ident}.mutate()
            .copy_${property.ident}_from(inherited_struct);
    }
    % else:
    /// Reset `${property.ident}` to the initial value.
    #[allow(non_snake_case)]
    pub fn reset_${property.ident}(&mut self) {
        let reset_struct =
            self.reset_style.get_${property.style_struct.name_lower}();

        if self.${property.style_struct.ident}.ptr_eq(reset_struct) {
            return;
        }

        self.${property.style_struct.ident}.mutate()
            .reset_${property.ident}(reset_struct);
    }
    % endif

    % if not property.is_vector or property.simple_vector_bindings or engine in ["servo-2013", "servo-2020"]:
    /// Set the `${property.ident}` to the computed value `value`.
    #[allow(non_snake_case)]
    pub fn set_${property.ident}(
        &mut self,
        value: longhands::${property.ident}::computed_value::T
    ) {
        % if not property.style_struct.inherited:
        self.modified_reset = true;
        % endif

        self.${property.style_struct.ident}.mutate()
            .set_${property.ident}(
                value,
                % if property.logical:
                self.writing_mode,
                % endif
            );
    }
    % endif
    % endif
    % endfor
    <% del property %>

    /// Inherits style from the parent element, accounting for the default
    /// computed values that need to be provided as well.
    pub fn for_inheritance(
        device: &'a Device,
        stylist: Option<<&'a Stylist>,
        parent: Option<<&'a ComputedValues>,
        pseudo: Option<<&'a PseudoElement>,
    ) -> Self {
        // Rebuild the visited style from the parent, ensuring that it will also
        // not have rules.  This matches the unvisited style that will be
        // produced by this builder.  This assumes that the caller doesn't need
        // to adjust or process visited style, so we can just build visited
        // style here for simplicity.
        let visited_style = parent.and_then(|parent| {
            parent.visited_style().map(|style| {
                Self::for_inheritance(
                    device,
                    stylist,
                    Some(style),
                    pseudo,
                ).build()
            })
        });
        let custom_properties = if let Some(p) = parent { p.custom_properties().clone() } else { crate::custom_properties::ComputedCustomProperties::default() };
        let mut ret = Self::new(
            device,
            stylist,
            parent,
            pseudo,
            /* rules = */ None,
            /* is_root_element = */ false,
        );
        ret.custom_properties = custom_properties;
        ret.visited_style = visited_style;
        ret
    }

    /// Returns whether we have a visited style.
    pub fn has_visited_style(&self) -> bool {
        self.visited_style.is_some()
    }

    /// Returns whether we're a pseudo-elements style.
    pub fn is_pseudo_element(&self) -> bool {
        self.pseudo.map_or(false, |p| !p.is_anon_box())
    }

    /// Returns the style we're getting reset properties from.
    pub fn default_style(&self) -> &'a ComputedValues {
        self.reset_style
    }

    % for style_struct in data.active_style_structs():
        /// Gets an immutable view of the current `${style_struct.name}` style.
        pub fn get_${style_struct.name_lower}(&self) -> &style_structs::${style_struct.name} {
            &self.${style_struct.ident}
        }

        /// Gets a mutable view of the current `${style_struct.name}` style.
        pub fn mutate_${style_struct.name_lower}(&mut self) -> &mut style_structs::${style_struct.name} {
            % if not style_struct.inherited:
            self.modified_reset = true;
            % endif
            self.${style_struct.ident}.mutate()
        }

        /// Gets a mutable view of the current `${style_struct.name}` style.
        pub fn take_${style_struct.name_lower}(&mut self) -> UniqueArc<style_structs::${style_struct.name}> {
            % if not style_struct.inherited:
            self.modified_reset = true;
            % endif
            self.${style_struct.ident}.take()
        }

        /// Gets a mutable view of the current `${style_struct.name}` style.
        pub fn put_${style_struct.name_lower}(&mut self, s: UniqueArc<style_structs::${style_struct.name}>) {
            self.${style_struct.ident}.put(s)
        }

        /// Gets a mutable view of the current `${style_struct.name}` style,
        /// only if it's been mutated before.
        pub fn get_${style_struct.name_lower}_if_mutated(&mut self)
                                                         -> Option<<&mut style_structs::${style_struct.name}> {
            self.${style_struct.ident}.get_if_mutated()
        }

        /// Reset the current `${style_struct.name}` style to its default value.
        pub fn reset_${style_struct.name_lower}_struct(&mut self) {
            self.${style_struct.ident} =
                StyleStructRef::Borrowed(self.reset_style.get_${style_struct.name_lower}());
        }
    % endfor
    <% del style_struct %>

    /// Returns whether this computed style represents a floated object.
    pub fn is_floating(&self) -> bool {
        self.get_box().clone_float().is_floating()
    }

    /// Returns whether this computed style represents an absolutely-positioned
    /// object.
    pub fn is_absolutely_positioned(&self) -> bool {
        self.get_box().clone_position().is_absolutely_positioned()
    }

    /// Whether this style has a top-layer style.
    #[cfg(feature = "servo")]
    pub fn in_top_layer(&self) -> bool {
        matches!(self.get_box().clone__servo_top_layer(),
                 longhands::_servo_top_layer::computed_value::T::Top)
    }

    /// Whether this style has a top-layer style.
    #[cfg(feature = "gecko")]
    pub fn in_top_layer(&self) -> bool {
        matches!(self.get_box().clone__moz_top_layer(),
                 longhands::_moz_top_layer::computed_value::T::Top)
    }

    /// Clears the "have any reset structs been modified" flag.
    pub fn clear_modified_reset(&mut self) {
        self.modified_reset = false;
    }

    /// Returns whether we have mutated any reset structs since the the last
    /// time `clear_modified_reset` was called.
    pub fn modified_reset(&self) -> bool {
        self.modified_reset
    }

    /// Return the current flags.
    #[inline]
    pub fn flags(&self) -> ComputedValueFlags {
        self.flags.get()
    }

    /// Add a flag to the current builder.
    #[inline]
    pub fn add_flags(&self, flag: ComputedValueFlags) {
        let flags = self.flags() | flag;
        self.flags.set(flags);
    }

    /// Removes a flag to the current builder.
    #[inline]
    pub fn remove_flags(&self, flag: ComputedValueFlags) {
        let flags = self.flags() & !flag;
        self.flags.set(flags);
    }

    /// Turns this `StyleBuilder` into a proper `ComputedValues` instance.
    pub fn build(self) -> Arc<ComputedValues> {
        ComputedValues::new(
            self.pseudo,
            self.custom_properties,
            self.writing_mode,
            self.effective_zoom,
            self.flags.get(),
            self.rules,
            self.visited_style,
            % for style_struct in data.active_style_structs():
            self.${style_struct.ident}.build(),
            % endfor
        )
    }

    /// Get the custom properties map if necessary.
    pub fn custom_properties(&self) -> &crate::custom_properties::ComputedCustomProperties {
        &self.custom_properties
    }


    /// Get the inherited custom properties map.
    pub fn inherited_custom_properties(&self) -> &crate::custom_properties::ComputedCustomProperties {
        &self.inherited_style.custom_properties
    }

    /// Access to various information about our inherited styles.  We don't
    /// expose an inherited ComputedValues directly, because in the
    /// ::first-line case some of the inherited information needs to come from
    /// one ComputedValues instance and some from a different one.

    /// Inherited writing-mode.
    pub fn inherited_writing_mode(&self) -> &WritingMode {
        &self.inherited_style.writing_mode
    }

    /// The effective zoom value that we should multiply absolute lengths by.
    pub fn effective_zoom(&self) -> computed::Zoom {
        self.effective_zoom
    }

    /// The zoom specified on this element.
    pub fn specified_zoom(&self) -> computed::Zoom {
        self.get_box().clone_zoom()
    }

    /// Inherited zoom.
    pub fn inherited_effective_zoom(&self) -> computed::Zoom {
        self.inherited_style.effective_zoom
    }

    /// The computed value flags of our parent.
    #[inline]
    pub fn get_parent_flags(&self) -> ComputedValueFlags {
        self.inherited_style.flags
    }

    /// Calculate the line height, given the currently resolved line-height and font.
    pub fn calc_line_height(
        &self,
        device: &Device,
        line_height_base: LineHeightBase,
        writing_mode: WritingMode,
    ) -> computed::NonNegativeLength {
        use crate::computed_value_flags::ComputedValueFlags;
        let (font, flag) = match line_height_base {
            LineHeightBase::CurrentStyle => (
                self.get_font(),
                ComputedValueFlags::DEPENDS_ON_SELF_FONT_METRICS,
            ),
            LineHeightBase::InheritedStyle => (
                self.get_parent_font(),
                ComputedValueFlags::DEPENDS_ON_INHERITED_FONT_METRICS,
            ),
        };
        let line_height = font.clone_line_height();
        if matches!(line_height, computed::LineHeight::Normal) {
            self.add_flags(flag);
        }
        device.calc_line_height(&font, writing_mode, None)
    }

    /// And access to inherited style structs.
    % for style_struct in data.active_style_structs():
        /// Gets our inherited `${style_struct.name}`.  We don't name these
        /// accessors `inherited_${style_struct.name_lower}` because we already
        /// have things like "box" vs "inherited_box" as struct names.  Do the
        /// next-best thing and call them `parent_${style_struct.name_lower}`
        /// instead.
        pub fn get_parent_${style_struct.name_lower}(&self) -> &style_structs::${style_struct.name} {
            self.inherited_style.get_${style_struct.name_lower}()
        }
    % endfor
}

#[cfg(feature = "servo")]
pub use self::lazy_static_module::INITIAL_SERVO_VALUES;

// Use a module to work around #[cfg] on lazy_static! not being applied to every generated item.
#[cfg(feature = "servo")]
#[allow(missing_docs)]
mod lazy_static_module {
    use crate::logical_geometry::WritingMode;
    use crate::computed_value_flags::ComputedValueFlags;
    use servo_arc::Arc;
    use super::{ComputedValues, ComputedValuesInner, longhands, style_structs};

    lazy_static! {
        /// The initial values for all style structs as defined by the specification.
        pub static ref INITIAL_SERVO_VALUES: ComputedValues = ComputedValues {
            inner: ComputedValuesInner {
                % for style_struct in data.active_style_structs():
                    ${style_struct.ident}: Arc::new(style_structs::${style_struct.name} {
                        % for longhand in style_struct.longhands:
                            % if not longhand.logical:
                                ${longhand.ident}: longhands::${longhand.ident}::get_initial_value(),
                            % endif
                        % endfor
                        % if style_struct.name == "InheritedText":
                            text_decorations_in_effect:
                                crate::values::computed::text::TextDecorationsInEffect::default(),
                        % endif
                        % if style_struct.name == "Font":
                            hash: 0,
                        % endif
                        % if style_struct.name == "Box":
                            original_display: longhands::display::get_initial_value(),
                        % endif
                    }),
                % endfor
                custom_properties,
                writing_mode: WritingMode::empty(),
                rules: None,
                visited_style: None,
                flags: ComputedValueFlags::empty(),
            },
            pseudo: None,
        };
    }
}

/// A per-longhand function that performs the CSS cascade for that longhand.
pub type CascadePropertyFn =
    unsafe extern "Rust" fn(
        declaration: &PropertyDeclaration,
        context: &mut computed::Context,
    );

/// A per-longhand array of functions to perform the CSS cascade on each of
/// them, effectively doing virtual dispatch.
pub static CASCADE_PROPERTY: [CascadePropertyFn; ${len(data.longhands)}] = [
    % for property in data.longhands:
        longhands::${property.ident}::cascade_property,
    % endfor
];

/// See StyleAdjuster::adjust_for_border_width.
pub fn adjust_border_width(style: &mut StyleBuilder) {
    % for side in ["top", "right", "bottom", "left"]:
        // Like calling to_computed_value, which wouldn't type check.
        if style.get_border().clone_border_${side}_style().none_or_hidden() &&
           style.get_border().border_${side}_has_nonzero_width() {
            style.set_border_${side}_width(Au(0));
        }
    % endfor
}

/// An identifier for a given alias property.
#[derive(Clone, Copy, Eq, PartialEq, MallocSizeOf)]
#[repr(u16)]
pub enum AliasId {
    % for i, property in enumerate(data.all_aliases()):
        /// ${property.name}
        ${property.camel_case} = ${i},
    % endfor
}

impl fmt::Debug for AliasId {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        let name = NonCustomPropertyId::from(*self).name();
        formatter.write_str(name)
    }
}

impl AliasId {
    /// Returns the property we're aliasing, as a longhand or a shorthand.
    #[inline]
    pub fn aliased_property(self) -> NonCustomPropertyId {
        static MAP: [NonCustomPropertyId; ${len(data.all_aliases())}] = [
        % for alias in data.all_aliases():
            % if alias.original.type() == "longhand":
            NonCustomPropertyId::from_longhand(LonghandId::${alias.original.camel_case}),
            % else:
            <% assert alias.original.type() == "shorthand" %>
            NonCustomPropertyId::from_shorthand(ShorthandId::${alias.original.camel_case}),
            % endif
        % endfor
        ];
        MAP[self as usize]
    }
}

/// Call the given macro with tokens like this for each longhand and shorthand properties
/// that is enabled in content:
///
/// ```
/// [CamelCaseName, SetCamelCaseName, PropertyId::Longhand(LonghandId::CamelCaseName)],
/// ```
///
/// NOTE(emilio): Callers are responsible to deal with prefs.
#[macro_export]
macro_rules! css_properties_accessors {
    ($macro_name: ident) => {
        $macro_name! {
            % for kind, props in [("Longhand", data.longhands), ("Shorthand", data.shorthands)]:
                % for property in props:
                    % if property.enabled_in_content():
                        % for prop in [property] + property.aliases:
                            % if '-' in prop.name:
                                [${prop.ident.capitalize()}, Set${prop.ident.capitalize()},
                                 PropertyId::${kind}(${kind}Id::${property.camel_case})],
                            % endif
                            [${prop.camel_case}, Set${prop.camel_case},
                             PropertyId::${kind}(${kind}Id::${property.camel_case})],
                        % endfor
                    % endif
                % endfor
            % endfor
        }
    }
}

/// Call the given macro with tokens like this for each longhand properties:
///
/// ```
/// { snake_case_ident }
/// ```
#[macro_export]
macro_rules! longhand_properties_idents {
    ($macro_name: ident) => {
        $macro_name! {
            % for property in data.longhands:
                { ${property.ident} }
            % endfor
        }
    }
}

// Large pages generate tens of thousands of ComputedValues.
size_of_test!(ComputedValues, 240);
// FFI relies on this.
size_of_test!(Option<Arc<ComputedValues>>, 8);

// There are two reasons for this test to fail:
//
//   * Your changes made a specified value type for a given property go
//     over the threshold. In that case, you should try to shrink it again
//     or, if not possible, mark the property as boxed in the property
//     definition.
//
//   * Your changes made a specified value type smaller, so that it no
//     longer needs to be boxed. In this case you just need to remove
//     boxed=True from the property definition. Nice job!
#[cfg(target_pointer_width = "64")]
#[allow(dead_code)] // https://github.com/rust-lang/rust/issues/96952
const BOX_THRESHOLD: usize = 24;
% for longhand in data.longhands:
#[cfg(target_pointer_width = "64")]
% if longhand.boxed:
const_assert!(std::mem::size_of::<longhands::${longhand.ident}::SpecifiedValue>() > BOX_THRESHOLD);
% else:
const_assert!(std::mem::size_of::<longhands::${longhand.ident}::SpecifiedValue>() <= BOX_THRESHOLD);
% endif
% endfor

% if engine in ["servo-2013", "servo-2020"]:
% for effect_name in ["repaint", "reflow_out_of_flow", "reflow", "rebuild_and_reflow_inline", "rebuild_and_reflow"]:
    macro_rules! restyle_damage_${effect_name} {
        ($old: ident, $new: ident, $damage: ident, [ $($effect:expr),* ]) => ({
            if
                % for style_struct in data.active_style_structs():
                    % for longhand in style_struct.longhands:
                        % if effect_name in longhand.servo_restyle_damage.split() and not longhand.logical:
                            $old.get_${style_struct.name_lower}().${longhand.ident} !=
                            $new.get_${style_struct.name_lower}().${longhand.ident} ||
                        % endif
                    % endfor
                % endfor

                false {
                    $damage.insert($($effect)|*);
                    true
            } else {
                false
            }
        })
    }
% endfor
% endif
