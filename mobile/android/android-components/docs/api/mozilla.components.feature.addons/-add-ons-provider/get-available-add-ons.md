[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddOnsProvider](index.md) / [getAvailableAddOns](./get-available-add-ons.md)

# getAvailableAddOns

`abstract suspend fun getAvailableAddOns(allowCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`AddOn`](../-add-on/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddOnsProvider.kt#L18)

Provides a list of all available add-ons.

### Parameters

`allowCache` - whether or not the result may be provided
from a previously cached response, defaults to true.