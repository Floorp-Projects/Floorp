[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [ContentState](./index.md)

# ContentState

`data class ContentState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/ContentState.kt#L33)

Value type that represents the state of the content within a [SessionState](../-session-state/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContentState(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", securityInfo: `[`SecurityInfoState`](../-security-info-state/index.md)` = SecurityInfoState(), thumbnail: <ERROR CLASS>? = null, icon: <ERROR CLASS>? = null, download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`? = null, hitResult: `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`? = null, promptRequest: `[`PromptRequest`](../../mozilla.components.concept.engine.prompt/-prompt-request/index.md)`? = null, findResults: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`FindResultState`](../../mozilla.components.browser.state.state.content/-find-result-state/index.md)`> = emptyList())`<br>Value type that represents the state of the content within a [SessionState](../-session-state/index.md). |

### Properties

| Name | Summary |
|---|---|
| [download](download.md) | `val download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`?`<br>Last unhandled download request. |
| [findResults](find-results.md) | `val findResults: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`FindResultState`](../../mozilla.components.browser.state.state.content/-find-result-state/index.md)`>`<br>the list of results of the latest "find in page" operation. |
| [hitResult](hit-result.md) | `val hitResult: `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`?`<br>the target of the latest long click operation. |
| [icon](icon.md) | `val icon: <ERROR CLASS>?`<br>the icon of the page currently loaded by this session. |
| [loading](loading.md) | `val loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [private](private.md) | `val private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether or not the session is private. |
| [progress](progress.md) | `val progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the loading progress of the current page. |
| [promptRequest](prompt-request.md) | `val promptRequest: `[`PromptRequest`](../../mozilla.components.concept.engine.prompt/-prompt-request/index.md)`?`<br>the last received [PromptRequest](../../mozilla.components.concept.engine.prompt/-prompt-request/index.md). |
| [searchTerms](search-terms.md) | `val searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the last used search terms, or an empty string if no search was executed for this session. |
| [securityInfo](security-info.md) | `val securityInfo: `[`SecurityInfoState`](../-security-info-state/index.md)<br>the security information as [SecurityInfoState](../-security-info-state/index.md), describing whether or not the this session is for a secure URL, as well as the host and SSL certificate authority. |
| [thumbnail](thumbnail.md) | `val thumbnail: <ERROR CLASS>?`<br>the last generated [Bitmap](#) of this session's content, to be used as a preview in e.g. a tab switcher. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the title of the current page. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the loading or loaded URL. |
