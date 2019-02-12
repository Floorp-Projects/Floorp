[android-components](../../index.md) / [mozilla.components.browser.search.provider.localization](../index.md) / [SearchLocalizationProvider](./index.md)

# SearchLocalizationProvider

`abstract class SearchLocalizationProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/localization/SearchLocalizationProvider.kt#L11)

Class providing language, country and optionally region (actual location) of the user/device to
customize the search experience.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchLocalizationProvider()`<br>Class providing language, country and optionally region (actual location) of the user/device to customize the search experience. |

### Properties

| Name | Summary |
|---|---|
| [country](country.md) | `abstract val country: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. |
| [language](language.md) | `abstract val language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 639 alpha-2 or alpha-3 language code, or registered language subtags up to 8 alpha letters (for future enhancements). |
| [languageTag](language-tag.md) | `val languageTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a language tag of the form -. |
| [region](region.md) | `abstract val region: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Optional actual location (region only) of the user/device. ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. |

### Inheritors

| Name | Summary |
|---|---|
| [LocaleSearchLocalizationProvider](../-locale-search-localization-provider/index.md) | `class LocaleSearchLocalizationProvider : `[`SearchLocalizationProvider`](./index.md)<br>LocalizationProvider implementation that only provides the language and country from the system's default languageTag. |
