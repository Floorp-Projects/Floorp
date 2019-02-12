[android-components](../../index.md) / [mozilla.components.browser.search.provider.localization](../index.md) / [LocaleSearchLocalizationProvider](./index.md)

# LocaleSearchLocalizationProvider

`class LocaleSearchLocalizationProvider : `[`SearchLocalizationProvider`](../-search-localization-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/localization/LocaleSearchLocalizationProvider.kt#L13)

LocalizationProvider implementation that only provides the language and country from the system's
default languageTag.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LocaleSearchLocalizationProvider()`<br>LocalizationProvider implementation that only provides the language and country from the system's default languageTag. |

### Properties

| Name | Summary |
|---|---|
| [country](country.md) | `val country: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. |
| [language](language.md) | `val language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 639 alpha-2 or alpha-3 language code, or registered language subtags up to 8 alpha letters (for future enhancements). |
| [region](region.md) | `val region: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Optional actual location (region only) of the user/device. ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. |

### Inherited Properties

| Name | Summary |
|---|---|
| [languageTag](../-search-localization-provider/language-tag.md) | `val languageTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a language tag of the form -. |
