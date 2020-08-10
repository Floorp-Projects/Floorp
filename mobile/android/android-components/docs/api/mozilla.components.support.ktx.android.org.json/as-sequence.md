[android-components](../index.md) / [mozilla.components.support.ktx.android.org.json](index.md) / [asSequence](./as-sequence.md)

# asSequence

`inline fun <V> <ERROR CLASS>.asSequence(crossinline getter: <ERROR CLASS>.(`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`) -> `[`V`](as-sequence.md#V)`): `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`V`](as-sequence.md#V)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/org/json/JSONArray.kt#L15)

Convenience method to convert a JSONArray into a sequence.

### Parameters

`getter` - callback to get the value for an index in the array.`fun <ERROR CLASS>.asSequence(): `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/org/json/JSONArray.kt#L20)