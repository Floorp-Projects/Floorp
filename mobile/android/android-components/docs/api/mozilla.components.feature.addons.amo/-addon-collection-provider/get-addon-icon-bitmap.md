[android-components](../../index.md) / [mozilla.components.feature.addons.amo](../index.md) / [AddonCollectionProvider](index.md) / [getAddonIconBitmap](./get-addon-icon-bitmap.md)

# getAddonIconBitmap

`suspend fun getAddonIconBitmap(addon: `[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`): <ERROR CLASS>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/amo/AddonCollectionProvider.kt#L110)

Fetches given Addon icon from the url and returns a decoded Bitmap

### Exceptions

`IOException` - if the request could not be executed due to cancellation,
a connectivity problem or a timeout.