[android-components](../../index.md) / [mozilla.components.browser.session.state](../index.md) / [SessionState](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SessionState(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), source: `[`SourceState`](../-source-state/index.md)` = SourceState.NONE, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", securityInfo: `[`SecurityInfoState`](../-security-info-state/index.md)` = SecurityInfoState(), download: `[`DownloadState`](../-download-state/index.md)`? = null, trackerBlockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, trackersBlocked: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyList(), hitResult: `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`? = null)`

Value type that represents the state of a browser session ("tab").

[SessionState](index.md) instances are immutable and state changes are communicated as new state objects emitted by the
[BrowserStore](../../mozilla.components.browser.session.store/-browser-store/index.md).

