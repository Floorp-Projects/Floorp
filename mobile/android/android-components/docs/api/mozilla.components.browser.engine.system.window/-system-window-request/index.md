[android-components](../../index.md) / [mozilla.components.browser.engine.system.window](../index.md) / [SystemWindowRequest](./index.md)

# SystemWindowRequest

`class SystemWindowRequest : `[`WindowRequest`](../../mozilla.components.concept.engine.window/-window-request/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/window/SystemWindowRequest.kt#L23)

WebView-based implementation of [WindowRequest](../../mozilla.components.concept.engine.window/-window-request/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SystemWindowRequest(webView: <ERROR CLASS>, newEngineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null, newWebView: <ERROR CLASS>? = null, openAsDialog: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, triggeredByUser: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, resultMsg: <ERROR CLASS>? = null, type: `[`Type`](../../mozilla.components.concept.engine.window/-window-request/-type/index.md)` = WindowRequest.Type.OPEN)`<br>WebView-based implementation of [WindowRequest](../../mozilla.components.concept.engine.window/-window-request/index.md). |

### Properties

| Name | Summary |
|---|---|
| [openAsDialog](open-as-dialog.md) | `val openAsDialog: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether or not the window should be opened as a dialog, defaults to false. |
| [triggeredByUser](triggered-by-user.md) | `val triggeredByUser: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether or not the request was triggered by the user, defaults to false. |
| [type](type.md) | `val type: `[`Type`](../../mozilla.components.concept.engine.window/-window-request/-type/index.md)<br>The [Type](../../mozilla.components.concept.engine.window/-window-request/-type/index.md) of this window request, indicating whether to open or close a window. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL which should be opened in a new window. May be empty if the request was created from JavaScript (using window.open()). |

### Functions

| Name | Summary |
|---|---|
| [prepare](prepare.md) | `fun prepare(): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)<br>Prepares an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the window request. This is used to attach state (e.g. a native session or view) to the engine session. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the window request. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
