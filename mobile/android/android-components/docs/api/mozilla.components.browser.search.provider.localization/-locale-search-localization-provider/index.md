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

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
