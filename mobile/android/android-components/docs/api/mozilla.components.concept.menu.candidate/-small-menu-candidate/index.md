[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [SmallMenuCandidate](./index.md)

# SmallMenuCandidate

`data class SmallMenuCandidate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/SmallMenuCandidate.kt#L16)

Small icon button menu option. Can only be used with [RowMenuCandidate](../-row-menu-candidate/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SmallMenuCandidate(contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, icon: `[`DrawableMenuIcon`](../-drawable-menu-icon/index.md)`, containerStyle: `[`ContainerStyle`](../-container-style/index.md)` = ContainerStyle(), onLongClick: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = null, onClick: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`<br>Small icon button menu option. Can only be used with [RowMenuCandidate](../-row-menu-candidate/index.md). |

### Properties

| Name | Summary |
|---|---|
| [containerStyle](container-style.md) | `val containerStyle: `[`ContainerStyle`](../-container-style/index.md)<br>Styling to apply to the container. |
| [contentDescription](content-description.md) | `val contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Description of the icon. |
| [icon](icon.md) | `val icon: `[`DrawableMenuIcon`](../-drawable-menu-icon/index.md)<br>Icon to display. |
| [onClick](on-click.md) | `val onClick: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Click listener called when this menu option is clicked. |
| [onLongClick](on-long-click.md) | `val onLongClick: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Listener called when this menu option is long clicked. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
