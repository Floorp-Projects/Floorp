[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngine](./index.md)

# GeckoEngine

`class GeckoEngine : `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngine.kt#L43)

Gecko-based implementation of Engine interface.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoEngine(context: <ERROR CLASS>, defaultSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)`? = null, runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)` = GeckoRuntime.getDefault(context), executorProvider: () -> `[`GeckoWebExecutor`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoWebExecutor.html)` = { GeckoWebExecutor(runtime) }, trackingProtectionExceptionStore: `[`TrackingProtectionExceptionStorage`](../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception-storage/index.md)` = TrackingProtectionExceptionFileStorage(context, runtime))`<br>Gecko-based implementation of Engine interface. |

### Properties

| Name | Summary |
|---|---|
| [settings](settings.md) | `val settings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)<br>See [Engine.settings](../../mozilla.components.concept.engine/-engine/settings.md) |
| [trackingProtectionExceptionStore](tracking-protection-exception-store.md) | `val trackingProtectionExceptionStore: `[`TrackingProtectionExceptionStorage`](../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception-storage/index.md)<br>Provides access to the tracking protection exception list for this engine. |
| [version](version.md) | `val version: `[`EngineVersion`](../../mozilla.components.concept.engine.utils/-engine-version/index.md)<br>Returns the version of the engine as [EngineVersion](../../mozilla.components.concept.engine.utils/-engine-version/index.md) object. |

### Functions

| Name | Summary |
|---|---|
| [clearData](clear-data.md) | `fun clearData(data: `[`BrowsingData`](../../mozilla.components.concept.engine/-engine/-browsing-data/index.md)`, host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [Engine.clearData](../../mozilla.components.concept.engine/-engine/clear-data.md). |
| [createSession](create-session.md) | `fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)<br>Creates a new Gecko-based EngineSession. |
| [createSessionState](create-session-state.md) | `fun createSessionState(json: <ERROR CLASS>): `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)<br>See [Engine.createSessionState](../../mozilla.components.concept.engine/-engine/create-session-state.md). |
| [createView](create-view.md) | `fun createView(context: <ERROR CLASS>, attrs: <ERROR CLASS>?): `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)<br>Creates a new Gecko-based EngineView. |
| [getTrackersLog](get-trackers-log.md) | `fun getTrackersLog(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, onSuccess: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackerLog`](../../mozilla.components.concept.engine.content.blocking/-tracker-log/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fetch a list of trackers logged for a given [session](get-trackers-log.md#mozilla.components.browser.engine.gecko.GeckoEngine$getTrackersLog(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.collections.List((mozilla.components.concept.engine.content.blocking.TrackerLog)), kotlin.Unit)), kotlin.Function1((kotlin.Throwable, kotlin.Unit)))/session) . |
| [installWebExtension](install-web-extension.md) | `fun installWebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, allowContentMessaging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, onSuccess: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [Engine.installWebExtension](../../mozilla.components.concept.engine/-engine/install-web-extension.md). |
| [name](name.md) | `fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name of this engine. The returned string might be used in filenames and must therefore only contain valid filename characters. |
| [registerWebExtensionDelegate](register-web-extension-delegate.md) | `fun registerWebExtensionDelegate(webExtensionDelegate: `[`WebExtensionDelegate`](../../mozilla.components.concept.engine.webextension/-web-extension-delegate/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [Engine.registerWebExtensionDelegate](../../mozilla.components.concept.engine/-engine/register-web-extension-delegate.md). |
| [registerWebNotificationDelegate](register-web-notification-delegate.md) | `fun registerWebNotificationDelegate(webNotificationDelegate: `[`WebNotificationDelegate`](../../mozilla.components.concept.engine.webnotifications/-web-notification-delegate/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [Engine.registerWebNotificationDelegate](../../mozilla.components.concept.engine/-engine/register-web-notification-delegate.md). |
| [speculativeConnect](speculative-connect.md) | `fun speculativeConnect(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Opens a speculative connection to the host of [url](speculative-connect.md#mozilla.components.browser.engine.gecko.GeckoEngine$speculativeConnect(kotlin.String)/url). |

### Inherited Functions

| Name | Summary |
|---|---|
| [registerWebNotificationDelegate](../../mozilla.components.concept.engine/-engine/register-web-notification-delegate.md) | `open fun registerWebNotificationDelegate(webNotificationDelegate: `[`WebNotificationDelegate`](../../mozilla.components.concept.engine.webnotifications/-web-notification-delegate/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [WebNotificationDelegate](../../mozilla.components.concept.engine.webnotifications/-web-notification-delegate/index.md) to be notified of engine events related to web notifications |
| [registerWebPushDelegate](../../mozilla.components.concept.engine/-engine/register-web-push-delegate.md) | `open fun registerWebPushDelegate(webPushDelegate: `[`WebPushDelegate`](../../mozilla.components.concept.engine.webpush/-web-push-delegate/index.md)`): `[`WebPushHandler`](../../mozilla.components.concept.engine.webpush/-web-push-handler/index.md)<br>Registers a [WebPushDelegate](../../mozilla.components.concept.engine.webpush/-web-push-delegate/index.md) to be notified of engine events related to web extensions. |
| [warmUp](../../mozilla.components.concept.engine/-engine/warm-up.md) | `open fun warmUp(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes sure all required engine initialization logic is executed. The details are specific to individual implementations, but the following must be true: |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
