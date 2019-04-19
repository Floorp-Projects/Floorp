[android-components](../../index.md) / [mozilla.components.browser.session.store](../index.md) / [BrowserStore](index.md) / [observe](./observe.md)

# observe

`fun observe(receiveInitialState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, observer: `[`Observer`](../-observer.md)`): `[`Subscription`](-subscription/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/store/BrowserStore.kt#L69)

Registers an observer function that will be invoked whenever the state changes.

### Parameters

`receiveInitialState` - If true the observer function will be invoked immediately with the current state.

**Return**
A subscription object that can be used to unsubscribe from further state changes.

