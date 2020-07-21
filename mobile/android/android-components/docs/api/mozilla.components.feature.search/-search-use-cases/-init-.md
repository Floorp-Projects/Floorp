[android-components](../../index.md) / [mozilla.components.feature.search](../index.md) / [SearchUseCases](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SearchUseCases(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../mozilla.components.browser.search/-search-engine-manager/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, onNoSession: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Session`](../../mozilla.components.browser.session/-session/index.md)` = { url ->
        Session(url).apply { sessionManager.add(this) }
    })`

Contains use cases related to the search feature.

### Parameters

`onNoSession` - When invoking a use case that requires a (selected) [Session](../../mozilla.components.browser.session/-session/index.md) and when no [Session](../../mozilla.components.browser.session/-session/index.md) is available
this (optional) lambda will be invoked to create a [Session](../../mozilla.components.browser.session/-session/index.md). The default implementation creates a [Session](../../mozilla.components.browser.session/-session/index.md) and adds
it to the [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md).