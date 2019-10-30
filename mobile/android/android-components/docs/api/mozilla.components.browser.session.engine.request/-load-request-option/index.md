[android-components](../../index.md) / [mozilla.components.browser.session.engine.request](../index.md) / [LoadRequestOption](./index.md)

# LoadRequestOption

`enum class LoadRequestOption` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/engine/request/LoadRequestMetadata.kt#L32)

Simple enum class for defining the set of characteristics of a [LoadRequest](#).

Facilities for combining these and testing the resulting bit mask also exist as operators.

This should be generalized, but it's not clear if it will be useful enough to go into [kotlin.support](#).

### Enum Values

| Name | Summary |
|---|---|
| [NONE](-n-o-n-e.md) |  |
| [REDIRECT](-r-e-d-i-r-e-c-t.md) |  |
| [WEB_CONTENT](-w-e-b_-c-o-n-t-e-n-t.md) |  |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
