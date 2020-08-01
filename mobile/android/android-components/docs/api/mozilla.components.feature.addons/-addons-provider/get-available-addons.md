[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonsProvider](index.md) / [getAvailableAddons](./get-available-addons.md)

# getAvailableAddons

`abstract suspend fun getAvailableAddons(allowCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, readTimeoutInSeconds: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../-addon/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonsProvider.kt#L20)

Provides a list of all available add-ons.

### Parameters

`allowCache` - whether or not the result may be provided
from a previously cached response, defaults to true.

`readTimeoutInSeconds` - optional timeout in seconds to use when fetching
available add-ons from a remote endpoint.