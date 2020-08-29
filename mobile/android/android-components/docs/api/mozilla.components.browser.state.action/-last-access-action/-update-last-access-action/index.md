[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [LastAccessAction](../index.md) / [UpdateLastAccessAction](./index.md)

# UpdateLastAccessAction

`data class UpdateLastAccessAction : `[`LastAccessAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L128)

Updates the timestamp of the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](tab-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateLastAccessAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, lastAccess: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis())`<br>Updates the timestamp of the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](tab-id.md). |

### Properties

| Name | Summary |
|---|---|
| [lastAccess](last-access.md) | `val lastAccess: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the value to signify when the tab was last accessed; defaults to [System.currentTimeMillis](http://docs.oracle.com/javase/7/docs/api/java/lang/System.html#currentTimeMillis()). |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the ID of the tab to update. |
