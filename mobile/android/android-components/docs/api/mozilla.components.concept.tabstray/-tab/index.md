[android-components](../../index.md) / [mozilla.components.concept.tabstray](../index.md) / [Tab](./index.md)

# Tab

`data class Tab` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/tabstray/src/main/java/mozilla/components/concept/tabstray/Tab.kt#L18)

Data class representing a tab to be displayed in a [TabsTray](../-tabs-tray/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Tab(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", icon: <ERROR CLASS>? = null, thumbnail: <ERROR CLASS>? = null)`<br>Data class representing a tab to be displayed in a [TabsTray](../-tabs-tray/index.md). |

### Properties

| Name | Summary |
|---|---|
| [icon](icon.md) | `val icon: <ERROR CLASS>?`<br>Current icon of the tab (or null) |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Unique ID of the tab. |
| [thumbnail](thumbnail.md) | `val thumbnail: <ERROR CLASS>?`<br>Current thumbnail of the tab (or null) |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Current title of the tab (or an empty [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)]). |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Current URL of the tab. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
