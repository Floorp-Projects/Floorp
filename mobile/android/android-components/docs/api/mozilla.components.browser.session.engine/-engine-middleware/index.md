[android-components](../../index.md) / [mozilla.components.browser.session.engine](../index.md) / [EngineMiddleware](./index.md)

# EngineMiddleware

`object EngineMiddleware` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/engine/EngineMiddleware.kt#L34)

Helper for creating a list of [Middleware](../../mozilla.components.lib.state/-middleware.md) instances for supporting all [EngineAction](../../mozilla.components.browser.state.action/-engine-action/index.md)s.

### Functions

| Name | Summary |
|---|---|
| [create](create.md) | `fun create(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, sessionLookup: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?, scope: CoroutineScope = MainScope()): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Middleware`](../../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>>`<br>Creates a list of [Middleware](../../mozilla.components.lib.state/-middleware.md) to be installed on a [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) in order to support all [EngineAction](../../mozilla.components.browser.state.action/-engine-action/index.md)s. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
