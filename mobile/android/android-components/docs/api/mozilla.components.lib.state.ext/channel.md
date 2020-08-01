[android-components](../index.md) / [mozilla.components.lib.state.ext](index.md) / [channel](./channel.md)

# channel

`@ExperimentalCoroutinesApi @MainThread fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../mozilla.components.lib.state/-action.md)`> `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`S`](channel.md#S)`, `[`A`](channel.md#A)`>.channel(owner: LifecycleOwner = ProcessLifecycleOwner.get()): ReceiveChannel<`[`S`](channel.md#S)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/ext/StoreExtensions.kt#L111)

Creates a conflated [Channel](#) for observing [State](../mozilla.components.lib.state/-state.md) changes in the [Store](../mozilla.components.lib.state/-store/index.md).

The advantage of a [Channel](#) is that [State](../mozilla.components.lib.state/-state.md) changes can be processed sequentially in order from
a single coroutine (e.g. on the main thread).

### Parameters

`owner` - A [LifecycleOwner](#) that will be used to determine when to pause and resume the store
subscription. When the [Lifecycle](#) is in STOPPED state then no [State](../mozilla.components.lib.state/-state.md) will be received. Once the
[Lifecycle](#) switches back to at least STARTED state then the latest [State](../mozilla.components.lib.state/-state.md) and further updates
will be received.