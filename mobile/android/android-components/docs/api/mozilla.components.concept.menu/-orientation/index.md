[android-components](../../index.md) / [mozilla.components.concept.menu](../index.md) / [Orientation](./index.md)

# Orientation

`enum class Orientation` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/Orientation.kt#L12)

Indicates the preferred orientation to show the menu.

### Enum Values

| Name | Summary |
|---|---|
| [UP](-u-p.md) | Position the menu above the toolbar. |
| [DOWN](-d-o-w-n.md) | Position the menu below the toolbar. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromGravity](from-gravity.md) | `fun fromGravity(gravity: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Orientation`](./index.md)<br>Returns an orientation that matches the given [Gravity](#) value. Meant to be used with a CoordinatorLayout's gravity. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
