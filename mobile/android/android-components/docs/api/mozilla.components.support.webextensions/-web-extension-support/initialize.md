[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionSupport](index.md) / [initialize](./initialize.md)

# initialize

`fun initialize(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, onNewTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onCloseTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionSupport.kt#L42)

Registers a listener for web extension related events on the provided
[Engine](../../mozilla.components.concept.engine/-engine/index.md) and reacts by dispatching the corresponding actions to the
provided [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

### Parameters

`engine` - the browser [Engine](../../mozilla.components.concept.engine/-engine/index.md) to use.

`store` - the application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

`onNewTabOverride` - (optional) override of behaviour that should
be triggered when web extensions open a new tab e.g. when dispatching
to the store isn't sufficient while migrating from browser-session
to browser-state. This is a lambda accepting the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), the
[EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to use, as well as the URL to load.

`onCloseTabOverride` - (optional) override of behaviour that should
be triggered when web extensions close tabs e.g. when dispatching
to the store isn't sufficient while migrating from browser-session
to browser-state. This is a lambda  accepting the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) and
the session/tab ID to close.