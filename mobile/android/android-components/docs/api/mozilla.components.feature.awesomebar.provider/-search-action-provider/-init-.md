[android-components](../../index.md) / [mozilla.components.feature.awesomebar.provider](../index.md) / [SearchActionProvider](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SearchActionProvider(searchEngineGetter: suspend () -> `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, searchUseCase: `[`SearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-search-use-case/index.md)`, icon: <ERROR CLASS>? = null, showDescription: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`

An [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) implementation that returns a suggestion that mirrors the
entered text and invokes a search with the given [SearchEngine](../../mozilla.components.browser.search/-search-engine/index.md) if clicked.

