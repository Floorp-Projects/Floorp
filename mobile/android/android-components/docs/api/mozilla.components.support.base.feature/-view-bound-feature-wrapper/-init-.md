[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [ViewBoundFeatureWrapper](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ViewBoundFeatureWrapper(feature: `[`T`](index.md#T)`, owner: LifecycleOwner, view: <ERROR CLASS>)`

Convenient constructor for creating a wrapper instance and calling [set](set.md) immediately.

`ViewBoundFeatureWrapper()`

Wrapper for [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) instances that keep a strong references to a [View](#). This wrapper is helpful
when the lifetime of the [View](#) may be shorter than the [Lifecycle](#) and you need to keep a reference to the
[LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) that may outlive the [View](#).

[ViewBoundFeatureWrapper](index.md) takes care of stopping [LifecycleAwareFeature](../-lifecycle-aware-feature/index.md) and clearing references once the bound
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

