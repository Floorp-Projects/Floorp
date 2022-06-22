/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use super::error_reporter::ErrorReporter;
use super::stylesheet_loader::{AsyncStylesheetParser, StylesheetLoader};
use bincode::{deserialize, serialize};
use cssparser::ToCss as ParserToCss;
use cssparser::{ParseErrorKind, Parser, ParserInput, SourceLocation, UnicodeRange};
use dom::{DocumentState, ElementState};
use malloc_size_of::MallocSizeOfOps;
use nsstring::{nsCString, nsString};
use selectors::{NthIndexCache, SelectorList};
use servo_arc::{Arc, ArcBorrow, RawOffsetArc};
use smallvec::SmallVec;
use std::cell::RefCell;
use std::collections::BTreeSet;
use std::fmt::Write;
use std::iter;
use std::os::raw::c_void;
use std::ptr;
use style::applicable_declarations::ApplicableDeclarationBlock;
use style::author_styles::AuthorStyles;
use style::context::ThreadLocalStyleContext;
use style::context::{CascadeInputs, QuirksMode, SharedStyleContext, StyleContext};
use style::counter_style;
use style::data::{self, ElementStyles};
use style::dom::{ShowSubtreeData, TDocument, TElement, TNode};
use style::driver;
use style::error_reporting::{ContextualParseError, ParseErrorReporter};
use style::font_face::{self, FontFaceSourceListComponent, Source};
use style::font_metrics::{get_metrics_provider_for_product, FontMetricsProvider};
use style::gecko::data::{GeckoStyleSheet, PerDocumentStyleData, PerDocumentStyleDataImpl};
use style::gecko::restyle_damage::GeckoRestyleDamage;
use style::gecko::selector_parser::{NonTSPseudoClass, PseudoElement};
use style::gecko::traversal::RecalcStyleOnly;
use style::gecko::url;
use style::gecko::wrapper::{GeckoElement, GeckoNode};
use style::gecko_bindings::bindings;
use style::gecko_bindings::bindings::nsACString;
use style::gecko_bindings::bindings::nsAString;
use style::gecko_bindings::bindings::Gecko_AddPropertyToSet;
use style::gecko_bindings::bindings::Gecko_AppendPropertyValuePair;
use style::gecko_bindings::bindings::Gecko_ConstructFontFeatureValueSet;
use style::gecko_bindings::bindings::Gecko_GetOrCreateFinalKeyframe;
use style::gecko_bindings::bindings::Gecko_GetOrCreateInitialKeyframe;
use style::gecko_bindings::bindings::Gecko_GetOrCreateKeyframeAtStart;
use style::gecko_bindings::bindings::Gecko_HaveSeenPtr;
use style::gecko_bindings::structs;
use style::gecko_bindings::structs::gfxFontFeatureValueSet;
use style::gecko_bindings::structs::ipc::ByteBuf;
use style::gecko_bindings::structs::nsAtom;
use style::gecko_bindings::structs::nsCSSCounterDesc;
use style::gecko_bindings::structs::nsCSSFontDesc;
use style::gecko_bindings::structs::nsCSSPropertyID;
use style::gecko_bindings::structs::nsChangeHint;
use style::gecko_bindings::structs::nsCompatibility;
use style::gecko_bindings::structs::nsStyleTransformMatrix::MatrixTransformOperator;
use style::gecko_bindings::structs::nsTArray;
use style::gecko_bindings::structs::nsTimingFunction;
use style::gecko_bindings::structs::nsresult;
use style::gecko_bindings::structs::CallerType;
use style::gecko_bindings::structs::CompositeOperation;
use style::gecko_bindings::structs::DeclarationBlockMutationClosure;
use style::gecko_bindings::structs::IterationCompositeOperation;
use style::gecko_bindings::structs::Loader;
use style::gecko_bindings::structs::LoaderReusableStyleSheets;
use style::gecko_bindings::structs::MallocSizeOf as GeckoMallocSizeOf;
use style::gecko_bindings::structs::OriginFlags;
use style::gecko_bindings::structs::PropertyValuePair;
use style::gecko_bindings::structs::PseudoStyleType;
use style::gecko_bindings::structs::RawServoSelectorList;
use style::gecko_bindings::structs::RawServoSourceSizeList;
use style::gecko_bindings::structs::RawServoStyleRule;
use style::gecko_bindings::structs::SeenPtrs;
use style::gecko_bindings::structs::ServoElementSnapshotTable;
use style::gecko_bindings::structs::ServoStyleSetSizes;
use style::gecko_bindings::structs::ServoTraversalFlags;
use style::gecko_bindings::structs::SheetLoadData;
use style::gecko_bindings::structs::SheetLoadDataHolder;
use style::gecko_bindings::structs::SheetParsingMode;
use style::gecko_bindings::structs::StyleRuleInclusion;
use style::gecko_bindings::structs::StyleSheet as DomStyleSheet;
use style::gecko_bindings::structs::URLExtraData;
use style::gecko_bindings::structs::{nsINode as RawGeckoNode, Element as RawGeckoElement};
use style::gecko_bindings::structs::{
    RawServoAnimationValue, RawServoAuthorStyles, RawServoCounterStyleRule,
    RawServoDeclarationBlock, RawServoFontFaceRule, RawServoFontFeatureValuesRule,
    RawServoImportRule, RawServoKeyframe, RawServoKeyframesRule, RawServoLayerBlockRule,
    RawServoLayerStatementRule, RawServoMediaList, RawServoMediaRule, RawServoMozDocumentRule,
    RawServoNamespaceRule, RawServoPageRule, RawServoScrollTimelineRule,
    RawServoSharedMemoryBuilder, RawServoStyleSet, RawServoStyleSheetContents,
    RawServoSupportsRule, RawServoContainerRule, ServoCssRules,
};
use style::gecko_bindings::sugar::ownership::{FFIArcHelpers, HasArcFFI, HasFFI};
use style::gecko_bindings::sugar::ownership::{
    HasBoxFFI, HasSimpleFFI, Owned, OwnedOrNull, Strong,
};
use style::gecko_bindings::sugar::refptr::RefPtr;
use style::global_style_data::{
    GlobalStyleData, StyleThreadPool, GLOBAL_STYLE_DATA, STYLE_THREAD_POOL,
};
use style::invalidation::element::restyle_hints::RestyleHint;
use style::invalidation::stylesheets::RuleChangeKind;
use style::media_queries::MediaList;
use style::parser::{self, Parse, ParserContext};
use style::piecewise_linear::PiecewiseLinearFunction;
use style::properties::animated_properties::{AnimationValue, AnimationValueMap};
use style::properties::{parse_one_declaration_into, parse_style_attribute};
use style::properties::{ComputedValues, CountedUnknownProperty, Importance, NonCustomPropertyId};
use style::properties::{LonghandId, LonghandIdSet, PropertyDeclarationBlock, PropertyId};
use style::properties::{PropertyDeclarationId, ShorthandId};
use style::properties::{SourcePropertyDeclaration, StyleBuilder};
use style::rule_cache::RuleCacheConditions;
use style::rule_tree::{CascadeLevel, StrongRuleNode};
use style::selector_parser::PseudoElementCascadeType;
use style::shared_lock::{Locked, SharedRwLockReadGuard, StylesheetGuards, ToCssWithGuard};
use style::string_cache::{Atom, WeakAtom};
use style::style_adjuster::StyleAdjuster;
use style::stylesheets::import_rule::ImportSheet;
use style::stylesheets::keyframes_rule::{Keyframe, KeyframeSelector, KeyframesStepValue};
use style::stylesheets::layer_rule::LayerOrder;
use style::stylesheets::scroll_timeline_rule::ScrollDirection;
use style::stylesheets::supports_rule::parse_condition_or_declaration;
use style::stylesheets::{
    AllowImportRules, CounterStyleRule, CssRule, CssRuleType, CssRules, CssRulesHelpers,
    DocumentRule, FontFaceRule, FontFeatureValuesRule, ImportRule, KeyframesRule, LayerBlockRule,
    LayerStatementRule, MediaRule, NamespaceRule, Origin, OriginSet, PageRule, SanitizationData,
    SanitizationKind, ScrollTimelineRule, StyleRule, StylesheetContents,
    StylesheetLoader as StyleStylesheetLoader, SupportsRule, UrlExtraData, ContainerRule,
};
use style::stylist::{add_size_of_ua_cache, AuthorStylesEnabled, RuleInclusion, Stylist};
use style::thread_state;
use style::traversal::resolve_style;
use style::traversal::DomTraversal;
use style::traversal_flags::{self, TraversalFlags};
use style::use_counters::UseCounters;
use style::values::animated::{Animate, Procedure, ToAnimatedZero};
use style::values::animated::color::AnimatedRGBA;
use style::values::generics::color::ColorInterpolationMethod;
use style::values::computed::easing::ComputedLinearStop;
use style::values::computed::font::{FontFamily, FontFamilyList, GenericFontFamily, FontWeight, FontStyle, FontStretch};
use style::values::computed::{self, Context, ToComputedValue};
use style::values::distance::ComputeSquaredDistance;
use style::values::specified::gecko::IntersectionObserverRootMargin;
use style::values::specified::source_size_list::SourceSizeList;
use style::values::{specified, AtomIdent, CustomIdent, KeyframesName};
use style_traits::{CssWriter, ParsingMode, StyleParseErrorKind, ToCss};
use to_shmem::SharedMemoryBuilder;

trait ClosureHelper {
    fn invoke(&self, property_id: Option<NonCustomPropertyId>);
}

impl ClosureHelper for DeclarationBlockMutationClosure {
    #[inline]
    fn invoke(&self, property_id: Option<NonCustomPropertyId>) {
        if let Some(function) = self.function.as_ref() {
            let gecko_prop_id = match property_id {
                Some(p) => p.to_nscsspropertyid(),
                None => nsCSSPropertyID::eCSSPropertyExtra_variable,
            };
            unsafe { function(self.data, gecko_prop_id) }
        }
    }
}

/*
 * For Gecko->Servo function calls, we need to redeclare the same signature that was declared in
 * the C header in Gecko. In order to catch accidental mismatches, we run rust-bindgen against
 * those signatures as well, giving us a second declaration of all the Servo_* functions in this
 * crate. If there's a mismatch, LLVM will assert and abort, which is a rather awful thing to
 * depend on but good enough for our purposes.
 */

// A dummy url data for where we don't pass url data in.
static mut DUMMY_URL_DATA: *mut URLExtraData = 0 as *mut _;
static mut DUMMY_CHROME_URL_DATA: *mut URLExtraData = 0 as *mut _;

#[no_mangle]
pub unsafe extern "C" fn Servo_Initialize(
    dummy_url_data: *mut URLExtraData,
    dummy_chrome_url_data: *mut URLExtraData,
) {
    use style::gecko_bindings::sugar::origin_flags;

    // Pretend that we're a Servo Layout thread, to make some assertions happy.
    thread_state::initialize(thread_state::ThreadState::LAYOUT);

    // Perform some debug-only runtime assertions.
    origin_flags::assert_flags_match();
    parser::assert_parsing_mode_match();
    traversal_flags::assert_traversal_flags_match();
    specified::font::assert_variant_east_asian_matches();
    specified::font::assert_variant_ligatures_matches();

    DUMMY_URL_DATA = dummy_url_data;
    DUMMY_CHROME_URL_DATA = dummy_chrome_url_data;
}

#[no_mangle]
pub unsafe extern "C" fn Servo_Shutdown() {
    DUMMY_URL_DATA = ptr::null_mut();
    DUMMY_CHROME_URL_DATA = ptr::null_mut();
    Stylist::shutdown();
    url::shutdown();
}

#[inline(always)]
unsafe fn dummy_url_data() -> &'static UrlExtraData {
    UrlExtraData::from_ptr_ref(&DUMMY_URL_DATA)
}

#[allow(dead_code)]
fn is_main_thread() -> bool {
    unsafe { bindings::Gecko_IsMainThread() }
}

#[allow(dead_code)]
fn is_in_servo_traversal() -> bool {
    unsafe { bindings::Gecko_IsInServoTraversal() }
}

fn create_shared_context<'a>(
    global_style_data: &GlobalStyleData,
    guard: &'a SharedRwLockReadGuard,
    stylist: &'a Stylist,
    traversal_flags: TraversalFlags,
    snapshot_map: &'a ServoElementSnapshotTable,
) -> SharedStyleContext<'a> {
    SharedStyleContext {
        stylist: &stylist,
        visited_styles_enabled: stylist.device().visited_styles_enabled(),
        options: global_style_data.options.clone(),
        guards: StylesheetGuards::same(guard),
        current_time_for_animations: 0.0, // Unused for Gecko, at least for now.
        traversal_flags,
        snapshot_map,
    }
}

fn traverse_subtree(
    element: GeckoElement,
    global_style_data: &GlobalStyleData,
    per_doc_data: &PerDocumentStyleDataImpl,
    guard: &SharedRwLockReadGuard,
    traversal_flags: TraversalFlags,
    snapshots: &ServoElementSnapshotTable,
) {
    let shared_style_context = create_shared_context(
        &global_style_data,
        &guard,
        &per_doc_data.stylist,
        traversal_flags,
        snapshots,
    );

    let token = RecalcStyleOnly::pre_traverse(element, &shared_style_context);

    if !token.should_traverse() {
        return;
    }

    debug!("Traversing subtree from {:?}", element);

    let thread_pool_holder = &*STYLE_THREAD_POOL;
    let pool;
    let thread_pool = if traversal_flags.contains(TraversalFlags::ParallelTraversal) {
        pool = thread_pool_holder.pool();
        pool.as_ref()
    } else {
        None
    };

    let traversal = RecalcStyleOnly::new(shared_style_context);
    driver::traverse_dom(&traversal, token, thread_pool);
}

/// Traverses the subtree rooted at `root` for restyling.
///
/// Returns whether the root was restyled. Whether anything else was restyled or
/// not can be inferred from the dirty bits in the rest of the tree.
#[no_mangle]
pub extern "C" fn Servo_TraverseSubtree(
    root: &RawGeckoElement,
    raw_data: &RawServoStyleSet,
    snapshots: *const ServoElementSnapshotTable,
    raw_flags: ServoTraversalFlags,
) -> bool {
    let traversal_flags = TraversalFlags::from_bits_truncate(raw_flags);
    debug_assert!(!snapshots.is_null());

    let element = GeckoElement(root);

    debug!("Servo_TraverseSubtree (flags={:?})", traversal_flags);
    debug!("{:?}", ShowSubtreeData(element.as_node()));

    if cfg!(debug_assertions) {
        if let Some(parent) = element.traversal_parent() {
            let data = parent
                .borrow_data()
                .expect("Styling element with unstyled parent");
            assert!(
                !data.styles.is_display_none(),
                "Styling element with display: none parent"
            );
        }
    }

    let needs_animation_only_restyle =
        element.has_animation_only_dirty_descendants() || element.has_animation_restyle_hints();

    let per_doc_data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    debug_assert!(!per_doc_data.stylist.stylesheets_have_changed());

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();

    let was_initial_style = !element.has_data();

    if needs_animation_only_restyle {
        debug!(
            "Servo_TraverseSubtree doing animation-only restyle (aodd={})",
            element.has_animation_only_dirty_descendants()
        );
        traverse_subtree(
            element,
            &global_style_data,
            &per_doc_data,
            &guard,
            traversal_flags | TraversalFlags::AnimationOnly,
            unsafe { &*snapshots },
        );
    }

    traverse_subtree(
        element,
        &global_style_data,
        &per_doc_data,
        &guard,
        traversal_flags,
        unsafe { &*snapshots },
    );

    debug!(
        "Servo_TraverseSubtree complete (dd={}, aodd={}, lfcd={}, lfc={}, data={:?})",
        element.has_dirty_descendants(),
        element.has_animation_only_dirty_descendants(),
        element.descendants_need_frames(),
        element.needs_frame(),
        element.borrow_data().unwrap()
    );

    if was_initial_style {
        debug_assert!(!element.borrow_data().unwrap().contains_restyle_data());
        false
    } else {
        let element_was_restyled = element.borrow_data().unwrap().contains_restyle_data();
        element_was_restyled
    }
}

