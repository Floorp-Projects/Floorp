/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />
<% from data import Keyword %>
<% data.new_style_struct("InheritedText", inherited=True, gecko_name="Text") %>

${helpers.predefined_type(
    "color",
    "ColorPropertyValue",
    "crate::color::AbsoluteColor::BLACK",
    engines="gecko servo",
    animation_value_type="AbsoluteColor",
    ignored_when_colors_disabled="True",
    spec="https://drafts.csswg.org/css-color/#color",
    affects="paint",
)}

// CSS Text Module Level 3

${helpers.predefined_type(
    "text-transform",
    "TextTransform",
    "computed::TextTransform::none()",
    engines="gecko servo",
    servo_pref="layout.legacy_layout",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text/#propdef-text-transform",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.single_keyword(
    "hyphens",
    "manual none auto",
    engines="gecko",
    gecko_enum_prefix="StyleHyphens",
    animation_value_type="discrete",
    extra_prefixes="moz",
    spec="https://drafts.csswg.org/css-text/#propdef-hyphens",
    affects="layout",
)}

// TODO: Support <percentage>
${helpers.single_keyword(
    "-moz-text-size-adjust",
    "auto none",
    engines="gecko",
    gecko_enum_prefix="StyleTextSizeAdjust",
    gecko_ffi_name="mTextSizeAdjust",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-size-adjust/#adjustment-control",
    aliases="-webkit-text-size-adjust",
    affects="layout",
)}

${helpers.predefined_type(
    "text-indent",
    "TextIndent",
    "computed::TextIndent::zero()",
    engines="gecko servo",
    animation_value_type="ComputedValue",
    spec="https://drafts.csswg.org/css-text/#propdef-text-indent",
    servo_restyle_damage = "reflow",
    affects="layout",
)}

// Also known as "word-wrap" (which is more popular because of IE), but this is
// the preferred name per CSS-TEXT 6.2.
${helpers.predefined_type(
    "overflow-wrap",
    "OverflowWrap",
    "computed::OverflowWrap::Normal",
    engines="gecko servo",
    servo_pref="layout.legacy_layout",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text/#propdef-overflow-wrap",
    aliases="word-wrap",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "word-break",
    "WordBreak",
    "computed::WordBreak::Normal",
    engines="gecko servo",
    servo_pref="layout.legacy_layout",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text/#propdef-word-break",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "text-justify",
    "TextJustify",
    "computed::TextJustify::Auto",
    engines="gecko servo",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text/#propdef-text-justify",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "text-align-last",
    "TextAlignLast",
    "computed::text::TextAlignLast::Auto",
    engines="gecko servo",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text/#propdef-text-align-last",
    affects="layout",
)}

