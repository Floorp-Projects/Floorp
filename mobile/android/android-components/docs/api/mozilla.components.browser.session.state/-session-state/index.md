[android-components](../../index.md) / [mozilla.components.browser.session.state](../index.md) / [SessionState](./index.md)

# SessionState

`data class SessionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/state/SessionState.kt#L35)

Value type that represents the state of a browser session ("tab").

[SessionState](./index.md) instances are immutable and state changes are communicated as new state objects emitted by the
[BrowserStore](../../mozilla.components.browser.session.store/-browser-store/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SessionState(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), source: `[`SourceState`](../-source-state/index.md)` = SourceState.NONE, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", securityInfo: `[`SecurityInfoState`](../-security-info-state/index.md)` = SecurityInfoState(), download: `[`DownloadState`](../-download-state/index.md)`? = null, trackerBlockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, trackersBlocked: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyList(), hitResult: `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`? = null)`<br>Value type that represents the state of a browser session ("tab"). |

### Properties

| Name | Summary |
|---|---|
| [canGoBack](can-go-back.md) | `val canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Navigation state, true if there's an history item to go back to, otherwise false. |
| [canGoForward](can-go-forward.md) | `val canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Navigation state, true if there's an history item to go forward to, otherwise false. |
| [download](download.md) | `val download: `[`DownloadState`](../-download-state/index.md)`?`<br>A download request triggered by this session (or null). |
| [hitResult](hit-result.md) | `val hitResult: `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`?`<br>The target of the latest long click operation on the currently displayed page. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A unique ID identifying this session. |
| [loading](loading.md) | `val loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [private](private.md) | `val private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not this session is in private mode (automatically erasing browsing information, such as passwords, cookies and history, leaving no trace after the end of the session). |
| [progress](progress.md) | `val progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The loading progress of the current page. |
| [searchTerms](search-terms.md) | `val searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The search terms used for the currently loaded page (or an empty string) if this page was loaded by performing a search. |
| [securityInfo](security-info.md) | `val securityInfo: `[`SecurityInfoState`](../-security-info-state/index.md)<br>security information indicating whether or not the current session is for a secure URL, as well as the host and SSL certificate authority, if applicable. |
| [source](source.md) | `val source: `[`SourceState`](../-source-state/index.md)<br>The source that created this session (e.g. a VIEW intent). |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The title of the currently displayed page (or an empty String). |
| [trackerBlockingEnabled](tracker-blocking-enabled.md) | `val trackerBlockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tracker blocking state, true if blocking trackers is enabled, otherwise false. |
| [trackersBlocked](trackers-blocked.md) | `val trackersBlocked: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>List of URIs that have been blocked on the currently displayed page. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL of the currently loading or loaded resource/page. |
