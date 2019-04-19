[android-components](../index.md) / [mozilla.components.browser.session.helper](index.md) / [onlyIfChanged](./only-if-changed.md)

# onlyIfChanged

`fun <T> onlyIfChanged(onMainThread: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, map: (`[`BrowserState`](../mozilla.components.browser.session.state/-browser-state/index.md)`) -> `[`T`](only-if-changed.md#T)`?, then: (`[`BrowserState`](../mozilla.components.browser.session.state/-browser-state/index.md)`, `[`T`](only-if-changed.md#T)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Observer`](../mozilla.components.browser.session.store/-observer.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/helper/Helpers.kt#L22)

Creates an [Observer](../mozilla.components.browser.session.store/-observer.md) that will map the received [BrowserState](../mozilla.components.browser.session.state/-browser-state/index.md) to [T](only-if-changed.md#T) (using [map](only-if-changed.md#mozilla.components.browser.session.helper$onlyIfChanged(kotlin.Boolean, kotlin.Function1((mozilla.components.browser.session.state.BrowserState, mozilla.components.browser.session.helper.onlyIfChanged.T)), kotlin.Function2((mozilla.components.browser.session.state.BrowserState, mozilla.components.browser.session.helper.onlyIfChanged.T, kotlin.Unit)))/map)) and will invoke the callback
[then](only-if-changed.md#mozilla.components.browser.session.helper$onlyIfChanged(kotlin.Boolean, kotlin.Function1((mozilla.components.browser.session.state.BrowserState, mozilla.components.browser.session.helper.onlyIfChanged.T)), kotlin.Function2((mozilla.components.browser.session.state.BrowserState, mozilla.components.browser.session.helper.onlyIfChanged.T, kotlin.Unit)))/then) only if the value has changed from the last mapped value.

### Parameters

`onMainThread` - Whether or not the [then](only-if-changed.md#mozilla.components.browser.session.helper$onlyIfChanged(kotlin.Boolean, kotlin.Function1((mozilla.components.browser.session.state.BrowserState, mozilla.components.browser.session.helper.onlyIfChanged.T)), kotlin.Function2((mozilla.components.browser.session.state.BrowserState, mozilla.components.browser.session.helper.onlyIfChanged.T, kotlin.Unit)))/then) function should be executed on the main thread.

`map` - A function that maps [BrowserState](../mozilla.components.browser.session.state/-browser-state/index.md) to the value we want to observe for changes.

`then` - Function that gets invoked whenever the mapped value changed.