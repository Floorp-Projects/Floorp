[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [Browsers](index.md) / [findResolvers](./find-resolvers.md)

# findResolvers

`fun findResolvers(context: <ERROR CLASS>, packageManager: <ERROR CLASS>, includeThisApp: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<<ERROR CLASS>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/Browsers.kt#L298)

Finds all the [ResolveInfo](#) for the installed browsers.

**Return**
A list of all [ResolveInfo](#) for the installed browsers.

`fun findResolvers(context: <ERROR CLASS>, packageManager: <ERROR CLASS>, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, includeThisApp: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<<ERROR CLASS>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/Browsers.kt#L331)

Finds all the [ResolveInfo](#) for the installed browsers that can handle the specified URL [url](find-resolvers.md#mozilla.components.support.utils.Browsers.Companion$findResolvers(, , kotlin.String, kotlin.Boolean, kotlin.String)/url).

**Return**
A list of all [ResolveInfo](#) that correspond to the given [url](find-resolvers.md#mozilla.components.support.utils.Browsers.Companion$findResolvers(, , kotlin.String, kotlin.Boolean, kotlin.String)/url).

