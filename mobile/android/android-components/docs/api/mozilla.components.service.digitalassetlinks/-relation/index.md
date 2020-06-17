[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [Relation](./index.md)

# Relation

`enum class Relation` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/Relation.kt#L11)

Describes the nature of a statement, and consists of a kind and a detail.

### Enum Values

| Name | Summary |
|---|---|
| [USE_AS_ORIGIN](-u-s-e_-a-s_-o-r-i-g-i-n.md) | Grants the target permission to retrieve sign-in credentials stored for the source. For App -&gt; Web transitions, requests the app to use the declared origin to be used as origin for the client app in the web APIs context. |
| [HANDLE_ALL_URLS](-h-a-n-d-l-e_-a-l-l_-u-r-l-s.md) | Requests the ability to handle all URLs from a given origin. |

### Properties

| Name | Summary |
|---|---|
| [kindAndDetail](kind-and-detail.md) | `val kindAndDetail: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Kind and detail, separated by a slash character. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
