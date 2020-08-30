[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [EngineAction](../index.md) / [ReloadAction](./index.md)

# ReloadAction

`data class ReloadAction : `[`EngineAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L486)

Reloads the tab with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ReloadAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = EngineSession.LoadUrlFlags.none())`<br>Reloads the tab with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [flags](flags.md) | `val flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
