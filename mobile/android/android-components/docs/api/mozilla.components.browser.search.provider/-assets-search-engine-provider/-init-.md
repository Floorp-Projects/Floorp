[android-components](../../index.md) / [mozilla.components.browser.search.provider](../index.md) / [AssetsSearchEngineProvider](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AssetsSearchEngineProvider(localizationProvider: `[`SearchLocalizationProvider`](../../mozilla.components.browser.search.provider.localization/-search-localization-provider/index.md)`, filters: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngineFilter`](../../mozilla.components.browser.search.provider.filter/-search-engine-filter/index.md)`> = emptyList(), additionalIdentifiers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyList())`

SearchEngineProvider implementation to load the included search engines from assets.

A SearchLocalizationProvider implementation is used to customize the returned search engines for
the language and country of the user/device.

Optionally SearchEngineFilter instances can be provided to remove unwanted search engines from
the loaded list.

Optionally additionalIdentifiers to be loaded can be specified. A search engine
identifier corresponds to the search plugin XML file name (e.g. duckduckgo -&gt; duckduckgo.xml).

