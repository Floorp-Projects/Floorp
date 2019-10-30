[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [PushError](./index.md)

# PushError

`sealed class PushError` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/PushProcessor.kt#L98)

Various error types.

### Types

| Name | Summary |
|---|---|
| [MalformedMessage](-malformed-message/index.md) | `data class MalformedMessage : `[`PushError`](./index.md) |
| [Network](-network/index.md) | `data class Network : `[`PushError`](./index.md) |
| [Registration](-registration/index.md) | `data class Registration : `[`PushError`](./index.md) |
| [Rust](-rust/index.md) | `data class Rust : `[`PushError`](./index.md) |
| [ServiceUnavailable](-service-unavailable/index.md) | `data class ServiceUnavailable : `[`PushError`](./index.md) |

### Properties

| Name | Summary |
|---|---|
| [desc](desc.md) | `open val desc: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [MalformedMessage](-malformed-message/index.md) | `data class MalformedMessage : `[`PushError`](./index.md) |
| [Network](-network/index.md) | `data class Network : `[`PushError`](./index.md) |
| [Registration](-registration/index.md) | `data class Registration : `[`PushError`](./index.md) |
| [Rust](-rust/index.md) | `data class Rust : `[`PushError`](./index.md) |
| [ServiceUnavailable](-service-unavailable/index.md) | `data class ServiceUnavailable : `[`PushError`](./index.md) |
