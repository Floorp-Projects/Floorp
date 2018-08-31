---
title: SearchLocalizationProvider - 
---

[mozilla.components.browser.search.provider.localization](../index.html) / [SearchLocalizationProvider](./index.html)

# SearchLocalizationProvider

`abstract class SearchLocalizationProvider`

Class providing language, country and optionally region (actual location) of the user/device to
customize the search experience.

### Constructors

| [&lt;init&gt;](-init-.html) | `SearchLocalizationProvider()`<br>Class providing language, country and optionally region (actual location) of the user/device to customize the search experience. |

### Properties

| [country](country.html) | `abstract val country: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. |
| [language](language.html) | `abstract val language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 639 alpha-2 or alpha-3 language code, or registered language subtags up to 8 alpha letters (for future enhancements). |
| [languageTag](language-tag.html) | `val languageTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a language tag of the form -. |
| [region](region.html) | `abstract val region: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Optional actual location (region only) of the user/device. ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. |

### Inheritors

| [LocaleSearchLocalizationProvider](../-locale-search-localization-provider/index.html) | `class LocaleSearchLocalizationProvider : `[`SearchLocalizationProvider`](./index.md)<br>LocalizationProvider implementation that only provides the language and country from the system's default languageTag. |

