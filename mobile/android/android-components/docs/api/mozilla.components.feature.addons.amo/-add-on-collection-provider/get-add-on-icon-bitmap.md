[android-components](../../index.md) / [mozilla.components.feature.addons.amo](../index.md) / [AddOnCollectionProvider](index.md) / [getAddOnIconBitmap](./get-add-on-icon-bitmap.md)

# getAddOnIconBitmap

`suspend fun getAddOnIconBitmap(addOn: `[`AddOn`](../../mozilla.components.feature.addons/-add-on/index.md)`): <ERROR CLASS>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/amo/AddOnCollectionProvider.kt#L100)

Fetches given AddOn icon from the url and returns a decoded Bitmap

### Exceptions

`IOException` - if the request could not be executed due to cancellation,
a connectivity problem or a timeout.