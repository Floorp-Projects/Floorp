---
title: LocaleSearchLocalizationProvider - 
---

[mozilla.components.browser.search.provider.localization](../index.html) / [LocaleSearchLocalizationProvider](./index.html)

# LocaleSearchLocalizationProvider

`class LocaleSearchLocalizationProvider : `[`SearchLocalizationProvider`](../-search-localization-provider/index.html)

LocalizationProvider implementation that only provides the language and country from the system's
default languageTag.

### Constructors

| [&lt;init&gt;](-init-.html) | `LocaleSearchLocalizationProvider()`<br>LocalizationProvider implementation that only provides the language and country from the system's default languageTag. |

### Properties

| [country](country.html) | `val country: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. |
| [language](language.html) | `val language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>ISO 639 alpha-2 or alpha-3 language code, or registered language subtags up to 8 alpha letters (for future enhancements). |
| [region](region.html) | `val region: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Optional actual location (region only) of the user/device. ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. |

### Inherited Properties

| [languageTag](../-search-localization-provider/language-tag.html) | `val languageTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a language tag of the form -. |

