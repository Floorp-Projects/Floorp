[android-components](../../../index.md) / [mozilla.components.feature.search](../../index.md) / [SearchUseCases](../index.md) / [NewTabSearchUseCase](./index.md)

# NewTabSearchUseCase

`class NewTabSearchUseCase : `[`SearchUseCase`](../-search-use-case/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/SearchUseCases.kt#L70)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `NewTabSearchUseCase(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../../mozilla.components.browser.search/-search-engine-manager/index.md)`, sessionManager: `[`SessionManager`](../../../mozilla.components.browser.session/-session-manager/index.md)`, isPrivate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)` |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, searchEngine: `[`SearchEngine`](../../../mozilla.components.browser.search/-search-engine/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`operator fun invoke(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, source: `[`Source`](../../../mozilla.components.browser.session/-session/-source/index.md)`, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, searchEngine: `[`SearchEngine`](../../../mozilla.components.browser.search/-search-engine/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Triggers a search on a new session, using the default search engine for the provided search terms. |
