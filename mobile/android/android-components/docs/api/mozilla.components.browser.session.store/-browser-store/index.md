[android-components](../../index.md) / [mozilla.components.browser.session.store](../index.md) / [BrowserStore](./index.md)

# BrowserStore

`class BrowserStore` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/store/BrowserStore.kt#L23)

The [BrowserStore](./index.md) holds the [BrowserState](../../mozilla.components.browser.session.state/-browser-state/index.md) (state tree).

The only way to change the [BrowserState](../../mozilla.components.browser.session.state/-browser-state/index.md) inside [BrowserStore](./index.md) is to dispatch an [Action](../../mozilla.components.browser.session.action/-action.md) on it.

### Types

| Name | Summary |
|---|---|
| [Subscription](-subscription/index.md) | `class Subscription`<br>A [Subscription](-subscription/index.md) is returned whenever an observer is registered via the [observe](observe.md) method. Calling [unsubscribe](-subscription/unsubscribe.md) on the [Subscription](-subscription/index.md) will unregister the observer. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserStore(initialState: `[`BrowserState`](../../mozilla.components.browser.session.state/-browser-state/index.md)`)`<br>The [BrowserStore](./index.md) holds the [BrowserState](../../mozilla.components.browser.session.state/-browser-state/index.md) (state tree). |

### Properties

| Name | Summary |
|---|---|
| [state](state.md) | `val state: `[`BrowserState`](../../mozilla.components.browser.session.state/-browser-state/index.md)<br>Returns the current state. |

### Functions

| Name | Summary |
|---|---|
| [dispatch](dispatch.md) | `fun dispatch(action: `[`Action`](../../mozilla.components.browser.session.action/-action.md)`): Job`<br>Dispatch an [Action](../../mozilla.components.browser.session.action/-action.md) to the store in order to trigger a [BrowserState](../../mozilla.components.browser.session.state/-browser-state/index.md) change. |
| [observe](observe.md) | `fun observe(receiveInitialState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, observer: `[`Observer`](../-observer.md)`): `[`Subscription`](-subscription/index.md)<br>Registers an observer function that will be invoked whenever the state changes. |

### Extension Functions

| Name | Summary |
|---|---|
| [observe](../../mozilla.components.browser.session.ext/observe.md) | `fun `[`BrowserStore`](./index.md)`.observe(owner: LifecycleOwner, receiveInitialState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, observer: `[`Observer`](../-observer.md)`): `[`Subscription`](-subscription/index.md)<br>Registers an [Observer](../-observer.md) function that will be invoked whenever the state changes. The [BrowserStore.Subscription](-subscription/index.md) will be bound to the passed in [LifecycleOwner](#). Once the [Lifecycle](#) state changes to DESTROYED the [Observer](../-observer.md) will be unregistered automatically.`fun `[`BrowserStore`](./index.md)`.observe(view: `[`View`](https://developer.android.com/reference/android/view/View.html)`, observer: `[`Observer`](../-observer.md)`, receiveInitialState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Subscription`](-subscription/index.md)<br>Registers an [Observer](../-observer.md) function that will be invoked whenever the state changes. The [BrowserStore.Subscription](-subscription/index.md) will be bound to the passed in [View](https://developer.android.com/reference/android/view/View.html). Once the [View](https://developer.android.com/reference/android/view/View.html) gets detached the [Observer](../-observer.md) will be unregistered automatically. |
