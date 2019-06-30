[android-components](../../../index.md) / [mozilla.components.feature.search](../../index.md) / [SearchUseCases](../index.md) / [DefaultSearchUseCase](./index.md)

# DefaultSearchUseCase

`class DefaultSearchUseCase : `[`SearchUseCase`](../-search-use-case/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/SearchUseCases.kt#L32)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultSearchUseCase(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../../mozilla.components.browser.search/-search-engine-manager/index.md)`, sessionManager: `[`SessionManager`](../../../mozilla.components.browser.session/-session-manager/index.md)`, onNoSession: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`)` |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, searchEngine: `[`SearchEngine`](../../../mozilla.components.browser.search/-search-engine/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Triggers a search in the currently selected session.`operator fun invoke(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = sessionManager.selectedSession, searchEngine: `[`SearchEngine`](../../../mozilla.components.browser.search/-search-engine/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Triggers a search using the default search engine for the provided search terms. |
