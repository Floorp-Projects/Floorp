/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />

macro_rules! try_parse_one {
    ($context: expr, $input: expr, $var: ident, $prop_module: ident) => {
        if $var.is_none() {
            if let Ok(value) = $input.try_parse(|i| {
                $prop_module::single_value::parse($context, i)
            }) {
                $var = Some(value);
                continue;
            }
        }
    };
}

<%helpers:shorthand name="transition"
                    engines="gecko servo"
                    extra_prefixes="moz:layout.css.prefixes.transitions webkit"
                    sub_properties="transition-property transition-duration
                                    transition-timing-function
                                    transition-delay transition-behavior"
                    spec="https://drafts.csswg.org/css-transitions/#propdef-transition">
    use crate::parser::Parse;
    % for prop in "delay duration property timing_function behavior".split():
    use crate::properties::longhands::transition_${prop};
    % endfor
    use crate::values::specified::TransitionProperty;

    pub fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Longhands, ParseError<'i>> {
        struct SingleTransition {
            % for prop in "property duration timing_function delay behavior".split():
            transition_${prop}: transition_${prop}::SingleSpecifiedValue,
            % endfor
        }

        fn parse_one_transition<'i, 't>(
            context: &ParserContext,
            input: &mut Parser<'i, 't>,
            first: bool,
        ) -> Result<SingleTransition,ParseError<'i>> {
            % for prop in "property duration timing_function delay behavior".split():
            let mut ${prop} = None;
            % endfor

            let mut parsed = 0;
            loop {
                parsed += 1;

                try_parse_one!(context, input, duration, transition_duration);
                try_parse_one!(context, input, timing_function, transition_timing_function);
                try_parse_one!(context, input, delay, transition_delay);
                if static_prefs::pref!("layout.css.transition-behavior.enabled") {
                    try_parse_one!(context, input, behavior, transition_behavior);
                }
                // Must check 'transition-property' after 'transition-timing-function' since
                // 'transition-property' accepts any keyword.
                if property.is_none() {
                    if let Ok(value) = input.try_parse(|i| TransitionProperty::parse(context, i)) {
                        property = Some(value);
                        continue;
                    }

                    // 'none' is not a valid value for <single-transition-property>,
                    // so it's only acceptable as the first item.
                    if first && input.try_parse(|i| i.expect_ident_matching("none")).is_ok() {
                        property = Some(TransitionProperty::none());
                        continue;
                    }
                }

                parsed -= 1;
                break
            }

            if parsed != 0 {
                Ok(SingleTransition {
                    % for prop in "property duration timing_function delay behavior".split():
                    transition_${prop}: ${prop}.unwrap_or_else(transition_${prop}::single_value
                                                                                 ::get_initial_specified_value),
                    % endfor
                })
            } else {
                Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
            }
        }

        % for prop in "property duration timing_function delay behavior".split():
        let mut ${prop}s = Vec::new();
        % endfor

        let mut first = true;
        let mut has_transition_property_none = false;
        let results = input.parse_comma_separated(|i| {
            if has_transition_property_none {
                // If you specify transition-property: none, multiple items are invalid.
                return Err(i.new_custom_error(StyleParseErrorKind::UnspecifiedError))
            }
            let transition = parse_one_transition(context, i, first)?;
            first = false;
            has_transition_property_none = transition.transition_property.is_none();
            Ok(transition)
        })?;
        for result in results {
            % for prop in "property duration timing_function delay behavior".split():
            ${prop}s.push(result.transition_${prop});
            % endfor
        }

        Ok(expanded! {
            % for prop in "property duration timing_function delay behavior".split():
            transition_${prop}: transition_${prop}::SpecifiedValue(${prop}s.into()),
            % endfor
        })
    }

    impl<'a> ToCss for LonghandsToSerialize<'a>  {
        fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: fmt::Write {
            use crate::Zero;
            use style_traits::values::SequenceWriter;

            let property_len = self.transition_property.0.len();

            // There are two cases that we can do shorthand serialization:
            // * when all value lists have the same length, or
            // * when transition-property is none, and other value lists have exactly one item.
            if property_len == 0 {
                % for name in "duration delay timing_function".split():
                    if self.transition_${name}.0.len() != 1 {
                        return Ok(());
                    }
                % endfor

                if let Some(behavior) = self.transition_behavior {
                    if behavior.0.len() != 1 {
                        return Ok(());
                    }
                }
            } else {
                % for name in "duration delay timing_function".split():
                    if self.transition_${name}.0.len() != property_len {
                        return Ok(());
                    }
                % endfor

                if let Some(behavior) = self.transition_behavior {
                    if behavior.0.len() != property_len {
                        return Ok(());
                    }
                }
            }

            // Representative length.
            let len = self.transition_duration.0.len();

            for i in 0..len {
                if i != 0 {
                    dest.write_str(", ")?;
                }

                let has_duration = !self.transition_duration.0[i].is_zero();
                let has_timing = !self.transition_timing_function.0[i].is_ease();
                let has_delay = !self.transition_delay.0[i].is_zero();
                let has_behavior = match self.transition_behavior {
                    Some(behavior) => !behavior.0[i].is_normal(),
                    _ => false,
                };
                let has_any = has_duration || has_timing || has_delay || has_behavior;

                let mut writer = SequenceWriter::new(dest, " ");

                if property_len == 0 {
                    writer.raw_item("none")?;
                } else if !self.transition_property.0[i].is_all() || !has_any {
                    writer.item(&self.transition_property.0[i])?;
                }

                // In order to avoid ambiguity, we have to serialize duration if we have delay.
                if has_duration || has_delay {
                    writer.item(&self.transition_duration.0[i])?;
                }

                if has_timing {
                    writer.item(&self.transition_timing_function.0[i])?;
                }

                if has_delay {
                    writer.item(&self.transition_delay.0[i])?;
                }

                if has_behavior {
                    writer.item(&self.transition_behavior.unwrap().0[i])?;
                }
            }
            Ok(())
        }
    }
