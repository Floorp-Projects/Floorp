[android-components](../../index.md) / [mozilla.components.support.ktx.android.org.json](../index.md) / [org.json.JSONObject](index.md) / [getOrPutJSONObject](./get-or-put-j-s-o-n-object.md)

# getOrPutJSONObject

`fun `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`.getOrPutJSONObject(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, defaultValue: () -> `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`): `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/org/json/JSONObject.kt#L84)

Gets the [JSONObject](https://developer.android.com/reference/org/json/JSONObject.html) value with the given key if it exists.
Otherwise calls the defaultValue function, adds its
result to the object, and returns that.

### Parameters

`key` - the key to get or create.

`defaultValue` - a function returning a new default value

**Return**
the existing or new value

