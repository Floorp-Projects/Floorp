[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [ViewBoundFeatureWrapper](index.md) / [set](./set.md)

# set

`@Synchronized fun set(feature: `[`T`](index.md#T)`, owner: LifecycleOwner, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/ViewBoundFeatureWrapper.kt#L84)

Sets the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) reference and binds it to the [Lifecycle](#) of the [LifecycleObserver](#) as well
as the [View](#).

The wrapper will take care of subscribing to the [Lifecycle](#) and forwarding start/stop events to the
[LifecycleAwareFeature](../-lifecycle-aware-feature/index.md).

Once the [View](#) gets detached the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) will be stopped and the wrapper will clear all
internal references.