</%helpers:shorthand>

<%helpers:shorthand name="animation"
                    engines="gecko servo"
                    extra_prefixes="moz:layout.css.prefixes.animations webkit"
                    sub_properties="animation-name animation-duration
                                    animation-timing-function animation-delay
                                    animation-iteration-count animation-direction
                                    animation-fill-mode animation-play-state animation-timeline"
                    rule_types_allowed="Style"
                    spec="https://drafts.csswg.org/css-animations/#propdef-animation">
    <%
        props = "name timeline duration timing_function delay iteration_count \
                 direction fill_mode play_state".split()
    %>
    % for prop in props:
    use crate::properties::longhands::animation_${prop};
    % endfor

    pub fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Longhands, ParseError<'i>> {
        struct SingleAnimation {
            % for prop in props:
            animation_${prop}: animation_${prop}::SingleSpecifiedValue,
            % endfor
        }

        fn parse_one_animation<'i, 't>(
            context: &ParserContext,
            input: &mut Parser<'i, 't>,
        ) -> Result<SingleAnimation, ParseError<'i>> {
            % for prop in props:
            let mut ${prop} = None;
            % endfor

            let mut parsed = 0;
            // NB: Name must be the last one here so that keywords valid for other
            // longhands are not interpreted as names.
            //
            // Also, duration must be before delay, see
            // https://drafts.csswg.org/css-animations/#typedef-single-animation
            loop {
                parsed += 1;
                try_parse_one!(context, input, duration, animation_duration);
                try_parse_one!(context, input, timing_function, animation_timing_function);
                try_parse_one!(context, input, delay, animation_delay);
                try_parse_one!(context, input, iteration_count, animation_iteration_count);
                try_parse_one!(context, input, direction, animation_direction);
                try_parse_one!(context, input, fill_mode, animation_fill_mode);
                try_parse_one!(context, input, play_state, animation_play_state);
                try_parse_one!(context, input, name, animation_name);
                if static_prefs::pref!("layout.css.scroll-driven-animations.enabled") {
                    try_parse_one!(context, input, timeline, animation_timeline);
                }

                parsed -= 1;
                break
            }

            // If nothing is parsed, this is an invalid entry.
            if parsed == 0 {
                Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
            } else {
                Ok(SingleAnimation {
                    % for prop in props:
                    animation_${prop}: ${prop}.unwrap_or_else(animation_${prop}::single_value
                                                              ::get_initial_specified_value),
                    % endfor
                })
            }
        }

        % for prop in props:
        let mut ${prop}s = vec![];
        % endfor

        let results = input.parse_comma_separated(|i| parse_one_animation(context, i))?;
        for result in results.into_iter() {
            % for prop in props:
            ${prop}s.push(result.animation_${prop});
            % endfor
        }

        Ok(expanded! {
            % for prop in props:
            animation_${prop}: animation_${prop}::SpecifiedValue(${prop}s.into()),
            % endfor
        })
    }

    impl<'a> ToCss for LonghandsToSerialize<'a>  {
        fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: fmt::Write {
            use crate::values::specified::{
                AnimationDirection, AnimationFillMode, AnimationPlayState,
            };
            use crate::Zero;
            use style_traits::values::SequenceWriter;

            let len = self.animation_name.0.len();
            // There should be at least one declared value
            if len == 0 {
                return Ok(());
            }

            // If any value list length is differs then we don't do a shorthand serialization
            // either.
            % for name in props[2:]:
                if len != self.animation_${name}.0.len() {
                    return Ok(())
                }
            % endfor

            // If the preference of animation-timeline is disabled, `self.animation_timeline` is
            // None.
            if self.animation_timeline.map_or(false, |v| len != v.0.len()) {
                return Ok(());
            }

            for i in 0..len {
                if i != 0 {
                    dest.write_str(", ")?;
                }

                // We follow the order of this syntax:
                // <single-animation> =
                //   <animation-duration> ||
                //   <easing-function> ||
                //   <animation-delay> ||
                //   <single-animation-iteration-count> ||
                //   <single-animation-direction> ||
                //   <single-animation-fill-mode> ||
                //   <single-animation-play-state> ||
                //   [ none | <keyframes-name> ] ||
                //   <single-animation-timeline>
                //
                // https://drafts.csswg.org/css-animations-2/#animation-shorthand
                //
                // FIXME: Bug 1804574. The initial value of duration should be auto, per
                // css-animations-2.
                let has_duration = !self.animation_duration.0[i].is_zero();
                let has_timing_function = !self.animation_timing_function.0[i].is_ease();
                let has_delay = !self.animation_delay.0[i].is_zero();
                let has_iteration_count = !self.animation_iteration_count.0[i].is_one();
                let has_direction =
                    !matches!(self.animation_direction.0[i], AnimationDirection::Normal);
                let has_fill_mode =
                    !matches!(self.animation_fill_mode.0[i], AnimationFillMode::None);
                let has_play_state =
                    !matches!(self.animation_play_state.0[i], AnimationPlayState::Running);
                let has_name = !self.animation_name.0[i].is_none();
                let has_timeline = match self.animation_timeline {
                    Some(timeline) => !timeline.0[i].is_auto(),
                    _ => false,
                };

                let mut writer = SequenceWriter::new(dest, " ");

                // To avoid ambiguity, we have to serialize duration if both duration is initial
                // but delay is not. (In other words, it's ambiguous if we serialize delay only.)
                if has_duration || has_delay {
                    writer.item(&self.animation_duration.0[i])?;
                }

                // For easing function, delay, iteration-count, direction, fill-mode, and
                // play-state.
                % for name in props[3:]:
                if has_${name} {
                    writer.item(&self.animation_${name}.0[i])?;
                }
                % endfor

                // If all values are initial, we must serialize animation-name.
                let has_any = {
                    has_timeline
                % for name in props[2:]:
                        || has_${name}
                % endfor
                };
                if has_name || !has_any {
                    writer.item(&self.animation_name.0[i])?;
                }

                if has_timeline {
                    writer.item(&self.animation_timeline.unwrap().0[i])?;
                }
            }
            Ok(())
        }
    }
