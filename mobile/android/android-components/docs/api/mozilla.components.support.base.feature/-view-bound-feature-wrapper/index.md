[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [ViewBoundFeatureWrapper](./index.md)

# ViewBoundFeatureWrapper

`class ViewBoundFeatureWrapper<T : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/ViewBoundFeatureWrapper.kt#L56)

Wrapper for [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) instances that keep a strong references to a [View](#). This wrapper is helpful
when the lifetime of the [View](#) may be shorter than the [Lifecycle](#) and you need to keep a reference to the
[LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) that may outlive the [View](#).

[ViewBoundFeatureWrapper](./index.md) takes care of stopping [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) and clearing references once the bound
[View](#) get detached.

A common use case is a `Fragment` that needs to keep a reference to a [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) (e.g. to invoke
`onBackPressed()` and the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) holds a reference to [View](#) instances. Once the `Fragment` gets
detached and not destroyed (e.g. when pushed to the back stack) it will still keep the reference to the
[LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) and therefore to the (now detached) [View](#) (-&gt; Leak). When the `Fragment` gets re-attached a
new [View](#) and matching [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) is getting created leading to multiple concurrent
[LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) and (off-screen) [View](#) instances existing in memory.

Example integration:

```
class MyFragment : Fragment {
    val myFeature = ViewBoundFeatureWrapper<MyFeature>()

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        // Bind wrapper to feature and view. Feature will be stopped and internal reference will be cleared
        // when the view gets detached.
        myFeature.set(
            feature = MyFeature(..., view),
            owner = this,
            view = view
        )
    }

    fun doSomething() {
        // Get will return the feature instance or null if the instance was cleared (e.g. the fragment is detached)
        myFeature.get()?.doSomething()
    }

    override fun onBackPressed(): Boolean {
        return myFeature.onBackPressed()
    }
}
```

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ViewBoundFeatureWrapper(feature: `[`T`](index.md#T)`, owner: LifecycleOwner, view: <ERROR CLASS>)`<br>Convenient constructor for creating a wrapper instance and calling [set](set.md) immediately.`ViewBoundFeatureWrapper()`<br>Wrapper for [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) instances that keep a strong references to a [View](#). This wrapper is helpful when the lifetime of the [View](#) may be shorter than the [Lifecycle](#) and you need to keep a reference to the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) that may outlive the [View](#). |

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the feature and clears all internal references and observers. |
| [get](get.md) | `fun get(): `[`T`](index.md#T)`?`<br>Returns the wrapped [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) or null if the [View](#) was detached and the reference was cleared. |
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Convenient method for invoking [BackHandler.onBackPressed](../-back-handler/on-back-pressed.md) on a wrapped [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) that implements [BackHandler](../-back-handler/index.md). Returns false if the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) was cleared already. |
| [set](set.md) | `fun set(feature: `[`T`](index.md#T)`, owner: LifecycleOwner, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) reference and binds it to the [Lifecycle](#) of the [LifecycleObserver](#) as well as the [View](#). |
| [withFeature](with-feature.md) | `fun withFeature(block: (`[`T`](index.md#T)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Runs the given [block](with-feature.md#mozilla.components.support.base.feature.ViewBoundFeatureWrapper$withFeature(kotlin.Function1((mozilla.components.support.base.feature.ViewBoundFeatureWrapper.T, kotlin.Unit)))/block) if this wrapper still has a reference to the [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md). |
