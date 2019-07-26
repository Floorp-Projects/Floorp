[android-components](../../index.md) / [mozilla.components.browser.state.store](../index.md) / [BrowserStore](./index.md)

# BrowserStore

`class BrowserStore : `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/store/BrowserStore.kt#L20)

The [BrowserStore](./index.md) holds the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) (state tree).

The only way to change the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) inside [BrowserStore](./index.md) is to dispatch an [Action](../../mozilla.components.lib.state/-action.md) on it.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserStore(initialState: `[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)` = BrowserState())`<br>The [BrowserStore](./index.md) holds the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) (state tree). |

### Inherited Properties

| Name | Summary |
|---|---|
| [state](../../mozilla.components.lib.state/-store/state.md) | `val state: `[`S`](../../mozilla.components.lib.state/-store/index.md#S)<br>The current [State](../../mozilla.components.lib.state/-state.md). |

### Inherited Functions

| Name | Summary |
|---|---|
| [dispatch](../../mozilla.components.lib.state/-store/dispatch.md) | `fun dispatch(action: `[`A`](../../mozilla.components.lib.state/-store/index.md#A)`): Job`<br>Dispatch an [Action](../../mozilla.components.lib.state/-action.md) to the store in order to trigger a [State](../../mozilla.components.lib.state/-state.md) change. |
| [observeManually](../../mozilla.components.lib.state/-store/observe-manually.md) | `fun observeManually(observer: `[`Observer`](../../mozilla.components.lib.state/-observer.md)`<`[`S`](../../mozilla.components.lib.state/-store/index.md#S)`>): `[`Subscription`](../../mozilla.components.lib.state/-store/-subscription/index.md)`<`[`S`](../../mozilla.components.lib.state/-store/index.md#S)`, `[`A`](../../mozilla.components.lib.state/-store/index.md#A)`>`<br>Registers an [Observer](../../mozilla.components.lib.state/-observer.md) function that will be invoked whenever the [State](../../mozilla.components.lib.state/-state.md) changes. |

### Extension Functions

| Name | Summary |
|---|---|
| [channel](../../mozilla.components.lib.state.ext/channel.md) | `fun <S : `[`State`](../../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../../mozilla.components.lib.state/-action.md)`> `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`S`](../../mozilla.components.lib.state.ext/channel.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/channel.md#A)`>.channel(owner: LifecycleOwner = ProcessLifecycleOwner.get()): ReceiveChannel<`[`S`](../../mozilla.components.lib.state.ext/channel.md#S)`>`<br>Creates a conflated [Channel](#) for observing [State](../../mozilla.components.lib.state/-state.md) changes in the [Store](../../mozilla.components.lib.state/-store/index.md). |
| [observe](../../mozilla.components.lib.state.ext/observe.md) | `fun <S : `[`State`](../../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../../mozilla.components.lib.state/-action.md)`> `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/observe.md#A)`>.observe(owner: LifecycleOwner, observer: `[`Observer`](../../mozilla.components.lib.state/-observer.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an [Observer](../../mozilla.components.lib.state/-observer.md) function that will be invoked whenever the state changes. The [Store.Subscription](../../mozilla.components.lib.state/-store/-subscription/index.md) will be bound to the passed in [LifecycleOwner](#). Once the [Lifecycle](#) state changes to DESTROYED the [Observer](../../mozilla.components.lib.state/-observer.md) will be unregistered automatically.`fun <S : `[`State`](../../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../../mozilla.components.lib.state/-action.md)`> `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/observe.md#A)`>.observe(view: <ERROR CLASS>, observer: `[`Observer`](../../mozilla.components.lib.state/-observer.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an [Observer](../../mozilla.components.lib.state/-observer.md) function that will be invoked whenever the state changes. The [Store.Subscription](../../mozilla.components.lib.state/-store/-subscription/index.md) will be bound to the passed in [View](#). Once the [View](#) gets detached the [Observer](../../mozilla.components.lib.state/-observer.md) will be unregistered automatically. |
| [observeForever](../../mozilla.components.lib.state.ext/observe-forever.md) | `fun <S : `[`State`](../../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../../mozilla.components.lib.state/-action.md)`> `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe-forever.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/observe-forever.md#A)`>.observeForever(observer: `[`Observer`](../../mozilla.components.lib.state/-observer.md)`<`[`S`](../../mozilla.components.lib.state.ext/observe-forever.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an [Observer](../../mozilla.components.lib.state/-observer.md) function that will observe the store indefinitely. |
