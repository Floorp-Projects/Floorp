[android-components](../../index.md) / [mozilla.components.feature.search](../index.md) / [SearchUseCases](./index.md)

# SearchUseCases

`class SearchUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/SearchUseCases.kt#L21)

Contains use cases related to the search feature.

### Parameters

`onNoSession` - When invoking a use case that requires a (selected) [Session](../../mozilla.components.browser.session/-session/index.md) and when no [Session](../../mozilla.components.browser.session/-session/index.md) is available
this (optional) lambda will be invoked to create a [Session](../../mozilla.components.browser.session/-session/index.md). The default implementation creates a [Session](../../mozilla.components.browser.session/-session/index.md) and adds
it to the [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md).

### Types

| Name | Summary |
|---|---|
| [DefaultSearchUseCase](-default-search-use-case/index.md) | `class DefaultSearchUseCase : `[`SearchUseCase`](-search-use-case/index.md) |
| [NewTabSearchUseCase](-new-tab-search-use-case/index.md) | `class NewTabSearchUseCase : `[`SearchUseCase`](-search-use-case/index.md) |
| [SearchUseCase](-search-use-case/index.md) | `interface SearchUseCase` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchUseCases(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../mozilla.components.browser.search/-search-engine-manager/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, onNoSession: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Session`](../../mozilla.components.browser.session/-session/index.md)` = { url ->
        Session(url).apply { sessionManager.add(this) }
    })`<br>Contains use cases related to the search feature. |

### Properties

| Name | Summary |
|---|---|
| [defaultSearch](default-search.md) | `val defaultSearch: `[`DefaultSearchUseCase`](-default-search-use-case/index.md) |
| [newPrivateTabSearch](new-private-tab-search.md) | `val newPrivateTabSearch: `[`NewTabSearchUseCase`](-new-tab-search-use-case/index.md) |
| [newTabSearch](new-tab-search.md) | `val newTabSearch: `[`NewTabSearchUseCase`](-new-tab-search-use-case/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
