[android-components](../../index.md) / [mozilla.components.concept.engine.mediaquery](../index.md) / [PreferredColorScheme](./index.md)

# PreferredColorScheme

`sealed class PreferredColorScheme` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/mediaquery/PreferredColorScheme.kt#L11)

A simple data class used to suggest to page content that the user prefers a particular color
scheme.

### Types

| Name | Summary |
|---|---|
| [Dark](-dark.md) | `object Dark : `[`PreferredColorScheme`](./index.md) |
| [Light](-light.md) | `object Light : `[`PreferredColorScheme`](./index.md) |
| [System](-system.md) | `object System : `[`PreferredColorScheme`](./index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Dark](-dark.md) | `object Dark : `[`PreferredColorScheme`](./index.md) |
| [Light](-light.md) | `object Light : `[`PreferredColorScheme`](./index.md) |
| [System](-system.md) | `object System : `[`PreferredColorScheme`](./index.md) |
