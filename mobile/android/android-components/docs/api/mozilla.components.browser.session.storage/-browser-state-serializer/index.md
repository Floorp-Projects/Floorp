[android-components](../../index.md) / [mozilla.components.browser.session.storage](../index.md) / [BrowserStateSerializer](./index.md)

# BrowserStateSerializer

`class BrowserStateSerializer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/storage/BrowserStateSerializer.kt#L17)

Helper to transform [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) instances to JSON and.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserStateSerializer()`<br>Helper to transform [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) instances to JSON and. |

### Functions

| Name | Summary |
|---|---|
| [toJSON](to-j-s-o-n.md) | `fun toJSON(state: `[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Serializes the provided [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to JSON. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
