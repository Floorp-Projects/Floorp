[android-components](../index.md) / [mozilla.components.support.ktx.android.content.res](index.md) / [getSpanned](./get-spanned.md)

# getSpanned

`fun <ERROR CLASS>.getSpanned(@StringRes id: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, vararg spanParts: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/res/Resources.kt#L40)

Returns the character sequence associated with a given resource [id](get-spanned.md#mozilla.components.support.ktx.android.content.res$getSpanned(, kotlin.Int, kotlin.Array((kotlin.Pair((kotlin.Any, )))))/id),
substituting format arguments with additional styling spans.

Credit to Michael Spitsin https://medium.com/@programmerr47/working-with-spans-in-android-ca4ab1327bc4

### Parameters

`id` - The desired resource identifier, corresponding to a string resource.

`spanParts` - The format arguments that will be used for substitution.
The first element of each pair is the text to insert, similar to [String.format](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.text/format.html).
The second element of each pair is a span that will be used to style the inserted string.