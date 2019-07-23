[android-components](../index.md) / [mozilla.components.support.ktx.android.org.json](index.md) / [mapNotNull](./map-not-null.md)

# mapNotNull

`inline fun <T, R : `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> <ERROR CLASS>.mapNotNull(getFromArray: <ERROR CLASS>.(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`) -> `[`T`](map-not-null.md#T)`, transform: (`[`T`](map-not-null.md#T)`) -> `[`R`](map-not-null.md#R)`?): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`R`](map-not-null.md#R)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/org/json/JSONArray.kt#L43)

Returns a list containing only the non-null results of applying the given [transform](map-not-null.md#mozilla.components.support.ktx.android.org.json$mapNotNull(, kotlin.Function2((, kotlin.Int, mozilla.components.support.ktx.android.org.json.mapNotNull.T)), kotlin.Function1((mozilla.components.support.ktx.android.org.json.mapNotNull.T, mozilla.components.support.ktx.android.org.json.mapNotNull.R)))/transform) function
to each element in the original collection as returned by [getFromArray](map-not-null.md#mozilla.components.support.ktx.android.org.json$mapNotNull(, kotlin.Function2((, kotlin.Int, mozilla.components.support.ktx.android.org.json.mapNotNull.T)), kotlin.Function1((mozilla.components.support.ktx.android.org.json.mapNotNull.T, mozilla.components.support.ktx.android.org.json.mapNotNull.R)))/getFromArray). If [getFromArray](map-not-null.md#mozilla.components.support.ktx.android.org.json$mapNotNull(, kotlin.Function2((, kotlin.Int, mozilla.components.support.ktx.android.org.json.mapNotNull.T)), kotlin.Function1((mozilla.components.support.ktx.android.org.json.mapNotNull.T, mozilla.components.support.ktx.android.org.json.mapNotNull.R)))/getFromArray)
or [transform](map-not-null.md#mozilla.components.support.ktx.android.org.json$mapNotNull(, kotlin.Function2((, kotlin.Int, mozilla.components.support.ktx.android.org.json.mapNotNull.T)), kotlin.Function1((mozilla.components.support.ktx.android.org.json.mapNotNull.T, mozilla.components.support.ktx.android.org.json.mapNotNull.R)))/transform) throws a [JSONException](#), these elements will also be omitted.

Here's an example call:

``` kotlin
jsonArray.mapNotNull(JSONArray::getJSONObject) { jsonObj -> jsonObj.getString("author") }
```

