[android-components](../../index.md) / [mozilla.components.concept.engine.window](../index.md) / [WindowRequest](./index.md)

# WindowRequest

`interface WindowRequest` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/window/WindowRequest.kt#L12)

Represents a request to open or close a browser window.

### Types

| Name | Summary |
|---|---|
| [Type](-type/index.md) | `enum class Type`<br>Describes the different types of window requests. |

### Properties

| Name | Summary |
|---|---|
| [type](type.md) | `abstract val type: `[`Type`](-type/index.md)<br>The [Type](-type/index.md) of this window request, indicating whether to open or close a window. |
| [url](url.md) | `abstract val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL which should be opened in a new window. May be empty if the request was created from JavaScript (using window.open()). |

### Functions

| Name | Summary |
|---|---|
| [prepare](prepare.md) | `abstract fun prepare(): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)<br>Prepares an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the window request. This is used to attach state (e.g. a native session or view) to the engine session. |
| [start](start.md) | `open fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the window request. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoWindowRequest](../../mozilla.components.browser.engine.gecko.window/-gecko-window-request/index.md) | `class GeckoWindowRequest : `[`WindowRequest`](./index.md)<br>Gecko-based implementation of [WindowRequest](./index.md). |
| [SystemWindowRequest](../../mozilla.components.browser.engine.system.window/-system-window-request/index.md) | `class SystemWindowRequest : `[`WindowRequest`](./index.md)<br>WebView-based implementation of [WindowRequest](./index.md). |
