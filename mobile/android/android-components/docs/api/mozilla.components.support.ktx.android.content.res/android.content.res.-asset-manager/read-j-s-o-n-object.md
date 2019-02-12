[android-components](../../index.md) / [mozilla.components.support.ktx.android.content.res](../index.md) / [android.content.res.AssetManager](index.md) / [readJSONObject](./read-j-s-o-n-object.md)

# readJSONObject

`fun `[`AssetManager`](https://developer.android.com/reference/android/content/res/AssetManager.html)`.readJSONObject(fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/res/AssetManager.kt#L16)

Read a file from the "assets" and create a a JSONObject from its content.

### Parameters

`fileName` - The name of the asset to open.  This name can be
    hierarchical.