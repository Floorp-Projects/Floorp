[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [ViewBoundFeatureWrapper](index.md) / [withFeature](./with-feature.md)

# withFeature

`@Synchronized fun withFeature(block: (`[`T`](index.md#T)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/ViewBoundFeatureWrapper.kt#L112)

Runs the given [block](with-feature.md#mozilla.components.support.base.feature.ViewBoundFeatureWrapper$withFeature(kotlin.Function1((mozilla.components.support.base.feature.ViewBoundFeatureWrapper.T, kotlin.Unit)))/block) if this wrapper still has a reference to the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md).

