[android-components](../../index.md) / [mozilla.components.browser.domains](../index.md) / [Domain](./index.md)

# Domain

`data class Domain` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/Domain.kt#L10)

Class intended for internal use which encapsulates meta data about a domain.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Domain(protocol: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, hasWww: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Class intended for internal use which encapsulates meta data about a domain. |

### Properties

| Name | Summary |
|---|---|
| [hasWww](has-www.md) | `val hasWww: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [host](host.md) | `val host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [protocol](protocol.md) | `val protocol: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [create](create.md) | `fun create(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Domain`](./index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