/// Checks whether the rule tree has crossed its threshold for unused nodes, and
/// if so, frees them.
#[no_mangle]
pub extern "C" fn Servo_MaybeGCRuleTree(raw_data: &RawServoStyleSet) {
    let per_doc_data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    per_doc_data.stylist.rule_tree().maybe_gc();
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValues_Interpolate(
    from: &RawServoAnimationValue,
    to: &RawServoAnimationValue,
    progress: f64,
) -> Strong<RawServoAnimationValue> {
    let from_value = AnimationValue::as_arc(&from);
    let to_value = AnimationValue::as_arc(&to);
    if let Ok(value) = from_value.animate(to_value, Procedure::Interpolate { progress }) {
        Arc::new(value).into_strong()
    } else {
        Strong::null()
    }
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValues_IsInterpolable(
    from: &RawServoAnimationValue,
    to: &RawServoAnimationValue,
) -> bool {
    let from_value = AnimationValue::as_arc(&from);
    let to_value = AnimationValue::as_arc(&to);
    from_value
        .animate(to_value, Procedure::Interpolate { progress: 0.5 })
        .is_ok()
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValues_Add(
    a: &RawServoAnimationValue,
    b: &RawServoAnimationValue,
) -> Strong<RawServoAnimationValue> {
    let a_value = AnimationValue::as_arc(&a);
    let b_value = AnimationValue::as_arc(&b);
    if let Ok(value) = a_value.animate(b_value, Procedure::Add) {
        Arc::new(value).into_strong()
    } else {
        Strong::null()
    }
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValues_Accumulate(
    a: &RawServoAnimationValue,
    b: &RawServoAnimationValue,
    count: u64,
) -> Strong<RawServoAnimationValue> {
    let a_value = AnimationValue::as_arc(&a);
    let b_value = AnimationValue::as_arc(&b);
    if let Ok(value) = a_value.animate(b_value, Procedure::Accumulate { count }) {
        Arc::new(value).into_strong()
    } else {
        Strong::null()
    }
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValues_GetZeroValue(
    value_to_match: &RawServoAnimationValue,
) -> Strong<RawServoAnimationValue> {
    let value_to_match = AnimationValue::as_arc(&value_to_match);
    if let Ok(zero_value) = value_to_match.to_animated_zero() {
        Arc::new(zero_value).into_strong()
    } else {
        Strong::null()
    }
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValues_ComputeDistance(
    from: &RawServoAnimationValue,
    to: &RawServoAnimationValue,
) -> f64 {
    let from_value = AnimationValue::as_arc(&from);
    let to_value = AnimationValue::as_arc(&to);
    // If compute_squared_distance() failed, this function will return negative value
    // in order to check whether we support the specified paced animation values.
    from_value
        .compute_squared_distance(to_value)
        .map(|d| d.sqrt())
        .unwrap_or(-1.0)
}

/// Compute one of the endpoints for the interpolation interval, compositing it with the
/// underlying value if needed.
/// An None returned value means, "Just use endpoint_value as-is."
/// It is the responsibility of the caller to ensure that |underlying_value| is provided
/// when it will be used.
fn composite_endpoint(
    endpoint_value: Option<&RawOffsetArc<AnimationValue>>,
    composite: CompositeOperation,
    underlying_value: Option<&AnimationValue>,
) -> Option<AnimationValue> {
    match endpoint_value {
        Some(endpoint_value) => match composite {
            CompositeOperation::Add => underlying_value
                .expect("We should have an underlying_value")
                .animate(endpoint_value, Procedure::Add)
                .ok(),
            CompositeOperation::Accumulate => underlying_value
                .expect("We should have an underlying value")
                .animate(endpoint_value, Procedure::Accumulate { count: 1 })
                .ok(),
            _ => None,
        },
        None => underlying_value.map(|v| v.clone()),
    }
}

/// Accumulate one of the endpoints of the animation interval.
/// A returned value of None means, "Just use endpoint_value as-is."
fn accumulate_endpoint(
    endpoint_value: Option<&RawOffsetArc<AnimationValue>>,
    composited_value: Option<AnimationValue>,
    last_value: &AnimationValue,
    current_iteration: u64,
) -> Option<AnimationValue> {
    debug_assert!(
        endpoint_value.is_some() || composited_value.is_some(),
        "Should have a suitable value to use"
    );

    let count = current_iteration;
    match composited_value {
        Some(endpoint) => last_value
            .animate(&endpoint, Procedure::Accumulate { count })
            .ok()
            .or(Some(endpoint)),
        None => last_value
            .animate(endpoint_value.unwrap(), Procedure::Accumulate { count })
            .ok(),
    }
}

/// Compose the animation segment. We composite it with the underlying_value and last_value if
/// needed.
/// The caller is responsible for providing an underlying value and last value
/// in all situations where there are needed.
fn compose_animation_segment(
    segment: &structs::AnimationPropertySegment,
    underlying_value: Option<&AnimationValue>,
    last_value: Option<&AnimationValue>,
    iteration_composite: IterationCompositeOperation,
    current_iteration: u64,
    total_progress: f64,
    segment_progress: f64,
) -> AnimationValue {
    // Extract keyframe values.
    let raw_from_value;
    let keyframe_from_value = if !segment.mFromValue.mServo.mRawPtr.is_null() {
        raw_from_value = unsafe { &*segment.mFromValue.mServo.mRawPtr };
        Some(AnimationValue::as_arc(&raw_from_value))
    } else {
        None
    };

    let raw_to_value;
    let keyframe_to_value = if !segment.mToValue.mServo.mRawPtr.is_null() {
        raw_to_value = unsafe { &*segment.mToValue.mServo.mRawPtr };
        Some(AnimationValue::as_arc(&raw_to_value))
    } else {
        None
    };

    let mut composited_from_value = composite_endpoint(
        keyframe_from_value,
        segment.mFromComposite,
        underlying_value,
    );
    let mut composited_to_value =
        composite_endpoint(keyframe_to_value, segment.mToComposite, underlying_value);

    debug_assert!(
        keyframe_from_value.is_some() || composited_from_value.is_some(),
        "Should have a suitable from value to use"
    );
    debug_assert!(
        keyframe_to_value.is_some() || composited_to_value.is_some(),
        "Should have a suitable to value to use"
    );

    // Apply iteration composite behavior.
    if iteration_composite == IterationCompositeOperation::Accumulate && current_iteration > 0 {
        let last_value = last_value
            .unwrap_or_else(|| underlying_value.expect("Should have a valid underlying value"));

        composited_from_value = accumulate_endpoint(
            keyframe_from_value,
            composited_from_value,
            last_value,
            current_iteration,
        );
        composited_to_value = accumulate_endpoint(
            keyframe_to_value,
            composited_to_value,
            last_value,
            current_iteration,
        );
    }

    // Use the composited value if there is one, otherwise, use the original keyframe value.
    let from = composited_from_value
        .as_ref()
        .unwrap_or_else(|| keyframe_from_value.unwrap());
    let to = composited_to_value
        .as_ref()
        .unwrap_or_else(|| keyframe_to_value.unwrap());

    if segment.mToKey == segment.mFromKey {
        return if total_progress < 0. {
            from.clone()
        } else {
            to.clone()
        };
    }

    match from.animate(
        to,
        Procedure::Interpolate {
            progress: segment_progress,
        },
    ) {
        Ok(value) => value,
        _ => {
            if segment_progress < 0.5 {
                from.clone()
            } else {
                to.clone()
            }
        },
    }
}

#[no_mangle]
pub extern "C" fn Servo_ComposeAnimationSegment(
    segment: &structs::AnimationPropertySegment,
    underlying_value: Option<&RawServoAnimationValue>,
    last_value: Option<&RawServoAnimationValue>,
    iteration_composite: IterationCompositeOperation,
    progress: f64,
    current_iteration: u64,
) -> Strong<RawServoAnimationValue> {
    let underlying_value = AnimationValue::arc_from_borrowed(&underlying_value).map(|v| &**v);
    let last_value = AnimationValue::arc_from_borrowed(&last_value).map(|v| &**v);
    let result = compose_animation_segment(
        segment,
        underlying_value,
        last_value,
        iteration_composite,
        current_iteration,
        progress,
        progress,
    );
    Arc::new(result).into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_AnimationCompose(
    raw_value_map: &mut structs::RawServoAnimationValueMap,
    base_values: &structs::RawServoAnimationValueTable,
    css_property: nsCSSPropertyID,
    segment: &structs::AnimationPropertySegment,
    last_segment: &structs::AnimationPropertySegment,
    computed_timing: &structs::ComputedTiming,
    iteration_composite: IterationCompositeOperation,
) {
    use style::gecko_bindings::bindings::Gecko_AnimationGetBaseStyle;
    use style::gecko_bindings::bindings::Gecko_GetPositionInSegment;
    use style::gecko_bindings::bindings::Gecko_GetProgressFromComputedTiming;

    let property = match LonghandId::from_nscsspropertyid(css_property) {
        Ok(longhand) if longhand.is_animatable() => longhand,
        _ => return,
    };
    let value_map = AnimationValueMap::from_ffi_mut(raw_value_map);

    // We will need an underlying value if either of the endpoints is null...
    let need_underlying_value = segment.mFromValue.mServo.mRawPtr.is_null() ||
                                segment.mToValue.mServo.mRawPtr.is_null() ||
                                // ... or if they have a non-replace composite mode ...
                                segment.mFromComposite != CompositeOperation::Replace ||
                                segment.mToComposite != CompositeOperation::Replace ||
                                // ... or if we accumulate onto the last value and it is null.
                                (iteration_composite == IterationCompositeOperation::Accumulate &&
                                 computed_timing.mCurrentIteration > 0 &&
                                 last_segment.mToValue.mServo.mRawPtr.is_null());

    // If either of the segment endpoints are null, get the underlying value to
    // use from the current value in the values map (set by a lower-priority
    // effect), or, if there is no current value, look up the cached base value
    // for this property.
    let underlying_value = if need_underlying_value {
        let previous_composed_value = value_map.get(&property).cloned();
        previous_composed_value.or_else(|| {
            let raw_base_style =
                unsafe { Gecko_AnimationGetBaseStyle(base_values, css_property).as_ref() };
            AnimationValue::arc_from_borrowed(&raw_base_style)
                .map(|v| &**v)
                .cloned()
        })
    } else {
        None
    };

    if need_underlying_value && underlying_value.is_none() {
        warn!("Underlying value should be valid when we expect to use it");
        return;
    }

    let raw_last_value;
    let last_value = if !last_segment.mToValue.mServo.mRawPtr.is_null() {
        raw_last_value = unsafe { &*last_segment.mToValue.mServo.mRawPtr };
        Some(&**AnimationValue::as_arc(&raw_last_value))
    } else {
        None
    };

    let progress = unsafe { Gecko_GetProgressFromComputedTiming(computed_timing) };
    let position = if segment.mToKey == segment.mFromKey {
        // Note: compose_animation_segment doesn't use this value
        // if segment.mFromKey == segment.mToKey, so assigning |progress| directly is fine.
        progress
    } else {
        unsafe { Gecko_GetPositionInSegment(segment, progress, computed_timing.mBeforeFlag) }
    };

    let result = compose_animation_segment(
        segment,
        underlying_value.as_ref(),
        last_value,
        iteration_composite,
        computed_timing.mCurrentIteration,
        progress,
        position,
    );
    value_map.insert(property, result);
}

macro_rules! get_property_id_from_nscsspropertyid {
    ($property_id: ident, $ret: expr) => {{
        match PropertyId::from_nscsspropertyid($property_id) {
            Ok(property_id) => property_id,
            Err(()) => {
                return $ret;
            },
        }
    }};
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_Serialize(
    value: &RawServoAnimationValue,
    property: nsCSSPropertyID,
    raw_data: &RawServoStyleSet,
    buffer: &mut nsACString,
) {
    let uncomputed_value = AnimationValue::as_arc(&value).uncompute();
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let rv = PropertyDeclarationBlock::with_one(uncomputed_value, Importance::Normal)
        .single_value_to_css(
            &get_property_id_from_nscsspropertyid!(property, ()),
            buffer,
            None,
            None, /* No extra custom properties */
            &data.stylist.device(),
        );
    debug_assert!(rv.is_ok());
}

/// Debug: MOZ_DBG for AnimationValue.
#[no_mangle]
pub extern "C" fn Servo_AnimationValue_Dump(
    value: &RawServoAnimationValue,
    result: &mut nsACString,
) {
    let value = AnimationValue::as_arc(&value);
    write!(result, "{:?}", value).unwrap();
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_GetColor(
    value: &RawServoAnimationValue,
    foreground_color: structs::nscolor,
) -> structs::nscolor {
    use style::gecko::values::convert_nscolor_to_rgba;
    use style::gecko::values::convert_rgba_to_nscolor;
    use style::values::animated::ToAnimatedValue;
    use style::values::computed::color::Color as ComputedColor;

    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::BackgroundColor(ref color) => {
            let computed: ComputedColor = ToAnimatedValue::from_animated_value(color.clone());
            let foreground_color = convert_nscolor_to_rgba(foreground_color);
            convert_rgba_to_nscolor(&computed.into_rgba(foreground_color))
        },
        _ => panic!("Other color properties are not supported yet"),
    }
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_IsCurrentColor(value: &RawServoAnimationValue) -> bool {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::BackgroundColor(ref color) => color.is_currentcolor(),
        _ => {
            debug_assert!(false, "Other color properties are not supported yet");
            false
        },
    }
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_GetOpacity(value: &RawServoAnimationValue) -> f32 {
    let value = AnimationValue::as_arc(&value);
    if let AnimationValue::Opacity(opacity) = **value {
        opacity
    } else {
        panic!("The AnimationValue should be Opacity");
    }
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_Opacity(opacity: f32) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::Opacity(opacity)).into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_Color(
    color_property: nsCSSPropertyID,
    color: structs::nscolor,
) -> Strong<RawServoAnimationValue> {
    use style::gecko::values::convert_nscolor_to_rgba;
    use style::values::animated::color::Color;

    let property = LonghandId::from_nscsspropertyid(color_property)
        .expect("We don't have shorthand property animation value");

    let rgba = convert_nscolor_to_rgba(color);

    let animated = AnimatedRGBA::new(
        rgba.red_f32(),
        rgba.green_f32(),
        rgba.blue_f32(),
        rgba.alpha_f32(),
    );
    match property {
        LonghandId::BackgroundColor => {
            Arc::new(AnimationValue::BackgroundColor(Color::rgba(animated))).into_strong()
        },
        _ => panic!("Should be background-color property"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_GetScale(
    value: &RawServoAnimationValue,
) -> *const computed::Scale {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::Scale(ref value) => value,
        _ => unreachable!("Expected scale"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_GetTranslate(
    value: &RawServoAnimationValue,
) -> *const computed::Translate {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::Translate(ref value) => value,
        _ => unreachable!("Expected translate"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_GetRotate(
    value: &RawServoAnimationValue,
) -> *const computed::Rotate {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::Rotate(ref value) => value,
        _ => unreachable!("Expected rotate"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_GetTransform(
    value: &RawServoAnimationValue,
) -> *const computed::Transform {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::Transform(ref value) => value,
        _ => unreachable!("Unsupported transform animation value"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_GetOffsetPath(
    value: &RawServoAnimationValue,
) -> *const computed::motion::OffsetPath {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::OffsetPath(ref value) => value,
        _ => unreachable!("Expected offset-path"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_GetOffsetDistance(
    value: &RawServoAnimationValue,
) -> *const computed::LengthPercentage {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::OffsetDistance(ref value) => value,
        _ => unreachable!("Expected offset-distance"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_GetOffsetRotate(
    value: &RawServoAnimationValue,
) -> *const computed::motion::OffsetRotate {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::OffsetRotate(ref value) => value,
        _ => unreachable!("Expected offset-rotate"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_GetOffsetAnchor(
    value: &RawServoAnimationValue,
) -> *const computed::position::PositionOrAuto {
    let value = AnimationValue::as_arc(&value);
    match **value {
        AnimationValue::OffsetAnchor(ref value) => value,
        _ => unreachable!("Expected offset-anchor"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_Rotate(
    r: &computed::Rotate,
) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::Rotate(r.clone())).into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_Translate(
    t: &computed::Translate,
) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::Translate(t.clone())).into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_Scale(
    s: &computed::Scale,
) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::Scale(s.clone())).into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_Transform(
    transform: &computed::Transform,
) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::Transform(transform.clone())).into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_OffsetPath(
    p: &computed::motion::OffsetPath,
) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::OffsetPath(p.clone())).into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_OffsetDistance(
    d: &computed::length::LengthPercentage,
) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::OffsetDistance(d.clone())).into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_OffsetRotate(
    r: &computed::motion::OffsetRotate,
) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::OffsetRotate(*r)).into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValue_OffsetAnchor(
    p: &computed::position::PositionOrAuto,
) -> Strong<RawServoAnimationValue> {
    Arc::new(AnimationValue::OffsetAnchor(p.clone())).into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_DeepEqual(
    this: &RawServoAnimationValue,
    other: &RawServoAnimationValue,
) -> bool {
    let this_value = AnimationValue::as_arc(&this);
    let other_value = AnimationValue::as_arc(&other);
    this_value == other_value
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_Uncompute(
    value: &RawServoAnimationValue,
) -> Strong<RawServoDeclarationBlock> {
    let value = AnimationValue::as_arc(&value);
    let global_style_data = &*GLOBAL_STYLE_DATA;
    Arc::new(
        global_style_data
            .shared_lock
            .wrap(PropertyDeclarationBlock::with_one(
                value.uncompute(),
                Importance::Normal,
            )),
    )
    .into_strong()
}

#[inline]
fn create_byte_buf_from_vec(mut v: Vec<u8>) -> ByteBuf {
    let w = ByteBuf {
        mData: v.as_mut_ptr(),
        mLen: v.len(),
        mCapacity: v.capacity(),
    };
    std::mem::forget(v);
    w
}

#[inline]
fn view_byte_buf(b: &ByteBuf) -> &[u8] {
    if b.mData.is_null() {
        debug_assert_eq!(b.mCapacity, 0);
        return &[];
    }
    unsafe { std::slice::from_raw_parts(b.mData, b.mLen) }
}

macro_rules! impl_basic_serde_funcs {
    ($ser_name:ident, $de_name:ident, $computed_type:ty) => {
        #[no_mangle]
        pub extern "C" fn $ser_name(v: &$computed_type, output: &mut ByteBuf) -> bool {
            let buf = match serialize(v) {
                Ok(buf) => buf,
                Err(..) => return false,
            };

            *output = create_byte_buf_from_vec(buf);
            true
        }

        #[no_mangle]
        pub extern "C" fn $de_name(input: &ByteBuf, v: &mut $computed_type) -> bool {
            let buf = match deserialize(view_byte_buf(input)) {
                Ok(buf) => buf,
                Err(..) => return false,
            };

            *v = buf;
            true
        }
    };
}

impl_basic_serde_funcs!(
    Servo_LengthPercentage_Serialize,
    Servo_LengthPercentage_Deserialize,
    computed::LengthPercentage
);

impl_basic_serde_funcs!(
    Servo_StyleRotate_Serialize,
    Servo_StyleRotate_Deserialize,
    computed::transform::Rotate
);

impl_basic_serde_funcs!(
    Servo_StyleScale_Serialize,
    Servo_StyleScale_Deserialize,
    computed::transform::Scale
);

impl_basic_serde_funcs!(
    Servo_StyleTranslate_Serialize,
    Servo_StyleTranslate_Deserialize,
    computed::transform::Translate
);

impl_basic_serde_funcs!(
    Servo_StyleTransform_Serialize,
    Servo_StyleTransform_Deserialize,
    computed::transform::Transform
);

impl_basic_serde_funcs!(
    Servo_StyleOffsetPath_Serialize,
    Servo_StyleOffsetPath_Deserialize,
    computed::motion::OffsetPath
);

impl_basic_serde_funcs!(
    Servo_StyleOffsetRotate_Serialize,
    Servo_StyleOffsetRotate_Deserialize,
    computed::motion::OffsetRotate
);

impl_basic_serde_funcs!(
    Servo_StylePositionOrAuto_Serialize,
    Servo_StylePositionOrAuto_Deserialize,
    computed::position::PositionOrAuto
);

#[no_mangle]
pub extern "C" fn Servo_SVGPathData_Normalize(
    input: &specified::SVGPathData,
    output: &mut specified::SVGPathData,
) {
    *output = input.normalize();
}

// Return the ComputedValues by a base ComputedValues and the rules.
fn resolve_rules_for_element_with_context<'a>(
    element: GeckoElement<'a>,
    mut context: StyleContext<GeckoElement<'a>>,
    rules: StrongRuleNode,
) -> Arc<ComputedValues> {
    use style::style_resolver::{PseudoElementResolution, StyleResolverForElement};

    // This currently ignores visited styles, which seems acceptable, as
    // existing browsers don't appear to animate visited styles.
    let inputs = CascadeInputs {
        rules: Some(rules),
        visited_rules: None,
    };

    // Actually `PseudoElementResolution` doesn't matter.
    let mut resolver = StyleResolverForElement::new(
        element,
        &mut context,
        RuleInclusion::All,
        PseudoElementResolution::IfApplicable,
    );
    resolver
        .cascade_style_and_visited_with_default_parents(inputs)
        .0
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValueMap_Create() -> Owned<structs::RawServoAnimationValueMap> {
    Box::<AnimationValueMap>::default().into_ffi()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AnimationValueMap_Drop(
    value_map: *mut structs::RawServoAnimationValueMap,
) {
    AnimationValueMap::drop_ffi(value_map)
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValueMap_GetValue(
    raw_value_map: &mut structs::RawServoAnimationValueMap,
    property_id: nsCSSPropertyID,
) -> Strong<RawServoAnimationValue> {
    let property = match LonghandId::from_nscsspropertyid(property_id) {
        Ok(longhand) => longhand,
        Err(()) => return Strong::null(),
    };
    let value_map = AnimationValueMap::from_ffi_mut(raw_value_map);

    value_map.get(&property).map_or(Strong::null(), |value| {
        Arc::new(value.clone()).into_strong()
    })
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_GetBaseComputedValuesForElement(
    raw_style_set: &RawServoStyleSet,
    element: &RawGeckoElement,
    computed_values: &ComputedValues,
    snapshots: *const ServoElementSnapshotTable,
) -> Strong<ComputedValues> {
    debug_assert!(!snapshots.is_null());
    let computed_values = unsafe { ArcBorrow::from_ref(computed_values) };

    let rules = match computed_values.rules {
        None => return computed_values.clone_arc().into(),
        Some(ref rules) => rules,
    };

    let doc_data = PerDocumentStyleData::from_ffi(raw_style_set).borrow();
    let without_animations_rules = doc_data.stylist.rule_tree().remove_animation_rules(rules);
    if without_animations_rules == *rules {
        return computed_values.clone_arc().into();
    }

    let element = GeckoElement(element);

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let shared = create_shared_context(
        &global_style_data,
        &guard,
        &doc_data.stylist,
        TraversalFlags::empty(),
        unsafe { &*snapshots },
    );
    let mut tlc = ThreadLocalStyleContext::new(&shared);
    let context = StyleContext {
        shared: &shared,
        thread_local: &mut tlc,
    };

    resolve_rules_for_element_with_context(element, context, without_animations_rules).into()
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_GetComputedValuesByAddingAnimation(
    raw_style_set: &RawServoStyleSet,
    element: &RawGeckoElement,
    computed_values: &ComputedValues,
    snapshots: *const ServoElementSnapshotTable,
    animation_value: &RawServoAnimationValue,
) -> Strong<ComputedValues> {
    debug_assert!(!snapshots.is_null());
    let rules = match computed_values.rules {
        None => return Strong::null(),
        Some(ref rules) => rules,
    };

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let uncomputed_value = AnimationValue::as_arc(&animation_value).uncompute();
    let doc_data = PerDocumentStyleData::from_ffi(raw_style_set).borrow();

    let with_animations_rules = {
        let guards = StylesheetGuards::same(&guard);
        let declarations = Arc::new(global_style_data.shared_lock.wrap(
            PropertyDeclarationBlock::with_one(uncomputed_value, Importance::Normal),
        ));
        doc_data
            .stylist
            .rule_tree()
            .add_animation_rules_at_transition_level(rules, declarations, &guards)
    };

    let element = GeckoElement(element);
    if element.borrow_data().is_none() {
        return Strong::null();
    }

    let shared = create_shared_context(
        &global_style_data,
        &guard,
        &doc_data.stylist,
        TraversalFlags::empty(),
        unsafe { &*snapshots },
    );
    let mut tlc: ThreadLocalStyleContext<GeckoElement> = ThreadLocalStyleContext::new(&shared);
    let context = StyleContext {
        shared: &shared,
        thread_local: &mut tlc,
    };

    resolve_rules_for_element_with_context(element, context, with_animations_rules).into()
}

#[no_mangle]
pub extern "C" fn Servo_ComputedValues_ExtractAnimationValue(
    computed_values: &ComputedValues,
    property_id: nsCSSPropertyID,
) -> Strong<RawServoAnimationValue> {
    let property = match LonghandId::from_nscsspropertyid(property_id) {
        Ok(longhand) => longhand,
        Err(()) => return Strong::null(),
    };
    match AnimationValue::from_computed_values(property, &computed_values) {
        Some(v) => Arc::new(v).into_strong(),
        None => Strong::null(),
    }
}

#[no_mangle]
pub extern "C" fn Servo_ResolveLogicalProperty(
    property_id: nsCSSPropertyID,
    style: &ComputedValues,
) -> nsCSSPropertyID {
    let longhand = LonghandId::from_nscsspropertyid(property_id)
        .expect("We shouldn't need to care about shorthands");

    longhand
        .to_physical(style.writing_mode)
        .to_nscsspropertyid()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_Property_LookupEnabledForAllContent(
    prop: &nsACString,
) -> nsCSSPropertyID {
    match PropertyId::parse_enabled_for_all_content(prop.as_str_unchecked()) {
        Ok(p) => p.to_nscsspropertyid_resolving_aliases(),
        Err(..) => nsCSSPropertyID::eCSSProperty_UNKNOWN,
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_Property_GetName(
    prop: nsCSSPropertyID,
    out_length: *mut u32,
) -> *const u8 {
    let (ptr, len) = match NonCustomPropertyId::from_nscsspropertyid(prop) {
        Ok(p) => {
            let name = p.name();
            (name.as_bytes().as_ptr(), name.len())
        },
        Err(..) => (ptr::null(), 0),
    };

    *out_length = len as u32;
    ptr
}

macro_rules! parse_enabled_property_name {
    ($prop_name:ident, $found:ident, $default:expr) => {{
        let prop_name = $prop_name.as_str_unchecked();
        match PropertyId::parse_enabled_for_all_content(prop_name) {
            Ok(p) => {
                *$found = true;
                p
            },
            Err(..) => {
                *$found = false;
                return $default;
            },
        }
    }};
}

#[no_mangle]
pub unsafe extern "C" fn Servo_Property_IsShorthand(
    prop_name: &nsACString,
    found: *mut bool,
) -> bool {
    let prop_id = parse_enabled_property_name!(prop_name, found, false);
    prop_id.is_shorthand()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_Property_IsInherited(prop_name: &nsACString) -> bool {
    let prop_name = prop_name.as_str_unchecked();
    let prop_id = match PropertyId::parse_enabled_for_all_content(prop_name) {
        Ok(id) => id,
        Err(_) => return false,
    };
    let longhand_id = match prop_id {
        PropertyId::Custom(_) => return true,
        PropertyId::Longhand(id) | PropertyId::LonghandAlias(id, _) => id,
        PropertyId::Shorthand(id) | PropertyId::ShorthandAlias(id, _) => {
            id.longhands().next().unwrap()
        },
    };
    longhand_id.inherited()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_Property_SupportsType(
    prop_name: &nsACString,
    ty: u8,
    found: *mut bool,
) -> bool {
    let prop_id = parse_enabled_property_name!(prop_name, found, false);
    prop_id.supports_type(ty)
}

// TODO(emilio): We could use ThinVec instead of nsTArray.
#[no_mangle]
pub unsafe extern "C" fn Servo_Property_GetCSSValuesForProperty(
    prop_name: &nsACString,
    found: *mut bool,
    result: &mut nsTArray<nsString>,
) {
    let prop_id = parse_enabled_property_name!(prop_name, found, ());
    // Use B-tree set for unique and sorted result.
    let mut values = BTreeSet::<&'static str>::new();
    prop_id.collect_property_completion_keywords(&mut |list| values.extend(list.iter()));

    let mut extras = vec![];
    if values.contains("transparent") {
        // This is a special value devtools use to avoid inserting the
        // long list of color keywords. We need to prepend it to values.
        extras.push("COLOR");
    }

    let len = extras.len() + values.len();
    bindings::Gecko_ResizeTArrayForStrings(result, len as u32);

    for (src, dest) in extras.iter().chain(values.iter()).zip(result.iter_mut()) {
        dest.write_str(src).unwrap();
    }
}

#[no_mangle]
pub extern "C" fn Servo_Property_IsAnimatable(prop: nsCSSPropertyID) -> bool {
    NonCustomPropertyId::from_nscsspropertyid(prop)
        .ok()
        .map_or(false, |p| p.is_animatable())
}

#[no_mangle]
pub extern "C" fn Servo_Property_IsTransitionable(prop: nsCSSPropertyID) -> bool {
    NonCustomPropertyId::from_nscsspropertyid(prop)
        .ok()
        .map_or(false, |p| p.is_transitionable())
}

#[no_mangle]
pub extern "C" fn Servo_Property_IsDiscreteAnimatable(property: nsCSSPropertyID) -> bool {
    match LonghandId::from_nscsspropertyid(property) {
        Ok(longhand) => longhand.is_discrete_animatable(),
        Err(()) => return false,
    }
}

#[no_mangle]
pub extern "C" fn Servo_Element_ClearData(element: &RawGeckoElement) {
    unsafe { GeckoElement(element).clear_data() };
}

#[no_mangle]
pub extern "C" fn Servo_Element_SizeOfExcludingThisAndCVs(
    malloc_size_of: GeckoMallocSizeOf,
    malloc_enclosing_size_of: GeckoMallocSizeOf,
    seen_ptrs: *mut SeenPtrs,
    element: &RawGeckoElement,
) -> usize {
    let element = GeckoElement(element);
    let borrow = element.borrow_data();
    if let Some(data) = borrow {
        let have_seen_ptr = move |ptr| unsafe { Gecko_HaveSeenPtr(seen_ptrs, ptr) };
        let mut ops = MallocSizeOfOps::new(
            malloc_size_of.unwrap(),
            Some(malloc_enclosing_size_of.unwrap()),
            Some(Box::new(have_seen_ptr)),
        );
        (*data).size_of_excluding_cvs(&mut ops)
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn Servo_Element_GetMaybeOutOfDateStyle(
    element: &RawGeckoElement,
) -> *const ComputedValues {
    let element = GeckoElement(element);
    let data = match element.borrow_data() {
        Some(d) => d,
        None => return ptr::null(),
    };
    &**data.styles.primary() as *const _
}

#[no_mangle]
pub extern "C" fn Servo_Element_GetMaybeOutOfDatePseudoStyle(
    element: &RawGeckoElement,
    index: usize,
) -> *const ComputedValues {
    let element = GeckoElement(element);
    let data = match element.borrow_data() {
        Some(d) => d,
        None => return ptr::null(),
    };
    match data.styles.pseudos.as_array()[index].as_ref() {
        Some(style) => &**style as *const _,
        None => ptr::null(),
    }
}

#[no_mangle]
pub extern "C" fn Servo_Element_IsDisplayNone(element: &RawGeckoElement) -> bool {
    let element = GeckoElement(element);
    let data = element
        .get_data()
        .expect("Invoking Servo_Element_IsDisplayNone on unstyled element");

    // This function is hot, so we bypass the AtomicRefCell.
    //
    // It would be nice to also assert that we're not in the servo traversal,
    // but this function is called at various intermediate checkpoints when
    // managing the traversal on the Gecko side.
    debug_assert!(is_main_thread());
    unsafe { &*data.as_ptr() }.styles.is_display_none()
}

#[no_mangle]
pub extern "C" fn Servo_Element_IsDisplayContents(element: &RawGeckoElement) -> bool {
    let element = GeckoElement(element);
    let data = element
        .get_data()
        .expect("Invoking Servo_Element_IsDisplayContents on unstyled element");

    debug_assert!(is_main_thread());
    unsafe { &*data.as_ptr() }
        .styles
        .primary()
        .get_box()
        .clone_display()
        .is_contents()
}

#[no_mangle]
pub extern "C" fn Servo_Element_IsPrimaryStyleReusedViaRuleNode(element: &RawGeckoElement) -> bool {
    let element = GeckoElement(element);
    let data = element
        .borrow_data()
        .expect("Invoking Servo_Element_IsPrimaryStyleReusedViaRuleNode on unstyled element");
    data.flags
        .contains(data::ElementDataFlags::PRIMARY_STYLE_REUSED_VIA_RULE_NODE)
}

fn mode_to_origin(mode: SheetParsingMode) -> Origin {
    match mode {
        SheetParsingMode::eAuthorSheetFeatures => Origin::Author,
        SheetParsingMode::eUserSheetFeatures => Origin::User,
        SheetParsingMode::eAgentSheetFeatures => Origin::UserAgent,
    }
}

#[no_mangle]
pub extern "C" fn Servo_StyleSheet_Empty(
    mode: SheetParsingMode,
) -> Strong<RawServoStyleSheetContents> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let origin = mode_to_origin(mode);
    let shared_lock = &global_style_data.shared_lock;
    StylesheetContents::from_str(
        "",
        unsafe { dummy_url_data() }.clone(),
        origin,
        shared_lock,
        /* loader = */ None,
        None,
        QuirksMode::NoQuirks,
        0,
        /* use_counters = */ None,
        AllowImportRules::Yes,
        /* sanitization_data = */ None,
    )
    .into_strong()
}

/// Note: The load_data corresponds to this sheet, and is passed as the parent
/// load data for child sheet loads. It may be null for certain cases where we
/// know we won't have child loads.
#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSheet_FromUTF8Bytes(
    loader: *mut Loader,
    stylesheet: *mut DomStyleSheet,
    load_data: *mut SheetLoadData,
    bytes: &nsACString,
    mode: SheetParsingMode,
    extra_data: *mut URLExtraData,
    line_number_offset: u32,
    quirks_mode: nsCompatibility,
    reusable_sheets: *mut LoaderReusableStyleSheets,
    use_counters: Option<&UseCounters>,
    allow_import_rules: AllowImportRules,
    sanitization_kind: SanitizationKind,
    sanitized_output: Option<&mut nsAString>,
) -> Strong<RawServoStyleSheetContents> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let input = bytes.as_str_unchecked();

    let reporter = ErrorReporter::new(stylesheet, loader, extra_data);
    let url_data = UrlExtraData::from_ptr_ref(&extra_data);
    let loader = if loader.is_null() {
        None
    } else {
        debug_assert!(
            sanitized_output.is_none(),
            "Shouldn't trigger @import loads for sanitization",
        );
        Some(StylesheetLoader::new(
            loader,
            stylesheet,
            load_data,
            reusable_sheets,
        ))
    };

    // FIXME(emilio): loader.as_ref() doesn't typecheck for some reason?
    let loader: Option<&dyn StyleStylesheetLoader> = match loader {
        None => None,
        Some(ref s) => Some(s),
    };

    let mut sanitization_data = SanitizationData::new(sanitization_kind);

    let contents = StylesheetContents::from_str(
        input,
        url_data.clone(),
        mode_to_origin(mode),
        &global_style_data.shared_lock,
        loader,
        reporter.as_ref().map(|r| r as &dyn ParseErrorReporter),
        quirks_mode.into(),
        line_number_offset,
        use_counters,
        allow_import_rules,
        sanitization_data.as_mut(),
    );

    if let Some(data) = sanitization_data {
        sanitized_output
            .unwrap()
            .assign_utf8(data.take().as_bytes());
    }

    contents.into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSheet_FromUTF8BytesAsync(
    load_data: *mut SheetLoadDataHolder,
    extra_data: *mut URLExtraData,
    bytes: &nsACString,
    mode: SheetParsingMode,
    line_number_offset: u32,
    quirks_mode: nsCompatibility,
    should_record_use_counters: bool,
    allow_import_rules: AllowImportRules,
) {
    let load_data = RefPtr::new(load_data);
    let extra_data = UrlExtraData::new(extra_data);

    let mut sheet_bytes = nsCString::new();
    sheet_bytes.assign(bytes);

    let async_parser = AsyncStylesheetParser::new(
        load_data,
        extra_data,
        sheet_bytes,
        mode_to_origin(mode),
        quirks_mode.into(),
        line_number_offset,
        should_record_use_counters,
        allow_import_rules,
    );

    if let Some(thread_pool) = STYLE_THREAD_POOL.pool().as_ref() {
        thread_pool.spawn(|| {
            gecko_profiler_label!(Layout, CSSParsing);
            async_parser.parse();
        });
    } else {
        async_parser.parse();
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_ShutdownThreadPool() {
    debug_assert!(is_main_thread() && !is_in_servo_traversal());
    StyleThreadPool::shutdown();
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSheet_FromSharedData(
    extra_data: *mut URLExtraData,
    shared_rules: &ServoCssRules,
) -> Strong<RawServoStyleSheetContents> {
    let shared_rules = Locked::<CssRules>::as_arc(&shared_rules);
    StylesheetContents::from_shared_data(
        shared_rules.clone_arc(),
        Origin::UserAgent,
        UrlExtraData::new(extra_data),
        QuirksMode::NoQuirks,
    )
    .into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_AppendStyleSheet(
    raw_data: &RawServoStyleSet,
    sheet: *const DomStyleSheet,
) {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let mut data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    let data = &mut *data;
    let guard = global_style_data.shared_lock.read();
    let sheet = unsafe { GeckoStyleSheet::new(sheet) };
    data.stylist.append_stylesheet(sheet, &guard);
}

#[no_mangle]
pub extern "C" fn Servo_AuthorStyles_Create() -> Owned<RawServoAuthorStyles> {
    Box::new(AuthorStyles::<GeckoStyleSheet>::new()).into_ffi()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AuthorStyles_Drop(styles: *mut RawServoAuthorStyles) {
    AuthorStyles::drop_ffi(styles)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AuthorStyles_AppendStyleSheet(
    styles: &mut RawServoAuthorStyles,
    sheet: *const DomStyleSheet,
) {
    let styles = AuthorStyles::<GeckoStyleSheet>::from_ffi_mut(styles);

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let sheet = GeckoStyleSheet::new(sheet);
    styles.stylesheets.append_stylesheet(None, sheet, &guard);
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AuthorStyles_InsertStyleSheetBefore(
    styles: &mut RawServoAuthorStyles,
    sheet: *const DomStyleSheet,
    before_sheet: *const DomStyleSheet,
) {
    let styles = AuthorStyles::<GeckoStyleSheet>::from_ffi_mut(styles);

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    styles.stylesheets.insert_stylesheet_before(
        None,
        GeckoStyleSheet::new(sheet),
        GeckoStyleSheet::new(before_sheet),
        &guard,
    );
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AuthorStyles_RemoveStyleSheet(
    styles: &mut RawServoAuthorStyles,
    sheet: *const DomStyleSheet,
) {
    let styles = AuthorStyles::<GeckoStyleSheet>::from_ffi_mut(styles);

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    styles
        .stylesheets
        .remove_stylesheet(None, GeckoStyleSheet::new(sheet), &guard);
}

#[no_mangle]
pub extern "C" fn Servo_AuthorStyles_ForceDirty(styles: &mut RawServoAuthorStyles) {
    let styles = AuthorStyles::<GeckoStyleSheet>::from_ffi_mut(styles);
    styles.stylesheets.force_dirty();
}

#[no_mangle]
pub extern "C" fn Servo_AuthorStyles_IsDirty(styles: &RawServoAuthorStyles) -> bool {
    let styles = AuthorStyles::<GeckoStyleSheet>::from_ffi(styles);
    styles.stylesheets.dirty()
}

#[no_mangle]
pub extern "C" fn Servo_AuthorStyles_Flush(
    styles: &mut RawServoAuthorStyles,
    document_set: &RawServoStyleSet,
) {
    let styles = AuthorStyles::<GeckoStyleSheet>::from_ffi_mut(styles);
    // Try to avoid the atomic borrow below if possible.
    if !styles.stylesheets.dirty() {
        return;
    }

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();

    let mut document_data = PerDocumentStyleData::from_ffi(document_set).borrow_mut();

    // TODO(emilio): This is going to need an element or something to do proper
    // invalidation in Shadow roots.
    styles.flush::<GeckoElement>(&mut document_data.stylist, &guard);
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_RemoveUniqueEntriesFromAuthorStylesCache(
    document_set: &RawServoStyleSet,
) {
    let mut document_data = PerDocumentStyleData::from_ffi(document_set).borrow_mut();
    document_data
        .stylist
        .remove_unique_author_data_cache_entries();
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_SizeOfIncludingThis(
    malloc_size_of: GeckoMallocSizeOf,
    malloc_enclosing_size_of: GeckoMallocSizeOf,
    declarations: &RawServoDeclarationBlock,
) -> usize {
    use malloc_size_of::MallocSizeOf;
    use malloc_size_of::MallocUnconditionalShallowSizeOf;

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();

    let mut ops = MallocSizeOfOps::new(
        malloc_size_of.unwrap(),
        Some(malloc_enclosing_size_of.unwrap()),
        None,
    );

    Locked::<PropertyDeclarationBlock>::as_arc(&declarations).with_arc(|declarations| {
        let mut n = 0;
        n += declarations.unconditional_shallow_size_of(&mut ops);
        n += declarations.read_with(&guard).size_of(&mut ops);
        n
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_AuthorStyles_SizeOfIncludingThis(
    malloc_size_of: GeckoMallocSizeOf,
    malloc_enclosing_size_of: GeckoMallocSizeOf,
    styles: &RawServoAuthorStyles,
) -> usize {
    // We cannot `use` MallocSizeOf at the top level, otherwise the compiler
    // would complain in `Servo_StyleSheet_SizeOfIncludingThis` for `size_of`
    // there.
    use malloc_size_of::MallocSizeOf;
    let malloc_size_of = malloc_size_of.unwrap();
    let malloc_size_of_this =
        malloc_size_of(styles as *const RawServoAuthorStyles as *const c_void);

    let styles = AuthorStyles::<GeckoStyleSheet>::from_ffi(styles);
    let mut ops = MallocSizeOfOps::new(
        malloc_size_of,
        Some(malloc_enclosing_size_of.unwrap()),
        None,
    );
    malloc_size_of_this + styles.size_of(&mut ops)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_MediumFeaturesChanged(
    document_set: &RawServoStyleSet,
    non_document_styles: &mut nsTArray<&mut RawServoAuthorStyles>,
    may_affect_default_style: bool,
) -> structs::MediumFeaturesChangedResult {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();

    // NOTE(emilio): We don't actually need to flush the stylist here and ensure
    // it's up to date.
    //
    // In case it isn't we would trigger a rebuild + restyle as needed too.
    //
    // We need to ensure the default computed values are up to date though,
    // because those can influence the result of media query evaluation.
    let mut document_data = PerDocumentStyleData::from_ffi(document_set).borrow_mut();

    if may_affect_default_style {
        document_data.stylist.device_mut().reset_computed_values();
    }
    let guards = StylesheetGuards::same(&guard);

    let origins_in_which_rules_changed = document_data
        .stylist
        .media_features_change_changed_style(&guards, document_data.stylist.device());

    let affects_document_rules = !origins_in_which_rules_changed.is_empty();
    if affects_document_rules {
        document_data
            .stylist
            .force_stylesheet_origins_dirty(origins_in_which_rules_changed);
    }

    let mut affects_non_document_rules = false;
    for author_styles in &mut **non_document_styles {
        let author_styles = AuthorStyles::<GeckoStyleSheet>::from_ffi_mut(&mut *author_styles);
        let affected_style = author_styles.stylesheets.iter().any(|sheet| {
            !author_styles.data.media_feature_affected_matches(
                sheet,
                &guards.author,
                document_data.stylist.device(),
                document_data.stylist.quirks_mode(),
            )
        });
        if affected_style {
            affects_non_document_rules = true;
            author_styles.stylesheets.force_dirty();
        }
    }

    structs::MediumFeaturesChangedResult {
        mAffectsDocumentRules: affects_document_rules,
        mAffectsNonDocumentRules: affects_non_document_rules,
    }
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_InsertStyleSheetBefore(
    raw_data: &RawServoStyleSet,
    sheet: *const DomStyleSheet,
    before_sheet: *const DomStyleSheet,
) {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let mut data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    let data = &mut *data;
    let guard = global_style_data.shared_lock.read();
    let sheet = unsafe { GeckoStyleSheet::new(sheet) };
    data.stylist.insert_stylesheet_before(
        sheet,
        unsafe { GeckoStyleSheet::new(before_sheet) },
        &guard,
    );
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_RemoveStyleSheet(
    raw_data: &RawServoStyleSet,
    sheet: *const DomStyleSheet,
) {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let mut data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    let data = &mut *data;
    let guard = global_style_data.shared_lock.read();
    let sheet = unsafe { GeckoStyleSheet::new(sheet) };
    data.stylist.remove_stylesheet(sheet, &guard);
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_GetSheetAt(
    raw_data: &RawServoStyleSet,
    origin: Origin,
    index: usize,
) -> *const DomStyleSheet {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    data.stylist
        .sheet_at(origin, index)
        .map_or(ptr::null(), |s| s.raw())
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_GetSheetCount(
    raw_data: &RawServoStyleSet,
    origin: Origin,
) -> usize {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    data.stylist.sheet_count(origin)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_FlushStyleSheets(
    raw_data: &RawServoStyleSet,
    doc_element: Option<&RawGeckoElement>,
    snapshots: *const ServoElementSnapshotTable,
) {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let mut data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    let doc_element = doc_element.map(GeckoElement);

    let have_invalidations = data.flush_stylesheets(&guard, doc_element, snapshots.as_ref());

    if have_invalidations && doc_element.is_some() {
        // The invalidation machinery propagates the bits up, but we still need
        // to tell the Gecko restyle root machinery about it.
        bindings::Gecko_NoteDirtySubtreeForInvalidation(doc_element.unwrap().0);
    }
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_NoteStyleSheetsChanged(
    raw_data: &RawServoStyleSet,
    changed_origins: OriginFlags,
) {
    let mut data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    data.stylist
        .force_stylesheet_origins_dirty(OriginSet::from(changed_origins));
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_SetAuthorStyleDisabled(
    raw_data: &RawServoStyleSet,
    author_style_disabled: bool,
) {
    let mut data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    let enabled = if author_style_disabled {
        AuthorStylesEnabled::No
    } else {
        AuthorStylesEnabled::Yes
    };
    data.stylist.set_author_styles_enabled(enabled);
}

#[no_mangle]
pub extern "C" fn Servo_StyleSheet_HasRules(raw_contents: &RawServoStyleSheetContents) -> bool {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    !StylesheetContents::as_arc(&raw_contents)
        .rules
        .read_with(&guard)
        .0
        .is_empty()
}

#[no_mangle]
pub extern "C" fn Servo_StyleSheet_GetRules(
    sheet: &RawServoStyleSheetContents,
) -> Strong<ServoCssRules> {
    StylesheetContents::as_arc(&sheet)
        .rules
        .clone()
        .into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_StyleSheet_Clone(
    raw_sheet: &RawServoStyleSheetContents,
    reference_sheet: *const DomStyleSheet,
) -> Strong<RawServoStyleSheetContents> {
    use style::shared_lock::{DeepCloneParams, DeepCloneWithLock};
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let contents = StylesheetContents::as_arc(&raw_sheet);
    let params = DeepCloneParams { reference_sheet };

    Arc::new(contents.deep_clone_with_lock(&global_style_data.shared_lock, &guard, &params))
        .into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_StyleSheet_SizeOfIncludingThis(
    malloc_size_of: GeckoMallocSizeOf,
    malloc_enclosing_size_of: GeckoMallocSizeOf,
    sheet: &RawServoStyleSheetContents,
) -> usize {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let mut ops = MallocSizeOfOps::new(
        malloc_size_of.unwrap(),
        Some(malloc_enclosing_size_of.unwrap()),
        None,
    );
    // TODO(emilio): We're not measuring the size of the Arc<StyleSheetContents>
    // allocation itself here.
    StylesheetContents::as_arc(&sheet).size_of(&guard, &mut ops)
}

#[no_mangle]
pub extern "C" fn Servo_StyleSheet_GetOrigin(sheet: &RawServoStyleSheetContents) -> Origin {
    StylesheetContents::as_arc(&sheet).origin
}

#[no_mangle]
pub extern "C" fn Servo_StyleSheet_GetSourceMapURL(
    sheet: &RawServoStyleSheetContents,
    result: &mut nsAString,
) {
    let contents = StylesheetContents::as_arc(&sheet);
    let url_opt = contents.source_map_url.read();
    if let Some(ref url) = *url_opt {
        write!(result, "{}", url).unwrap();
    }
}

#[no_mangle]
pub extern "C" fn Servo_StyleSheet_GetSourceURL(
    sheet: &RawServoStyleSheetContents,
    result: &mut nsAString,
) {
    let contents = StylesheetContents::as_arc(&sheet);
    let url_opt = contents.source_url.read();
    if let Some(ref url) = *url_opt {
        write!(result, "{}", url).unwrap();
    }
}

fn read_locked_arc<T, R, F>(raw: &<Locked<T> as HasFFI>::FFIType, func: F) -> R
where
    Locked<T>: HasArcFFI,
    F: FnOnce(&T) -> R,
{
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    func(Locked::<T>::as_arc(&raw).read_with(&guard))
}

#[cfg(debug_assertions)]
unsafe fn read_locked_arc_unchecked<T, R, F>(raw: &<Locked<T> as HasFFI>::FFIType, func: F) -> R
where
    Locked<T>: HasArcFFI,
    F: FnOnce(&T) -> R,
{
    debug_assert!(is_main_thread() && !is_in_servo_traversal());
    read_locked_arc(raw, func)
}

#[cfg(not(debug_assertions))]
unsafe fn read_locked_arc_unchecked<T, R, F>(raw: &<Locked<T> as HasFFI>::FFIType, func: F) -> R
where
    Locked<T>: HasArcFFI,
    F: FnOnce(&T) -> R,
{
    func(Locked::<T>::as_arc(&raw).read_unchecked())
}

fn write_locked_arc<T, R, F>(raw: &<Locked<T> as HasFFI>::FFIType, func: F) -> R
where
    Locked<T>: HasArcFFI,
    F: FnOnce(&mut T) -> R,
{
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let mut guard = global_style_data.shared_lock.write();
    func(Locked::<T>::as_arc(&raw).write_with(&mut guard))
}

#[no_mangle]
pub extern "C" fn Servo_CssRules_ListTypes(rules: &ServoCssRules, result: &mut nsTArray<usize>) {
    read_locked_arc(rules, |rules: &CssRules| {
        result.assign_from_iter_pod(rules.0.iter().map(|rule| rule.rule_type() as usize));
    })
}

#[no_mangle]
pub extern "C" fn Servo_CssRules_InsertRule(
    rules: &ServoCssRules,
    contents: &RawServoStyleSheetContents,
    rule: &nsACString,
    index: u32,
    nested: bool,
    loader: *mut Loader,
    allow_import_rules: AllowImportRules,
    gecko_stylesheet: *mut DomStyleSheet,
    rule_type: &mut CssRuleType,
) -> nsresult {
    let loader = if loader.is_null() {
        None
    } else {
        Some(StylesheetLoader::new(
            loader,
            gecko_stylesheet,
            ptr::null_mut(),
            ptr::null_mut(),
        ))
    };
    let loader = loader
        .as_ref()
        .map(|loader| loader as &dyn StyleStylesheetLoader);
    let rule = unsafe { rule.as_str_unchecked() };

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let contents = StylesheetContents::as_arc(&contents);
    let result = Locked::<CssRules>::as_arc(&rules).insert_rule(
        &global_style_data.shared_lock,
        rule,
        contents,
        index as usize,
        nested,
        loader,
        allow_import_rules,
    );

    match result {
        Ok(new_rule) => {
            *rule_type = new_rule.rule_type();
            nsresult::NS_OK
        },
        Err(err) => err.into(),
    }
}

#[no_mangle]
pub extern "C" fn Servo_CssRules_DeleteRule(rules: &ServoCssRules, index: u32) -> nsresult {
    write_locked_arc(rules, |rules: &mut CssRules| {
        match rules.remove_rule(index as usize) {
            Ok(_) => nsresult::NS_OK,
            Err(err) => err.into(),
        }
    })
}

macro_rules! impl_basic_rule_funcs_without_getter {
    { ($rule_type:ty, $raw_type:ty),
        debug: $debug:ident,
        to_css: $to_css:ident,
    } => {
        #[cfg(debug_assertions)]
        #[no_mangle]
        pub extern "C" fn $debug(rule: &$raw_type, result: *mut nsACString) {
            read_locked_arc(rule, |rule: &$rule_type| {
                write!(unsafe { result.as_mut().unwrap() }, "{:?}", *rule).unwrap();
            })
        }

        #[cfg(not(debug_assertions))]
        #[no_mangle]
        pub extern "C" fn $debug(_: &$raw_type, _: *mut nsACString) {
            unreachable!()
        }

        #[no_mangle]
        pub extern "C" fn $to_css(rule: &$raw_type, result: &mut nsACString) {
            let global_style_data = &*GLOBAL_STYLE_DATA;
            let guard = global_style_data.shared_lock.read();
            let rule = Locked::<$rule_type>::as_arc(&rule);
            rule.read_with(&guard).to_css(&guard, result).unwrap();
        }
    }
}

macro_rules! impl_basic_rule_funcs {
    { ($name:ident, $rule_type:ty, $raw_type:ty),
        getter: $getter:ident,
        debug: $debug:ident,
        to_css: $to_css:ident,
        changed: $changed:ident,
    } => {
        #[no_mangle]
        pub extern "C" fn $getter(
            rules: &ServoCssRules,
            index: u32,
            line: *mut u32,
            column: *mut u32,
        ) -> Strong<$raw_type> {
            let global_style_data = &*GLOBAL_STYLE_DATA;
            let guard = global_style_data.shared_lock.read();
            let rules = Locked::<CssRules>::as_arc(&rules).read_with(&guard);
            let index = index as usize;

            if index >= rules.0.len() {
                return Strong::null();
            }

            match rules.0[index] {
                CssRule::$name(ref rule) => {
                    let location = rule.read_with(&guard).source_location;
                    *unsafe { line.as_mut().unwrap() } = location.line as u32;
                    *unsafe { column.as_mut().unwrap() } = location.column as u32;
                    rule.clone().into_strong()
                },
                _ => {
                    Strong::null()
                }
            }
        }

        #[no_mangle]
        pub extern "C" fn $changed(
            styleset: &RawServoStyleSet,
            rule: &$raw_type,
            sheet: &DomStyleSheet,
            change_kind: RuleChangeKind,
        ) {
            let mut data = PerDocumentStyleData::from_ffi(styleset).borrow_mut();
            let data = &mut *data;
            let global_style_data = &*GLOBAL_STYLE_DATA;
            let guard = global_style_data.shared_lock.read();
            // TODO(emilio): Would be nice not to deal with refcount bumps here,
            // but it's probably not a huge deal.
            let rule = Locked::<$rule_type>::as_arc(&rule);
            let rule = CssRule::$name(rule.clone_arc());
            let sheet = unsafe { GeckoStyleSheet::new(sheet) };
            data.stylist.rule_changed(&sheet, &rule, &guard, change_kind);
        }

        impl_basic_rule_funcs_without_getter! { ($rule_type, $raw_type),
            debug: $debug,
            to_css: $to_css,
        }
    }
}

macro_rules! impl_group_rule_funcs {
    { ($name:ident, $rule_type:ty, $raw_type:ty),
      get_rules: $get_rules:ident,
      $($basic:tt)+
    } => {
        impl_basic_rule_funcs! { ($name, $rule_type, $raw_type), $($basic)+ }

        #[no_mangle]
        pub extern "C" fn $get_rules(rule: &$raw_type) -> Strong<ServoCssRules> {
            read_locked_arc(rule, |rule: &$rule_type| {
                rule.rules.clone().into_strong()
            })
        }
    }
}

impl_basic_rule_funcs! { (Style, StyleRule, RawServoStyleRule),
    getter: Servo_CssRules_GetStyleRuleAt,
    debug: Servo_StyleRule_Debug,
    to_css: Servo_StyleRule_GetCssText,
    changed: Servo_StyleSet_StyleRuleChanged,
}

impl_basic_rule_funcs! { (Import, ImportRule, RawServoImportRule),
    getter: Servo_CssRules_GetImportRuleAt,
    debug: Servo_ImportRule_Debug,
    to_css: Servo_ImportRule_GetCssText,
    changed: Servo_StyleSet_ImportRuleChanged,
}

impl_basic_rule_funcs_without_getter! { (Keyframe, RawServoKeyframe),
    debug: Servo_Keyframe_Debug,
    to_css: Servo_Keyframe_GetCssText,
}

impl_basic_rule_funcs! { (Keyframes, KeyframesRule, RawServoKeyframesRule),
    getter: Servo_CssRules_GetKeyframesRuleAt,
    debug: Servo_KeyframesRule_Debug,
    to_css: Servo_KeyframesRule_GetCssText,
    changed: Servo_StyleSet_KeyframesRuleChanged,
}

impl_group_rule_funcs! { (Media, MediaRule, RawServoMediaRule),
    get_rules: Servo_MediaRule_GetRules,
    getter: Servo_CssRules_GetMediaRuleAt,
    debug: Servo_MediaRule_Debug,
    to_css: Servo_MediaRule_GetCssText,
    changed: Servo_StyleSet_MediaRuleChanged,
}

impl_basic_rule_funcs! { (Namespace, NamespaceRule, RawServoNamespaceRule),
    getter: Servo_CssRules_GetNamespaceRuleAt,
    debug: Servo_NamespaceRule_Debug,
    to_css: Servo_NamespaceRule_GetCssText,
    changed: Servo_StyleSet_NamespaceRuleChanged,
}

impl_basic_rule_funcs! { (Page, PageRule, RawServoPageRule),
    getter: Servo_CssRules_GetPageRuleAt,
    debug: Servo_PageRule_Debug,
    to_css: Servo_PageRule_GetCssText,
    changed: Servo_StyleSet_PageRuleChanged,
}

impl_basic_rule_funcs! { (ScrollTimeline, ScrollTimelineRule, RawServoScrollTimelineRule),
    getter: Servo_CssRules_GetScrollTimelineRuleAt,
    debug: Servo_ScrollTimelineRule_Debug,
    to_css: Servo_ScrollTimelineRule_GetCssText,
    changed: Servo_StyleSet_ScrollTimelineRuleChanged,
}

impl_group_rule_funcs! { (Supports, SupportsRule, RawServoSupportsRule),
    get_rules: Servo_SupportsRule_GetRules,
    getter: Servo_CssRules_GetSupportsRuleAt,
    debug: Servo_SupportsRule_Debug,
    to_css: Servo_SupportsRule_GetCssText,
    changed: Servo_StyleSet_SupportsRuleChanged,
}

impl_group_rule_funcs! { (Container, ContainerRule, RawServoContainerRule),
    get_rules: Servo_ContainerRule_GetRules,
    getter: Servo_CssRules_GetContainerRuleAt,
    debug: Servo_ContainerRule_Debug,
    to_css: Servo_ContainerRule_GetCssText,
    changed: Servo_StyleSet_ContainerRuleChanged,
}

impl_group_rule_funcs! { (LayerBlock, LayerBlockRule, RawServoLayerBlockRule),
    get_rules: Servo_LayerBlockRule_GetRules,
    getter: Servo_CssRules_GetLayerBlockRuleAt,
    debug: Servo_LayerBlockRule_Debug,
    to_css: Servo_LayerBlockRule_GetCssText,
    changed: Servo_StyleSet_LayerBlockRuleChanged,
}

impl_basic_rule_funcs! { (LayerStatement, LayerStatementRule, RawServoLayerStatementRule),
    getter: Servo_CssRules_GetLayerStatementRuleAt,
    debug: Servo_LayerStatementRule_Debug,
    to_css: Servo_LayerStatementRule_GetCssText,
    changed: Servo_StyleSet_LayerStatementRuleChanged,
}

impl_group_rule_funcs! { (Document, DocumentRule, RawServoMozDocumentRule),
    get_rules: Servo_MozDocumentRule_GetRules,
    getter: Servo_CssRules_GetMozDocumentRuleAt,
    debug: Servo_MozDocumentRule_Debug,
    to_css: Servo_MozDocumentRule_GetCssText,
    changed: Servo_StyleSet_MozDocumentRuleChanged,
}

impl_basic_rule_funcs! { (FontFeatureValues, FontFeatureValuesRule, RawServoFontFeatureValuesRule),
    getter: Servo_CssRules_GetFontFeatureValuesRuleAt,
    debug: Servo_FontFeatureValuesRule_Debug,
    to_css: Servo_FontFeatureValuesRule_GetCssText,
    changed: Servo_StyleSet_FontFeatureValuesRuleChanged,
}

impl_basic_rule_funcs! { (FontFace, FontFaceRule, RawServoFontFaceRule),
    getter: Servo_CssRules_GetFontFaceRuleAt,
    debug: Servo_FontFaceRule_Debug,
    to_css: Servo_FontFaceRule_GetCssText,
    changed: Servo_StyleSet_FontFaceRuleChanged,
}

impl_basic_rule_funcs! { (CounterStyle, CounterStyleRule, RawServoCounterStyleRule),
    getter: Servo_CssRules_GetCounterStyleRuleAt,
    debug: Servo_CounterStyleRule_Debug,
    to_css: Servo_CounterStyleRule_GetCssText,
    changed: Servo_StyleSet_CounterStyleRuleChanged,
}

#[no_mangle]
pub extern "C" fn Servo_StyleRule_GetStyle(
    rule: &RawServoStyleRule,
) -> Strong<RawServoDeclarationBlock> {
    read_locked_arc(rule, |rule: &StyleRule| rule.block.clone().into_strong())
}

#[no_mangle]
pub extern "C" fn Servo_StyleRule_SetStyle(
    rule: &RawServoStyleRule,
    declarations: &RawServoDeclarationBlock,
) {
    let declarations = Locked::<PropertyDeclarationBlock>::as_arc(&declarations);
    write_locked_arc(rule, |rule: &mut StyleRule| {
        rule.block = declarations.clone_arc();
    })
}

#[no_mangle]
pub extern "C" fn Servo_StyleRule_GetSelectorText(
    rule: &RawServoStyleRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &StyleRule| {
        rule.selectors.to_css(result).unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_StyleRule_GetSelectorTextAtIndex(
    rule: &RawServoStyleRule,
    index: u32,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &StyleRule| {
        let index = index as usize;
        if index >= rule.selectors.0.len() {
            return;
        }
        rule.selectors.0[index].to_css(result).unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_StyleRule_GetSelectorCount(rule: &RawServoStyleRule, count: *mut u32) {
    read_locked_arc(rule, |rule: &StyleRule| {
        *unsafe { count.as_mut().unwrap() } = rule.selectors.0.len() as u32;
    })
}

#[no_mangle]
pub extern "C" fn Servo_StyleRule_GetSpecificityAtIndex(
    rule: &RawServoStyleRule,
    index: u32,
    specificity: *mut u64,
) {
    read_locked_arc(rule, |rule: &StyleRule| {
        let specificity = unsafe { specificity.as_mut().unwrap() };
        let index = index as usize;
        if index >= rule.selectors.0.len() {
            *specificity = 0;
            return;
        }
        *specificity = rule.selectors.0[index].specificity() as u64;
    })
}

#[no_mangle]
pub extern "C" fn Servo_StyleRule_SelectorMatchesElement(
    rule: &RawServoStyleRule,
    element: &RawGeckoElement,
    index: u32,
    pseudo_type: PseudoStyleType,
    relevant_link_visited: bool,
) -> bool {
    use selectors::matching::{matches_selector, MatchingContext, MatchingMode, VisitedHandlingMode, NeedsSelectorFlags};
    read_locked_arc(rule, |rule: &StyleRule| {
        let index = index as usize;
        if index >= rule.selectors.0.len() {
            return false;
        }

        let selector = &rule.selectors.0[index];
        let mut matching_mode = MatchingMode::Normal;

        match PseudoElement::from_pseudo_type(pseudo_type) {
            Some(pseudo) => {
                // We need to make sure that the requested pseudo element type
                // matches the selector pseudo element type before proceeding.
                match selector.pseudo_element() {
                    Some(selector_pseudo) if *selector_pseudo == pseudo => {
                        matching_mode = MatchingMode::ForStatelessPseudoElement
                    },
                    _ => return false,
                };
            },
            None => {
                // Do not attempt to match if a pseudo element is requested and
                // this is not a pseudo element selector, or vice versa.
                if selector.has_pseudo_element() {
                    return false;
                }
            },
        };

        let element = GeckoElement(element);
        let quirks_mode = element.as_node().owner_doc().quirks_mode();
        let visited_mode = if relevant_link_visited {
            VisitedHandlingMode::RelevantLinkVisited
        } else {
            VisitedHandlingMode::AllLinksUnvisited
        };
        let mut ctx = MatchingContext::new_for_visited(
            matching_mode,
            None,
            None,
            visited_mode,
            quirks_mode,
            NeedsSelectorFlags::No,
        );
        matches_selector(selector, 0, None, &element, &mut ctx)
    })
}

#[no_mangle]
pub extern "C" fn Servo_StyleRule_SetSelectorText(
    sheet: &RawServoStyleSheetContents,
    rule: &RawServoStyleRule,
    text: &nsACString,
) -> bool {
    let value_str = unsafe { text.as_str_unchecked() };

    write_locked_arc(rule, |rule: &mut StyleRule| {
        use style::selector_parser::SelectorParser;

        let contents = StylesheetContents::as_arc(&sheet);
        let namespaces = contents.namespaces.read();
        let url_data = contents.url_data.read();
        let parser = SelectorParser {
            stylesheet_origin: contents.origin,
            namespaces: &namespaces,
            url_data: &url_data,
        };

        let mut parser_input = ParserInput::new(&value_str);
        match SelectorList::parse(&parser, &mut Parser::new(&mut parser_input)) {
            Ok(selectors) => {
                rule.selectors = selectors;
                true
            },
            Err(_) => false,
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SelectorList_Closest(
    element: &RawGeckoElement,
    selectors: &RawServoSelectorList,
) -> *const RawGeckoElement {
    use std::borrow::Borrow;
    use style::dom_apis;

    let element = GeckoElement(element);
    let quirks_mode = element.as_node().owner_doc().quirks_mode();
    let selectors = ::selectors::SelectorList::from_ffi(selectors).borrow();

    dom_apis::element_closest(element, &selectors, quirks_mode).map_or(ptr::null(), |e| e.0)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SelectorList_Matches(
    element: &RawGeckoElement,
    selectors: &RawServoSelectorList,
) -> bool {
    use std::borrow::Borrow;
    use style::dom_apis;

    let element = GeckoElement(element);
    let quirks_mode = element.as_node().owner_doc().quirks_mode();
    let selectors = ::selectors::SelectorList::from_ffi(selectors).borrow();
    dom_apis::element_matches(&element, &selectors, quirks_mode)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SelectorList_QueryFirst(
    node: &RawGeckoNode,
    selectors: &RawServoSelectorList,
    may_use_invalidation: bool,
) -> *const RawGeckoElement {
    use std::borrow::Borrow;
    use style::dom_apis::{self, MayUseInvalidation, QueryFirst};

    let node = GeckoNode(node);
    let selectors = ::selectors::SelectorList::from_ffi(selectors).borrow();
    let mut result = None;

    let may_use_invalidation = if may_use_invalidation {
        MayUseInvalidation::Yes
    } else {
        MayUseInvalidation::No
    };

    dom_apis::query_selector::<GeckoElement, QueryFirst>(
        node,
        &selectors,
        &mut result,
        may_use_invalidation,
    );

    result.map_or(ptr::null(), |e| e.0)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SelectorList_QueryAll(
    node: &RawGeckoNode,
    selectors: &RawServoSelectorList,
    content_list: *mut structs::nsSimpleContentList,
    may_use_invalidation: bool,
) {
    use std::borrow::Borrow;
    use style::dom_apis::{self, MayUseInvalidation, QueryAll};

    let node = GeckoNode(node);
    let selectors = ::selectors::SelectorList::from_ffi(selectors).borrow();
    let mut result = SmallVec::new();

    let may_use_invalidation = if may_use_invalidation {
        MayUseInvalidation::Yes
    } else {
        MayUseInvalidation::No
    };

    dom_apis::query_selector::<GeckoElement, QueryAll>(
        node,
        &selectors,
        &mut result,
        may_use_invalidation,
    );

    if !result.is_empty() {
        // NOTE(emilio): This relies on a slice of GeckoElement having the same
        // memory representation than a slice of element pointers.
        bindings::Gecko_ContentList_AppendAll(
            content_list,
            result.as_ptr() as *mut *const _,
            result.len(),
        )
    }
}

#[no_mangle]
pub extern "C" fn Servo_ImportRule_GetHref(rule: &RawServoImportRule, result: &mut nsAString) {
    read_locked_arc(rule, |rule: &ImportRule| {
        write!(result, "{}", rule.url.as_str()).unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_ImportRule_GetLayerName(
    rule: &RawServoImportRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &ImportRule| match rule.layer {
        Some(ref layer) => layer.name.to_css(&mut CssWriter::new(result)).unwrap(),
        None => result.set_is_void(true),
    })
}

#[no_mangle]
pub extern "C" fn Servo_ImportRule_GetSheet(rule: &RawServoImportRule) -> *const DomStyleSheet {
    read_locked_arc(rule, |rule: &ImportRule| {
        rule.stylesheet.as_sheet().unwrap().raw() as *const DomStyleSheet
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_ImportRule_SetSheet(
    rule: &RawServoImportRule,
    sheet: *mut DomStyleSheet,
) {
    write_locked_arc(rule, |rule: &mut ImportRule| {
        rule.stylesheet = ImportSheet::new(GeckoStyleSheet::new(sheet));
    })
}

#[no_mangle]
pub extern "C" fn Servo_Keyframe_GetKeyText(keyframe: &RawServoKeyframe, result: &mut nsACString) {
    read_locked_arc(keyframe, |keyframe: &Keyframe| {
        keyframe
            .selector
            .to_css(&mut CssWriter::new(result))
            .unwrap()
    })
}

#[no_mangle]
pub extern "C" fn Servo_Keyframe_SetKeyText(
    keyframe: &RawServoKeyframe,
    text: &nsACString,
) -> bool {
    let text = unsafe { text.as_str_unchecked() };
    let mut input = ParserInput::new(&text);
    if let Ok(selector) = Parser::new(&mut input).parse_entirely(KeyframeSelector::parse) {
        write_locked_arc(keyframe, |keyframe: &mut Keyframe| {
            keyframe.selector = selector;
        });
        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn Servo_Keyframe_GetStyle(
    keyframe: &RawServoKeyframe,
) -> Strong<RawServoDeclarationBlock> {
    read_locked_arc(keyframe, |keyframe: &Keyframe| {
        keyframe.block.clone().into_strong()
    })
}

#[no_mangle]
pub extern "C" fn Servo_Keyframe_SetStyle(
    keyframe: &RawServoKeyframe,
    declarations: &RawServoDeclarationBlock,
) {
    let declarations = Locked::<PropertyDeclarationBlock>::as_arc(&declarations);
    write_locked_arc(keyframe, |keyframe: &mut Keyframe| {
        keyframe.block = declarations.clone_arc();
    })
}

#[no_mangle]
pub extern "C" fn Servo_KeyframesRule_GetName(rule: &RawServoKeyframesRule) -> *mut nsAtom {
    read_locked_arc(rule, |rule: &KeyframesRule| rule.name.as_atom().as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn Servo_KeyframesRule_SetName(
    rule: &RawServoKeyframesRule,
    name: *mut nsAtom,
) {
    write_locked_arc(rule, |rule: &mut KeyframesRule| {
        rule.name = KeyframesName::from_atom(Atom::from_addrefed(name));
    })
}

#[no_mangle]
pub extern "C" fn Servo_KeyframesRule_GetCount(rule: &RawServoKeyframesRule) -> u32 {
    read_locked_arc(rule, |rule: &KeyframesRule| rule.keyframes.len() as u32)
}

#[no_mangle]
pub extern "C" fn Servo_KeyframesRule_GetKeyframeAt(
    rule: &RawServoKeyframesRule,
    index: u32,
    line: *mut u32,
    column: *mut u32,
) -> Strong<RawServoKeyframe> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let key = Locked::<KeyframesRule>::as_arc(&rule)
        .read_with(&guard)
        .keyframes[index as usize]
        .clone();
    let location = key.read_with(&guard).source_location;
    *unsafe { line.as_mut().unwrap() } = location.line as u32;
    *unsafe { column.as_mut().unwrap() } = location.column as u32;
    key.into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_KeyframesRule_FindRule(
    rule: &RawServoKeyframesRule,
    key: &nsACString,
) -> u32 {
    let key = unsafe { key.as_str_unchecked() };
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    Locked::<KeyframesRule>::as_arc(&rule)
        .read_with(&guard)
        .find_rule(&guard, key)
        .map(|index| index as u32)
        .unwrap_or(u32::max_value())
}

#[no_mangle]
pub extern "C" fn Servo_KeyframesRule_AppendRule(
    rule: &RawServoKeyframesRule,
    contents: &RawServoStyleSheetContents,
    css: &nsACString,
) -> bool {
    let css = unsafe { css.as_str_unchecked() };
    let contents = StylesheetContents::as_arc(&contents);
    let global_style_data = &*GLOBAL_STYLE_DATA;

    match Keyframe::parse(css, &contents, &global_style_data.shared_lock) {
        Ok(keyframe) => {
            write_locked_arc(rule, |rule: &mut KeyframesRule| {
                rule.keyframes.push(keyframe);
            });
            true
        },
        Err(..) => false,
    }
}

#[no_mangle]
pub extern "C" fn Servo_KeyframesRule_DeleteRule(rule: &RawServoKeyframesRule, index: u32) {
    write_locked_arc(rule, |rule: &mut KeyframesRule| {
        rule.keyframes.remove(index as usize);
    })
}

#[no_mangle]
pub extern "C" fn Servo_ScrollTimelineRule_GetName(
    rule: &RawServoScrollTimelineRule,
) -> *mut nsAtom {
    read_locked_arc(rule, |rule: &ScrollTimelineRule| {
        rule.name.as_atom().as_ptr()
    })
}

#[no_mangle]
pub extern "C" fn Servo_ScrollTimelineRule_GetSource(
    rule: &RawServoScrollTimelineRule,
    result: &mut nsString,
) {
    read_locked_arc(rule, |rule: &ScrollTimelineRule| {
        rule.descriptors
            .source
            .as_ref()
            .unwrap_or(&Default::default())
            .to_css(&mut CssWriter::new(result))
            .unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_ScrollTimelineRule_GetOrientationAsString(
    rule: &RawServoScrollTimelineRule,
    result: &mut nsString,
) {
    read_locked_arc(rule, |rule: &ScrollTimelineRule| {
        rule.descriptors
            .orientation
            .unwrap_or_default()
            .to_css(&mut CssWriter::new(result))
            .unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_ScrollTimelineRule_GetOrientation(
    rule: &RawServoScrollTimelineRule,
) -> ScrollDirection {
    read_locked_arc(rule, |rule: &ScrollTimelineRule| {
        rule.descriptors.orientation.unwrap_or_default()
    })
}

#[no_mangle]
pub extern "C" fn Servo_ScrollTimelineRule_GetScrollOffsets(
    rule: &RawServoScrollTimelineRule,
    result: &mut nsString,
) {
    read_locked_arc(rule, |rule: &ScrollTimelineRule| {
        rule.descriptors
            .offsets
            .as_ref()
            .unwrap_or(&Default::default())
            .to_css(&mut CssWriter::new(result))
            .unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_MediaRule_GetMedia(rule: &RawServoMediaRule) -> Strong<RawServoMediaList> {
    read_locked_arc(rule, |rule: &MediaRule| {
        rule.media_queries.clone().into_strong()
    })
}

#[no_mangle]
pub extern "C" fn Servo_NamespaceRule_GetPrefix(rule: &RawServoNamespaceRule) -> *mut nsAtom {
    read_locked_arc(rule, |rule: &NamespaceRule| {
        rule.prefix
            .as_ref()
            .map_or(atom!("").as_ptr(), |a| a.as_ptr())
    })
}

#[no_mangle]
pub extern "C" fn Servo_NamespaceRule_GetURI(rule: &RawServoNamespaceRule) -> *mut nsAtom {
    read_locked_arc(rule, |rule: &NamespaceRule| rule.url.0.as_ptr())
}

#[no_mangle]
pub extern "C" fn Servo_PageRule_GetStyle(
    rule: &RawServoPageRule,
) -> Strong<RawServoDeclarationBlock> {
    read_locked_arc(rule, |rule: &PageRule| rule.block.clone().into_strong())
}

#[no_mangle]
pub extern "C" fn Servo_PageRule_SetStyle(
    rule: &RawServoPageRule,
    declarations: &RawServoDeclarationBlock,
) {
    let declarations = Locked::<PropertyDeclarationBlock>::as_arc(&declarations);
    write_locked_arc(rule, |rule: &mut PageRule| {
        rule.block = declarations.clone_arc();
    })
}

#[no_mangle]
pub extern "C" fn Servo_PageRule_GetSelectorText(
    rule: &RawServoPageRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &PageRule| {
        rule.selectors.to_css(&mut CssWriter::new(result)).unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_PageRule_SetSelectorText(
    sheet: &RawServoStyleSheetContents,
    rule: &RawServoPageRule,
    text: &nsACString,
) -> bool {

    let value_str = unsafe { text.as_str_unchecked() };

    write_locked_arc(rule, |rule: &mut PageRule| {
        use style::stylesheets::PageSelectors;

        let mut parser_input = ParserInput::new(&value_str);
        let mut parser = Parser::new(&mut parser_input);

        // Ensure that a blank input results in empty page selectors
        if parser.is_exhausted() {
            rule.selectors = PageSelectors::default();
            return true;
        }

        let contents = StylesheetContents::as_arc(&sheet);
        let url_data = contents.url_data.read();
        let context = ParserContext::new(
            Origin::Author,
            &url_data,
            None,
            ParsingMode::DEFAULT,
            QuirksMode::NoQuirks,
            None,
            None,
        );

        match parser.parse_entirely(|i| PageSelectors::parse(&context, i)) {
            Ok(selectors) => {
                rule.selectors = selectors;
                true
            },
            Err(_) => false,
        }
    })
}


#[no_mangle]
pub extern "C" fn Servo_SupportsRule_GetConditionText(
    rule: &RawServoSupportsRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &SupportsRule| {
        rule.condition.to_css(&mut CssWriter::new(result)).unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_ContainerRule_GetConditionText(
    rule: &RawServoContainerRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &ContainerRule| {
        rule.query_condition().to_css(&mut CssWriter::new(result)).unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_MozDocumentRule_GetConditionText(
    rule: &RawServoMozDocumentRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &DocumentRule| {
        rule.condition.to_css(&mut CssWriter::new(result)).unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_FontFeatureValuesRule_GetFontFamily(
    rule: &RawServoFontFeatureValuesRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &FontFeatureValuesRule| {
        rule.font_family_to_css(&mut CssWriter::new(result))
            .unwrap()
    })
}

#[no_mangle]
pub extern "C" fn Servo_FontFeatureValuesRule_GetValueText(
    rule: &RawServoFontFeatureValuesRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &FontFeatureValuesRule| {
        rule.value_to_css(&mut CssWriter::new(result)).unwrap();
    })
}

#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_CreateEmpty() -> Strong<RawServoFontFaceRule> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    // XXX This is not great. We should split FontFace descriptor data
    // from the rule, so that we don't need to create the rule like this
    // and the descriptor data itself can be hold in UniquePtr from the
    // Gecko side. See bug 1450904.
    Arc::new(
        global_style_data
            .shared_lock
            .wrap(FontFaceRule::empty(SourceLocation { line: 0, column: 0 })),
    )
    .into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_Clone(
    rule: &RawServoFontFaceRule,
) -> Strong<RawServoFontFaceRule> {
    let clone = read_locked_arc(rule, |rule: &FontFaceRule| rule.clone());
    let global_style_data = &*GLOBAL_STYLE_DATA;
    Arc::new(global_style_data.shared_lock.wrap(clone)).into_strong()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_GetSourceLocation(
    rule: &RawServoFontFaceRule,
    line: *mut u32,
    column: *mut u32,
) {
    read_locked_arc(rule, |rule: &FontFaceRule| {
        let location = rule.source_location;
        *line.as_mut().unwrap() = location.line as u32;
        *column.as_mut().unwrap() = location.column as u32;
    });
}

macro_rules! apply_font_desc_list {
    ($apply_macro:ident) => {
        $apply_macro! {
            valid: [
                eCSSFontDesc_Family => family,
                eCSSFontDesc_Style => style,
                eCSSFontDesc_Weight => weight,
                eCSSFontDesc_Stretch => stretch,
                eCSSFontDesc_Src => sources,
                eCSSFontDesc_UnicodeRange => unicode_range,
                eCSSFontDesc_FontFeatureSettings => feature_settings,
                eCSSFontDesc_FontVariationSettings => variation_settings,
                eCSSFontDesc_FontLanguageOverride => language_override,
                eCSSFontDesc_Display => display,
                eCSSFontDesc_AscentOverride => ascent_override,
                eCSSFontDesc_DescentOverride => descent_override,
                eCSSFontDesc_LineGapOverride => line_gap_override,
                eCSSFontDesc_SizeAdjust => size_adjust,
            ]
            invalid: [
                eCSSFontDesc_UNKNOWN,
                eCSSFontDesc_COUNT,
            ]
        }
    };
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_Length(rule: &RawServoFontFaceRule) -> u32 {
    read_locked_arc(rule, |rule: &FontFaceRule| {
        let mut result = 0;
        macro_rules! count_values {
            (
                valid: [$($v_enum_name:ident => $field:ident,)*]
                invalid: [$($i_enum_name:ident,)*]
            ) => {
                $(if rule.$field.is_some() {
                    result += 1;
                })*
            }
        }
        apply_font_desc_list!(count_values);
        result
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_IndexGetter(
    rule: &RawServoFontFaceRule,
    index: u32,
) -> nsCSSFontDesc {
    read_locked_arc(rule, |rule: &FontFaceRule| {
        let mut count = 0;
        macro_rules! lookup_index {
            (
                valid: [$($v_enum_name:ident => $field:ident,)*]
                invalid: [$($i_enum_name:ident,)*]
            ) => {
                $(if rule.$field.is_some() {
                    count += 1;
                    if count - 1 == index {
                        return nsCSSFontDesc::$v_enum_name;
                    }
                })*
            }
        }
        apply_font_desc_list!(lookup_index);
        return nsCSSFontDesc::eCSSFontDesc_UNKNOWN;
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_GetDeclCssText(
    rule: &RawServoFontFaceRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &FontFaceRule| {
        rule.decl_to_css(result).unwrap();
    })
}

macro_rules! simple_font_descriptor_getter_impl {
    ($rule:ident, $out:ident, $field:ident, $compute:ident) => {
        read_locked_arc($rule, |rule: &FontFaceRule| {
            match rule.$field {
                None => return false,
                Some(ref f) => *$out = f.$compute(),
            }
            true
        })
    };
}

#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetFontWeight(
    rule: &RawServoFontFaceRule,
    out: &mut font_face::ComputedFontWeightRange,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, weight, compute)
}

#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetFontStretch(
    rule: &RawServoFontFaceRule,
    out: &mut font_face::ComputedFontStretchRange,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, stretch, compute)
}

#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetFontStyle(
    rule: &RawServoFontFaceRule,
    out: &mut font_face::ComputedFontStyleDescriptor,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, style, compute)
}

#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetFontDisplay(
    rule: &RawServoFontFaceRule,
    out: &mut font_face::FontDisplay,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, display, clone)
}

#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetFontLanguageOverride(
    rule: &RawServoFontFaceRule,
    out: &mut computed::FontLanguageOverride,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, language_override, compute_non_system)
}

// Returns a Percentage of -1.0 if the override descriptor is present but 'normal'
// rather than an actual percentage value.
#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetAscentOverride(
    rule: &RawServoFontFaceRule,
    out: &mut computed::Percentage,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, ascent_override, compute)
}

// Returns a Percentage of -1.0 if the override descriptor is present but 'normal'
// rather than an actual percentage value.
#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetDescentOverride(
    rule: &RawServoFontFaceRule,
    out: &mut computed::Percentage,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, descent_override, compute)
}

// Returns a Percentage of -1.0 if the override descriptor is present but 'normal'
// rather than an actual percentage value.
#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetLineGapOverride(
    rule: &RawServoFontFaceRule,
    out: &mut computed::Percentage,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, line_gap_override, compute)
}

#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetSizeAdjust(
    rule: &RawServoFontFaceRule,
    out: &mut computed::Percentage,
) -> bool {
    simple_font_descriptor_getter_impl!(rule, out, size_adjust, compute)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_GetFamilyName(
    rule: &RawServoFontFaceRule,
) -> *mut nsAtom {
    read_locked_arc(rule, |rule: &FontFaceRule| {
        // TODO(emilio): font-family is a mandatory descriptor, can't we unwrap
        // here, and remove the null-checks in Gecko?
        rule.family
            .as_ref()
            .map_or(ptr::null_mut(), |f| f.name.as_ptr())
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_GetUnicodeRanges(
    rule: &RawServoFontFaceRule,
    out_len: *mut usize,
) -> *const UnicodeRange {
    *out_len = 0;
    read_locked_arc(rule, |rule: &FontFaceRule| {
        let ranges = match rule.unicode_range {
            Some(ref ranges) => ranges,
            None => return ptr::null(),
        };
        *out_len = ranges.len();
        ranges.as_ptr() as *const _
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_GetSources(
    rule: &RawServoFontFaceRule,
    out: *mut nsTArray<FontFaceSourceListComponent>,
) {
    let out = &mut *out;
    read_locked_arc(rule, |rule: &FontFaceRule| {
        let sources = match rule.sources {
            Some(ref s) => s,
            None => return,
        };
        let len = sources.iter().fold(0, |acc, src| {
            acc + match *src {
                // Each format hint takes one position in the array of mSrc.
                Source::Url(ref url) => url.format_hints.len() + 1,
                Source::Local(_) => 1,
            }
        });

        out.set_len(len as u32);

        let mut iter = out.iter_mut();

        {
            let mut set_next = |component: FontFaceSourceListComponent| {
                *iter.next().expect("miscalculated length") = component;
            };

            for source in sources.iter() {
                match *source {
                    Source::Url(ref url) => {
                        set_next(FontFaceSourceListComponent::Url(&url.url));
                        for hint in url.format_hints.iter() {
                            set_next(FontFaceSourceListComponent::FormatHint {
                                length: hint.len(),
                                utf8_bytes: hint.as_ptr(),
                            });
                        }
                    },
                    Source::Local(ref name) => {
                        set_next(FontFaceSourceListComponent::Local(name.name.as_ptr()));
                    },
                }
            }
        }

        assert!(iter.next().is_none(), "miscalculated");
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_GetVariationSettings(
    rule: &RawServoFontFaceRule,
    variations: *mut nsTArray<structs::gfxFontVariation>,
) {
    read_locked_arc(rule, |rule: &FontFaceRule| {
        let source_variations = match rule.variation_settings {
            Some(ref v) => v,
            None => return,
        };

        (*variations).set_len(source_variations.0.len() as u32);
        for (target, source) in (*variations).iter_mut().zip(source_variations.0.iter()) {
            *target = structs::gfxFontVariation {
                mTag: source.tag.0,
                mValue: source.value.get(),
            };
        }
    });
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_GetFeatureSettings(
    rule: &RawServoFontFaceRule,
    features: *mut nsTArray<structs::gfxFontFeature>,
) {
    read_locked_arc(rule, |rule: &FontFaceRule| {
        let source_features = match rule.feature_settings {
            Some(ref v) => v,
            None => return,
        };

        (*features).set_len(source_features.0.len() as u32);
        for (target, source) in (*features).iter_mut().zip(source_features.0.iter()) {
            *target = structs::gfxFontFeature {
                mTag: source.tag.0,
                mValue: source.value.value() as u32,
            };
        }
    });
}

#[no_mangle]
pub extern "C" fn Servo_FontFaceRule_GetDescriptorCssText(
    rule: &RawServoFontFaceRule,
    desc: nsCSSFontDesc,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &FontFaceRule| {
        let mut writer = CssWriter::new(result);
        macro_rules! to_css_text {
            (
                valid: [$($v_enum_name:ident => $field:ident,)*]
                invalid: [$($i_enum_name:ident,)*]
            ) => {
                match desc {
                    $(
                        nsCSSFontDesc::$v_enum_name => {
                            if let Some(ref value) = rule.$field {
                                value.to_css(&mut writer).unwrap();
                            }
                        }
                    )*
                    $(
                        nsCSSFontDesc::$i_enum_name => {
                            debug_assert!(false, "not a valid font descriptor");
                        }
                    )*
                }
            }
        }
        apply_font_desc_list!(to_css_text)
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_SetDescriptor(
    rule: &RawServoFontFaceRule,
    desc: nsCSSFontDesc,
    value: &nsACString,
    data: *mut URLExtraData,
    out_changed: *mut bool,
) -> bool {
    let value = value.as_str_unchecked();
    let mut input = ParserInput::new(&value);
    let mut parser = Parser::new(&mut input);
    let url_data = UrlExtraData::from_ptr_ref(&data);
    let context = ParserContext::new(
        Origin::Author,
        url_data,
        Some(CssRuleType::FontFace),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );

    write_locked_arc(rule, |rule: &mut FontFaceRule| {
        macro_rules! to_css_text {
            (
                valid: [$($v_enum_name:ident => $field:ident,)*]
                invalid: [$($i_enum_name:ident,)*]
            ) => {
                match desc {
                    $(
                        nsCSSFontDesc::$v_enum_name => {
                            if let Ok(value) = parser.parse_entirely(|i| Parse::parse(&context, i)) {
                                let result = Some(value);
                                *out_changed = result != rule.$field;
                                rule.$field = result;
                                true
                            } else {
                                false
                            }
                        }
                    )*
                    $(
                        nsCSSFontDesc::$i_enum_name => {
                            debug_assert!(false, "not a valid font descriptor");
                            false
                        }
                    )*
                }
            }
        }
        apply_font_desc_list!(to_css_text)
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_FontFaceRule_ResetDescriptor(
    rule: &RawServoFontFaceRule,
    desc: nsCSSFontDesc,
) {
    write_locked_arc(rule, |rule: &mut FontFaceRule| {
        macro_rules! reset_desc {
            (
                valid: [$($v_enum_name:ident => $field:ident,)*]
                invalid: [$($i_enum_name:ident,)*]
            ) => {
                match desc {
                    $(nsCSSFontDesc::$v_enum_name => rule.$field = None,)*
                    $(nsCSSFontDesc::$i_enum_name => debug_assert!(false, "not a valid font descriptor"),)*
                }
            }
        }
        apply_font_desc_list!(reset_desc)
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetName(
    rule: &RawServoCounterStyleRule,
) -> *mut nsAtom {
    read_locked_arc(rule, |rule: &CounterStyleRule| rule.name().0.as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_SetName(
    rule: &RawServoCounterStyleRule,
    value: &nsACString,
) -> bool {
    let value = value.as_str_unchecked();
    let mut input = ParserInput::new(&value);
    let mut parser = Parser::new(&mut input);
    match parser.parse_entirely(counter_style::parse_counter_style_name_definition) {
        Ok(name) => {
            write_locked_arc(rule, |rule: &mut CounterStyleRule| rule.set_name(name));
            true
        },
        Err(_) => false,
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetGeneration(
    rule: &RawServoCounterStyleRule,
) -> u32 {
    read_locked_arc(rule, |rule: &CounterStyleRule| rule.generation())
}

fn symbol_to_string(s: &counter_style::Symbol) -> nsString {
    match *s {
        counter_style::Symbol::String(ref s) => nsString::from(&**s),
        counter_style::Symbol::Ident(ref i) => nsString::from(i.0.as_slice()),
    }
}

// TODO(emilio): Cbindgen could be used to simplify a bunch of code here.
#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetPad(
    rule: &RawServoCounterStyleRule,
    width: &mut i32,
    symbol: &mut nsString,
) -> bool {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        let pad = match rule.pad() {
            Some(pad) => pad,
            None => return false,
        };
        *width = pad.0.value();
        *symbol = symbol_to_string(&pad.1);
        true
    })
}

fn get_symbol(s: Option<&counter_style::Symbol>, out: &mut nsString) -> bool {
    let s = match s {
        Some(s) => s,
        None => return false,
    };
    *out = symbol_to_string(s);
    true
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetPrefix(
    rule: &RawServoCounterStyleRule,
    out: &mut nsString,
) -> bool {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        get_symbol(rule.prefix(), out)
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetSuffix(
    rule: &RawServoCounterStyleRule,
    out: &mut nsString,
) -> bool {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        get_symbol(rule.suffix(), out)
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetNegative(
    rule: &RawServoCounterStyleRule,
    prefix: &mut nsString,
    suffix: &mut nsString,
) -> bool {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        let negative = match rule.negative() {
            Some(n) => n,
            None => return false,
        };
        *prefix = symbol_to_string(&negative.0);
        *suffix = match negative.1 {
            Some(ref s) => symbol_to_string(s),
            None => nsString::new(),
        };
        true
    })
}

#[repr(u8)]
pub enum IsOrdinalInRange {
    Auto,
    InRange,
    NotInRange,
    NoOrdinalSpecified,
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_IsInRange(
    rule: &RawServoCounterStyleRule,
    ordinal: i32,
) -> IsOrdinalInRange {
    use style::counter_style::CounterBound;
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        let range = match rule.range() {
            Some(r) => r,
            None => return IsOrdinalInRange::NoOrdinalSpecified,
        };

        if range.0.is_empty() {
            return IsOrdinalInRange::Auto;
        }

        let in_range = range.0.iter().any(|r| {
            if let CounterBound::Integer(start) = r.start {
                if start.value() > ordinal {
                    return false;
                }
            }

            if let CounterBound::Integer(end) = r.end {
                if end.value() < ordinal {
                    return false;
                }
            }

            true
        });

        if in_range {
            IsOrdinalInRange::InRange
        } else {
            IsOrdinalInRange::NotInRange
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetSymbols(
    rule: &RawServoCounterStyleRule,
    symbols: &mut style::OwnedSlice<nsString>,
) {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        *symbols = match rule.symbols() {
            Some(s) => s.0.iter().map(symbol_to_string).collect(),
            None => style::OwnedSlice::default(),
        };
    })
}

#[repr(C)]
pub struct AdditiveSymbol {
    pub weight: i32,
    pub symbol: nsString,
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetAdditiveSymbols(
    rule: &RawServoCounterStyleRule,
    symbols: &mut style::OwnedSlice<AdditiveSymbol>,
) {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        *symbols = match rule.additive_symbols() {
            Some(s) => {
                s.0.iter()
                    .map(|s| AdditiveSymbol {
                        weight: s.weight.value(),
                        symbol: symbol_to_string(&s.symbol),
                    })
                    .collect()
            },
            None => style::OwnedSlice::default(),
        };
    })
}

#[repr(C, u8)]
pub enum CounterSpeakAs {
    None,
    Auto,
    Bullets,
    Numbers,
    Words,
    Ident(*mut nsAtom),
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetSpeakAs(
    rule: &RawServoCounterStyleRule,
    out: &mut CounterSpeakAs,
) {
    use style::counter_style::SpeakAs;
    *out = read_locked_arc(rule, |rule: &CounterStyleRule| {
        let speak_as = match rule.speak_as() {
            Some(s) => s,
            None => return CounterSpeakAs::None,
        };
        match *speak_as {
            SpeakAs::Auto => CounterSpeakAs::Auto,
            SpeakAs::Bullets => CounterSpeakAs::Bullets,
            SpeakAs::Numbers => CounterSpeakAs::Numbers,
            SpeakAs::Words => CounterSpeakAs::Words,
            SpeakAs::Other(ref other) => CounterSpeakAs::Ident(other.0.as_ptr()),
        }
    });
}

#[repr(u8)]
pub enum CounterSystem {
    Cyclic = 0,
    Numeric,
    Alphabetic,
    Symbolic,
    Additive,
    Fixed,
    Extends,
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetSystem(
    rule: &RawServoCounterStyleRule,
) -> CounterSystem {
    use style::counter_style::System;
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        match *rule.resolved_system() {
            System::Cyclic => CounterSystem::Cyclic,
            System::Numeric => CounterSystem::Numeric,
            System::Alphabetic => CounterSystem::Alphabetic,
            System::Symbolic => CounterSystem::Symbolic,
            System::Additive => CounterSystem::Additive,
            System::Fixed { .. } => CounterSystem::Fixed,
            System::Extends(_) => CounterSystem::Extends,
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetExtended(
    rule: &RawServoCounterStyleRule,
) -> *mut nsAtom {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        match *rule.resolved_system() {
            counter_style::System::Extends(ref name) => name.0.as_ptr(),
            _ => {
                debug_assert!(false, "Not extends system");
                ptr::null_mut()
            },
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetFixedFirstValue(
    rule: &RawServoCounterStyleRule,
) -> i32 {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        match *rule.resolved_system() {
            counter_style::System::Fixed { first_symbol_value } => {
                first_symbol_value.map_or(1, |v| v.value())
            },
            _ => {
                debug_assert!(false, "Not fixed system");
                0
            },
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CounterStyleRule_GetFallback(
    rule: &RawServoCounterStyleRule,
) -> *mut nsAtom {
    read_locked_arc(rule, |rule: &CounterStyleRule| {
        rule.fallback().map_or(ptr::null_mut(), |i| i.0 .0.as_ptr())
    })
}

macro_rules! counter_style_descriptors {
    {
        valid: [
            $($desc:ident => $getter:ident / $setter:ident,)+
        ]
        invalid: [
            $($i_desc:ident,)+
        ]
    } => {
        #[no_mangle]
        pub unsafe extern "C" fn Servo_CounterStyleRule_GetDescriptorCssText(
            rule: &RawServoCounterStyleRule,
            desc: nsCSSCounterDesc,
            result: &mut nsACString,
        ) {
            let mut writer = CssWriter::new(result);
            read_locked_arc(rule, |rule: &CounterStyleRule| {
                match desc {
                    $(nsCSSCounterDesc::$desc => {
                        if let Some(value) = rule.$getter() {
                            value.to_css(&mut writer).unwrap();
                        }
                    })+
                    $(nsCSSCounterDesc::$i_desc => unreachable!(),)+
                }
            });
        }

        #[no_mangle]
        pub unsafe extern "C" fn Servo_CounterStyleRule_SetDescriptor(
            rule: &RawServoCounterStyleRule,
            desc: nsCSSCounterDesc,
            value: &nsACString,
        ) -> bool {
            let value = value.as_str_unchecked();
            let mut input = ParserInput::new(&value);
            let mut parser = Parser::new(&mut input);
            let url_data = dummy_url_data();
            let context = ParserContext::new(
                Origin::Author,
                url_data,
                Some(CssRuleType::CounterStyle),
                ParsingMode::DEFAULT,
                QuirksMode::NoQuirks,
                None,
                None,
            );

            write_locked_arc(rule, |rule: &mut CounterStyleRule| {
                match desc {
                    $(nsCSSCounterDesc::$desc => {
                        match parser.parse_entirely(|i| Parse::parse(&context, i)) {
                            Ok(value) => rule.$setter(value),
                            Err(_) => false,
                        }
                    })+
                    $(nsCSSCounterDesc::$i_desc => unreachable!(),)+
                }
            })
        }
    }
}

counter_style_descriptors! {
    valid: [
        eCSSCounterDesc_System => system / set_system,
        eCSSCounterDesc_Symbols => symbols / set_symbols,
        eCSSCounterDesc_AdditiveSymbols => additive_symbols / set_additive_symbols,
        eCSSCounterDesc_Negative => negative / set_negative,
        eCSSCounterDesc_Prefix => prefix / set_prefix,
        eCSSCounterDesc_Suffix => suffix / set_suffix,
        eCSSCounterDesc_Range => range / set_range,
        eCSSCounterDesc_Pad => pad / set_pad,
        eCSSCounterDesc_Fallback => fallback / set_fallback,
        eCSSCounterDesc_SpeakAs => speak_as / set_speak_as,
    ]
    invalid: [
        eCSSCounterDesc_UNKNOWN,
        eCSSCounterDesc_COUNT,
    ]
}

#[no_mangle]
pub unsafe extern "C" fn Servo_ComputedValues_GetForAnonymousBox(
    parent_style_or_null: Option<&ComputedValues>,
    pseudo: PseudoStyleType,
    raw_data: &RawServoStyleSet,
) -> Strong<ComputedValues> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let guards = StylesheetGuards::same(&guard);
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    let pseudo = PseudoElement::from_pseudo_type(pseudo).unwrap();
    debug_assert!(pseudo.is_anon_box());

    let metrics = get_metrics_provider_for_product();

    // If the pseudo element is PageContent, we should append @page rules to the
    // precomputed pseudo.
    //
    // TODO(emilio): We'll need a separate code path or extra arguments for
    // named pages, etc.
    let mut extra_declarations = vec![];
    if pseudo == PseudoElement::PageContent {
        let iter = data.stylist.iter_extra_data_origins_rev();
        for (data, origin) in iter {
            let level = match origin {
                Origin::UserAgent => CascadeLevel::UANormal,
                Origin::User => CascadeLevel::UserNormal,
                Origin::Author => CascadeLevel::same_tree_author_normal(),
            };
            for &(ref rule, _layer_id) in data.pages.global.iter() {
                extra_declarations.push(ApplicableDeclarationBlock::from_declarations(
                    rule.0.read_with(level.guard(&guards)).block.clone(),
                    level,
                    LayerOrder::root(),
                ));
            }
        }
    }

    let rule_node =
        data.stylist
            .rule_node_for_precomputed_pseudo(&guards, &pseudo, extra_declarations);

    data.stylist
        .precomputed_values_for_pseudo_with_rule_node::<GeckoElement>(
            &guards,
            &pseudo,
            parent_style_or_null.map(|x| &*x),
            &metrics,
            rule_node,
        )
        .into()
}

#[no_mangle]
pub extern "C" fn Servo_ResolvePseudoStyle(
    element: &RawGeckoElement,
    pseudo_type: PseudoStyleType,
    is_probe: bool,
    inherited_style: Option<&ComputedValues>,
    raw_data: &RawServoStyleSet,
) -> Strong<ComputedValues> {
    let element = GeckoElement(element);
    let doc_data = PerDocumentStyleData::from_ffi(raw_data).borrow();

    debug!(
        "Servo_ResolvePseudoStyle: {:?} {:?}, is_probe: {}",
        element,
        PseudoElement::from_pseudo_type(pseudo_type),
        is_probe
    );

    let data = element.borrow_data();

    let data = match data.as_ref() {
        Some(data) if data.has_styles() => data,
        _ => {
            // FIXME(bholley, emilio): Assert against this.
            //
            // Known offender is nsMathMLmoFrame::MarkIntrinsicISizesDirty,
            // which goes and does a bunch of work involving style resolution.
            //
            // Bug 1403865 tracks fixing it, and potentially adding an assert
            // here instead.
            warn!("Calling Servo_ResolvePseudoStyle on unstyled element");
            return if is_probe {
                Strong::null()
            } else {
                doc_data.default_computed_values().clone().into()
            };
        },
    };

    let pseudo = PseudoElement::from_pseudo_type(pseudo_type)
        .expect("ResolvePseudoStyle with a non-pseudo?");

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let style = get_pseudo_style(
        &guard,
        element,
        &pseudo,
        RuleInclusion::All,
        &data.styles,
        inherited_style,
        &doc_data.stylist,
        is_probe,
        /* matching_func = */ None,
    );

    match style {
        Some(s) => s.into(),
        None => {
            debug_assert!(is_probe);
            Strong::null()
        },
    }
}

fn debug_atom_array(atoms: &nsTArray<structs::RefPtr<nsAtom>>) -> String {
    let mut result = String::from("[");
    for atom in atoms.iter() {
        if atom.mRawPtr.is_null() {
            result += "(null), ";
        } else {
            let atom = unsafe { WeakAtom::new(atom.mRawPtr) };
            write!(result, "{}, ", atom).unwrap();
        }
    }
    result.push(']');
    result
}

#[no_mangle]
pub extern "C" fn Servo_ComputedValues_ResolveXULTreePseudoStyle(
    element: &RawGeckoElement,
    pseudo_tag: *mut nsAtom,
    inherited_style: &ComputedValues,
    input_word: &nsTArray<structs::RefPtr<nsAtom>>,
    raw_data: &RawServoStyleSet,
) -> Strong<ComputedValues> {
    let element = GeckoElement(element);
    let data = element
        .borrow_data()
        .expect("Calling ResolveXULTreePseudoStyle on unstyled element?");

    let pseudo = unsafe {
        Atom::with(pseudo_tag, |atom| {
            PseudoElement::from_tree_pseudo_atom(atom, Box::new([]))
        })
        .expect("ResolveXULTreePseudoStyle with a non-tree pseudo?")
    };
    let doc_data = PerDocumentStyleData::from_ffi(raw_data).borrow();

    debug!(
        "ResolveXULTreePseudoStyle: {:?} {:?} {}",
        element,
        pseudo,
        debug_atom_array(input_word)
    );

    let matching_fn = |pseudo: &PseudoElement| {
        let args = pseudo
            .tree_pseudo_args()
            .expect("Not a tree pseudo-element?");
        args.iter()
            .all(|atom| input_word.iter().any(|item| atom.as_ptr() == item.mRawPtr))
    };

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    get_pseudo_style(
        &guard,
        element,
        &pseudo,
        RuleInclusion::All,
        &data.styles,
        Some(inherited_style),
        &doc_data.stylist,
        /* is_probe = */ false,
        Some(&matching_fn),
    )
    .unwrap()
    .into()
}

#[no_mangle]
pub extern "C" fn Servo_SetExplicitStyle(element: &RawGeckoElement, style: &ComputedValues) {
    let element = GeckoElement(element);
    debug!("Servo_SetExplicitStyle: {:?}", element);
    // We only support this API for initial styling. There's no reason it couldn't
    // work for other things, we just haven't had a reason to do so.
    debug_assert!(!element.has_data());
    let mut data = unsafe { element.ensure_data() };
    data.styles.primary = Some(unsafe { ArcBorrow::from_ref(style) }.clone_arc());
}

fn get_pseudo_style(
    guard: &SharedRwLockReadGuard,
    element: GeckoElement,
    pseudo: &PseudoElement,
    rule_inclusion: RuleInclusion,
    styles: &ElementStyles,
    inherited_styles: Option<&ComputedValues>,
    stylist: &Stylist,
    is_probe: bool,
    matching_func: Option<&dyn Fn(&PseudoElement) -> bool>,
) -> Option<Arc<ComputedValues>> {
    let style = match pseudo.cascade_type() {
        PseudoElementCascadeType::Eager => {
            match *pseudo {
                PseudoElement::FirstLetter => {
                    styles.pseudos.get(&pseudo).map(|pseudo_styles| {
                        // inherited_styles can be None when doing lazy resolution
                        // (e.g. for computed style) or when probing.  In that case
                        // we just inherit from our element, which is what Gecko
                        // does in that situation.  What should actually happen in
                        // the computed style case is a bit unclear.
                        let inherited_styles = inherited_styles.unwrap_or(styles.primary());
                        let guards = StylesheetGuards::same(guard);
                        let metrics = get_metrics_provider_for_product();
                        let inputs = CascadeInputs::new_from_style(pseudo_styles);
                        stylist.compute_pseudo_element_style_with_inputs(
                            inputs,
                            pseudo,
                            &guards,
                            Some(inherited_styles),
                            &metrics,
                            Some(element),
                        )
                    })
                },
                _ => {
                    // Unfortunately, we can't assert that inherited_styles, if
                    // present, is pointer-equal to styles.primary(), or even
                    // equal in any meaningful way.  The way it can fail is as
                    // follows.  Say we append an element with a ::before,
                    // ::after, or ::first-line to a parent with a ::first-line,
                    // such that the element ends up on the first line of the
                    // parent (e.g. it's an inline-block in the case it has a
                    // ::first-line, or any container in the ::before/::after
                    // cases).  Then gecko will update its frame's style to
                    // inherit from the parent's ::first-line.  The next time we
                    // try to get the ::before/::after/::first-line style for
                    // the kid, we'll likely pass in the frame's style as
                    // inherited_styles, but that's not pointer-identical to
                    // styles.primary(), because it got reparented.
                    //
                    // Now in practice this turns out to be OK, because all the
                    // cases in which there's a mismatch go ahead and reparent
                    // styles again as needed to make sure the ::first-line
                    // affects all the things it should affect.  But it makes it
                    // impossible to assert anything about the two styles
                    // matching here, unfortunately.
                    styles.pseudos.get(&pseudo).cloned()
                },
            }
        },
        PseudoElementCascadeType::Precomputed => unreachable!("No anonymous boxes"),
        PseudoElementCascadeType::Lazy => {
            debug_assert!(
                inherited_styles.is_none() ||
                    ptr::eq(inherited_styles.unwrap(), &**styles.primary())
            );
            let base = if pseudo.inherits_from_default_values() {
                stylist.device().default_computed_values_arc()
            } else {
                styles.primary()
            };
            let guards = StylesheetGuards::same(guard);
            let metrics = get_metrics_provider_for_product();
            stylist.lazily_compute_pseudo_element_style(
                &guards,
                element,
                &pseudo,
                rule_inclusion,
                base,
                is_probe,
                &metrics,
                matching_func,
            )
        },
    };

    if is_probe {
        return style;
    }

    Some(style.unwrap_or_else(|| {
        StyleBuilder::for_inheritance(
            stylist.device(),
            Some(styles.primary()),
            Some(pseudo),
        )
        .build()
    }))
}

#[no_mangle]
pub unsafe extern "C" fn Servo_ComputedValues_Inherit(
    raw_data: &RawServoStyleSet,
    pseudo: PseudoStyleType,
    parent_style_context: Option<&ComputedValues>,
    target: structs::InheritTarget,
) -> Strong<ComputedValues> {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();

    let for_text = target == structs::InheritTarget::Text;
    let pseudo = PseudoElement::from_pseudo_type(pseudo).unwrap();
    debug_assert!(pseudo.is_anon_box());

    let mut style =
        StyleBuilder::for_inheritance(data.stylist.device(), parent_style_context, Some(&pseudo));

    if for_text {
        StyleAdjuster::new(&mut style).adjust_for_text();
    }

    style.build().into()
}

#[no_mangle]
pub extern "C" fn Servo_ComputedValues_SpecifiesAnimationsOrTransitions(
    values: &ComputedValues,
) -> bool {
    let ui = values.get_ui();
    ui.specifies_animations() || ui.specifies_transitions()
}

#[no_mangle]
pub extern "C" fn Servo_ComputedValues_GetStyleRuleList(
    values: &ComputedValues,
    rules: &mut nsTArray<*const RawServoStyleRule>,
) {
    let rule_node = match values.rules {
        Some(ref r) => r,
        None => return,
    };

    let mut result = SmallVec::<[_; 10]>::new();
    for node in rule_node.self_and_ancestors() {
        let style_rule = match node.style_source().and_then(|x| x.as_rule()) {
            Some(rule) => rule,
            _ => continue,
        };

        // For the rules with any important declaration, we insert them into
        // rule tree twice, one for normal level and another for important
        // level. So, we skip the important one to keep the specificity order of
        // rules.
        if node.importance().important() {
            continue;
        }

        result.push(style_rule);
    }

    rules.assign_from_iter_pod(result.into_iter().map(|src| {
        src.with_arc(|a| {
            a.with_raw_offset_arc(|arc| *Locked::<StyleRule>::arc_as_borrowed(arc) as *const _)
        })
    }))
}

/// println_stderr!() calls Gecko's printf_stderr(), which, unlike eprintln!(),
/// will funnel output to Android logcat.
#[cfg(feature = "gecko_debug")]
macro_rules! println_stderr {
    ($($e:expr),+) => {
        {
            let mut s = nsCString::new();
            write!(s, $($e),+).unwrap();
            s.write_char('\n').unwrap();
            unsafe { bindings::Gecko_PrintfStderr(&s); }
        }
    }
}

#[cfg(feature = "gecko_debug")]
fn dump_properties_and_rules(cv: &ComputedValues, properties: &LonghandIdSet) {
    println_stderr!("  Properties:");
    for p in properties.iter() {
        let mut v = nsCString::new();
        cv.get_resolved_value(p, &mut v).unwrap();
        println_stderr!("    {:?}: {}", p, v);
    }
    dump_rules(cv);
}

#[cfg(feature = "gecko_debug")]
fn dump_rules(cv: &ComputedValues) {
    println_stderr!("  Rules:");
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    for rn in cv.rules().self_and_ancestors() {
        if rn.importance().important() {
            continue;
        }
        if let Some(d) = rn.style_source().and_then(|s| s.as_declarations()) {
            println_stderr!("    [DeclarationBlock: {:?}]", d);
        }
        if let Some(r) = rn.style_source().and_then(|s| s.as_rule()) {
            let mut s = nsCString::new();
            r.read_with(&guard).to_css(&guard, &mut s).unwrap();
            println_stderr!("    {}", s);
        }
    }
}

#[cfg(feature = "gecko_debug")]
#[no_mangle]
pub extern "C" fn Servo_ComputedValues_EqualForCachedAnonymousContentStyle(
    a: &ComputedValues,
    b: &ComputedValues,
) -> bool {
    let mut differing_properties = a.differing_properties(b);

    // Ignore any difference in -x-lang, which we can't override in the
    // rules in minimal-xul.css, but which makes no difference for the
    // anonymous content subtrees we cache style for.
    differing_properties.remove(LonghandId::XLang);
    // Similarly, -x-lang can influence the font-family fallback we have for
    // the initial font-family so remove it as well.
    differing_properties.remove(LonghandId::FontFamily);

    // Ignore any difference in pref-controlled, inherited properties.  These
    // properties may or may not be set by the 'all' declaration in the
    // minimal-xul.css rule, depending on whether the pref was enabled at the
    // time the UA sheets were parsed.
    //
    // If you add a new pref-controlled, inherited property, it must be defined
    // with `has_effect_on_gecko_scrollbars=False` to declare that
    // different values of this property on a <scrollbar> element or its
    // descendant scrollbar part elements should have no effect on their
    // rendering and behavior.
    //
    // If you do need a pref-controlled, inherited property to have an effect
    // on these elements, then you will need to add some checks to the
    // nsIAnonymousContentCreator::CreateAnonymousContent implementations of
    // ScrollFrameHelper and nsScrollbarFrame to clear the AnonymousContentKey
    // if a non-initial value is used.
    differing_properties.remove_all(&LonghandIdSet::has_no_effect_on_gecko_scrollbars());

    if !differing_properties.is_empty() {
        println_stderr!("Actual style:");
        dump_properties_and_rules(a, &differing_properties);
        println_stderr!("Expected style:");
        dump_properties_and_rules(b, &differing_properties);
    }

    differing_properties.is_empty()
}

#[cfg(feature = "gecko_debug")]
#[no_mangle]
pub extern "C" fn Servo_ComputedValues_DumpMatchedRules(s: &ComputedValues) {
    dump_rules(s);
}

#[no_mangle]
pub extern "C" fn Servo_ComputedValues_BlockifiedDisplay(
    style: &ComputedValues,
    is_root_element: bool,
) -> u16 {
    let display = style.get_box().clone_display();
    let blockified_display = display.equivalent_block_display(is_root_element);
    blockified_display.to_u16()
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_Init(doc: &structs::Document) -> *mut RawServoStyleSet {
    let data = Box::new(PerDocumentStyleData::new(doc));

    // Do this here rather than in Servo_Initialize since we need a document to
    // get the default computed values from.
    style::gecko_properties::assert_initial_values_match(&data);

    Box::into_raw(data) as *mut RawServoStyleSet
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_RebuildCachedData(raw_data: &RawServoStyleSet) {
    let mut data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    data.stylist.device_mut().rebuild_cached_data();
    data.undisplayed_style_cache.clear();
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_Drop(data: *mut RawServoStyleSet) {
    PerDocumentStyleData::drop_ffi(data);
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_CompatModeChanged(raw_data: &RawServoStyleSet) {
    let mut data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    let quirks_mode = data.stylist.device().document().mCompatMode;
    data.stylist.set_quirks_mode(quirks_mode.into());
}

fn parse_property_into(
    declarations: &mut SourcePropertyDeclaration,
    property_id: PropertyId,
    value: &nsACString,
    origin: Origin,
    url_data: &UrlExtraData,
    parsing_mode: structs::ParsingMode,
    quirks_mode: QuirksMode,
    rule_type: CssRuleType,
    reporter: Option<&dyn ParseErrorReporter>,
) -> Result<(), ()> {
    let value = unsafe { value.as_str_unchecked() };
    let parsing_mode = ParsingMode::from_bits_truncate(parsing_mode);

    if let Some(non_custom) = property_id.non_custom_id() {
        if !non_custom.allowed_in_rule(rule_type) {
            return Err(());
        }
    }

    parse_one_declaration_into(
        declarations,
        property_id,
        value,
        origin,
        url_data,
        reporter,
        parsing_mode,
        quirks_mode,
        rule_type,
    )
}

#[no_mangle]
pub unsafe extern "C" fn Servo_ParseProperty(
    property: nsCSSPropertyID,
    value: &nsACString,
    data: *mut URLExtraData,
    parsing_mode: structs::ParsingMode,
    quirks_mode: nsCompatibility,
    loader: *mut Loader,
    rule_type: CssRuleType,
) -> Strong<RawServoDeclarationBlock> {
    let id = get_property_id_from_nscsspropertyid!(property, Strong::null());
    let mut declarations = SourcePropertyDeclaration::new();
    let reporter = ErrorReporter::new(ptr::null_mut(), loader, data);
    let data = UrlExtraData::from_ptr_ref(&data);
    let result = parse_property_into(
        &mut declarations,
        id,
        value,
        Origin::Author,
        data,
        parsing_mode,
        quirks_mode.into(),
        rule_type,
        reporter.as_ref().map(|r| r as &dyn ParseErrorReporter),
    );

    match result {
        Ok(()) => {
            let global_style_data = &*GLOBAL_STYLE_DATA;
            let mut block = PropertyDeclarationBlock::new();
            block.extend(declarations.drain(), Importance::Normal);
            Arc::new(global_style_data.shared_lock.wrap(block)).into_strong()
        },
        Err(_) => Strong::null(),
    }
}

#[no_mangle]
pub extern "C" fn Servo_ParseEasing(easing: &nsACString, output: &mut nsTimingFunction) -> bool {
    use style::properties::longhands::transition_timing_function;

    let context = ParserContext::new(
        Origin::Author,
        unsafe { dummy_url_data() },
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );
    let easing = easing.to_string();
    let mut input = ParserInput::new(&easing);
    let mut parser = Parser::new(&mut input);
    let result =
        parser.parse_entirely(|p| transition_timing_function::single_value::parse(&context, p));
    match result {
        Ok(parsed_easing) => {
            // We store as computed value in nsTimingFunction.
            (*output).mTiming = parsed_easing.to_computed_value_without_context();
            true
        },
        Err(_) => false,
    }
}

#[no_mangle]
pub extern "C" fn Servo_SerializeEasing(easing: &nsTimingFunction, output: &mut nsACString) {
    easing.mTiming.to_css(&mut CssWriter::new(output)).unwrap();
}

#[no_mangle]
pub extern "C" fn Servo_GetProperties_Overriding_Animation(
    element: &RawGeckoElement,
    list: &nsTArray<nsCSSPropertyID>,
    set: &mut structs::nsCSSPropertyIDSet,
) {
    let element = GeckoElement(element);
    let element_data = match element.borrow_data() {
        Some(data) => data,
        None => return,
    };
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let guards = StylesheetGuards::same(&guard);
    let (overridden, custom) = element_data
        .styles
        .primary()
        .rules()
        .get_properties_overriding_animations(&guards);
    for p in list.iter() {
        match PropertyId::from_nscsspropertyid(*p) {
            Ok(property) => {
                if let PropertyId::Longhand(id) = property {
                    if overridden.contains(id) {
                        unsafe { Gecko_AddPropertyToSet(set, *p) };
                    }
                }
            },
            Err(_) => {
                if *p == nsCSSPropertyID::eCSSPropertyExtra_variable && custom {
                    unsafe { Gecko_AddPropertyToSet(set, *p) };
                }
            },
        }
    }
}

#[no_mangle]
pub extern "C" fn Servo_MatrixTransform_Operate(
    matrix_operator: MatrixTransformOperator,
    from: *const structs::Matrix4x4Components,
    to: *const structs::Matrix4x4Components,
    progress: f64,
    output: *mut structs::Matrix4x4Components,
) {
    use self::MatrixTransformOperator::{Accumulate, Interpolate};
    use style::values::computed::transform::Matrix3D;

    let from = Matrix3D::from(unsafe { from.as_ref() }.expect("not a valid 'from' matrix"));
    let to = Matrix3D::from(unsafe { to.as_ref() }.expect("not a valid 'to' matrix"));
    let result = match matrix_operator {
        Interpolate => from.animate(&to, Procedure::Interpolate { progress }),
        Accumulate => from.animate(
            &to,
            Procedure::Accumulate {
                count: progress as u64,
            },
        ),
    };

    let output = unsafe { output.as_mut() }.expect("not a valid 'output' matrix");
    if let Ok(result) = result {
        *output = result.into();
    } else if progress < 0.5 {
        *output = from.clone().into();
    } else {
        *output = to.clone().into();
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_ParseStyleAttribute(
    data: &nsACString,
    raw_extra_data: *mut URLExtraData,
    quirks_mode: nsCompatibility,
    loader: *mut Loader,
    rule_type: CssRuleType,
) -> Strong<RawServoDeclarationBlock> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let value = data.as_str_unchecked();
    let reporter = ErrorReporter::new(ptr::null_mut(), loader, raw_extra_data);
    let url_data = UrlExtraData::from_ptr_ref(&raw_extra_data);
    Arc::new(global_style_data.shared_lock.wrap(parse_style_attribute(
        value,
        url_data,
        reporter.as_ref().map(|r| r as &dyn ParseErrorReporter),
        quirks_mode.into(),
        rule_type,
    )))
    .into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_CreateEmpty() -> Strong<RawServoDeclarationBlock> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    Arc::new(
        global_style_data
            .shared_lock
            .wrap(PropertyDeclarationBlock::new()),
    )
    .into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_Clone(
    declarations: &RawServoDeclarationBlock,
) -> Strong<RawServoDeclarationBlock> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let declarations = Locked::<PropertyDeclarationBlock>::as_arc(&declarations);
    Arc::new(
        global_style_data
            .shared_lock
            .wrap(declarations.read_with(&guard).clone()),
    )
    .into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_Equals(
    a: &RawServoDeclarationBlock,
    b: &RawServoDeclarationBlock,
) -> bool {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    *Locked::<PropertyDeclarationBlock>::as_arc(&a)
        .read_with(&guard)
        .declarations() ==
        *Locked::<PropertyDeclarationBlock>::as_arc(&b)
            .read_with(&guard)
            .declarations()
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_GetCssText(
    declarations: &RawServoDeclarationBlock,
    result: &mut nsACString,
) {
    read_locked_arc(declarations, |decls: &PropertyDeclarationBlock| {
        decls.to_css(result).unwrap()
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SerializeOneValue(
    declarations: &RawServoDeclarationBlock,
    property_id: nsCSSPropertyID,
    buffer: &mut nsACString,
    computed_values: Option<&ComputedValues>,
    custom_properties: Option<&RawServoDeclarationBlock>,
    raw_data: &RawServoStyleSet,
) {
    let property_id = get_property_id_from_nscsspropertyid!(property_id, ());

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let decls = Locked::<PropertyDeclarationBlock>::as_arc(&declarations).read_with(&guard);

    let custom_properties =
        Locked::<PropertyDeclarationBlock>::arc_from_borrowed(&custom_properties);
    let custom_properties = custom_properties.map(|block| block.read_with(&guard));
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let rv = decls.single_value_to_css(
        &property_id,
        buffer,
        computed_values,
        custom_properties,
        &data.stylist.device(),
    );
    debug_assert!(rv.is_ok());
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SerializeFontValueForCanvas(
    declarations: &RawServoDeclarationBlock,
    buffer: &mut nsACString,
) {
    use style::properties::shorthands::font;
    read_locked_arc(declarations, |decls: &PropertyDeclarationBlock| {
        let longhands = match font::LonghandsToSerialize::from_iter(decls.declarations().iter()) {
            Ok(l) => l,
            Err(()) => {
                warn!("Unexpected property!");
                return;
            },
        };

        let rv = longhands.to_css(&mut CssWriter::new(buffer));
        debug_assert!(rv.is_ok());
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_Count(declarations: &RawServoDeclarationBlock) -> u32 {
    read_locked_arc(declarations, |decls: &PropertyDeclarationBlock| {
        decls.declarations().len() as u32
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_GetNthProperty(
    declarations: &RawServoDeclarationBlock,
    index: u32,
    result: &mut nsACString,
) -> bool {
    read_locked_arc(declarations, |decls: &PropertyDeclarationBlock| {
        if let Some(decl) = decls.declarations().get(index as usize) {
            result.assign(&decl.id().name());
            true
        } else {
            false
        }
    })
}

macro_rules! get_property_id_from_property {
    ($property: ident, $ret: expr) => {{
        let property = $property.as_str_unchecked();
        match PropertyId::parse_enabled_for_all_content(property) {
            Ok(property_id) => property_id,
            Err(_) => return $ret,
        }
    }};
}

unsafe fn get_property_value(
    declarations: &RawServoDeclarationBlock,
    property_id: PropertyId,
    value: &mut nsACString,
) {
    // This callsite is hot enough that the lock acquisition shows up in profiles.
    // Using an unchecked read here improves our performance by ~10% on the
    // microbenchmark in bug 1355599.
    read_locked_arc_unchecked(declarations, |decls: &PropertyDeclarationBlock| {
        decls.property_value_to_css(&property_id, value).unwrap();
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_GetPropertyValue(
    declarations: &RawServoDeclarationBlock,
    property: &nsACString,
    value: &mut nsACString,
) {
    get_property_value(
        declarations,
        get_property_id_from_property!(property, ()),
        value,
    )
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_GetPropertyValueById(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: &mut nsACString,
) {
    get_property_value(
        declarations,
        get_property_id_from_nscsspropertyid!(property, ()),
        value,
    )
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_GetPropertyIsImportant(
    declarations: &RawServoDeclarationBlock,
    property: &nsACString,
) -> bool {
    let property_id = get_property_id_from_property!(property, false);
    read_locked_arc(declarations, |decls: &PropertyDeclarationBlock| {
        decls.property_priority(&property_id).important()
    })
}

#[inline(always)]
fn set_property_to_declarations(
    non_custom_property_id: Option<NonCustomPropertyId>,
    block: &RawServoDeclarationBlock,
    parsed_declarations: &mut SourcePropertyDeclaration,
    before_change_closure: DeclarationBlockMutationClosure,
    importance: Importance,
) -> bool {
    let mut updates = Default::default();
    let will_change = read_locked_arc(block, |decls: &PropertyDeclarationBlock| {
        decls.prepare_for_update(&parsed_declarations, importance, &mut updates)
    });
    if !will_change {
        return false;
    }

    before_change_closure.invoke(non_custom_property_id);
    write_locked_arc(block, |decls: &mut PropertyDeclarationBlock| {
        decls.update(parsed_declarations.drain(), importance, &mut updates)
    });
    true
}

fn set_property(
    declarations: &RawServoDeclarationBlock,
    property_id: PropertyId,
    value: &nsACString,
    is_important: bool,
    data: &UrlExtraData,
    parsing_mode: structs::ParsingMode,
    quirks_mode: QuirksMode,
    loader: *mut Loader,
    rule_type: CssRuleType,
    before_change_closure: DeclarationBlockMutationClosure,
) -> bool {
    let mut source_declarations = SourcePropertyDeclaration::new();
    let reporter = ErrorReporter::new(ptr::null_mut(), loader, data.ptr());
    let non_custom_property_id = property_id.non_custom_id();
    let result = parse_property_into(
        &mut source_declarations,
        property_id,
        value,
        Origin::Author,
        data,
        parsing_mode,
        quirks_mode,
        rule_type,
        reporter.as_ref().map(|r| r as &dyn ParseErrorReporter),
    );

    if result.is_err() {
        return false;
    }

    let importance = if is_important {
        Importance::Important
    } else {
        Importance::Normal
    };

    set_property_to_declarations(
        non_custom_property_id,
        declarations,
        &mut source_declarations,
        before_change_closure,
        importance,
    )
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_SetProperty(
    declarations: &RawServoDeclarationBlock,
    property: &nsACString,
    value: &nsACString,
    is_important: bool,
    data: *mut URLExtraData,
    parsing_mode: structs::ParsingMode,
    quirks_mode: nsCompatibility,
    loader: *mut Loader,
    rule_type: CssRuleType,
    before_change_closure: DeclarationBlockMutationClosure,
) -> bool {
    set_property(
        declarations,
        get_property_id_from_property!(property, false),
        value,
        is_important,
        UrlExtraData::from_ptr_ref(&data),
        parsing_mode,
        quirks_mode.into(),
        loader,
        rule_type,
        before_change_closure,
    )
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_SetPropertyToAnimationValue(
    declarations: &RawServoDeclarationBlock,
    animation_value: &RawServoAnimationValue,
    before_change_closure: DeclarationBlockMutationClosure,
) -> bool {
    let animation_value = AnimationValue::as_arc(&animation_value);
    let non_custom_property_id = animation_value.id().into();
    let mut source_declarations =
        SourcePropertyDeclaration::with_one(animation_value.uncompute());

    set_property_to_declarations(
        Some(non_custom_property_id),
        declarations,
        &mut source_declarations,
        before_change_closure,
        Importance::Normal,
    )
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_SetPropertyById(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: &nsACString,
    is_important: bool,
    data: *mut URLExtraData,
    parsing_mode: structs::ParsingMode,
    quirks_mode: nsCompatibility,
    loader: *mut Loader,
    rule_type: CssRuleType,
    before_change_closure: DeclarationBlockMutationClosure,
) -> bool {
    set_property(
        declarations,
        get_property_id_from_nscsspropertyid!(property, false),
        value,
        is_important,
        UrlExtraData::from_ptr_ref(&data),
        parsing_mode,
        quirks_mode.into(),
        loader,
        rule_type,
        before_change_closure,
    )
}

fn remove_property(
    declarations: &RawServoDeclarationBlock,
    property_id: PropertyId,
    before_change_closure: DeclarationBlockMutationClosure,
) -> bool {
    let first_declaration = read_locked_arc(declarations, |decls: &PropertyDeclarationBlock| {
        decls.first_declaration_to_remove(&property_id)
    });

    let first_declaration = match first_declaration {
        Some(i) => i,
        None => return false,
    };

    before_change_closure.invoke(property_id.non_custom_id());
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.remove_property(&property_id, first_declaration)
    });

    true
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_RemoveProperty(
    declarations: &RawServoDeclarationBlock,
    property: &nsACString,
    before_change_closure: DeclarationBlockMutationClosure,
) -> bool {
    remove_property(
        declarations,
        get_property_id_from_property!(property, false),
        before_change_closure,
    )
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_RemovePropertyById(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    before_change_closure: DeclarationBlockMutationClosure,
) -> bool {
    remove_property(
        declarations,
        get_property_id_from_nscsspropertyid!(property, false),
        before_change_closure,
    )
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_Create() -> Strong<RawServoMediaList> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    Arc::new(global_style_data.shared_lock.wrap(MediaList::empty())).into_strong()
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_DeepClone(list: &RawServoMediaList) -> Strong<RawServoMediaList> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    read_locked_arc(list, |list: &MediaList| {
        Arc::new(global_style_data.shared_lock.wrap(list.clone())).into_strong()
    })
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_Matches(
    list: &RawServoMediaList,
    raw_data: &RawServoStyleSet,
) -> bool {
    let per_doc_data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    read_locked_arc(list, |list: &MediaList| {
        list.evaluate(
            per_doc_data.stylist.device(),
            per_doc_data.stylist.quirks_mode(),
        )
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_HasCSSWideKeyword(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
) -> bool {
    let property_id = get_property_id_from_nscsspropertyid!(property, false);
    read_locked_arc(declarations, |decls: &PropertyDeclarationBlock| {
        decls.has_css_wide_keyword(&property_id)
    })
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_GetText(list: &RawServoMediaList, result: &mut nsACString) {
    read_locked_arc(list, |list: &MediaList| {
        list.to_css(&mut CssWriter::new(result)).unwrap();
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_MediaList_SetText(
    list: &RawServoMediaList,
    text: &nsACString,
    caller_type: CallerType,
) {
    let text = text.as_str_unchecked();

    let mut input = ParserInput::new(&text);
    let mut parser = Parser::new(&mut input);
    let url_data = dummy_url_data();

    // TODO(emilio): If the need for `CallerType` appears in more places,
    // consider adding an explicit member in `ParserContext` instead of doing
    // this (or adding a dummy "chrome://" url data).
    //
    // For media query parsing it's effectively the same, so for now...
    let origin = match caller_type {
        CallerType::System => Origin::UserAgent,
        CallerType::NonSystem => Origin::Author,
    };

    let context = ParserContext::new(
        origin,
        url_data,
        Some(CssRuleType::Media),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        // TODO(emilio): Looks like error reporting could be useful here?
        None,
        None,
    );

    write_locked_arc(list, |list: &mut MediaList| {
        *list = MediaList::parse(&context, &mut parser);
    })
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_GetLength(list: &RawServoMediaList) -> u32 {
    read_locked_arc(list, |list: &MediaList| list.media_queries.len() as u32)
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_GetMediumAt(
    list: &RawServoMediaList,
    index: u32,
    result: &mut nsACString,
) -> bool {
    read_locked_arc(list, |list: &MediaList| {
        let media_query = match list.media_queries.get(index as usize) {
            Some(mq) => mq,
            None => return false,
        };
        media_query.to_css(&mut CssWriter::new(result)).unwrap();
        true
    })
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_AppendMedium(list: &RawServoMediaList, new_medium: &nsACString) {
    let new_medium = unsafe { new_medium.as_str_unchecked() };
    let url_data = unsafe { dummy_url_data() };
    let context = ParserContext::new(
        Origin::Author,
        url_data,
        Some(CssRuleType::Media),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );
    write_locked_arc(list, |list: &mut MediaList| {
        list.append_medium(&context, new_medium);
    })
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_DeleteMedium(
    list: &RawServoMediaList,
    old_medium: &nsACString,
) -> bool {
    let old_medium = unsafe { old_medium.as_str_unchecked() };
    let url_data = unsafe { dummy_url_data() };
    let context = ParserContext::new(
        Origin::Author,
        url_data,
        Some(CssRuleType::Media),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );
    write_locked_arc(list, |list: &mut MediaList| {
        list.delete_medium(&context, old_medium)
    })
}

#[no_mangle]
pub extern "C" fn Servo_MediaList_SizeOfIncludingThis(
    malloc_size_of: GeckoMallocSizeOf,
    malloc_enclosing_size_of: GeckoMallocSizeOf,
    list: &RawServoMediaList,
) -> usize {
    use malloc_size_of::MallocSizeOf;
    use malloc_size_of::MallocUnconditionalShallowSizeOf;

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();

    let mut ops = MallocSizeOfOps::new(
        malloc_size_of.unwrap(),
        Some(malloc_enclosing_size_of.unwrap()),
        None,
    );

    Locked::<MediaList>::as_arc(&list).with_arc(|list| {
        let mut n = 0;
        n += list.unconditional_shallow_size_of(&mut ops);
        n += list.read_with(&guard).size_of(&mut ops);
        n
    })
}

macro_rules! get_longhand_from_id {
    ($id:expr) => {
        match PropertyId::from_nscsspropertyid($id) {
            Ok(PropertyId::Longhand(long)) => long,
            _ => {
                panic!("stylo: unknown presentation property with id");
            },
        }
    };
}

macro_rules! match_wrap_declared {
    ($longhand:ident, $($property:ident => $inner:expr,)*) => (
        match $longhand {
            $(
                LonghandId::$property => PropertyDeclaration::$property($inner),
            )*
            _ => {
                panic!("stylo: Don't know how to handle presentation property");
            }
        }
    )
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_PropertyIsSet(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
) -> bool {
    read_locked_arc(declarations, |decls: &PropertyDeclarationBlock| {
        decls.contains(get_longhand_from_id!(property))
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_SetIdentStringValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: *mut nsAtom,
) {
    use style::properties::longhands::_x_lang::computed_value::T as Lang;
    use style::properties::PropertyDeclaration;

    let long = get_longhand_from_id!(property);
    let prop = match_wrap_declared! { long,
        XLang => Lang(Atom::from_raw(value)),
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
#[allow(unreachable_code)]
pub extern "C" fn Servo_DeclarationBlock_SetKeywordValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: i32,
) {
    use num_traits::FromPrimitive;
    use style::properties::longhands;
    use style::properties::PropertyDeclaration;
    use style::values::generics::box_::{VerticalAlign, VerticalAlignKeyword};
    use style::values::generics::font::FontStyle;
    use style::values::specified::{
        table::CaptionSide, BorderStyle, Clear, Display, Float, TextAlign,
    };

    fn get_from_computed<T>(value: u32) -> T
    where
        T: ToComputedValue,
        T::ComputedValue: FromPrimitive,
    {
        T::from_computed_value(&T::ComputedValue::from_u32(value).unwrap())
    }

    let long = get_longhand_from_id!(property);
    let value = value as u32;

    let prop = match_wrap_declared! { long,
        MozUserModify => longhands::_moz_user_modify::SpecifiedValue::from_gecko_keyword(value),
        Direction => get_from_computed::<longhands::direction::SpecifiedValue>(value),
        Display => get_from_computed::<Display>(value),
        Float => get_from_computed::<Float>(value),
        Clear => get_from_computed::<Clear>(value),
        VerticalAlign => VerticalAlign::Keyword(VerticalAlignKeyword::from_u32(value).unwrap()),
        TextAlign => get_from_computed::<TextAlign>(value),
        TextEmphasisPosition => longhands::text_emphasis_position::SpecifiedValue::from_gecko_keyword(value),
        FontSize => {
            // We rely on Gecko passing in font-size values (0...7) here.
            longhands::font_size::SpecifiedValue::from_html_size(value as u8)
        },
        FontStyle => {
            style::values::specified::FontStyle::Specified(if value == structs::NS_FONT_STYLE_ITALIC {
                FontStyle::Italic
            } else {
                debug_assert_eq!(value, structs::NS_FONT_STYLE_NORMAL);
                FontStyle::Normal
            })
        },
        FontWeight => longhands::font_weight::SpecifiedValue::from_gecko_keyword(value),
        ListStyleType => Box::new(longhands::list_style_type::SpecifiedValue::from_gecko_keyword(value)),
        MathStyle => longhands::math_style::SpecifiedValue::from_gecko_keyword(value),
        MozMathVariant => longhands::_moz_math_variant::SpecifiedValue::from_gecko_keyword(value),
        WhiteSpace => longhands::white_space::SpecifiedValue::from_gecko_keyword(value),
        CaptionSide => get_from_computed::<CaptionSide>(value),
        BorderTopStyle => get_from_computed::<BorderStyle>(value),
        BorderRightStyle => get_from_computed::<BorderStyle>(value),
        BorderBottomStyle => get_from_computed::<BorderStyle>(value),
        BorderLeftStyle => get_from_computed::<BorderStyle>(value),
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetIntValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: i32,
) {
    use style::properties::PropertyDeclaration;
    use style::values::specified::Integer;

    let long = get_longhand_from_id!(property);
    let prop = match_wrap_declared! { long,
        XSpan => Integer::new(value),
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetMathDepthValue(
    declarations: &RawServoDeclarationBlock,
    value: i32,
    is_relative: bool,
) {
    use style::properties::longhands::math_depth::SpecifiedValue as MathDepth;
    use style::properties::PropertyDeclaration;

    let integer_value = style::values::specified::Integer::new(value);
    let prop = PropertyDeclaration::MathDepth(if is_relative {
        MathDepth::Add(integer_value)
    } else {
        MathDepth::Absolute(integer_value)
    });
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetCounterResetListItem(
    declarations: &RawServoDeclarationBlock,
    counter_value: i32,
    is_reversed: bool,
) {
    use style::properties::PropertyDeclaration;
    use style::values::generics::counters::{CounterPair, CounterReset};

    let prop = PropertyDeclaration::CounterReset(CounterReset::new(vec![CounterPair {
        name: CustomIdent(atom!("list-item")),
        value: style::values::specified::Integer::new(counter_value),
        is_reversed,
    }]));
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetCounterSetListItem(
    declarations: &RawServoDeclarationBlock,
    counter_value: i32,
) {
    use style::properties::PropertyDeclaration;
    use style::values::generics::counters::{CounterPair, CounterSet};

    let prop = PropertyDeclaration::CounterSet(CounterSet::new(vec![CounterPair {
        name: CustomIdent(atom!("list-item")),
        value: style::values::specified::Integer::new(counter_value),
        is_reversed: false,
    }]));
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetPixelValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: f32,
) {
    use style::properties::longhands::border_spacing::SpecifiedValue as BorderSpacing;
    use style::properties::PropertyDeclaration;
    use style::values::generics::length::LengthPercentageOrAuto;
    use style::values::generics::length::Size;
    use style::values::generics::NonNegative;
    use style::values::specified::length::LengthPercentage;
    use style::values::specified::length::NonNegativeLengthPercentage;
    use style::values::specified::length::{NoCalcLength, NonNegativeLength};
    use style::values::specified::{BorderCornerRadius, BorderSideWidth};

    let long = get_longhand_from_id!(property);
    let nocalc = NoCalcLength::from_px(value);
    let lp = LengthPercentage::Length(nocalc);
    let lp_or_auto = LengthPercentageOrAuto::LengthPercentage(lp.clone());
    let prop = match_wrap_declared! { long,
        Height => Size::LengthPercentage(NonNegative(lp)),
        Width => Size::LengthPercentage(NonNegative(lp)),
        BorderTopWidth => BorderSideWidth::Length(nocalc.into()),
        BorderRightWidth => BorderSideWidth::Length(nocalc.into()),
        BorderBottomWidth => BorderSideWidth::Length(nocalc.into()),
        BorderLeftWidth => BorderSideWidth::Length(nocalc.into()),
        MarginTop => lp_or_auto,
        MarginRight => lp_or_auto,
        MarginBottom => lp_or_auto,
        MarginLeft => lp_or_auto,
        PaddingTop => NonNegative(lp),
        PaddingRight => NonNegative(lp),
        PaddingBottom => NonNegative(lp),
        PaddingLeft => NonNegative(lp),
        BorderSpacing => {
            let v = NonNegativeLength::from(nocalc);
            Box::new(BorderSpacing::new(v.clone(), v))
        },
        BorderTopLeftRadius => {
            let length = NonNegativeLengthPercentage::from(nocalc);
            Box::new(BorderCornerRadius::new(length.clone(), length))
        },
        BorderTopRightRadius => {
            let length = NonNegativeLengthPercentage::from(nocalc);
            Box::new(BorderCornerRadius::new(length.clone(), length))
        },
        BorderBottomLeftRadius => {
            let length = NonNegativeLengthPercentage::from(nocalc);
            Box::new(BorderCornerRadius::new(length.clone(), length))
        },
        BorderBottomRightRadius => {
            let length = NonNegativeLengthPercentage::from(nocalc);
            Box::new(BorderCornerRadius::new(length.clone(), length))
        },
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetLengthValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: f32,
    unit: structs::nsCSSUnit,
) {
    use style::properties::longhands::_moz_script_min_size::SpecifiedValue as MozScriptMinSize;
    use style::properties::PropertyDeclaration;
    use style::values::generics::length::{LengthPercentageOrAuto, Size};
    use style::values::generics::NonNegative;
    use style::values::specified::length::{AbsoluteLength, FontRelativeLength};
    use style::values::specified::length::{LengthPercentage, NoCalcLength};
    use style::values::specified::FontSize;

    let long = get_longhand_from_id!(property);
    let nocalc = match unit {
        structs::nsCSSUnit::eCSSUnit_EM => {
            NoCalcLength::FontRelative(FontRelativeLength::Em(value))
        },
        structs::nsCSSUnit::eCSSUnit_XHeight => {
            NoCalcLength::FontRelative(FontRelativeLength::Ex(value))
        },
        structs::nsCSSUnit::eCSSUnit_Pixel => NoCalcLength::Absolute(AbsoluteLength::Px(value)),
        structs::nsCSSUnit::eCSSUnit_Inch => NoCalcLength::Absolute(AbsoluteLength::In(value)),
        structs::nsCSSUnit::eCSSUnit_Centimeter => {
            NoCalcLength::Absolute(AbsoluteLength::Cm(value))
        },
        structs::nsCSSUnit::eCSSUnit_Millimeter => {
            NoCalcLength::Absolute(AbsoluteLength::Mm(value))
        },
        structs::nsCSSUnit::eCSSUnit_Point => NoCalcLength::Absolute(AbsoluteLength::Pt(value)),
        structs::nsCSSUnit::eCSSUnit_Pica => NoCalcLength::Absolute(AbsoluteLength::Pc(value)),
        structs::nsCSSUnit::eCSSUnit_Quarter => NoCalcLength::Absolute(AbsoluteLength::Q(value)),
        _ => unreachable!("Unknown unit passed to SetLengthValue"),
    };

    let prop = match_wrap_declared! { long,
        Width => Size::LengthPercentage(NonNegative(LengthPercentage::Length(nocalc))),
        Height => Size::LengthPercentage(NonNegative(LengthPercentage::Length(nocalc))),
        X =>  LengthPercentage::Length(nocalc),
        Y =>  LengthPercentage::Length(nocalc),
        Cx => LengthPercentage::Length(nocalc),
        Cy => LengthPercentage::Length(nocalc),
        R =>  NonNegative(LengthPercentage::Length(nocalc)),
        Rx => LengthPercentageOrAuto::LengthPercentage(NonNegative(LengthPercentage::Length(nocalc))),
        Ry => LengthPercentageOrAuto::LengthPercentage(NonNegative(LengthPercentage::Length(nocalc))),
        FontSize => FontSize::Length(LengthPercentage::Length(nocalc)),
        MozScriptMinSize => MozScriptMinSize(nocalc),
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetPathValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    path: &nsTArray<f32>,
) {
    use style::properties::PropertyDeclaration;
    use style::values::specified::DProperty;

    // 1. Decode the path data from SVG.
    let path = match specified::SVGPathData::decode_from_f32_array(path) {
        Ok(p) => p,
        Err(()) => return,
    };

    // 2. Set decoded path into style.
    let long = get_longhand_from_id!(property);
    let prop = match_wrap_declared! { long,
        D => if path.0.is_empty() { DProperty::None } else { DProperty::Path(path) },
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetNumberValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: f32,
) {
    use style::properties::longhands::_moz_script_size_multiplier::SpecifiedValue as MozScriptSizeMultiplier;
    use style::properties::PropertyDeclaration;

    let long = get_longhand_from_id!(property);

    let prop = match_wrap_declared! { long,
        MozScriptSizeMultiplier => MozScriptSizeMultiplier(value),
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetPercentValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: f32,
) {
    use style::properties::PropertyDeclaration;
    use style::values::computed::Percentage;
    use style::values::generics::length::{LengthPercentageOrAuto, Size};
    use style::values::generics::NonNegative;
    use style::values::specified::length::LengthPercentage;
    use style::values::specified::FontSize;

    let long = get_longhand_from_id!(property);
    let pc = Percentage(value);
    let lp = LengthPercentage::Percentage(pc);
    let lp_or_auto = LengthPercentageOrAuto::LengthPercentage(lp.clone());

    let prop = match_wrap_declared! { long,
        Height => Size::LengthPercentage(NonNegative(lp)),
        Width => Size::LengthPercentage(NonNegative(lp)),
        X =>  lp,
        Y =>  lp,
        Cx => lp,
        Cy => lp,
        R =>  NonNegative(lp),
        Rx => LengthPercentageOrAuto::LengthPercentage(NonNegative(lp)),
        Ry => LengthPercentageOrAuto::LengthPercentage(NonNegative(lp)),
        MarginTop => lp_or_auto,
        MarginRight => lp_or_auto,
        MarginBottom => lp_or_auto,
        MarginLeft => lp_or_auto,
        FontSize => FontSize::Length(LengthPercentage::Percentage(pc)),
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetAutoValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
) {
    use style::properties::PropertyDeclaration;
    use style::values::generics::length::{LengthPercentageOrAuto, Size};

    let long = get_longhand_from_id!(property);
    let auto = LengthPercentageOrAuto::Auto;

    let prop = match_wrap_declared! { long,
        Height => Size::auto(),
        Width => Size::auto(),
        MarginTop => auto,
        MarginRight => auto,
        MarginBottom => auto,
        MarginLeft => auto,
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetCurrentColor(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
) {
    use style::properties::PropertyDeclaration;
    use style::values::specified::Color;

    let long = get_longhand_from_id!(property);
    let cc = Color::currentcolor();

    let prop = match_wrap_declared! { long,
        BorderTopColor => cc,
        BorderRightColor => cc,
        BorderBottomColor => cc,
        BorderLeftColor => cc,
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetColorValue(
    declarations: &RawServoDeclarationBlock,
    property: nsCSSPropertyID,
    value: structs::nscolor,
) {
    use style::gecko::values::convert_nscolor_to_rgba;
    use style::properties::longhands;
    use style::properties::PropertyDeclaration;
    use style::values::specified::Color;

    let long = get_longhand_from_id!(property);
    let rgba = convert_nscolor_to_rgba(value);
    let color = Color::rgba(rgba);

    let prop = match_wrap_declared! { long,
        BorderTopColor => color,
        BorderRightColor => color,
        BorderBottomColor => color,
        BorderLeftColor => color,
        Color => longhands::color::SpecifiedValue(color),
        BackgroundColor => color,
    };
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(prop, Importance::Normal);
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_SetFontFamily(
    declarations: &RawServoDeclarationBlock,
    value: &nsACString,
) {
    use style::properties::longhands::font_family::SpecifiedValue as FontFamily;
    use style::properties::PropertyDeclaration;

    let string = value.as_str_unchecked();
    let mut input = ParserInput::new(&string);
    let mut parser = Parser::new(&mut input);
    let context = ParserContext::new(
        Origin::Author,
        dummy_url_data(),
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );
    let result = FontFamily::parse(&context, &mut parser);
    if let Ok(family) = result {
        if parser.is_exhausted() {
            let decl = PropertyDeclaration::FontFamily(family);
            write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
                decls.push(decl, Importance::Normal);
            })
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_DeclarationBlock_SetBackgroundImage(
    declarations: &RawServoDeclarationBlock,
    value: &nsACString,
    raw_extra_data: *mut URLExtraData,
) {
    use style::properties::longhands::background_image::SpecifiedValue as BackgroundImage;
    use style::properties::PropertyDeclaration;
    use style::stylesheets::CorsMode;
    use style::values::generics::image::Image;
    use style::values::specified::url::SpecifiedImageUrl;

    let url_data = UrlExtraData::from_ptr_ref(&raw_extra_data);
    let string = value.as_str_unchecked();
    let context = ParserContext::new(
        Origin::Author,
        url_data,
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );
    let url = SpecifiedImageUrl::parse_from_string(string.into(), &context, CorsMode::None);
    let decl = PropertyDeclaration::BackgroundImage(BackgroundImage(vec![Image::Url(url)].into()));
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(decl, Importance::Normal);
    });
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetTextDecorationColorOverride(
    declarations: &RawServoDeclarationBlock,
) {
    use style::properties::PropertyDeclaration;
    use style::values::specified::text::TextDecorationLine;

    let decoration = TextDecorationLine::COLOR_OVERRIDE;
    let decl = PropertyDeclaration::TextDecorationLine(decoration);
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(decl, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_DeclarationBlock_SetAspectRatio(
    declarations: &RawServoDeclarationBlock,
    width: f32,
    height: f32,
) {
    use style::properties::PropertyDeclaration;
    use style::values::generics::position::AspectRatio;

    let decl = PropertyDeclaration::AspectRatio(AspectRatio::from_mapped_ratio(width, height));
    write_locked_arc(declarations, |decls: &mut PropertyDeclarationBlock| {
        decls.push(decl, Importance::Normal);
    })
}

#[no_mangle]
pub extern "C" fn Servo_CSSSupports2(property: &nsACString, value: &nsACString) -> bool {
    let id = unsafe { get_property_id_from_property!(property, false) };

    let mut declarations = SourcePropertyDeclaration::new();
    parse_property_into(
        &mut declarations,
        id,
        value,
        Origin::Author,
        unsafe { dummy_url_data() },
        structs::ParsingMode_Default,
        QuirksMode::NoQuirks,
        CssRuleType::Style,
        None,
    )
    .is_ok()
}

#[no_mangle]
pub extern "C" fn Servo_CSSSupports(
    cond: &nsACString,
    ua_origin: bool,
    chrome_sheet: bool,
    quirks: bool,
) -> bool {
    let condition = unsafe { cond.as_str_unchecked() };
    let mut input = ParserInput::new(&condition);
    let mut input = Parser::new(&mut input);
    let cond = match input.parse_entirely(parse_condition_or_declaration) {
        Ok(c) => c,
        Err(..) => return false,
    };

    let origin = if ua_origin {
        Origin::UserAgent
    } else {
        Origin::Author
    };
    let url_data = unsafe {
        UrlExtraData::from_ptr_ref(if chrome_sheet {
            &DUMMY_CHROME_URL_DATA
        } else {
            &DUMMY_URL_DATA
        })
    };
    let quirks_mode = if quirks {
        QuirksMode::Quirks
    } else {
        QuirksMode::NoQuirks
    };

    // NOTE(emilio): The supports API is not associated to any stylesheet,
    // so the fact that there is no namespace map here is fine.
    let context = ParserContext::new(
        origin,
        url_data,
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        quirks_mode,
        None,
        None,
    );

    let namespaces = Default::default();
    cond.eval(&context, &namespaces)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_NoteExplicitHints(
    element: &RawGeckoElement,
    restyle_hint: RestyleHint,
    change_hint: nsChangeHint,
) {
    GeckoElement(element).note_explicit_hints(restyle_hint, change_hint);
}

#[no_mangle]
pub extern "C" fn Servo_TakeChangeHint(element: &RawGeckoElement, was_restyled: *mut bool) -> u32 {
    let was_restyled = unsafe { was_restyled.as_mut().unwrap() };
    let element = GeckoElement(element);

    let damage = match element.mutate_data() {
        Some(mut data) => {
            *was_restyled = data.is_restyle();

            let damage = data.damage;
            data.clear_restyle_state();
            damage
        },
        None => {
            warn!("Trying to get change hint from unstyled element");
            *was_restyled = false;
            GeckoRestyleDamage::empty()
        },
    };

    debug!("Servo_TakeChangeHint: {:?}, damage={:?}", element, damage);
    // We'd like to return `nsChangeHint` here, but bindgen bitfield enums don't
    // work as return values with the Linux 32-bit ABI at the moment because
    // they wrap the value in a struct, so for now just unwrap it.
    damage.as_change_hint().0
}

#[no_mangle]
pub extern "C" fn Servo_ResolveStyle(element: &RawGeckoElement) -> Strong<ComputedValues> {
    let element = GeckoElement(element);
    debug!("Servo_ResolveStyle: {:?}", element);
    let data = element
        .borrow_data()
        .expect("Resolving style on unstyled element");

    debug_assert!(
        element.has_current_styles(&*data),
        "Resolving style on {:?} without current styles: {:?}",
        element,
        data
    );
    data.styles.primary().clone().into()
}

#[no_mangle]
pub extern "C" fn Servo_ResolveStyleLazily(
    element: &RawGeckoElement,
    pseudo_type: PseudoStyleType,
    rule_inclusion: StyleRuleInclusion,
    snapshots: *const ServoElementSnapshotTable,
    cache_generation: u64,
    can_use_cache: bool,
    raw_data: &RawServoStyleSet,
) -> Strong<ComputedValues> {
    use selectors::Element;
    debug_assert!(!snapshots.is_null());
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let element = GeckoElement(element);
    let doc_data = PerDocumentStyleData::from_ffi(raw_data);
    let mut data = doc_data.borrow_mut();
    let mut data = &mut *data;
    let rule_inclusion = RuleInclusion::from(rule_inclusion);
    let pseudo = PseudoElement::from_pseudo_type(pseudo_type);

    if cache_generation != data.undisplayed_style_cache_generation {
        data.undisplayed_style_cache.clear();
        data.undisplayed_style_cache_generation = cache_generation;
    }

    let stylist = &data.stylist;
    let finish = |styles: &ElementStyles, is_probe: bool| -> Option<Arc<ComputedValues>> {
        match pseudo {
            Some(ref pseudo) => {
                get_pseudo_style(
                    &guard,
                    element,
                    pseudo,
                    rule_inclusion,
                    styles,
                    /* inherited_styles = */ None,
                    &stylist,
                    is_probe,
                    /* matching_func = */ None,
                )
            },
            None => Some(styles.primary().clone()),
        }
    };

    let is_before_or_after = pseudo.as_ref().map_or(false, |p| p.is_before_or_after());

    // In the common case we already have the style. Check that before setting
    // up all the computation machinery.
    //
    // Also, only probe in the ::before or ::after case, since their styles may
    // not be in the `ElementData`, given they may exist but not be applicable
    // to generate an actual pseudo-element (like, having a `content: none`).
    if rule_inclusion == RuleInclusion::All {
        let styles = element.borrow_data().and_then(|d| {
            if d.has_styles() {
                finish(&d.styles, is_before_or_after)
            } else {
                None
            }
        });
        if let Some(result) = styles {
            return result.into();
        }
        if pseudo.is_none() && can_use_cache {
            if let Some(style) = data.undisplayed_style_cache.get(&element.opaque()) {
                return style.clone().into();
            }
        }
    }

    // We don't have the style ready. Go ahead and compute it as necessary.
    let shared = create_shared_context(
        &global_style_data,
        &guard,
        &stylist,
        TraversalFlags::empty(),
        unsafe { &*snapshots },
    );
    let mut tlc = ThreadLocalStyleContext::new(&shared);
    let mut context = StyleContext {
        shared: &shared,
        thread_local: &mut tlc,
    };

    let styles = resolve_style(
        &mut context,
        element,
        rule_inclusion,
        pseudo.as_ref(),
        if can_use_cache {
            Some(&mut data.undisplayed_style_cache)
        } else {
            None
        },
    );

    finish(&styles, /* is_probe = */ false)
        .expect("We're not probing, so we should always get a style back")
        .into()
}

#[no_mangle]
pub extern "C" fn Servo_ReparentStyle(
    style_to_reparent: &ComputedValues,
    parent_style: &ComputedValues,
    parent_style_ignoring_first_line: &ComputedValues,
    layout_parent_style: &ComputedValues,
    element: Option<&RawGeckoElement>,
    raw_data: &RawServoStyleSet,
) -> Strong<ComputedValues> {
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let doc_data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let inputs = CascadeInputs::new_from_style(style_to_reparent);
    let metrics = get_metrics_provider_for_product();
    let pseudo = style_to_reparent.pseudo();
    let element = element.map(GeckoElement);

    doc_data
        .stylist
        .cascade_style_and_visited(
            element,
            pseudo.as_ref(),
            inputs,
            &StylesheetGuards::same(&guard),
            Some(parent_style),
            Some(parent_style_ignoring_first_line),
            Some(layout_parent_style),
            &metrics,
            /* rule_cache = */ None,
            &mut RuleCacheConditions::default(),
        )
        .into()
}

#[cfg(feature = "gecko_debug")]
fn simulate_compute_values_failure(property: &PropertyValuePair) -> bool {
    let p = property.mProperty;
    let id = get_property_id_from_nscsspropertyid!(p, false);
    id.as_shorthand().is_ok() && property.mSimulateComputeValuesFailure
}

#[cfg(not(feature = "gecko_debug"))]
fn simulate_compute_values_failure(_: &PropertyValuePair) -> bool {
    false
}

fn create_context_for_animation<'a>(
    per_doc_data: &'a PerDocumentStyleDataImpl,
    font_metrics_provider: &'a dyn FontMetricsProvider,
    style: &'a ComputedValues,
    parent_style: Option<&'a ComputedValues>,
    for_smil_animation: bool,
    rule_cache_conditions: &'a mut RuleCacheConditions,
) -> Context<'a> {
    Context {
        builder: StyleBuilder::for_animation(per_doc_data.stylist.device(), style, parent_style),
        font_metrics_provider,
        cached_system_font: None,
        in_media_query: false,
        quirks_mode: per_doc_data.stylist.quirks_mode(),
        for_smil_animation,
        for_non_inherited_property: None,
        container_info: None,
        rule_cache_conditions: RefCell::new(rule_cache_conditions),
    }
}

struct PropertyAndIndex {
    property: PropertyId,
    index: usize,
}

struct PrioritizedPropertyIter<'a> {
    properties: &'a [PropertyValuePair],
    sorted_property_indices: Vec<PropertyAndIndex>,
    curr: usize,
}

impl<'a> PrioritizedPropertyIter<'a> {
    fn new(properties: &'a [PropertyValuePair]) -> PrioritizedPropertyIter {
        use style::values::animated::compare_property_priority;

        // If we fail to convert a nsCSSPropertyID into a PropertyId we
        // shouldn't fail outright but instead by treating that property as the
        // 'all' property we make it sort last.
        let mut sorted_property_indices: Vec<PropertyAndIndex> = properties
            .iter()
            .enumerate()
            .map(|(index, pair)| {
                let property = PropertyId::from_nscsspropertyid(pair.mProperty)
                    .unwrap_or(PropertyId::Shorthand(ShorthandId::All));

                PropertyAndIndex { property, index }
            })
            .collect();
        sorted_property_indices.sort_by(|a, b| compare_property_priority(&a.property, &b.property));

        PrioritizedPropertyIter {
            properties,
            sorted_property_indices,
            curr: 0,
        }
    }
}

impl<'a> Iterator for PrioritizedPropertyIter<'a> {
    type Item = &'a PropertyValuePair;

    fn next(&mut self) -> Option<&'a PropertyValuePair> {
        if self.curr >= self.sorted_property_indices.len() {
            return None;
        }
        self.curr += 1;
        Some(&self.properties[self.sorted_property_indices[self.curr - 1].index])
    }
}

#[no_mangle]
pub extern "C" fn Servo_GetComputedKeyframeValues(
    keyframes: &nsTArray<structs::Keyframe>,
    element: &RawGeckoElement,
    pseudo_type: PseudoStyleType,
    style: &ComputedValues,
    raw_data: &RawServoStyleSet,
    computed_keyframes: &mut nsTArray<structs::ComputedKeyframeValues>,
) {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let metrics = get_metrics_provider_for_product();

    let element = GeckoElement(element);
    let pseudo = PseudoElement::from_pseudo_type(pseudo_type);
    let parent_element = if pseudo.is_none() {
        element.inheritance_parent()
    } else {
        Some(element)
    };
    let parent_data = parent_element.as_ref().and_then(|e| e.borrow_data());
    let parent_style = parent_data
        .as_ref()
        .map(|d| d.styles.primary())
        .map(|x| &**x);

    let mut conditions = Default::default();
    let mut context = create_context_for_animation(
        &data,
        &metrics,
        &style,
        parent_style,
        /* for_smil_animation = */ false,
        &mut conditions,
    );

    let restriction = pseudo.and_then(|p| p.property_restriction());

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let default_values = data.default_computed_values();

    let mut raw_custom_properties_block; // To make the raw block alive in the scope.
    for (index, keyframe) in keyframes.iter().enumerate() {
        let mut custom_properties = None;
        for property in keyframe.mPropertyValues.iter() {
            // Find the block for custom properties first.
            if property.mProperty == nsCSSPropertyID::eCSSPropertyExtra_variable {
                raw_custom_properties_block =
                    unsafe { &*property.mServoDeclarationBlock.mRawPtr.clone() };
                let guard =
                    Locked::<PropertyDeclarationBlock>::as_arc(&raw_custom_properties_block)
                        .read_with(&guard);
                custom_properties = guard.cascade_custom_properties_with_context(&context);
                // There should be one PropertyDeclarationBlock for custom properties.
                break;
            }
        }

        let ref mut animation_values = computed_keyframes[index];

        let mut seen = LonghandIdSet::new();

        let mut property_index = 0;
        for property in PrioritizedPropertyIter::new(&keyframe.mPropertyValues) {
            if simulate_compute_values_failure(property) {
                continue;
            }

            let mut maybe_append_animation_value =
                |property: LonghandId, value: Option<AnimationValue>| {
                    debug_assert!(!property.is_logical());
                    debug_assert!(property.is_animatable());

                    // 'display' is only animatable from SMIL
                    if property == LonghandId::Display {
                        return;
                    }

                    // Skip restricted properties
                    if restriction.map_or(false, |r| !property.flags().contains(r)) {
                        return;
                    }

                    if seen.contains(property) {
                        return;
                    }
                    seen.insert(property);

                    // This is safe since we immediately write to the uninitialized values.
                    unsafe { animation_values.set_len((property_index + 1) as u32) };
                    animation_values[property_index].mProperty = property.to_nscsspropertyid();
                    match value {
                        Some(v) => {
                            animation_values[property_index]
                                .mValue
                                .mServo
                                .set_arc_leaky(Arc::new(v));
                        },
                        None => {
                            animation_values[property_index].mValue.mServo.mRawPtr =
                                ptr::null_mut();
                        },
                    }
                    property_index += 1;
                };

            if property.mServoDeclarationBlock.mRawPtr.is_null() {
                let property = LonghandId::from_nscsspropertyid(property.mProperty);
                if let Ok(prop) = property {
                    maybe_append_animation_value(prop, None);
                }
                continue;
            }

            let declarations = unsafe { &*property.mServoDeclarationBlock.mRawPtr.clone() };
            let declarations = Locked::<PropertyDeclarationBlock>::as_arc(&declarations);
            let guard = declarations.read_with(&guard);
            let iter = guard.to_animation_value_iter(
                &mut context,
                &default_values,
                custom_properties.as_ref(),
            );

            for value in iter {
                let id = value.id();
                maybe_append_animation_value(id, Some(value));
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn Servo_GetAnimationValues(
    declarations: &RawServoDeclarationBlock,
    element: &RawGeckoElement,
    style: &ComputedValues,
    raw_data: &RawServoStyleSet,
    animation_values: &mut nsTArray<structs::RefPtr<structs::RawServoAnimationValue>>,
) {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let metrics = get_metrics_provider_for_product();

    let element = GeckoElement(element);
    let parent_element = element.inheritance_parent();
    let parent_data = parent_element.as_ref().and_then(|e| e.borrow_data());
    let parent_style = parent_data
        .as_ref()
        .map(|d| d.styles.primary())
        .map(|x| &**x);

    let mut conditions = Default::default();
    let mut context = create_context_for_animation(
        &data,
        &metrics,
        &style,
        parent_style,
        /* for_smil_animation = */ true,
        &mut conditions,
    );

    let default_values = data.default_computed_values();
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();

    let declarations = Locked::<PropertyDeclarationBlock>::as_arc(&declarations);
    let guard = declarations.read_with(&guard);
    let iter = guard.to_animation_value_iter(
        &mut context,
        &default_values,
        None, // SMIL has no extra custom properties.
    );
    for (index, anim) in iter.enumerate() {
        unsafe { animation_values.set_len((index + 1) as u32) };
        animation_values[index].set_arc_leaky(Arc::new(anim));
    }
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_GetPropertyId(
    value: &RawServoAnimationValue,
) -> nsCSSPropertyID {
    let value = AnimationValue::as_arc(&value);
    value.id().to_nscsspropertyid()
}

#[no_mangle]
pub extern "C" fn Servo_AnimationValue_Compute(
    element: &RawGeckoElement,
    declarations: &RawServoDeclarationBlock,
    style: &ComputedValues,
    raw_data: &RawServoStyleSet,
) -> Strong<RawServoAnimationValue> {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let metrics = get_metrics_provider_for_product();

    let element = GeckoElement(element);
    let parent_element = element.inheritance_parent();
    let parent_data = parent_element.as_ref().and_then(|e| e.borrow_data());
    let parent_style = parent_data
        .as_ref()
        .map(|d| d.styles.primary())
        .map(|x| &**x);

    let mut conditions = Default::default();
    let mut context = create_context_for_animation(
        &data,
        &metrics,
        style,
        parent_style,
        /* for_smil_animation = */ false,
        &mut conditions,
    );

    let default_values = data.default_computed_values();
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let declarations = Locked::<PropertyDeclarationBlock>::as_arc(&declarations);
    // We only compute the first element in declarations.
    match declarations
        .read_with(&guard)
        .declaration_importance_iter()
        .next()
    {
        Some((decl, imp)) if imp == Importance::Normal => {
            let animation = AnimationValue::from_declaration(
                decl,
                &mut context,
                None, // No extra custom properties for devtools.
                default_values,
            );
            animation.map_or(Strong::null(), |value| Arc::new(value).into_strong())
        },
        _ => Strong::null(),
    }
}

#[no_mangle]
pub extern "C" fn Servo_AssertTreeIsClean(root: &RawGeckoElement) {
    if !cfg!(feature = "gecko_debug") {
        panic!("Calling Servo_AssertTreeIsClean in release build");
    }

    let root = GeckoElement(root);
    debug!("Servo_AssertTreeIsClean: ");
    debug!("{:?}", ShowSubtreeData(root.as_node()));

    fn assert_subtree_is_clean<'le>(el: GeckoElement<'le>) {
        debug_assert!(
            !el.has_dirty_descendants() && !el.has_animation_only_dirty_descendants(),
            "{:?} has still dirty bit {:?} or animation-only dirty bit {:?}",
            el,
            el.has_dirty_descendants(),
            el.has_animation_only_dirty_descendants()
        );
        for child in el.traversal_children() {
            if let Some(child) = child.as_element() {
                assert_subtree_is_clean(child);
            }
        }
    }

    assert_subtree_is_clean(root);
}

#[no_mangle]
pub extern "C" fn Servo_IsWorkerThread() -> bool {
    thread_state::get().is_worker()
}

enum Offset {
    Zero,
    One,
}

fn fill_in_missing_keyframe_values(
    all_properties: &LonghandIdSet,
    timing_function: &nsTimingFunction,
    longhands_at_offset: &LonghandIdSet,
    offset: Offset,
    keyframes: &mut nsTArray<structs::Keyframe>,
) {
    // Return early if all animated properties are already set.
    if longhands_at_offset.contains_all(all_properties) {
        return;
    }

    let keyframe = match offset {
        Offset::Zero => unsafe { Gecko_GetOrCreateInitialKeyframe(keyframes, timing_function) },
        Offset::One => unsafe { Gecko_GetOrCreateFinalKeyframe(keyframes, timing_function) },
    };

    // Append properties that have not been set at this offset.
    for property in all_properties.iter() {
        if !longhands_at_offset.contains(property) {
            unsafe {
                Gecko_AppendPropertyValuePair(
                    &mut *(*keyframe).mPropertyValues,
                    property.to_nscsspropertyid(),
                );
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_GetKeyframesForName(
    raw_data: &RawServoStyleSet,
    element: &RawGeckoElement,
    style: &ComputedValues,
    name: *mut nsAtom,
    inherited_timing_function: &nsTimingFunction,
    keyframes: &mut nsTArray<structs::Keyframe>,
) -> bool {
    debug_assert!(keyframes.len() == 0, "keyframes should be initially empty");

    let element = GeckoElement(element);
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let name = Atom::from_raw(name);

    let animation = match data.stylist.get_animation(&name, element) {
        Some(animation) => animation,
        None => return false,
    };

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();

    let mut properties_set_at_current_offset = LonghandIdSet::new();
    let mut properties_set_at_start = LonghandIdSet::new();
    let mut properties_set_at_end = LonghandIdSet::new();
    let mut has_complete_initial_keyframe = false;
    let mut has_complete_final_keyframe = false;
    let mut current_offset = -1.;

    let writing_mode = style.writing_mode;

    // Iterate over the keyframe rules backwards so we can drop overridden
    // properties (since declarations in later rules override those in earlier
    // ones).
    for step in animation.steps.iter().rev() {
        if step.start_percentage.0 != current_offset {
            properties_set_at_current_offset.clear();
            current_offset = step.start_percentage.0;
        }

        // Override timing_function if the keyframe has an animation-timing-function.
        let timing_function = nsTimingFunction {
            mTiming: match step.get_animation_timing_function(&guard) {
                Some(val) => val.to_computed_value_without_context(),
                None => (*inherited_timing_function).mTiming.clone(),
            },
        };

        // Look for an existing keyframe with the same offset and timing
        // function or else add a new keyframe at the beginning of the keyframe
        // array.
        let keyframe = Gecko_GetOrCreateKeyframeAtStart(
            keyframes,
            step.start_percentage.0 as f32,
            &timing_function,
        );

        match step.value {
            KeyframesStepValue::ComputedValues => {
                // In KeyframesAnimation::from_keyframes if there is no 0% or
                // 100% keyframe at all, we will create a 'ComputedValues' step
                // to represent that all properties animated by the keyframes
                // animation should be set to the underlying computed value for
                // that keyframe.
                let mut seen = LonghandIdSet::new();
                for property in animation.properties_changed.iter() {
                    let property = property.to_physical(writing_mode);
                    if seen.contains(property) {
                        continue;
                    }
                    seen.insert(property);

                    Gecko_AppendPropertyValuePair(
                        &mut *(*keyframe).mPropertyValues,
                        property.to_nscsspropertyid(),
                    );
                }
                if current_offset == 0.0 {
                    has_complete_initial_keyframe = true;
                } else if current_offset == 1.0 {
                    has_complete_final_keyframe = true;
                }
            },
            KeyframesStepValue::Declarations { ref block } => {
                let guard = block.read_with(&guard);

                let mut custom_properties = PropertyDeclarationBlock::new();

                // Filter out non-animatable properties and properties with
                // !important.
                //
                // Also, iterate in reverse to respect the source order in case
                // there are logical and physical longhands in the same block.
                for declaration in guard.normal_declaration_iter().rev() {
                    let id = declaration.id();

                    let id = match id {
                        PropertyDeclarationId::Longhand(id) => {
                            // Skip the 'display' property because although it
                            // is animatable from SMIL, it should not be
                            // animatable from CSS Animations.
                            if id == LonghandId::Display {
                                continue;
                            }

                            if !id.is_animatable() {
                                continue;
                            }

                            id.to_physical(writing_mode)
                        },
                        PropertyDeclarationId::Custom(..) => {
                            custom_properties.push(declaration.clone(), Importance::Normal);
                            continue;
                        },
                    };

                    if properties_set_at_current_offset.contains(id) {
                        continue;
                    }

                    let pair = Gecko_AppendPropertyValuePair(
                        &mut *(*keyframe).mPropertyValues,
                        id.to_nscsspropertyid(),
                    );

                    (*pair).mServoDeclarationBlock.set_arc_leaky(Arc::new(
                        global_style_data
                            .shared_lock
                            .wrap(PropertyDeclarationBlock::with_one(
                                declaration.to_physical(writing_mode),
                                Importance::Normal,
                            )),
                    ));

                    if current_offset == 0.0 {
                        properties_set_at_start.insert(id);
                    } else if current_offset == 1.0 {
                        properties_set_at_end.insert(id);
                    }
                    properties_set_at_current_offset.insert(id);
                }

                if custom_properties.any_normal() {
                    let pair = Gecko_AppendPropertyValuePair(
                        &mut *(*keyframe).mPropertyValues,
                        nsCSSPropertyID::eCSSPropertyExtra_variable,
                    );

                    (*pair).mServoDeclarationBlock.set_arc_leaky(Arc::new(
                        global_style_data.shared_lock.wrap(custom_properties),
                    ));
                }
            },
        }
    }

    let mut properties_changed = LonghandIdSet::new();
    for property in animation.properties_changed.iter() {
        properties_changed.insert(property.to_physical(writing_mode));
    }

    // Append property values that are missing in the initial or the final keyframes.
    if !has_complete_initial_keyframe {
        fill_in_missing_keyframe_values(
            &properties_changed,
            inherited_timing_function,
            &properties_set_at_start,
            Offset::Zero,
            keyframes,
        );
    }
    if !has_complete_final_keyframe {
        fill_in_missing_keyframe_values(
            &properties_changed,
            inherited_timing_function,
            &properties_set_at_end,
            Offset::One,
            keyframes,
        );
    }
    true
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_GetFontFaceRules(
    raw_data: &RawServoStyleSet,
    rules: &mut nsTArray<structs::nsFontFaceRuleContainer>,
) {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    debug_assert_eq!(rules.len(), 0);

    let len: u32 = data
        .stylist
        .iter_extra_data_origins()
        .map(|(d, _)| d.font_faces.len() as u32)
        .sum();

    // Reversed iterator because Gecko expects rules to appear sorted
    // UserAgent first, Author last.
    let font_face_iter = data
        .stylist
        .iter_extra_data_origins_rev()
        .flat_map(|(d, o)| d.font_faces.iter().zip(iter::repeat(o)));

    unsafe { rules.set_len(len) };
    for ((&(ref rule, _), origin), dest) in font_face_iter.zip(rules.iter_mut()) {
        dest.mRule.set_arc_leaky(rule.clone());
        dest.mOrigin = origin;
    }
}

// XXX Ideally this should return a Option<&RawServoCounterStyleRule>,
// but we cannot, because the value from AtomicRefCell::borrow() can only
// live in this function, and thus anything derived from it cannot get the
// same lifetime as raw_data in parameter. See bug 1451543.
#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_GetCounterStyleRule(
    raw_data: &RawServoStyleSet,
    name: *mut nsAtom,
) -> *const RawServoCounterStyleRule {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    Atom::with(name, |name| {
        data.stylist
            .iter_extra_data_origins()
            .filter_map(|(d, _)| d.counter_styles.get(name))
            .next()
            .map_or(ptr::null(), |rule| rule.as_borrowed())
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleSet_GetScrollTimelineRule(
    raw_data: &RawServoStyleSet,
    name: *mut nsAtom,
) -> *const RawServoScrollTimelineRule {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    Atom::with(name, |name| {
        data.stylist
            .iter_extra_data_origins()
            .filter_map(|(d, _)| d.scroll_timelines.get(name))
            .next()
            .map_or(ptr::null(), |rule| rule.as_borrowed())
    })
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_BuildFontFeatureValueSet(
    raw_data: &RawServoStyleSet,
) -> *mut gfxFontFeatureValueSet {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();

    let has_rule = data
        .stylist
        .iter_extra_data_origins()
        .any(|(d, _)| !d.font_feature_values.is_empty());

    if !has_rule {
        return ptr::null_mut();
    }

    let font_feature_values_iter = data
        .stylist
        .iter_extra_data_origins_rev()
        .flat_map(|(d, _)| d.font_feature_values.iter());

    let set = unsafe { Gecko_ConstructFontFeatureValueSet() };
    for &(ref src, _) in font_feature_values_iter {
        let rule = src.read_with(&guard);
        rule.set_at_rules(set);
    }
    set
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_ResolveForDeclarations(
    raw_data: &RawServoStyleSet,
    parent_style_context: Option<&ComputedValues>,
    declarations: &RawServoDeclarationBlock,
) -> Strong<ComputedValues> {
    let doc_data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let guards = StylesheetGuards::same(&guard);

    let parent_style = match parent_style_context {
        Some(parent) => &*parent,
        None => doc_data.default_computed_values(),
    };

    let declarations = Locked::<PropertyDeclarationBlock>::as_arc(&declarations);

    doc_data
        .stylist
        .compute_for_declarations::<GeckoElement>(&guards, parent_style, declarations.clone_arc())
        .into()
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_AddSizeOfExcludingThis(
    malloc_size_of: GeckoMallocSizeOf,
    malloc_enclosing_size_of: GeckoMallocSizeOf,
    sizes: *mut ServoStyleSetSizes,
    raw_data: &RawServoStyleSet,
) {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow_mut();
    let mut ops = MallocSizeOfOps::new(
        malloc_size_of.unwrap(),
        Some(malloc_enclosing_size_of.unwrap()),
        None,
    );
    let sizes = unsafe { sizes.as_mut() }.unwrap();
    data.add_size_of(&mut ops, sizes);
}

#[no_mangle]
pub extern "C" fn Servo_UACache_AddSizeOf(
    malloc_size_of: GeckoMallocSizeOf,
    malloc_enclosing_size_of: GeckoMallocSizeOf,
    sizes: *mut ServoStyleSetSizes,
) {
    let mut ops = MallocSizeOfOps::new(
        malloc_size_of.unwrap(),
        Some(malloc_enclosing_size_of.unwrap()),
        None,
    );
    let sizes = unsafe { sizes.as_mut() }.unwrap();
    add_size_of_ua_cache(&mut ops, sizes);
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_MightHaveAttributeDependency(
    raw_data: &RawServoStyleSet,
    element: &RawGeckoElement,
    local_name: *mut nsAtom,
) -> bool {
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let element = GeckoElement(element);

    unsafe {
        AtomIdent::with(local_name, |atom| {
            data.stylist.any_applicable_rule_data(element, |data| {
                data.might_have_attribute_dependency(atom)
            })
        })
    }
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_HasStateDependency(
    raw_data: &RawServoStyleSet,
    element: &RawGeckoElement,
    state: u64,
) -> bool {
    let element = GeckoElement(element);

    let state = ElementState::from_bits_truncate(state);
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();

    data.stylist
        .any_applicable_rule_data(element, |data| data.has_state_dependency(state))
}

#[no_mangle]
pub extern "C" fn Servo_StyleSet_HasDocumentStateDependency(
    raw_data: &RawServoStyleSet,
    state: u64,
) -> bool {
    let state = DocumentState::from_bits_truncate(state);
    let data = PerDocumentStyleData::from_ffi(raw_data).borrow();

    data.stylist.has_document_state_dependency(state)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_GetPropertyValue(
    style: &ComputedValues,
    prop: nsCSSPropertyID,
    value: &mut nsACString,
) {
    if let Ok(longhand) = LonghandId::from_nscsspropertyid(prop) {
        style.get_resolved_value(longhand, value).unwrap();
        return;
    }

    let shorthand =
        ShorthandId::from_nscsspropertyid(prop).expect("Not a shorthand nor a longhand?");
    let mut block = PropertyDeclarationBlock::new();
    for longhand in shorthand.longhands() {
        block.push(style.resolved_declaration(longhand), Importance::Normal);
    }
    block.shorthand_to_css(shorthand, value).unwrap();
}

#[no_mangle]
pub unsafe extern "C" fn Servo_GetCustomPropertyValue(
    computed_values: &ComputedValues,
    name: &nsACString,
    value: &mut nsACString,
) -> bool {
    let custom_properties = match computed_values.custom_properties() {
        Some(p) => p,
        None => return false,
    };

    let name = Atom::from(name.as_str_unchecked());
    let computed_value = match custom_properties.get(&name) {
        Some(v) => v,
        None => return false,
    };

    computed_value.to_css(&mut CssWriter::new(value)).unwrap();
    true
}

#[no_mangle]
pub extern "C" fn Servo_GetCustomPropertiesCount(computed_values: &ComputedValues) -> u32 {
    match computed_values.custom_properties() {
        Some(p) => p.len() as u32,
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn Servo_GetCustomPropertyNameAt(
    computed_values: &ComputedValues,
    index: u32,
) -> *mut nsAtom {
    let custom_properties = match computed_values.custom_properties() {
        Some(p) => p,
        None => return ptr::null_mut(),
    };

    let property_name = match custom_properties.get_index(index as usize) {
        Some((key, _value)) => key,
        None => return ptr::null_mut(),
    };

    property_name.as_ptr()
}

#[no_mangle]
pub extern "C" fn Servo_CssUrl_IsLocalRef(url: &url::CssUrl) -> bool {
    url.is_fragment()
}

#[no_mangle]
pub extern "C" fn Servo_ProcessInvalidations(
    set: &RawServoStyleSet,
    element: &RawGeckoElement,
    snapshots: *const ServoElementSnapshotTable,
) {
    debug_assert!(!snapshots.is_null());

    let element = GeckoElement(element);
    debug_assert!(element.has_snapshot());
    debug_assert!(!element.handled_snapshot());

    let mut data = element.mutate_data();
    debug_assert!(data.is_some());

    let global_style_data = &*GLOBAL_STYLE_DATA;
    let guard = global_style_data.shared_lock.read();
    let per_doc_data = PerDocumentStyleData::from_ffi(set).borrow();
    let shared_style_context = create_shared_context(
        &global_style_data,
        &guard,
        &per_doc_data.stylist,
        TraversalFlags::empty(),
        unsafe { &*snapshots },
    );
    let mut data = data.as_mut().map(|d| &mut **d);

    if let Some(ref mut data) = data {
        // FIXME(emilio): Ideally we could share the nth-index-cache across all
        // the elements?
        let result = data.invalidate_style_if_needed(
            element,
            &shared_style_context,
            None,
            &mut NthIndexCache::default(),
        );

        if result.has_invalidated_siblings() {
            let parent = element
                .traversal_parent()
                .expect("How could we invalidate siblings without a common parent?");
            unsafe {
                parent.set_dirty_descendants();
                bindings::Gecko_NoteDirtySubtreeForInvalidation(parent.0);
            }
        } else if result.has_invalidated_descendants() {
            unsafe { bindings::Gecko_NoteDirtySubtreeForInvalidation(element.0) };
        } else if result.has_invalidated_self() {
            unsafe { bindings::Gecko_NoteDirtyElement(element.0) };
        }
    }
}

#[no_mangle]
pub extern "C" fn Servo_HasPendingRestyleAncestor(
    element: &RawGeckoElement,
    may_need_to_flush_layout: bool,
) -> bool {
    let mut has_yet_to_be_styled = false;
    let mut element = Some(GeckoElement(element));
    while let Some(e) = element {
        if e.has_any_animation() {
            return true;
        }

        // If the element needs a frame, it means that we haven't styled it yet
        // after it got inserted in the document, and thus we may need to do
        // that for transitions and animations to trigger.
        //
        // This is a fast path in the common case, but `has_yet_to_be_styled` is
        // the real check for this.
        if e.needs_frame() {
            return true;
        }

        let data = e.borrow_data();
        if let Some(ref data) = data {
            if !data.hint.is_empty() {
                return true;
            }
            if has_yet_to_be_styled && !data.styles.is_display_none() {
                return true;
            }
            // Ideally, DOM mutations wouldn't affect layout trees of siblings.
            //
            // In practice, this can happen because Gecko deals pretty badly
            // with some kinds of content insertion and removals.
            //
            // If we may need to flush layout, we need frames to accurately
            // determine whether we'll actually flush, so if we have to
            // reconstruct we need to flush style, which is what will take care
            // of ensuring that frames are constructed, even if the style itself
            // is up-to-date.
            if may_need_to_flush_layout && data.damage.contains(GeckoRestyleDamage::reconstruct()) {
                return true;
            }
        }
        has_yet_to_be_styled = data.is_none();

        element = e.traversal_parent();
    }
    false
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SelectorList_Parse(
    selector_list: &nsACString,
    is_chrome: bool,
) -> OwnedOrNull<RawServoSelectorList> {
    use style::selector_parser::SelectorParser;

    let url_data = UrlExtraData::from_ptr_ref(if is_chrome {
        &DUMMY_CHROME_URL_DATA
    } else {
        &DUMMY_URL_DATA
    });

    let input = selector_list.as_str_unchecked();
    let selector_list = match SelectorParser::parse_author_origin_no_namespace(&input, url_data) {
        Ok(selector_list) => selector_list,
        Err(..) => return OwnedOrNull::null(),
    };

    Box::new(selector_list).into_ffi().maybe()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SelectorList_Drop(list: *mut RawServoSelectorList) {
    SelectorList::drop_ffi(list)
}

fn parse_color(
    value: &str,
    error_reporter: Option<&dyn ParseErrorReporter>,
) -> Result<specified::Color, ()> {
    let mut input = ParserInput::new(value);
    let mut parser = Parser::new(&mut input);
    let url_data = unsafe { dummy_url_data() };
    let context = ParserContext::new(
        Origin::Author,
        url_data,
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        error_reporter,
        None,
    );

    let start_position = parser.position();
    parser
        .parse_entirely(|i| specified::Color::parse(&context, i))
        .map_err(|err| {
            if error_reporter.is_some() {
                match err.kind {
                    ParseErrorKind::Custom(StyleParseErrorKind::ValueError(..)) => {
                        let location = err.location.clone();
                        let error = ContextualParseError::UnsupportedValue(
                            parser.slice_from(start_position),
                            err,
                        );
                        context.log_css_error(location, error);
                    },
                    // Ignore other kinds of errors that might be reported, such as
                    // ParseErrorKind::Basic(BasicParseErrorKind::UnexpectedToken),
                    // since Gecko doesn't report those to the error console.
                    _ => {},
                }
            }
        })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_IsValidCSSColor(value: &nsACString) -> bool {
    parse_color(value.as_str_unchecked(), None).is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_ComputeColor(
    raw_data: Option<&RawServoStyleSet>,
    current_color: structs::nscolor,
    value: &nsACString,
    result_color: &mut structs::nscolor,
    was_current_color: *mut bool,
    loader: *mut Loader,
) -> bool {
    use style::gecko;

    let current_color = gecko::values::convert_nscolor_to_rgba(current_color);

    let reporter = loader.as_mut().and_then(|loader| {
        // Make an ErrorReporter that will report errors as being "from DOM".
        ErrorReporter::new(ptr::null_mut(), loader, ptr::null_mut())
    });

    let specified_color = match parse_color(
        value.as_str_unchecked(),
        reporter.as_ref().map(|r| r as &dyn ParseErrorReporter),
    ) {
        Ok(c) => c,
        Err(..) => return false,
    };

    let computed_color = match raw_data {
        Some(raw_data) => {
            let data = PerDocumentStyleData::from_ffi(raw_data).borrow();
            let device = data.stylist.device();
            let quirks_mode = data.stylist.quirks_mode();
            Context::for_media_query_evaluation(device, quirks_mode, |context| {
                specified_color.to_computed_color(Some(&context))
            })
        },
        None => specified_color.to_computed_color(None),
    };

    let computed_color = match computed_color {
        Some(c) => c,
        None => return false,
    };

    if !was_current_color.is_null() {
        *was_current_color = computed_color.is_currentcolor();
    }

    let rgba = computed_color.into_rgba(current_color);
    *result_color = gecko::values::convert_rgba_to_nscolor(&rgba);

    true
}

#[no_mangle]
pub extern "C" fn Servo_ResolveColor(
    color: &computed::Color,
    foreground: &style::values::RGBA,
) -> structs::nscolor {
    use style::gecko::values::convert_rgba_to_nscolor;
    convert_rgba_to_nscolor(&color.clone().into_rgba(*foreground))
}

#[no_mangle]
pub unsafe extern "C" fn Servo_IntersectionObserverRootMargin_Parse(
    value: &nsACString,
    result: *mut IntersectionObserverRootMargin,
) -> bool {
    let value = value.as_str_unchecked();
    let result = result.as_mut().unwrap();

    let mut input = ParserInput::new(&value);
    let mut parser = Parser::new(&mut input);

    let url_data = dummy_url_data();
    let context = ParserContext::new(
        Origin::Author,
        url_data,
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );

    let margin = parser.parse_entirely(|p| IntersectionObserverRootMargin::parse(&context, p));
    match margin {
        Ok(margin) => {
            *result = margin;
            true
        },
        Err(..) => false,
    }
}

#[no_mangle]
pub extern "C" fn Servo_IntersectionObserverRootMargin_ToString(
    root_margin: &IntersectionObserverRootMargin,
    result: &mut nsACString,
) {
    let mut writer = CssWriter::new(result);
    root_margin.to_css(&mut writer).unwrap();
}

#[no_mangle]
pub extern "C" fn Servo_ParseTransformIntoMatrix(
    value: &nsACString,
    contain_3d: &mut bool,
    result: &mut structs::Matrix4x4Components,
) -> bool {
    use style::properties::longhands::transform;

    let string = unsafe { value.as_str_unchecked() };
    let mut input = ParserInput::new(&string);
    let mut parser = Parser::new(&mut input);
    let context = ParserContext::new(
        Origin::Author,
        unsafe { dummy_url_data() },
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );

    let transform = match parser.parse_entirely(|t| transform::parse(&context, t)) {
        Ok(t) => t,
        Err(..) => return false,
    };

    let (m, is_3d) = match transform.to_transform_3d_matrix(None) {
        Ok(result) => result,
        Err(..) => return false,
    };

    *result = m.to_array();
    *contain_3d = is_3d;
    true
}

#[no_mangle]
pub unsafe extern "C" fn Servo_ParseFontShorthandForMatching(
    value: &nsACString,
    data: *mut URLExtraData,
    family: &mut FontFamilyList,
    style: &mut FontStyle,
    stretch: &mut FontStretch,
    weight: &mut FontWeight,
    size: Option<&mut f32>,
) -> bool {
    use style::properties::shorthands::font;
    use style::values::specified::font as specified;
    use style::values::generics::font::FontStyle as GenericFontStyle;

    let string = value.as_str_unchecked();
    let mut input = ParserInput::new(&string);
    let mut parser = Parser::new(&mut input);
    let url_data = UrlExtraData::from_ptr_ref(&data);
    let context = ParserContext::new(
        Origin::Author,
        url_data,
        Some(CssRuleType::FontFace),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );

    let font = match parser.parse_entirely(|f| font::parse_value(&context, f)) {
        Ok(f) => f,
        Err(..) => return false,
    };

    // The system font is not acceptable, so we return false.
    match font.font_family {
        specified::FontFamily::Values(list) => *family = list,
        specified::FontFamily::System(_) => return false,
    }

    let specified_font_style = match font.font_style {
        specified::FontStyle::Specified(ref s) => s,
        specified::FontStyle::System(_) => return false,
    };

    *style = match *specified_font_style {
        GenericFontStyle::Normal => FontStyle::NORMAL,
        GenericFontStyle::Italic => FontStyle::ITALIC,
        GenericFontStyle::Oblique(ref angle) => FontStyle::oblique(angle.degrees()),
    };

    *stretch = match font.font_stretch {
        specified::FontStretch::Keyword(ref k) => k.compute(),
        specified::FontStretch::Stretch(ref p) => FontStretch::from_percentage(p.0.get()),
        specified::FontStretch::System(_) => return false,
    };

    *weight = match font.font_weight {
        specified::FontWeight::Absolute(w) => w.compute(),
        // Resolve relative font weights against the initial of font-weight
        // (normal, which is equivalent to 400).
        specified::FontWeight::Bolder => FontWeight::normal().bolder(),
        specified::FontWeight::Lighter => FontWeight::normal().lighter(),
        specified::FontWeight::System(_) => return false,
    };

    // XXX This is unfinished; see values::specified::FontSize::ToComputedValue
    // for a more complete implementation (but we can't use it as-is).
    if let Some(size) = size {
        *size = match font.font_size {
            specified::FontSize::Length(lp) => {
                use style::values::generics::transform::ToAbsoluteLength;
                match lp.to_pixel_length(None) {
                    Ok(len) => len,
                    Err(..) => return false,
                }
            },
            // Map absolute-size keywords to sizes.
            specified::FontSize::Keyword(info) => {
                let metrics = get_metrics_provider_for_product();
                // TODO: Maybe get a meaningful language / quirks-mode from the
                // caller?
                let language = atom!("x-western");
                let quirks_mode = QuirksMode::NoQuirks;
                info.kw.to_length_without_context(quirks_mode, &metrics, &language, family).0.px()
            }
            // smaller, larger not currently supported
            specified::FontSize::Smaller | specified::FontSize::Larger | specified::FontSize::System(_) => {
                return false;
            }
        };
    }

    true
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SourceSizeList_Parse(
    value: &nsACString,
) -> Owned<RawServoSourceSizeList> {
    let value = value.as_str_unchecked();
    let mut input = ParserInput::new(value);
    let mut parser = Parser::new(&mut input);

    let context = ParserContext::new(
        Origin::Author,
        dummy_url_data(),
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );

    // NB: Intentionally not calling parse_entirely.
    let list = SourceSizeList::parse(&context, &mut parser);
    Box::new(list).into_ffi()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SourceSizeList_Evaluate(
    raw_data: &RawServoStyleSet,
    list: Option<&RawServoSourceSizeList>,
) -> i32 {
    let doc_data = PerDocumentStyleData::from_ffi(raw_data).borrow();
    let device = doc_data.stylist.device();
    let quirks_mode = doc_data.stylist.quirks_mode();

    let result = match list {
        Some(list) => SourceSizeList::from_ffi(list).evaluate(device, quirks_mode),
        None => SourceSizeList::empty().evaluate(device, quirks_mode),
    };

    result.0
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SourceSizeList_Drop(list: *mut RawServoSourceSizeList) {
    SourceSizeList::drop_ffi(list);
}

#[no_mangle]
pub unsafe extern "C" fn Servo_InvalidateStyleForDocStateChanges(
    root: &RawGeckoElement,
    document_style: &RawServoStyleSet,
    non_document_styles: &nsTArray<&RawServoAuthorStyles>,
    states_changed: u64,
) {
    use style::invalidation::element::document_state::DocumentStateInvalidationProcessor;
    use style::invalidation::element::invalidator::TreeStyleInvalidator;

    let document_data = PerDocumentStyleData::from_ffi(document_style).borrow();

    let iter = document_data
        .stylist
        .iter_origins()
        .map(|(data, _origin)| data)
        .chain(non_document_styles.iter().map(|author_styles| {
            let styles: &_ = AuthorStyles::<GeckoStyleSheet>::from_ffi(author_styles);
            &*styles.data
        }));

    let root = GeckoElement(root);
    let mut processor = DocumentStateInvalidationProcessor::new(
        iter,
        DocumentState::from_bits_truncate(states_changed),
        root.as_node().owner_doc().quirks_mode(),
    );

    let result =
        TreeStyleInvalidator::new(root, /* stack_limit_checker = */ None, &mut processor)
            .invalidate();

    debug_assert!(!result.has_invalidated_siblings(), "How in the world?");
    if result.has_invalidated_descendants() {
        bindings::Gecko_NoteDirtySubtreeForInvalidation(root.0);
    } else if result.has_invalidated_self() {
        bindings::Gecko_NoteDirtyElement(root.0);
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_PseudoClass_GetStates(name: &nsACString) -> u64 {
    let name = name.as_str_unchecked();
    match NonTSPseudoClass::parse_non_functional(name) {
        None => 0,
        // Ignore :any-link since it contains both visited and unvisited state.
        Some(NonTSPseudoClass::AnyLink) => 0,
        Some(pseudo_class) => pseudo_class.state_flag().bits(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_UseCounters_Create() -> Owned<structs::StyleUseCounters> {
    Box::<UseCounters>::default().into_ffi()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_UseCounters_Drop(c: *mut structs::StyleUseCounters) {
    UseCounters::drop_ffi(c);
}

#[no_mangle]
pub unsafe extern "C" fn Servo_UseCounters_Merge(
    doc_counters: &UseCounters,
    sheet_counters: &UseCounters,
) {
    doc_counters.merge(sheet_counters)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_IsPropertyIdRecordedInUseCounter(
    use_counters: &UseCounters,
    id: nsCSSPropertyID,
) -> bool {
    let id = NonCustomPropertyId::from_nscsspropertyid(id).unwrap();
    use_counters.non_custom_properties.recorded(id)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_IsUnknownPropertyRecordedInUseCounter(
    use_counters: &UseCounters,
    p: CountedUnknownProperty,
) -> bool {
    use_counters.counted_unknown_properties.recorded(p)
}

#[no_mangle]
pub unsafe extern "C" fn Servo_IsCssPropertyRecordedInUseCounter(
    use_counters: &UseCounters,
    property: &nsACString,
    known_prop: *mut bool,
) -> bool {
    *known_prop = false;

    let prop_name = property.as_str_unchecked();
    if let Ok(p) = PropertyId::parse_unchecked_for_testing(prop_name) {
        if let Some(id) = p.non_custom_id() {
            *known_prop = true;
            return use_counters.non_custom_properties.recorded(id);
        }
    }

    if let Some(p) = CountedUnknownProperty::parse_for_testing(prop_name) {
        *known_prop = true;
        return use_counters.counted_unknown_properties.recorded(p);
    }

    false
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SharedMemoryBuilder_Create(
    buffer: *mut u8,
    len: usize,
) -> *mut RawServoSharedMemoryBuilder {
    let builder = Box::new(SharedMemoryBuilder::new(buffer, len));
    Box::into_raw(builder) as *mut _
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SharedMemoryBuilder_AddStylesheet(
    builder: &mut RawServoSharedMemoryBuilder,
    raw_contents: &RawServoStyleSheetContents,
    error_message: &mut nsACString,
) -> *const ServoCssRules {
    let builder = SharedMemoryBuilder::from_ffi_mut(builder);
    let contents = StylesheetContents::as_arc(&raw_contents);

    // Assert some things we assume when we create a style sheet from shared
    // memory.
    debug_assert_eq!(contents.origin, Origin::UserAgent);
    debug_assert_eq!(contents.quirks_mode, QuirksMode::NoQuirks);
    debug_assert!(contents.source_map_url.read().is_none());
    debug_assert!(contents.source_url.read().is_none());

    match builder.write(&contents.rules) {
        Ok(rules_ptr) => {
            (*rules_ptr).with_raw_offset_arc(|arc| *Locked::arc_as_borrowed(arc) as *const _)
        },
        Err(message) => {
            error_message.assign(&message);
            ptr::null()
        },
    }
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SharedMemoryBuilder_GetLength(
    builder: &mut RawServoSharedMemoryBuilder,
) -> usize {
    let builder = SharedMemoryBuilder::from_ffi_mut(builder);
    builder.len()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_SharedMemoryBuilder_Drop(builder: *mut RawServoSharedMemoryBuilder) {
    SharedMemoryBuilder::drop_ffi(builder)
}

/// Returns a unique pointer to a clone of the shape image.
///
/// Probably temporary, as we move more stuff to cbindgen.
#[no_mangle]
#[must_use]
pub unsafe extern "C" fn Servo_CloneBasicShape(
    v: &computed::basic_shape::BasicShape,
) -> *mut computed::basic_shape::BasicShape {
    Box::into_raw(Box::new(v.clone()))
}

#[no_mangle]
pub unsafe extern "C" fn Servo_StyleArcSlice_EmptyPtr() -> *mut c_void {
    style_traits::arc_slice::ArcSlice::<u64>::leaked_empty_ptr()
}

#[no_mangle]
pub unsafe extern "C" fn Servo_LoadData_GetLazy(
    source: &url::LoadDataSource,
) -> *const url::LoadData {
    source.get()
}

#[no_mangle]
pub extern "C" fn Servo_LengthPercentage_ToCss(
    lp: &computed::LengthPercentage,
    result: &mut nsACString,
) {
    lp.to_css(&mut CssWriter::new(result)).unwrap();
}

#[no_mangle]
pub extern "C" fn Servo_FontStyle_ToCss(s: &FontStyle, result: &mut nsACString) {
    s.to_css(&mut CssWriter::new(result)).unwrap()
}

#[no_mangle]
pub extern "C" fn Servo_FontWeight_ToCss(w: &FontWeight, result: &mut nsACString) {
    w.to_css(&mut CssWriter::new(result)).unwrap()
}

#[no_mangle]
pub extern "C" fn Servo_FontStretch_ToCss(s: &FontStretch, result: &mut nsACString) {
    s.to_css(&mut CssWriter::new(result)).unwrap()
}

#[no_mangle]
pub extern "C" fn Servo_FontStretch_SerializeKeyword(s: &FontStretch, result: &mut nsACString) -> bool {
    let kw = match s.as_keyword() {
        Some(kw) => kw,
        None => return false,
    };
    kw.to_css(&mut CssWriter::new(result)).unwrap();
    true
}

#[no_mangle]
pub unsafe extern "C" fn Servo_CursorKind_Parse(
    cursor: &nsACString,
    result: &mut computed::ui::CursorKind,
) -> bool {
    match computed::ui::CursorKind::from_ident(cursor.as_str_unchecked()) {
        Ok(c) => {
            *result = c;
            true
        },
        Err(..) => false,
    }
}

#[no_mangle]
pub extern "C" fn Servo_FontFamily_Generic(generic: GenericFontFamily) -> &'static FontFamily {
    FontFamily::generic(generic)
}

#[no_mangle]
pub extern "C" fn Servo_FontFamily_ForSystemFont(name: &nsACString, out: &mut FontFamily) {
    *out = FontFamily::for_system_font(&name.to_utf8());
}

#[no_mangle]
pub extern "C" fn Servo_FontFamilyList_WithNames(
    names: &nsTArray<computed::font::SingleFontFamily>,
    out: &mut FontFamilyList,
) {
    *out = FontFamilyList {
        list: style_traits::arc_slice::ArcSlice::from_iter(names.iter().cloned()),
    };
}

#[no_mangle]
pub extern "C" fn Servo_GenericFontFamily_Parse(input: &nsACString) -> GenericFontFamily {
    let context = ParserContext::new(
        Origin::Author,
        unsafe { dummy_url_data() },
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );
    let value = input.to_utf8();
    let mut input = ParserInput::new(&value);
    let mut input = Parser::new(&mut input);
    GenericFontFamily::parse(&context, &mut input).unwrap_or(GenericFontFamily::None)
}

#[no_mangle]
pub extern "C" fn Servo_ColorScheme_Parse(input: &nsACString, out: &mut u8) -> bool {
    use style::values::specified::ColorScheme;

    let context = ParserContext::new(
        Origin::Author,
        unsafe { dummy_url_data() },
        Some(CssRuleType::Style),
        ParsingMode::DEFAULT,
        QuirksMode::NoQuirks,
        None,
        None,
    );
    let input = unsafe { input.as_str_unchecked() };
    let mut input = ParserInput::new(&input);
    let mut input = Parser::new(&mut input);
    let scheme = match input.parse_entirely(|i| ColorScheme::parse(&context, i)) {
        Ok(scheme) => scheme,
        Err(..) => return false,
    };
    *out = scheme.raw_bits();
    true
}

#[no_mangle]
pub extern "C" fn Servo_LayerBlockRule_GetName(
    rule: &RawServoLayerBlockRule,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &LayerBlockRule| {
        if let Some(ref name) = rule.name {
            name.to_css(&mut CssWriter::new(result)).unwrap()
        }
    })
}

#[no_mangle]
pub extern "C" fn Servo_LayerStatementRule_GetNameCount(
    rule: &RawServoLayerStatementRule,
) -> usize {
    read_locked_arc(rule, |rule: &LayerStatementRule| rule.names.len())
}

#[no_mangle]
pub extern "C" fn Servo_LayerStatementRule_GetNameAt(
    rule: &RawServoLayerStatementRule,
    index: usize,
    result: &mut nsACString,
) {
    read_locked_arc(rule, |rule: &LayerStatementRule| {
        if let Some(ref name) = rule.names.get(index) {
            name.to_css(&mut CssWriter::new(result)).unwrap()
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn Servo_InvalidateForViewportUnits(
    document_style: &RawServoStyleSet,
    root: &RawGeckoElement,
    dynamic_only: bool,
) {
    let document_data = PerDocumentStyleData::from_ffi(document_style).borrow();
    let device = document_data.stylist.device();

    if !device.used_viewport_size() {
        return;
    }

    if dynamic_only && !device.used_dynamic_viewport_size() {
        return;
    }

    if style::invalidation::viewport_units::invalidate(GeckoElement(root)) {
        // The invalidation machinery propagates the bits up, but we still need
        // to tell the Gecko restyle root machinery about it.
        bindings::Gecko_NoteDirtySubtreeForInvalidation(root);
    }
}

#[no_mangle]
pub extern "C" fn Servo_CreatePiecewiseLinearFunction(
    entries: &style::OwnedSlice<ComputedLinearStop>,
    result: &mut PiecewiseLinearFunction,
) {
    *result = PiecewiseLinearFunction::from_iter(
        entries
            .iter()
            .map(ComputedLinearStop::to_piecewise_linear_build_parameters),
    );
}

#[no_mangle]
pub extern "C" fn Servo_PiecewiseLinearFunctionAt(
    function: &PiecewiseLinearFunction,
    progress: f32,
) -> f32 {
    function.at(progress)
}

#[no_mangle]
pub extern "C" fn Servo_InterpolateColor(
    interpolation: &ColorInterpolationMethod,
    left: &AnimatedRGBA,
    right: &AnimatedRGBA,
    progress: f32,
) -> AnimatedRGBA {
    style::values::animated::color::Color::mix(
        interpolation,
        left,
        progress,
        right,
        1.0 - progress,
        /* normalize_weights = */ false,
    )
}
