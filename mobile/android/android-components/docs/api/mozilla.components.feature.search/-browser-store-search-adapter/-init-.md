[android-components](../../index.md) / [mozilla.components.feature.search](../index.md) / [BrowserStoreSearchAdapter](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserStoreSearchAdapter(browserStore: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`

Adapter which wraps a [browserStore](#) in order to fulfill the [SearchAdapter](../-search-adapter/index.md) interface.

This class uses the [browserStore](#) to determine when private mode is active, and updates the
[browserStore](#) whenever a new search has been requested.

NOTE: this will add [SearchRequest](../../mozilla.components.concept.engine.search/-search-request/index.md)s to [BrowserStore.state](../../mozilla.components.lib.state/-store/state.md) when [sendSearch](send-search.md) is called. Client
code is responsible for consuming these requests and displaying something to the user.

NOTE: client code is responsible for sending [ContentAction.ConsumeSearchRequestAction](../../mozilla.components.browser.state.action/-content-action/-consume-search-request-action/index.md)s
after consuming events. See [SearchFeature](../-search-feature/index.md) for a component that will handle this for you.

### Parameters

`browserStore` - The application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

`tabId` - ID of the tab that requests the search, or null to use the selected tab.