[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [ViewBoundFeatureWrapper](index.md) / [onBackPressed](./on-back-pressed.md)

# onBackPressed

`@Synchronized fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/ViewBoundFeatureWrapper.kt#L146)

Convenient method for invoking [UserInteractionHandler.onBackPressed](../-user-interaction-handler/on-back-pressed.md) on a wrapped
[LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) that implements [UserInteractionHandler](../-user-interaction-handler/index.md). Returns false if
the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) was cleared already.

