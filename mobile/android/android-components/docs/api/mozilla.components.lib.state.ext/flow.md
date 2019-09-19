[android-components](../index.md) / [mozilla.components.lib.state.ext](index.md) / [flow](./flow.md)

# flow

`@ExperimentalCoroutinesApi @MainThread fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../mozilla.components.lib.state/-action.md)`> `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`S`](flow.md#S)`, `[`A`](flow.md#A)`>.flow(owner: LifecycleOwner? = null): Flow<`[`S`](flow.md#S)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/ext/StoreExtensions.kt#L144)

Creates a [Flow](#) for observing [State](../mozilla.components.lib.state/-state.md) changes in the [Store](../mozilla.components.lib.state/-store/index.md).

### Parameters

`owner` - An optional [LifecycleOwner](#) that will be used to determine when to pause and resume
the store subscription. When the [Lifecycle](#) is in STOPPED state then no [State](../mozilla.components.lib.state/-state.md) will be received.
Once the [Lifecycle](#) switches back to at least STARTED state then the latest [State](../mozilla.components.lib.state/-state.md) and further
updates will be emitted.