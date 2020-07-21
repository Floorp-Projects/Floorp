[android-components](../index.md) / [mozilla.components.lib.state.ext](index.md) / [flowScoped](./flow-scoped.md)

# flowScoped

`@ExperimentalCoroutinesApi @MainThread fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../mozilla.components.lib.state/-action.md)`> `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`S`](flow-scoped.md#S)`, `[`A`](flow-scoped.md#A)`>.flowScoped(owner: LifecycleOwner? = null, block: suspend (Flow<`[`S`](flow-scoped.md#S)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): CoroutineScope` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/ext/StoreExtensions.kt#L178)

Launches a coroutine in a new [MainScope](#) and creates a [Flow](#) for observing [State](../mozilla.components.lib.state/-state.md) changes in
the [Store](../mozilla.components.lib.state/-store/index.md) in that scope. Invokes [block](flow-scoped.md#mozilla.components.lib.state.ext$flowScoped(mozilla.components.lib.state.Store((mozilla.components.lib.state.ext.flowScoped.S, mozilla.components.lib.state.ext.flowScoped.A)), androidx.lifecycle.LifecycleOwner, kotlin.SuspendFunction1((kotlinx.coroutines.flow.Flow((mozilla.components.lib.state.ext.flowScoped.S)), kotlin.Unit)))/block) inside that scope and passes the [Flow](#) to it.

### Parameters

`owner` - An optional [LifecycleOwner](#) that will be used to determine when to pause and resume
the store subscription. When the [Lifecycle](#) is in STOPPED state then no [State](../mozilla.components.lib.state/-state.md) will be received.
Once the [Lifecycle](#) switches back to at least STARTED state then the latest [State](../mozilla.components.lib.state/-state.md) and further
updates will be emitted.

**Return**
The [CoroutineScope](flow-scoped.md#mozilla.components.lib.state.ext$flowScoped(mozilla.components.lib.state.Store((mozilla.components.lib.state.ext.flowScoped.S, mozilla.components.lib.state.ext.flowScoped.A)), androidx.lifecycle.LifecycleOwner, kotlin.SuspendFunction1((kotlinx.coroutines.flow.Flow((mozilla.components.lib.state.ext.flowScoped.S)), kotlin.Unit)))/block) is getting executed in.

