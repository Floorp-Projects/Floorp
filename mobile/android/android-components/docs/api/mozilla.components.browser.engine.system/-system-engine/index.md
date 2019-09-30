[android-components](../../index.md) / [mozilla.components.browser.engine.system](../index.md) / [SystemEngine](./index.md)

# SystemEngine

`class SystemEngine : `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/SystemEngine.kt#L27)

WebView-based implementation of the Engine interface.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SystemEngine(context: <ERROR CLASS>, defaultSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)` = DefaultSettings())`<br>WebView-based implementation of the Engine interface. |

### Properties

| Name | Summary |
|---|---|
| [settings](settings.md) | `val settings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)<br>See [Engine.settings](../../mozilla.components.concept.engine/-engine/settings.md) |
| [version](version.md) | `val version: `[`EngineVersion`](../../mozilla.components.concept.engine.utils/-engine-version/index.md)<br>Returns the version of the engine as [EngineVersion](../../mozilla.components.concept.engine.utils/-engine-version/index.md) object. |

### Inherited Properties

| Name | Summary |
|---|---|
| [trackingProtectionExceptionStore](../../mozilla.components.concept.engine/-engine/tracking-protection-exception-store.md) | `open val trackingProtectionExceptionStore: `[`TrackingProtectionExceptionStorage`](../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception-storage/index.md)<br>Provides access to the tracking protection exception list for this engine. |

### Functions

| Name | Summary |
|---|---|
| [createSession](create-session.md) | `fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)<br>Creates a new WebView-based EngineSession implementation. |
| [createSessionState](create-session-state.md) | `fun createSessionState(json: <ERROR CLASS>): `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)<br>Create a new [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md) instance from the serialized JSON representation. |
| [createView](create-view.md) | `fun createView(context: <ERROR CLASS>, attrs: <ERROR CLASS>?): `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)<br>Creates a new WebView-based EngineView implementation. |
| [name](name.md) | `fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>See [Engine.name](../../mozilla.components.concept.engine/-engine/name.md) |
| [speculativeConnect](speculative-connect.md) | `fun speculativeConnect(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Opens a speculative connection to the host of [url](speculative-connect.md#mozilla.components.browser.engine.system.SystemEngine$speculativeConnect(kotlin.String)/url). |

### Inherited Functions

| Name | Summary |
|---|---|
| [clearData](../../mozilla.components.concept.engine/-engine/clear-data.md) | `open fun clearData(data: `[`BrowsingData`](../../mozilla.components.concept.engine/-engine/-browsing-data/index.md)` = BrowsingData.all(), host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears browsing data stored by the engine. |
| [getTrackersLog](../../mozilla.components.concept.engine/-engine/get-trackers-log.md) | `open fun getTrackersLog(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, onSuccess: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackerLog`](../../mozilla.components.concept.engine.content.blocking/-tracker-log/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fetch a list of trackers logged for a given [session](../../mozilla.components.concept.engine/-engine/get-trackers-log.md#mozilla.components.concept.engine.Engine$getTrackersLog(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.collections.List((mozilla.components.concept.engine.content.blocking.TrackerLog)), kotlin.Unit)), kotlin.Function1((kotlin.Throwable, kotlin.Unit)))/session) . |
| [installWebExtension](../../mozilla.components.concept.engine/-engine/install-web-extension.md) | `open fun installWebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, allowContentMessaging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, onSuccess: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Installs the provided extension in this engine. |
| [warmUp](../../mozilla.components.concept.engine/-engine/warm-up.md) | `open fun warmUp(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes sure all required engine initialization logic is executed. The details are specific to individual implementations, but the following must be true: |

### Companion Object Properties

| Name | Summary |
|---|---|
| [defaultUserAgent](default-user-agent.md) | `var defaultUserAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
