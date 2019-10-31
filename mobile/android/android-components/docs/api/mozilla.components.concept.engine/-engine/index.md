[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](./index.md)

# Engine

`interface Engine` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L25)

Entry point for interacting with the engine implementation.

### Types

| Name | Summary |
|---|---|
| [BrowsingData](-browsing-data/index.md) | `class BrowsingData`<br>Describes a combination of browsing data types stored by the engine. |

### Properties

| Name | Summary |
|---|---|
| [settings](settings.md) | `abstract val settings: `[`Settings`](../-settings/index.md)<br>Provides access to the settings of this engine. |
| [trackingProtectionExceptionStore](tracking-protection-exception-store.md) | `open val trackingProtectionExceptionStore: `[`TrackingProtectionExceptionStorage`](../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception-storage/index.md)<br>Provides access to the tracking protection exception list for this engine. |
| [version](version.md) | `abstract val version: `[`EngineVersion`](../../mozilla.components.concept.engine.utils/-engine-version/index.md)<br>Returns the version of the engine as [EngineVersion](../../mozilla.components.concept.engine.utils/-engine-version/index.md) object. |

### Functions

| Name | Summary |
|---|---|
| [clearData](clear-data.md) | `open fun clearData(data: `[`BrowsingData`](-browsing-data/index.md)` = BrowsingData.all(), host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears browsing data stored by the engine. |
| [createSession](create-session.md) | `abstract fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`EngineSession`](../-engine-session/index.md)<br>Creates a new engine session. |
| [createSessionState](create-session-state.md) | `abstract fun createSessionState(json: <ERROR CLASS>): `[`EngineSessionState`](../-engine-session-state/index.md)<br>Create a new [EngineSessionState](../-engine-session-state/index.md) instance from the serialized JSON representation. |
| [createView](create-view.md) | `abstract fun createView(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null): `[`EngineView`](../-engine-view/index.md)<br>Creates a new view for rendering web content. |
| [getTrackersLog](get-trackers-log.md) | `open fun getTrackersLog(session: `[`EngineSession`](../-engine-session/index.md)`, onSuccess: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackerLog`](../../mozilla.components.concept.engine.content.blocking/-tracker-log/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fetch a list of trackers logged for a given [session](get-trackers-log.md#mozilla.components.concept.engine.Engine$getTrackersLog(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.collections.List((mozilla.components.concept.engine.content.blocking.TrackerLog)), kotlin.Unit)), kotlin.Function1((kotlin.Throwable, kotlin.Unit)))/session) . |
| [installWebExtension](install-web-extension.md) | `open fun installWebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, allowContentMessaging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, onSuccess: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Installs the provided extension in this engine. |
| [name](name.md) | `abstract fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name of this engine. The returned string might be used in filenames and must therefore only contain valid filename characters. |
| [registerWebExtensionDelegate](register-web-extension-delegate.md) | `open fun registerWebExtensionDelegate(webExtensionDelegate: `[`WebExtensionDelegate`](../../mozilla.components.concept.engine.webextension/-web-extension-delegate/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [WebExtensionDelegate](../../mozilla.components.concept.engine.webextension/-web-extension-delegate/index.md) to be notified of engine events related to web extensions |
| [registerWebNotificationDelegate](register-web-notification-delegate.md) | `open fun registerWebNotificationDelegate(webNotificationDelegate: `[`WebNotificationDelegate`](../../mozilla.components.concept.engine.webnotifications/-web-notification-delegate/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [WebNotificationDelegate](../../mozilla.components.concept.engine.webnotifications/-web-notification-delegate/index.md) to be notified of engine events related to web notifications |
| [registerWebPushDelegate](register-web-push-delegate.md) | `open fun registerWebPushDelegate(webPushDelegate: `[`WebPushDelegate`](../../mozilla.components.concept.engine.webpush/-web-push-delegate/index.md)`): `[`WebPushHandler`](../../mozilla.components.concept.engine.webpush/-web-push-handler/index.md)<br>Registers a [WebPushDelegate](../../mozilla.components.concept.engine.webpush/-web-push-delegate/index.md) to be notified of engine events related to web extensions. |
| [speculativeConnect](speculative-connect.md) | `abstract fun speculativeConnect(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Opens a speculative connection to the host of [url](speculative-connect.md#mozilla.components.concept.engine.Engine$speculativeConnect(kotlin.String)/url). |
| [warmUp](warm-up.md) | `open fun warmUp(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes sure all required engine initialization logic is executed. The details are specific to individual implementations, but the following must be true: |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoEngine](../../mozilla.components.browser.engine.gecko/-gecko-engine/index.md) | `class GeckoEngine : `[`Engine`](./index.md)<br>Gecko-based implementation of Engine interface. |
| [SystemEngine](../../mozilla.components.browser.engine.system/-system-engine/index.md) | `class SystemEngine : `[`Engine`](./index.md)<br>WebView-based implementation of the Engine interface. |
