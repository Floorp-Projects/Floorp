[android-components](../../index.md) / [mozilla.components.concept.tabstray](../index.md) / [Tabs](./index.md)

# Tabs

`data class Tabs` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/tabstray/src/main/java/mozilla/components/concept/tabstray/Tabs.kt#L13)

Aggregate data type keeping a reference to the list of tabs and the index of the selected tab.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Tabs(list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>, selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Aggregate data type keeping a reference to the list of tabs and the index of the selected tab. |

### Properties

| Name | Summary |
|---|---|
| [list](list.md) | `val list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>`<br>The list of tabs. |
| [selectedIndex](selected-index.md) | `val selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Index of the selected tab in the list of tabs (or -1). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