// TODO make this a shorthand and implement text-align-last/text-align-all
${helpers.predefined_type(
    "text-align",
    "TextAlign",
    "computed::TextAlign::Start",
    engines="gecko servo",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text/#propdef-text-align",
    servo_restyle_damage = "reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "letter-spacing",
    "LetterSpacing",
    "computed::LetterSpacing::normal()",
    engines="gecko servo",
    animation_value_type="ComputedValue",
    spec="https://drafts.csswg.org/css-text/#propdef-letter-spacing",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "word-spacing",
    "WordSpacing",
    "computed::WordSpacing::zero()",
    engines="gecko servo",
    animation_value_type="ComputedValue",
    spec="https://drafts.csswg.org/css-text/#propdef-word-spacing",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

// TODO: `white-space-collapse: discard` not yet supported
${helpers.single_keyword(
    name="white-space-collapse",
    values="collapse preserve preserve-breaks preserve-spaces break-spaces",
    engines="gecko",
    gecko_enum_prefix="StyleWhiteSpaceCollapse",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text-4/#propdef-white-space-collapse",
    affects="layout",
)}

${helpers.predefined_type(
    "text-shadow",
    "SimpleShadow",
    None,
    engines="gecko servo",
    servo_pref="layout.legacy_layout",
    vector=True,
    vector_animation_type="with_zero",
    animation_value_type="AnimatedTextShadowList",
    ignored_when_colors_disabled=True,
    simple_vector_bindings=True,
    spec="https://drafts.csswg.org/css-text-decor-3/#text-shadow-property",
    affects="overflow",
)}

${helpers.predefined_type(
    "text-emphasis-style",
    "TextEmphasisStyle",
    "computed::TextEmphasisStyle::None",
    engines="gecko",
    initial_specified_value="SpecifiedValue::None",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text-decor/#propdef-text-emphasis-style",
    affects="overflow",
)}

${helpers.predefined_type(
    "text-emphasis-position",
    "TextEmphasisPosition",
    "computed::TextEmphasisPosition::OVER",
    engines="gecko",
    initial_specified_value="specified::TextEmphasisPosition::OVER",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text-decor/#propdef-text-emphasis-position",
    affects="layout",
)}

${helpers.predefined_type(
    "text-emphasis-color",
    "Color",
    "computed_value::T::currentcolor()",
    engines="gecko",
    initial_specified_value="specified::Color::currentcolor()",
    animation_value_type="AnimatedColor",
    ignored_when_colors_disabled=True,
    spec="https://drafts.csswg.org/css-text-decor/#propdef-text-emphasis-color",
    affects="paint",
)}

${helpers.predefined_type(
    "tab-size",
    "NonNegativeLengthOrNumber",
    "generics::length::LengthOrNumber::Number(From::from(8.0))",
    engines="gecko",
    animation_value_type="LengthOrNumber",
    spec="https://drafts.csswg.org/css-text-3/#tab-size-property",
    aliases="-moz-tab-size",
    affects="layout",
)}

${helpers.predefined_type(
    "line-break",
    "LineBreak",
    "computed::LineBreak::Auto",
    engines="gecko",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text-3/#line-break-property",
    affects="layout",
)}

// CSS Compatibility
// https://compat.spec.whatwg.org
${helpers.predefined_type(
    "-webkit-text-fill-color",
    "Color",
    "computed_value::T::currentcolor()",
    engines="gecko",
    animation_value_type="AnimatedColor",
    ignored_when_colors_disabled=True,
    spec="https://compat.spec.whatwg.org/#the-webkit-text-fill-color",
    affects="paint",
)}

${helpers.predefined_type(
    "-webkit-text-stroke-color",
    "Color",
    "computed_value::T::currentcolor()",
    initial_specified_value="specified::Color::currentcolor()",
    engines="gecko",
    animation_value_type="AnimatedColor",
    ignored_when_colors_disabled=True,
    spec="https://compat.spec.whatwg.org/#the-webkit-text-stroke-color",
    affects="paint",
)}

${helpers.predefined_type(
    "-webkit-text-stroke-width",
    "LineWidth",
    "app_units::Au(0)",
    engines="gecko",
    initial_specified_value="specified::LineWidth::zero()",
    spec="https://compat.spec.whatwg.org/#the-webkit-text-stroke-width",
    animation_value_type="discrete",
    affects="overflow",
)}

// CSS Ruby Layout Module Level 1
// https://drafts.csswg.org/css-ruby/
${helpers.single_keyword(
    "ruby-align",
    "space-around start center space-between",
    engines="gecko",
    animation_value_type="discrete",
    gecko_enum_prefix="StyleRubyAlign",
    spec="https://drafts.csswg.org/css-ruby/#ruby-align-property",
    affects="layout",
)}

${helpers.predefined_type(
    "ruby-position",
    "RubyPosition",
    "computed::RubyPosition::AlternateOver",
    engines="gecko",
    spec="https://drafts.csswg.org/css-ruby/#ruby-position-property",
    animation_value_type="discrete",
    affects="layout",
)}

// CSS Writing Modes Module Level 3
// https://drafts.csswg.org/css-writing-modes-3/

${helpers.single_keyword(
    "text-combine-upright",
    "none all",
    engines="gecko",
    gecko_enum_prefix="StyleTextCombineUpright",
    animation_value_type="none",
    spec="https://drafts.csswg.org/css-writing-modes-3/#text-combine-upright",
    affects="layout",
)}

// SVG 2: Section 13 - Painting: Filling, Stroking and Marker Symbols
${helpers.single_keyword(
    "text-rendering",
    "auto optimizespeed optimizelegibility geometricprecision",
    engines="gecko servo",
    gecko_enum_prefix="StyleTextRendering",
    animation_value_type="discrete",
    spec="https://svgwg.org/svg2-draft/painting.html#TextRenderingProperty",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "-moz-control-character-visibility",
    "text::MozControlCharacterVisibility",
    "Default::default()",
    engines="gecko",
    enabled_in="chrome",
    gecko_pref="layout.css.moz-control-character-visibility.enabled",
    has_effect_on_gecko_scrollbars=False,
    animation_value_type="none",
    spec="Nonstandard",
    affects="layout",
)}

// text underline offset
${helpers.predefined_type(
    "text-underline-offset",
    "LengthPercentageOrAuto",
    "computed::LengthPercentageOrAuto::auto()",
    engines="gecko",
    animation_value_type="ComputedValue",
    spec="https://drafts.csswg.org/css-text-decor-4/#underline-offset",
    affects="overflow",
)}

// text underline position
${helpers.predefined_type(
    "text-underline-position",
    "TextUnderlinePosition",
    "computed::TextUnderlinePosition::AUTO",
    engines="gecko",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text-decor-3/#text-underline-position-property",
    affects="overflow",
)}

// text decoration skip ink
${helpers.predefined_type(
    "text-decoration-skip-ink",
    "TextDecorationSkipInk",
    "computed::TextDecorationSkipInk::Auto",
    engines="gecko",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text-decor-4/#text-decoration-skip-ink-property",
    affects="overflow",
)}

// hyphenation character
${helpers.predefined_type(
    "hyphenate-character",
    "HyphenateCharacter",
    "computed::HyphenateCharacter::Auto",
    engines="gecko",
    animation_value_type="discrete",
    spec="https://www.w3.org/TR/css-text-4/#hyphenate-character",
    affects="layout",
)}

${helpers.predefined_type(
    "forced-color-adjust",
    "ForcedColorAdjust",
    "computed::ForcedColorAdjust::Auto",
    engines="gecko",
    gecko_pref="layout.css.forced-color-adjust.enabled",
    has_effect_on_gecko_scrollbars=False,
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-color-adjust-1/#forced-color-adjust-prop",
    affects="paint",
)}

${helpers.single_keyword(
    "-webkit-text-security",
    "none circle disc square",
    engines="gecko",
    gecko_enum_prefix="StyleTextSecurity",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text/#MISSING",
    affects="layout",
)}

${helpers.single_keyword(
    "text-wrap-mode",
    "wrap nowrap",
    engines="gecko",
    gecko_enum_prefix="StyleTextWrapMode",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text-4/#propdef-text-wrap-mode",
    affects="layout",
)}

${helpers.single_keyword(
    "text-wrap-style",
    "auto stable balance",
    engines="gecko",
    gecko_pref="layout.css.text-wrap-balance.enabled",
    has_effect_on_gecko_scrollbars=False,
    gecko_enum_prefix="StyleTextWrapStyle",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-text-4/#text-wrap-style",
    affects="layout",
)}
