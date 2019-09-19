[android-components](../index.md) / [mozilla.components.support.ktx.android.org.json](index.md) / [getOrPutJSONObject](./get-or-put-j-s-o-n-object.md)

# getOrPutJSONObject

`fun <ERROR CLASS>.getOrPutJSONObject(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, defaultValue: () -> <ERROR CLASS>): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/org/json/JSONObject.kt#L90)

Gets the [JSONObject](#) value with the given key if it exists.
Otherwise calls the defaultValue function, adds its
result to the object, and returns that.

### Parameters

`key` - the key to get or create.

`defaultValue` - a function returning a new default value

**Return**
the existing or new value

