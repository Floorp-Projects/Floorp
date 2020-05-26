[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [DrawableMenuIcon](./index.md)

# DrawableMenuIcon

`data class DrawableMenuIcon : `[`MenuIcon`](../-menu-icon.md)`, `[`MenuIconWithDrawable`](../-menu-icon-with-drawable/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuIcon.kt#L25)

Menu icon that displays a drawable.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DrawableMenuIcon(context: <ERROR CLASS>, resource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, tint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, effect: `[`MenuIconEffect`](../-menu-icon-effect.md)`? = null)``DrawableMenuIcon(drawable: <ERROR CLASS>?, tint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, effect: `[`MenuIconEffect`](../-menu-icon-effect.md)`? = null)`<br>Menu icon that displays a drawable. |

### Properties

| Name | Summary |
|---|---|
| [drawable](drawable.md) | `val drawable: <ERROR CLASS>?`<br>Drawable icon to display. |
| [effect](effect.md) | `val effect: `[`MenuIconEffect`](../-menu-icon-effect.md)`?`<br>Effects to apply to the icon. |
| [tint](tint.md) | `val tint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>Tint to apply to the drawable. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
