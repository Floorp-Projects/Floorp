[android-components](../../index.md) / [mozilla.components.feature.tab.collections](../index.md) / [TabCollection](./index.md)

# TabCollection

`interface TabCollection` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tab-collections/src/main/java/mozilla/components/feature/tab/collections/TabCollection.kt#L15)

A collection of tabs.

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `abstract val id: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Unique ID of this tab collection. |
| [tabs](tabs.md) | `abstract val tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>`<br>List of tabs in this tab collection. |
| [title](title.md) | `abstract val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Title of this tab collection. |

### Functions

| Name | Summary |
|---|---|
| [restore](restore.md) | `abstract fun restore(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, restoreSessionId: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)<br>Restores all tabs in this collection and returns a matching [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md). |
| [restoreSubset](restore-subset.md) | `abstract fun restoreSubset(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>, restoreSessionId: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)<br>Restores a subset of the tabs in this collection and returns a matching [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md). |
