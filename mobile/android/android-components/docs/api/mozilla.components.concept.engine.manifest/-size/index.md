[android-components](../../index.md) / [mozilla.components.concept.engine.manifest](../index.md) / [Size](./index.md)

# Size

`data class Size` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/Size.kt#L17)

Represents dimensions for an image.
Corresponds to values of the "sizes" HTML attribute.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Size(width: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Represents dimensions for an image. Corresponds to values of the "sizes" HTML attribute. |

### Properties

| Name | Summary |
|---|---|
| [height](height.md) | `val height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Height of the image. |
| [maxLength](max-length.md) | `val maxLength: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Gets the longest length between width and height. |
| [minLength](min-length.md) | `val minLength: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Gets the shortest length between width and height. |
| [width](width.md) | `val width: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Width of the image. |

### Functions

| Name | Summary |
|---|---|
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ANY](-a-n-y.md) | `val ANY: `[`Size`](./index.md)<br>Represents the "any" size. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [parse](parse.md) | `fun parse(raw: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Size`](./index.md)`?`<br>Parses a value from an HTML sizes attribute (512x512, 16x16, etc). Returns null if the value was invalid. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
