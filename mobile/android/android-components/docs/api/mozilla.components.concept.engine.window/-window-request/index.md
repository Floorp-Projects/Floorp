[android-components](../../index.md) / [mozilla.components.concept.engine.window](../index.md) / [WindowRequest](./index.md)

# WindowRequest

`interface WindowRequest` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/window/WindowRequest.kt#L12)

Represents a request to open or close a browser window.

### Properties

| Name | Summary |
|---|---|
| [url](url.md) | `abstract val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL which should be opened in a new window. May be empty if the request was created from JavaScript (using window.open()). |

### Functions

| Name | Summary |
|---|---|
| [prepare](prepare.md) | `abstract fun prepare(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Prepares the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the window request. This is used to attach state (e.g. a native session) to the engine session. |
| [start](start.md) | `abstract fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the window request. |

### Inheritors

| Name | Summary |
|---|---|
| [SystemWindowRequest](../../mozilla.components.browser.engine.system.window/-system-window-request/index.md) | `class SystemWindowRequest : `[`WindowRequest`](./index.md)<br>WebView-based implementation of [WindowRequest](./index.md). |
