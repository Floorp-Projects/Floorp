[android-components](../../index.md) / [mozilla.components.feature.addons.amo](../index.md) / [AddonCollectionProvider](index.md) / [getAvailableAddons](./get-available-addons.md)

# getAvailableAddons

`suspend fun getAvailableAddons(allowCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, readTimeoutInSeconds: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/amo/AddonCollectionProvider.kt#L75)

Overrides [AddonsProvider.getAvailableAddons](../../mozilla.components.feature.addons/-addons-provider/get-available-addons.md)

Interacts with the collections endpoint to provide a list of available
add-ons. May return a cached response, if available, not expired (see
[maxCacheAgeInMinutes](#)) and allowed (see [allowCache](get-available-addons.md#mozilla.components.feature.addons.amo.AddonCollectionProvider$getAvailableAddons(kotlin.Boolean, kotlin.Long)/allowCache)).

### Parameters

`allowCache` - whether or not the result may be provided
from a previously cached response, defaults to true.

`readTimeoutInSeconds` - optional timeout in seconds to use when fetching
available add-ons from a remote endpoint. If not specified [DEFAULT_READ_TIMEOUT_IN_SECONDS](#)
will be used.

### Exceptions

`IOException` - if the request failed, or could not be executed due to cancellation,
a connectivity problem or a timeout.