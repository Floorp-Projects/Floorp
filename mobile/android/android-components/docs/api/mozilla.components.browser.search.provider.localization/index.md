[android-components](../index.md) / [mozilla.components.browser.search.provider.localization](./index.md)

## Package mozilla.components.browser.search.provider.localization

### Types

| Name | Summary |
|---|---|
| [LocaleSearchLocalizationProvider](-locale-search-localization-provider/index.md) | `class LocaleSearchLocalizationProvider : `[`SearchLocalizationProvider`](-search-localization-provider/index.md)<br>LocalizationProvider implementation that only provides the language and country from the system's default languageTag. |
| [SearchLocalization](-search-localization/index.md) | `data class SearchLocalization`<br>Data class representing the result of calling [SearchLocalizationProvider.determineRegion](-search-localization-provider/determine-region.md). |
| [SearchLocalizationProvider](-search-localization-provider/index.md) | `interface SearchLocalizationProvider`<br>Class providing language, country and optionally region (actual location) of the user/device to customize the search experience. |
