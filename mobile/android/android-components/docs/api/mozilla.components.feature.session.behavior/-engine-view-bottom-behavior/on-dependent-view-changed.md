[android-components](../../index.md) / [mozilla.components.feature.session.behavior](../index.md) / [EngineViewBottomBehavior](index.md) / [onDependentViewChanged](./on-dependent-view-changed.md)

# onDependentViewChanged

`fun onDependentViewChanged(parent: CoordinatorLayout, child: <ERROR CLASS>, dependency: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/behavior/EngineViewBottomBehavior.kt#L45)

Apply vertical clipping to [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md). This requires [EngineViewBottomBehavior](index.md) to be set
in/on the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) or its parent. Must be a direct descending child of [CoordinatorLayout](#).

