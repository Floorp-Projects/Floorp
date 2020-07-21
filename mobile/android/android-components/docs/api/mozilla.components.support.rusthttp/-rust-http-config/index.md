[android-components](../../index.md) / [mozilla.components.support.rusthttp](../index.md) / [RustHttpConfig](./index.md)

# RustHttpConfig

`object RustHttpConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/rusthttp/src/main/java/mozilla/components/support/rusthttp/RustHttpConfig.kt#L13)

An object allowing configuring the HTTP client used by Rust code.

### Functions

| Name | Summary |
|---|---|
| [setClient](set-client.md) | `fun setClient(c: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set the HTTP client to be used by all Rust code. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
