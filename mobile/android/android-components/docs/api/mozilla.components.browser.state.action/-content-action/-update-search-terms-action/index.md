[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateSearchTermsAction](./index.md)

# UpdateSearchTermsAction

`data class UpdateSearchTermsAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L178)

Updates the search terms of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateSearchTermsAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Updates the search terms of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [searchTerms](search-terms.md) | `val searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
