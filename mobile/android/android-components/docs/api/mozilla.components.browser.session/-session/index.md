[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [Session](./index.md)

# Session

`class Session : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/Session.kt#L50)

Value type that represents the state of a browser session. Changes can be observed.

### Types

| Name | Summary |
|---|---|
| [Observer](-observer/index.md) | `interface Observer`<br>Interface to be implemented by classes that want to observe a session. |
| [SecurityInfo](-security-info/index.md) | `data class SecurityInfo`<br>A value type holding security information for a Session. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Session(initialUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, source: `[`Source`](../../mozilla.components.browser.state.state/-session-state/-source/index.md)` = SessionState.Source.NONE, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`> = ObserverRegistry())`<br>Value type that represents the state of a browser session. Changes can be observed. |

### Properties

| Name | Summary |
|---|---|
| [appPermissionRequest](app-permission-request.md) | `var appPermissionRequest: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`>`<br>[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) permission request for the app. A [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) must be consumed i.e. either [PermissionRequest.grant](../../mozilla.components.concept.engine.permission/-permission-request/grant.md) or [PermissionRequest.reject](../../mozilla.components.concept.engine.permission/-permission-request/reject.md) must be called. |
| [canGoBack](can-go-back.md) | `var canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Navigation state, true if there's an history item to go back to, otherwise false. |
| [canGoForward](can-go-forward.md) | `var canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Navigation state, true if there's an history item to go forward to, otherwise false. |
| [contentPermissionRequest](content-permission-request.md) | `var contentPermissionRequest: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`>`<br>[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) permission request from web content. A [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) must be consumed i.e. either [PermissionRequest.grant](../../mozilla.components.concept.engine.permission/-permission-request/grant.md) or [PermissionRequest.reject](../../mozilla.components.concept.engine.permission/-permission-request/reject.md) must be called. A content permission request can also be cancelled, which will result in a new empty [Consumable](../../mozilla.components.support.base.observer/-consumable/index.md). |
| [contextId](context-id.md) | `val contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [crashed](crashed.md) | `var crashed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether this [Session](./index.md) has crashed. |
| [customTabConfig](custom-tab-config.md) | `var customTabConfig: `[`CustomTabConfig`](../../mozilla.components.browser.state.state/-custom-tab-config/index.md)`?`<br>Configuration data in case this session is used for a Custom Tab. |
| [desktopMode](desktop-mode.md) | `var desktopMode: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Desktop Mode state, true if the desktop mode is requested, otherwise false. |
| [hasParentSession](has-parent-session.md) | `val hasParentSession: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this [Session](./index.md) has a parent [Session](./index.md). |
| [icon](icon.md) | `val icon: <ERROR CLASS>?`<br>An icon for the currently visible page. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [launchIntentMetadata](launch-intent-metadata.md) | `var launchIntentMetadata: `[`LaunchIntentMetadata`](../../mozilla.components.browser.session.engine.request/-launch-intent-metadata/index.md)<br>Set when a launch intent is received. |
| [loadRequestMetadata](load-request-metadata.md) | `var loadRequestMetadata: `[`LoadRequestMetadata`](../../mozilla.components.browser.session.engine.request/-load-request-metadata/index.md)<br>Set when a load request is received, indicating if the request came from web content, or via a redirect. |
| [loading](loading.md) | `var loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Loading state, true if this session's url is currently loading, otherwise false. |
| [private](private.md) | `val private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [progress](progress.md) | `var progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The progress loading the current URL. |
| [recordingDevices](recording-devices.md) | `var recordingDevices: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`RecordingDevice`](../../mozilla.components.concept.engine.media/-recording-device/index.md)`>`<br>List of recording devices (e.g. camera or microphone) currently in use by web content. |
| [searchTerms](search-terms.md) | `var searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The currently / last used search terms (or an empty string). |
| [securityInfo](security-info.md) | `var securityInfo: `[`SecurityInfo`](-security-info/index.md)<br>Security information indicating whether or not the current session is for a secure URL, as well as the host and SSL certificate authority, if applicable. |
| [source](source.md) | `val source: `[`Source`](../../mozilla.components.browser.state.state/-session-state/-source/index.md) |
| [thumbnail](thumbnail.md) | `var thumbnail: <ERROR CLASS>?`<br>The target of the latest thumbnail. |
| [title](title.md) | `var title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The title of the currently displayed website changed. |
| [trackerBlockingEnabled](tracker-blocking-enabled.md) | `var trackerBlockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tracker blocking state, true if blocking trackers is enabled, otherwise false. |
| [trackersBlocked](trackers-blocked.md) | `var trackersBlocked: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`>`<br>List of [Tracker](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)s that have been blocked in this session. |
| [trackersLoaded](trackers-loaded.md) | `var trackersLoaded: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`>`<br>List of [Tracker](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)s that could be blocked but have been loaded in this session. |
| [url](url.md) | `var url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The currently loading or loaded URL. |
| [webAppManifest](web-app-manifest.md) | `var webAppManifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?`<br>The Web App Manifest for the currently visited page (or null). |

### Functions

| Name | Summary |
|---|---|
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [isCustomTabSession](is-custom-tab-session.md) | `fun isCustomTabSession(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this session is used for a Custom Tab. |
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [installableManifest](../../mozilla.components.feature.pwa.ext/installable-manifest.md) | `fun `[`Session`](./index.md)`.installableManifest(): `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?`<br>Checks if the current session represents an installable web app. If so, return the web app manifest. Otherwise, return null. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [toCustomTabSessionState](../../mozilla.components.browser.session.ext/to-custom-tab-session-state.md) | `fun `[`Session`](./index.md)`.toCustomTabSessionState(): `[`CustomTabSessionState`](../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md)<br>Creates a matching [CustomTabSessionState](../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) from a custom tab [Session](./index.md). |
| [toTabSessionState](../../mozilla.components.browser.session.ext/to-tab-session-state.md) | `fun `[`Session`](./index.md)`.toTabSessionState(): `[`TabSessionState`](../../mozilla.components.browser.state.state/-tab-session-state/index.md)<br>Create a matching [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) from a [Session](./index.md). |
