[android-components](../../index.md) / [mozilla.components.feature.addons.amo](../index.md) / [AddOnCollectionProvider](index.md) / [getAvailableAddOns](./get-available-add-ons.md)

# getAvailableAddOns

`suspend fun getAvailableAddOns(allowCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`AddOn`](../../mozilla.components.feature.addons/-add-on/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/amo/AddOnCollectionProvider.kt#L67)

Overrides [AddOnsProvider.getAvailableAddOns](../../mozilla.components.feature.addons/-add-ons-provider/get-available-add-ons.md)

Interacts with the collections endpoint to provide a list of available
add-ons. May return a cached response, if available, not expired (see
[maxCacheAgeInMinutes](#)) and allowed (see [allowCache](get-available-add-ons.md#mozilla.components.feature.addons.amo.AddOnCollectionProvider$getAvailableAddOns(kotlin.Boolean)/allowCache)).

### Parameters

`allowCache` - whether or not the result may be provided
from a previously cached response, defaults to true.

### Exceptions

`IOException` - if the request could not be executed due to cancellation,
a connectivity problem or a timeout.