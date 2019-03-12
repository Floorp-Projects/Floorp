[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [Session](./index.md)

# Session

`class Session : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/Session.kt#L25)

Value type that represents the state of a browser session. Changes can be observed.

### Types

| Name | Summary |
|---|---|
| [FindResult](-find-result/index.md) | `data class FindResult`<br>A value type representing a result of a "find in page" operation. |
| [Observer](-observer/index.md) | `interface Observer`<br>Interface to be implemented by classes that want to observe a session. |
| [SecurityInfo](-security-info/index.md) | `data class SecurityInfo`<br>A value type holding security information for a Session. |
| [Source](-source/index.md) | `enum class Source`<br>Represents the origin of a session to describe how and why it was created. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Session(initialUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, source: `[`Source`](-source/index.md)` = Source.NONE, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`> = ObserverRegistry())`<br>Value type that represents the state of a browser session. Changes can be observed. |

### Properties

| Name | Summary |
|---|---|
| [appPermissionRequest](app-permission-request.md) | `var appPermissionRequest: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`>`<br>[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) permission request for the app. A [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) must be consumed i.e. either [PermissionRequest.grant](../../mozilla.components.concept.engine.permission/-permission-request/grant.md) or [PermissionRequest.reject](../../mozilla.components.concept.engine.permission/-permission-request/reject.md) must be called. |
| [canGoBack](can-go-back.md) | `var canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Navigation state, true if there's an history item to go back to, otherwise false. |
| [canGoForward](can-go-forward.md) | `var canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Navigation state, true if there's an history item to go forward to, otherwise false. |
| [closeWindowRequest](close-window-request.md) | `var closeWindowRequest: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`WindowRequest`](../../mozilla.components.concept.engine.window/-window-request/index.md)`>`<br>[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) request to close a window. |
| [contentPermissionRequest](content-permission-request.md) | `var contentPermissionRequest: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`>`<br>[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) permission request from web content. A [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) must be consumed i.e. either [PermissionRequest.grant](../../mozilla.components.concept.engine.permission/-permission-request/grant.md) or [PermissionRequest.reject](../../mozilla.components.concept.engine.permission/-permission-request/reject.md) must be called. A content permission request can also be cancelled, which will result in a new empty [Consumable](../../mozilla.components.support.base.observer/-consumable/index.md). |
| [customTabConfig](custom-tab-config.md) | `var customTabConfig: `[`CustomTabConfig`](../../mozilla.components.browser.session.tab/-custom-tab-config/index.md)`?`<br>Configuration data in case this session is used for a Custom Tab. |
| [desktopMode](desktop-mode.md) | `var desktopMode: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Desktop Mode state, true if the desktop mode is requested, otherwise false. |
| [download](download.md) | `var download: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`Download`](../-download/index.md)`>`<br>Last download request if it wasn't consumed by at least one observer. |
| [findResults](find-results.md) | `var findResults: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`FindResult`](-find-result/index.md)`>`<br>List of results of that latest "find in page" operation. |
| [fullScreenMode](full-screen-mode.md) | `var fullScreenMode: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Exits fullscreen mode if it's in that state. |
| [hitResult](hit-result.md) | `var hitResult: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`>`<br>The target of the latest long click operation. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [loading](loading.md) | `var loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Loading state, true if this session's url is currently loading, otherwise false. |
| [openWindowRequest](open-window-request.md) | `var openWindowRequest: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`WindowRequest`](../../mozilla.components.concept.engine.window/-window-request/index.md)`>`<br>[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) request to open/create a window. |
| [private](private.md) | `val private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [progress](progress.md) | `var progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The progress loading the current URL. |
| [promptRequest](prompt-request.md) | `var promptRequest: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`PromptRequest`](../../mozilla.components.concept.engine.prompt/-prompt-request/index.md)`>`<br>[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) State for a prompt request from web content. |
| [searchTerms](search-terms.md) | `var searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The currently / last used search terms. |
| [securityInfo](security-info.md) | `var securityInfo: `[`SecurityInfo`](-security-info/index.md)<br>Security information indicating whether or not the current session is for a secure URL, as well as the host and SSL certificate authority, if applicable. |
| [source](source.md) | `val source: `[`Source`](-source/index.md) |
| [thumbnail](thumbnail.md) | `var thumbnail: `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`?`<br>The target of the latest thumbnail. |
| [title](title.md) | `var title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The title of the currently displayed website changed. |
| [trackerBlockingEnabled](tracker-blocking-enabled.md) | `var trackerBlockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tracker blocking state, true if blocking trackers is enabled, otherwise false. |
| [trackersBlocked](trackers-blocked.md) | `var trackersBlocked: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>List of URIs that have been blocked in this session. |
| [url](url.md) | `var url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The currently loading or loaded URL. |
| [webAppManifest](web-app-manifest.md) | `var webAppManifest: `[`WebAppManifest`](../../mozilla.components.browser.session.manifest/-web-app-manifest/index.md)`?`<br>The Web App Manifest for the currently visited page (or null). |

### Functions

| Name | Summary |
|---|---|
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [isCustomTabSession](is-custom-tab-session.md) | `fun isCustomTabSession(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this session is used for a Custom Tab. |
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
