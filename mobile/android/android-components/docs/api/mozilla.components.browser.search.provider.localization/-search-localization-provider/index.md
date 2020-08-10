[android-components](../../index.md) / [mozilla.components.browser.search.provider.localization](../index.md) / [SearchLocalizationProvider](./index.md)

# SearchLocalizationProvider

`interface SearchLocalizationProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/localization/SearchLocalizationProvider.kt#L11)

Class providing language, country and optionally region (actual location) of the user/device to
customize the search experience.

### Functions

| Name | Summary |
|---|---|
| [determineRegion](determine-region.md) | `abstract suspend fun determineRegion(): `[`SearchLocalization`](../-search-localization/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [LocaleSearchLocalizationProvider](../-locale-search-localization-provider/index.md) | `class LocaleSearchLocalizationProvider : `[`SearchLocalizationProvider`](./index.md)<br>LocalizationProvider implementation that only provides the language and country from the system's default languageTag. |
| [RegionSearchLocalizationProvider](../../mozilla.components.service.location.search/-region-search-localization-provider/index.md) | `class RegionSearchLocalizationProvider : `[`SearchLocalizationProvider`](./index.md)<br>[SearchLocalizationProvider](./index.md) implementation that uses a [MozillaLocationService](../../mozilla.components.service.location/-mozilla-location-service/index.md) instance to do a region lookup via GeoIP. |
