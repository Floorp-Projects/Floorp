[android-components](../../index.md) / [mozilla.components.feature.search](../index.md) / [BrowserStoreSearchAdapter](./index.md)

# BrowserStoreSearchAdapter

`class BrowserStoreSearchAdapter : `[`SearchAdapter`](../-search-adapter/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/BrowserStoreSearchAdapter.kt#L24)

Adapter which wraps a [browserStore](#) in order to fulfill the [SearchAdapter](../-search-adapter/index.md) interface.

This class uses the [browserStore](#) to determine when private mode is active, and updates the
[browserStore](#) whenever a new search has been requested.

NOTE: this will add [SearchRequest](../../mozilla.components.concept.engine.search/-search-request/index.md)s to [browserStore.state](#) when [sendSearch](send-search.md) is called. Client
code is responsible for consuming these requests and displaying something to the user.

NOTE: client code is responsible for sending [ContentAction.ConsumeSearchRequestAction](../../mozilla.components.browser.state.action/-content-action/-consume-search-request-action/index.md)s
after consuming events. See [SearchFeature](../-search-feature/index.md) for a component that will handle this for you.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserStoreSearchAdapter(browserStore: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`)`<br>Adapter which wraps a [browserStore](#) in order to fulfill the [SearchAdapter](../-search-adapter/index.md) interface. |

### Functions

| Name | Summary |
|---|---|
| [isPrivateSession](is-private-session.md) | `fun isPrivateSession(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Called by the component to check whether or not the currently selected session is private. |
| [sendSearch](send-search.md) | `fun sendSearch(isPrivate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the component to indicate that the user should be shown a search. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
