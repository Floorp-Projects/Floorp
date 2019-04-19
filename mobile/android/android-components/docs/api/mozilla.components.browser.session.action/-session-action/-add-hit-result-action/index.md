[android-components](../../../index.md) / [mozilla.components.browser.session.action](../../index.md) / [SessionAction](../index.md) / [AddHitResultAction](./index.md)

# AddHitResultAction

`data class AddHitResultAction : `[`SessionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/action/BrowserAction.kt#L63)

Sets the [HitResult](../../../mozilla.components.concept.engine/-hit-result/index.md) state of the [SessionState](../../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddHitResultAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, hitResult: `[`HitResult`](../../../mozilla.components.concept.engine/-hit-result/index.md)`)`<br>Sets the [HitResult](../../../mozilla.components.concept.engine/-hit-result/index.md) state of the [SessionState](../../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [hitResult](hit-result.md) | `val hitResult: `[`HitResult`](../../../mozilla.components.concept.engine/-hit-result/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
