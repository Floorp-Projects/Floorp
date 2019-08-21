[android-components](../../index.md) / [mozilla.components.browser.search.provider.localization](../index.md) / [LocaleSearchLocalizationProvider](./index.md)

# LocaleSearchLocalizationProvider

`class LocaleSearchLocalizationProvider : `[`SearchLocalizationProvider`](../-search-localization-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/localization/LocaleSearchLocalizationProvider.kt#L13)

LocalizationProvider implementation that only provides the language and country from the system's
default languageTag.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LocaleSearchLocalizationProvider()`<br>LocalizationProvider implementation that only provides the language and country from the system's default languageTag. |

### Functions

| Name | Summary |
|---|---|
| [determineRegion](determine-region.md) | `suspend fun determineRegion(): `[`SearchLocalization`](../-search-localization/index.md) |
