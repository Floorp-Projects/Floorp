[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [MenuIconWithDrawable](./index.md)

# MenuIconWithDrawable

`interface MenuIconWithDrawable` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuIcon.kt#L76)

Interface shared by all [MenuIcon](../-menu-icon.md)s with drawables.

### Properties

| Name | Summary |
|---|---|
| [drawable](drawable.md) | `abstract val drawable: <ERROR CLASS>?` |
| [tint](tint.md) | `abstract val tint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DrawableButtonMenuIcon](../-drawable-button-menu-icon/index.md) | `data class DrawableButtonMenuIcon : `[`MenuIcon`](../-menu-icon.md)`, `[`MenuIconWithDrawable`](./index.md)<br>Menu icon that displays an image button. |
| [DrawableMenuIcon](../-drawable-menu-icon/index.md) | `data class DrawableMenuIcon : `[`MenuIcon`](../-menu-icon.md)`, `[`MenuIconWithDrawable`](./index.md)<br>Menu icon that displays a drawable. |
