[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [EngineAction](../index.md) / [ClearDataAction](./index.md)

# ClearDataAction

`data class ClearDataAction : `[`EngineAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L531)

Clears browsing data for the tab with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ClearDataAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, data: `[`BrowsingData`](../../../mozilla.components.concept.engine/-engine/-browsing-data/index.md)`)`<br>Clears browsing data for the tab with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [data](data.md) | `val data: `[`BrowsingData`](../../../mozilla.components.concept.engine/-engine/-browsing-data/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
