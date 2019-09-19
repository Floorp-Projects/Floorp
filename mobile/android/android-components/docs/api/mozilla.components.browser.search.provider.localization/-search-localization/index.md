[android-components](../../index.md) / [mozilla.components.browser.search.provider.localization](../index.md) / [SearchLocalization](./index.md)

# SearchLocalization

`data class SearchLocalization` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/localization/SearchLocalization.kt#L16)

Data class representing the result of calling [SearchLocalizationProvider.determineRegion](../-search-localization-provider/determine-region.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchLocalization(language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, country: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, region: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Data class representing the result of calling [SearchLocalizationProvider.determineRegion](../-search-localization-provider/determine-region.md). |

### Properties

| Name | Summary |
|---|---|
| [country](country.md) | `val country: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. Example: "US" (United States), "FR" (France), "029" (Caribbean) |
| [language](language.md) | `val language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 639 alpha-2 or alpha-3 language code, or registered language subtags up to 8 alpha letters (for future enhancements). Example: "en" (English), "ja" (Japanese), "kok" (Konkani) |
| [languageTag](language-tag.md) | `val languageTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a language tag of the form -. |
| [region](region.md) | `val region: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
