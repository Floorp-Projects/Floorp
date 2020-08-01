[android-components](../../index.md) / [mozilla.components.support.base.ids](../index.md) / [SharedIdsHelper](./index.md)

# SharedIdsHelper

`object SharedIdsHelper` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/ids/SharedIdsHelper.kt#L24)

Helper for component and app code to use unique notification ids without conflicts.

### Functions

| Name | Summary |
|---|---|
| [getIdForTag](get-id-for-tag.md) | `fun getIdForTag(context: <ERROR CLASS>, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Get a unique notification ID for the provided unique tag. |
| [getNextIdForTag](get-next-id-for-tag.md) | `fun getNextIdForTag(context: <ERROR CLASS>, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Get the next available unique notification ID for the provided unique tag. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
