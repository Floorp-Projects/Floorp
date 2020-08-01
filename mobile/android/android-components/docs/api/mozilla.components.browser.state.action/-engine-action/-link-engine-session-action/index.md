[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [EngineAction](../index.md) / [LinkEngineSessionAction](./index.md)

# LinkEngineSessionAction

`data class LinkEngineSessionAction : `[`EngineAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L436)

Attaches the provided [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) to the session with the provided [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LinkEngineSessionAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, engineSession: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`)`<br>Attaches the provided [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) to the session with the provided [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [engineSession](engine-session.md) | `val engineSession: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
