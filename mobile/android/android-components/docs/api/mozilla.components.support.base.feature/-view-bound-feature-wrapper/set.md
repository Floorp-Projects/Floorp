[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [ViewBoundFeatureWrapper](index.md) / [set](./set.md)

# set

`@Synchronized fun set(feature: `[`T`](index.md#T)`, owner: LifecycleOwner, view: `[`View`](https://developer.android.com/reference/android/view/View.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/ViewBoundFeatureWrapper.kt#L84)

Sets the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) reference and binds it to the [Lifecycle](#) of the [LifecycleObserver](#) as well
as the [View](https://developer.android.com/reference/android/view/View.html).

The wrapper will take care of subscribing to the [Lifecycle](#) and forwarding start/stop events to the
[LifecycleAwareFeature](../-lifecycle-aware-feature/index.md).

Once the [View](https://developer.android.com/reference/android/view/View.html) gets detached the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) will be stopped and the wrapper will clear all
internal references.

