[android-components](../../index.md) / [mozilla.components.browser.search.provider.localization](../index.md) / [LocaleSearchLocalizationProvider](index.md) / [region](./region.md)

# region

`val region: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/localization/LocaleSearchLocalizationProvider.kt#L20)

Overrides [SearchLocalizationProvider.region](../-search-localization-provider/region.md)

Optional actual location (region only) of the user/device. ISO 3166 alpha-2 country code or
UN M.49 numeric-3 area code.

This value is used to customize the experience for users that use a languageTag that doesn't match
their geographic region. For example a user in Russia with "en-US" languageTag might get offered
an English version of Yandex if the region is set to "RU".

Can be null if no location information is available.

