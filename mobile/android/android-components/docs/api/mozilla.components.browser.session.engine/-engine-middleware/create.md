[android-components](../../index.md) / [mozilla.components.browser.session.engine](../index.md) / [EngineMiddleware](index.md) / [create](./create.md)

# create

`fun create(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, sessionLookup: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?, scope: CoroutineScope = MainScope()): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Middleware`](../../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/engine/EngineMiddleware.kt#L39)

Creates a list of [Middleware](../../mozilla.components.lib.state/-middleware.md) to be installed on a [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) in order to support all
[EngineAction](../../mozilla.components.browser.state.action/-engine-action/index.md)s.