</%helpers:shorthand>

<%helpers:shorthand
    engines="gecko"
    name="scroll-timeline"
    sub_properties="scroll-timeline-name scroll-timeline-axis"
    gecko_pref="layout.css.scroll-driven-animations.enabled",
    spec="https://drafts.csswg.org/scroll-animations-1/#scroll-timeline-shorthand"
>
    pub fn parse_value<'i>(
        context: &ParserContext,
        input: &mut Parser<'i, '_>,
    ) -> Result<Longhands, ParseError<'i>> {
        use crate::properties::longhands::{scroll_timeline_axis, scroll_timeline_name};

        let mut names = Vec::with_capacity(1);
        let mut axes = Vec::with_capacity(1);
        input.parse_comma_separated(|input| {
            let name = scroll_timeline_name::single_value::parse(context, input)?;
            let axis = input.try_parse(|i| scroll_timeline_axis::single_value::parse(context, i));

            names.push(name);
            axes.push(axis.unwrap_or_default());

            Ok(())
        })?;

        Ok(expanded! {
            scroll_timeline_name: scroll_timeline_name::SpecifiedValue(names.into()),
            scroll_timeline_axis: scroll_timeline_axis::SpecifiedValue(axes.into()),
        })
    }

    impl<'a> ToCss for LonghandsToSerialize<'a>  {
        fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: fmt::Write {
            // If any value list length is differs then we don't do a shorthand serialization
            // either.
            let len = self.scroll_timeline_name.0.len();
            if len != self.scroll_timeline_axis.0.len() {
                return Ok(());
            }

            for i in 0..len {
                if i != 0 {
                    dest.write_str(", ")?;
                }

                self.scroll_timeline_name.0[i].to_css(dest)?;

                if self.scroll_timeline_axis.0[i] != Default::default() {
                    dest.write_char(' ')?;
                    self.scroll_timeline_axis.0[i].to_css(dest)?;
                }

            }
            Ok(())
        }
    }
