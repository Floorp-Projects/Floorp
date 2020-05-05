[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [Tab](./index.md)

# Tab

`data class Tab` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/RemoteTabsStorage.kt#L119)

A tab, which is defined by an history (think the previous/next button in your web browser) and
a currently active history entry.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Tab(history: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabEntry`](../-tab-entry/index.md)`>, active: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, lastUsed: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)`<br>A tab, which is defined by an history (think the previous/next button in your web browser) and a currently active history entry. |

### Properties

| Name | Summary |
|---|---|
| [active](active.md) | `val active: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [history](history.md) | `val history: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabEntry`](../-tab-entry/index.md)`>` |
| [lastUsed](last-used.md) | `val lastUsed: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Functions

| Name | Summary |
|---|---|
| [active](active.md) | `fun active(): `[`TabEntry`](../-tab-entry/index.md)<br>The current active tab entry. In other words, this is the page that's currently shown for a tab. |
| [next](next.md) | `fun next(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabEntry`](../-tab-entry/index.md)`>`<br>The list of tabs history entries that come after this tab. In other words, the "next" navigation button history list. |
| [previous](previous.md) | `fun previous(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabEntry`](../-tab-entry/index.md)`>`<br>The list of tabs history entries that come before this tab. In other words, the "previous" navigation button history list. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
