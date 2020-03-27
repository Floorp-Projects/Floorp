[android-components](../../index.md) / [mozilla.components.feature.tab.collections](../index.md) / [Tab](./index.md)

# Tab

`interface Tab` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tab-collections/src/main/java/mozilla/components/feature/tab/collections/Tab.kt#L15)

A tab of a [TabCollection](../-tab-collection/index.md).

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `abstract val id: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Unique ID identifying this tab. |
| [title](title.md) | `abstract val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The title of the tab. |
| [url](url.md) | `abstract val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL of the tab. |

### Functions

| Name | Summary |
|---|---|
| [restore](restore.md) | `abstract fun restore(context: <ERROR CLASS>, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, restoreSessionId: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Item`](../../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md)`?`<br>Restores a single tab from this collection and returns a matching [Session](../../mozilla.components.browser.session/-session/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
