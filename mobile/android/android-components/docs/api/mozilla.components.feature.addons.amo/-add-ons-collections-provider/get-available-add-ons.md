[android-components](../../index.md) / [mozilla.components.feature.addons.amo](../index.md) / [AddOnsCollectionsProvider](index.md) / [getAvailableAddOns](./get-available-add-ons.md)

# getAvailableAddOns

`suspend fun getAvailableAddOns(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`AddOn`](../../mozilla.components.feature.addons/-add-on/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/amo/AddOnsCollectionsProvider.kt#L44)

Overrides [AddOnsProvider.getAvailableAddOns](../../mozilla.components.feature.addons/-add-ons-provider/get-available-add-ons.md)

Interacts with the collections endpoint to provide a list of available add-ons.

### Exceptions

`IOException` - if the request could not be executed due to cancellation,
a connectivity problem or a timeout.