[android-components](../../index.md) / [mozilla.components.browser.engine.system](../index.md) / [SystemEngine](./index.md)

# SystemEngine

`class SystemEngine : `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/SystemEngine.kt#L28)

WebView-based implementation of the Engine interface.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SystemEngine(context: <ERROR CLASS>, defaultSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)` = DefaultSettings())`<br>WebView-based implementation of the Engine interface. |

### Properties

| Name | Summary |
|---|---|
| [profiler](profiler.md) | `val profiler: `[`Profiler`](../../mozilla.components.concept.engine.profiler/-profiler/index.md)`?`<br>See [Engine.profiler](../../mozilla.components.concept.engine/-engine/profiler.md). |
| [settings](settings.md) | `val settings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)<br>See [Engine.settings](../../mozilla.components.concept.engine/-engine/settings.md) |
| [version](version.md) | `val version: `[`EngineVersion`](../../mozilla.components.concept.engine.utils/-engine-version/index.md)<br>Returns the version of the engine as [EngineVersion](../../mozilla.components.concept.engine.utils/-engine-version/index.md) object. |

### Inherited Properties

| Name | Summary |
|---|---|
| [trackingProtectionExceptionStore](../../mozilla.components.concept.engine/-engine/tracking-protection-exception-store.md) | `open val trackingProtectionExceptionStore: `[`TrackingProtectionExceptionStorage`](../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception-storage/index.md)<br>Provides access to the tracking protection exception list for this engine. |

### Functions

| Name | Summary |
|---|---|
| [createSession](create-session.md) | `fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)<br>Creates a new WebView-based EngineSession implementation. |
| [createSessionState](create-session-state.md) | `fun createSessionState(json: <ERROR CLASS>): `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)<br>Create a new [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md) instance from the serialized JSON representation. |
| [createView](create-view.md) | `fun createView(context: <ERROR CLASS>, attrs: <ERROR CLASS>?): `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)<br>Creates a new WebView-based EngineView implementation. |
| [name](name.md) | `fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>See [Engine.name](../../mozilla.components.concept.engine/-engine/name.md) |
| [speculativeConnect](speculative-connect.md) | `fun speculativeConnect(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Opens a speculative connection to the host of [url](speculative-connect.md#mozilla.components.browser.engine.system.SystemEngine$speculativeConnect(kotlin.String)/url). |

### Inherited Functions

| Name | Summary |
|---|---|
| [clearSpeculativeSession](../../mozilla.components.concept.engine/-engine/clear-speculative-session.md) | `open fun clearSpeculativeSession(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes and closes a speculative session created by [speculativeCreateSession](../../mozilla.components.concept.engine/-engine/speculative-create-session.md). This is useful in case the session should no longer be used e.g. because engine settings have changed. |
| [getTrackersLog](../../mozilla.components.concept.engine/-engine/get-trackers-log.md) | `open fun getTrackersLog(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, onSuccess: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackerLog`](../../mozilla.components.concept.engine.content.blocking/-tracker-log/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fetch a list of trackers logged for a given [session](../../mozilla.components.concept.engine/-engine/get-trackers-log.md#mozilla.components.concept.engine.Engine$getTrackersLog(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.collections.List((mozilla.components.concept.engine.content.blocking.TrackerLog)), kotlin.Unit)), kotlin.Function1((kotlin.Throwable, kotlin.Unit)))/session) . |
| [registerWebNotificationDelegate](../../mozilla.components.concept.engine/-engine/register-web-notification-delegate.md) | `open fun registerWebNotificationDelegate(webNotificationDelegate: `[`WebNotificationDelegate`](../../mozilla.components.concept.engine.webnotifications/-web-notification-delegate/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [WebNotificationDelegate](../../mozilla.components.concept.engine.webnotifications/-web-notification-delegate/index.md) to be notified of engine events related to web notifications |
| [registerWebPushDelegate](../../mozilla.components.concept.engine/-engine/register-web-push-delegate.md) | `open fun registerWebPushDelegate(webPushDelegate: `[`WebPushDelegate`](../../mozilla.components.concept.engine.webpush/-web-push-delegate/index.md)`): `[`WebPushHandler`](../../mozilla.components.concept.engine.webpush/-web-push-handler/index.md)<br>Registers a [WebPushDelegate](../../mozilla.components.concept.engine.webpush/-web-push-delegate/index.md) to be notified of engine events related to web extensions. |
| [speculativeCreateSession](../../mozilla.components.concept.engine/-engine/speculative-create-session.md) | `open fun speculativeCreateSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Informs the engine that an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) is likely to be requested soon via [createSession](../../mozilla.components.concept.engine/-engine/create-session.md). This is useful in case creating an engine session is costly and an application wants to decide when the session should be created without having to manage the session itself i.e. when it may or may not need it. |
| [warmUp](../../mozilla.components.concept.engine/-engine/warm-up.md) | `open fun warmUp(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes sure all required engine initialization logic is executed. The details are specific to individual implementations, but the following must be true: |

### Companion Object Properties

| Name | Summary |
|---|---|
| [defaultUserAgent](default-user-agent.md) | `var defaultUserAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
