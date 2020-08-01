[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [Addon](index.md) / [filterTranslations](./filter-translations.md)

# filterTranslations

`fun filterTranslations(locales: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Addon`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/Addon.kt#L152)

Returns a copy of this [Addon](index.md) containing only translations (description,
name, summary) of the provided locales. All other translations
except the [defaultLocale](default-locale.md) will be removed.

### Parameters

`locales` - list of locales to keep.

**Return**
copy of the addon with all other translations removed.

