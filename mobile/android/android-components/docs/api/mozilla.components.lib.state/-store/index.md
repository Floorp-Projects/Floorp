[android-components](../../index.md) / [mozilla.components.lib.state](../index.md) / [Store](./index.md)

# Store

`open class Store<S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt#L31)

A generic store holding an immutable [State](../-state.md).

The [State](../-state.md) can only be modified by dispatching [Action](../-action.md)s which will create a new state and notify all registered
[Observer](../-observer.md)s.

### Parameters

`initialState` - The initial state until a dispatched [Action](../-action.md) creates a new state.

`reducer` - A function that gets the current [State](../-state.md) and [Action](../-action.md) passed in and will return a new [State](../-state.md).

`middleware` - Optional list of [Middleware](../-middleware.md) sitting between the [Store](./index.md) and the [Reducer](../-reducer.md).

### Types

| Name | Summary |
|---|---|
| [Subscription](-subscription/index.md) | `class Subscription<S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`>`<br>A [Subscription](-subscription/index.md) is returned whenever an observer is registered via the [observeManually](observe-manually.md) method. Calling [unsubscribe](-subscription/unsubscribe.md) on the [Subscription](-subscription/index.md) will unregister the observer. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Store(initialState: `[`S`](index.md#S)`, reducer: `[`Reducer`](../-reducer.md)`<`[`S`](index.md#S)`, `[`A`](index.md#A)`>, middleware: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Middleware`](../-middleware.md)`<`[`S`](index.md#S)`, `[`A`](index.md#A)`>> = emptyList())`<br>A generic store holding an immutable [State](../-state.md). |

### Properties

| Name | Summary |
|---|---|
| [state](state.md) | `val state: `[`S`](index.md#S)<br>The current [State](../-state.md). |

### Functions

| Name | Summary |
|---|---|
| [dispatch](dispatch.md) | `fun dispatch(action: `[`A`](index.md#A)`): Job`<br>Dispatch an [Action](../-action.md) to the store in order to trigger a [State](../-state.md) change. |
| [observeManually](observe-manually.md) | `fun observeManually(observer: `[`Observer`](../-observer.md)`<`[`S`](index.md#S)`>): `[`Subscription`](-subscription/index.md)`<`[`S`](index.md#S)`, `[`A`](index.md#A)`>`<br>Registers an [Observer](../-observer.md) function that will be invoked whenever the [State](../-state.md) changes. |

### Extension Functions

| Name | Summary |
|---|---|
| [channel](../../mozilla.components.lib.state.ext/channel.md) | `fun <S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`> `[`Store`](./index.md)`<`[`S`](../../mozilla.components.lib.state.ext/channel.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/channel.md#A)`>.channel(owner: LifecycleOwner = ProcessLifecycleOwner.get()): ReceiveChannel<`[`S`](../../mozilla.components.lib.state.ext/channel.md#S)`>`<br>Creates a conflated [Channel](#) for observing [State](../-state.md) changes in the [Store](./index.md). |
| [flow](../../mozilla.components.lib.state.ext/flow.md) | `fun <S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`> `[`Store`](./index.md)`<`[`S`](../../mozilla.components.lib.state.ext/flow.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/flow.md#A)`>.flow(owner: LifecycleOwner? = null): Flow<`[`S`](../../mozilla.components.lib.state.ext/flow.md#S)`>`<br>Creates a [Flow](#) for observing [State](../-state.md) changes in the [Store](./index.md). |
| [flowScoped](../../mozilla.components.lib.state.ext/flow-scoped.md) | `fun <S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`> `[`Store`](./index.md)`<`[`S`](../../mozilla.components.lib.state.ext/flow-scoped.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/flow-scoped.md#A)`>.flowScoped(owner: LifecycleOwner? = null, block: suspend (Flow<`[`S`](../../mozilla.components.lib.state.ext/flow-scoped.md#S)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): CoroutineScope`<br>Launches a coroutine in a new [MainScope](#) and creates a [Flow](#) for observing [State](../-state.md) changes in the [Store](./index.md) in that scope. Invokes [block](../../mozilla.components.lib.state.ext/flow-scoped.md#mozilla.components.lib.state.ext$flowScoped(mozilla.components.lib.state.Store((mozilla.components.lib.state.ext.flowScoped.S, mozilla.components.lib.state.ext.flowScoped.A)), androidx.lifecycle.LifecycleOwner, kotlin.SuspendFunction1((kotlinx.coroutines.flow.Flow((mozilla.components.lib.state.ext.flowScoped.S)), kotlin.Unit)))/block) inside that scope and passes the [Flow](#) to it. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [observe](../../mozilla.components.lib.state.ext/observe.md) | `fun <S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`> `[`Store`](./index.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/observe.md#A)`>.observe(owner: LifecycleOwner, observer: `[`Observer`](../-observer.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an [Observer](../-observer.md) function that will be invoked whenever the state changes. The [Store.Subscription](-subscription/index.md) will be bound to the passed in [LifecycleOwner](#). Once the [Lifecycle](#) state changes to DESTROYED the [Observer](../-observer.md) will be unregistered automatically.`fun <S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`> `[`Store`](./index.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/observe.md#A)`>.observe(view: <ERROR CLASS>, observer: `[`Observer`](../-observer.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an [Observer](../-observer.md) function that will be invoked whenever the state changes. The [Store.Subscription](-subscription/index.md) will be bound to the passed in [View](#). Once the [View](#) gets detached the [Observer](../-observer.md) will be unregistered automatically. |
| [observeForever](../../mozilla.components.lib.state.ext/observe-forever.md) | `fun <S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`> `[`Store`](./index.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe-forever.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/observe-forever.md#A)`>.observeForever(observer: `[`Observer`](../-observer.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe-forever.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an [Observer](../-observer.md) function that will observe the store indefinitely. |
| [waitUntilIdle](../../mozilla.components.support.test.libstate.ext/wait-until-idle.md) | `fun <S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`> `[`Store`](./index.md)`<`[`S`](../../mozilla.components.support.test.libstate.ext/wait-until-idle.md#S)`, `[`A`](../../mozilla.components.support.test.libstate.ext/wait-until-idle.md#A)`>.waitUntilIdle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Blocks and returns once all dispatched actions have been processed i.e. the reducers have run and all observers have been notified of state changes. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) | `class BrowserStore : `[`Store`](./index.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>`<br>The [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) holds the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) (state tree). |
| [CustomTabsServiceStore](../../mozilla.components.feature.customtabs.store/-custom-tabs-service-store/index.md) | `class CustomTabsServiceStore : `[`Store`](./index.md)`<`[`CustomTabsServiceState`](../../mozilla.components.feature.customtabs.store/-custom-tabs-service-state/index.md)`, `[`CustomTabsAction`](../../mozilla.components.feature.customtabs.store/-custom-tabs-action/index.md)`>` |
| [MigrationStore](../../mozilla.components.support.migration.state/-migration-store/index.md) | `class MigrationStore : `[`Store`](./index.md)`<`[`MigrationState`](../../mozilla.components.support.migration.state/-migration-state/index.md)`, `[`MigrationAction`](../../mozilla.components.support.migration.state/-migration-action/index.md)`>`<br>[Store](./index.md) keeping track of the current [MigrationState](../../mozilla.components.support.migration.state/-migration-state/index.md). |
