[android-components](../../index.md) / [mozilla.components.feature.readerview](../index.md) / [ReaderViewMiddleware](./index.md)

# ReaderViewMiddleware

`class ReaderViewMiddleware : `[`Middleware`](../../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/readerview/src/main/java/mozilla/components/feature/readerview/ReaderViewMiddleware.kt#L28)

[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for translating [BrowserAction](../../mozilla.components.browser.state.action/-browser-action.md)s to
[ReaderAction](../../mozilla.components.browser.state.action/-reader-action/index.md)s (e.g. if the URL is updated a new "readerable"
check should be executed.)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ReaderViewMiddleware()`<br>[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for translating [BrowserAction](../../mozilla.components.browser.state.action/-browser-action.md)s to [ReaderAction](../../mozilla.components.browser.state.action/-reader-action/index.md)s (e.g. if the URL is updated a new "readerable" check should be executed.) |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(store: `[`MiddlewareStore`](../../mozilla.components.lib.state/-middleware-store/index.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>, next: (`[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, action: `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
