[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngine](./index.md)

# GeckoEngine

`class GeckoEngine : `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngine.kt#L27)

Gecko-based implementation of Engine interface.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoEngine(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, defaultSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)`? = null, runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)` = GeckoRuntime.getDefault(context), executorProvider: () -> `[`GeckoWebExecutor`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoWebExecutor.html)` = { GeckoWebExecutor(runtime) })`<br>Gecko-based implementation of Engine interface. |

### Properties

| Name | Summary |
|---|---|
| [settings](settings.md) | `val settings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)<br>See [Engine.settings](../../mozilla.components.concept.engine/-engine/settings.md) |

### Functions

| Name | Summary |
|---|---|
| [createSession](create-session.md) | `fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)<br>Creates a new Gecko-based EngineSession. |
| [createSessionState](create-session-state.md) | `fun createSessionState(json: `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`): `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)<br>See [Engine.createSessionState](../../mozilla.components.concept.engine/-engine/create-session-state.md). |
| [createView](create-view.md) | `fun createView(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`?): `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)<br>Creates a new Gecko-based EngineView. |
| [installWebExtension](install-web-extension.md) | `fun installWebExtension(ext: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, onSuccess: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [Engine.installWebExtension](../../mozilla.components.concept.engine/-engine/install-web-extension.md). |
| [name](name.md) | `fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name of this engine. The returned string might be used in filenames and must therefore only contain valid filename characters. |
| [speculativeConnect](speculative-connect.md) | `fun speculativeConnect(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Opens a speculative connection to the host of [url](speculative-connect.md#mozilla.components.browser.engine.gecko.GeckoEngine$speculativeConnect(kotlin.String)/url). |
