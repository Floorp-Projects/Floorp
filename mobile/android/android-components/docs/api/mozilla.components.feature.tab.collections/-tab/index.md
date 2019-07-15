[android-components](../../index.md) / [mozilla.components.feature.tab.collections](../index.md) / [Tab](./index.md)

# Tab

`interface Tab` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tab-collections/src/main/java/mozilla/components/feature/tab/collections/Tab.kt#L14)

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
| [restore](restore.md) | `abstract fun restore(context: <ERROR CLASS>, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, tab: `[`Tab`](./index.md)`, restoreSessionId: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?`<br>Restores a single tab from this collection and returns a matching [Session](../../mozilla.components.browser.session/-session/index.md). |