</%helpers:shorthand>

// Note: view-timeline shorthand doesn't take view-timeline-inset into account.
<%helpers:shorthand
    engines="gecko"
    name="view-timeline"
    sub_properties="view-timeline-name view-timeline-axis"
    gecko_pref="layout.css.scroll-driven-animations.enabled",
    spec="https://drafts.csswg.org/scroll-animations-1/#view-timeline-shorthand"
>
    pub fn parse_value<'i>(
        context: &ParserContext,
        input: &mut Parser<'i, '_>,
    ) -> Result<Longhands, ParseError<'i>> {
        use crate::properties::longhands::{view_timeline_axis, view_timeline_name};

        let mut names = Vec::with_capacity(1);
        let mut axes = Vec::with_capacity(1);
        input.parse_comma_separated(|input| {
            let name = view_timeline_name::single_value::parse(context, input)?;
            let axis = input.try_parse(|i| view_timeline_axis::single_value::parse(context, i));

            names.push(name);
            axes.push(axis.unwrap_or_default());

            Ok(())
        })?;

        Ok(expanded! {
            view_timeline_name: view_timeline_name::SpecifiedValue(names.into()),
            view_timeline_axis: view_timeline_axis::SpecifiedValue(axes.into()),
        })
    }

    impl<'a> ToCss for LonghandsToSerialize<'a>  {
        fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: fmt::Write {
            // If any value list length is differs then we don't do a shorthand serialization
            // either.
            let len = self.view_timeline_name.0.len();
            if len != self.view_timeline_axis.0.len() {
                return Ok(());
            }

            for i in 0..len {
                if i != 0 {
                    dest.write_str(", ")?;
                }

                self.view_timeline_name.0[i].to_css(dest)?;

                if self.view_timeline_axis.0[i] != Default::default() {
                    dest.write_char(' ')?;
                    self.view_timeline_axis.0[i].to_css(dest)?;
                }

            }
            Ok(())
        }
    }
</%helpers:shorthand>
