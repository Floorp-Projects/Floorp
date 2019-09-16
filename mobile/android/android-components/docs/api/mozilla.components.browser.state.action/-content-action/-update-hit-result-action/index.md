[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateHitResultAction](./index.md)

# UpdateHitResultAction

`data class UpdateHitResultAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L190)

Updates the [HitResult](../../../mozilla.components.concept.engine/-hit-result/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateHitResultAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, hitResult: `[`HitResult`](../../../mozilla.components.concept.engine/-hit-result/index.md)`)`<br>Updates the [HitResult](../../../mozilla.components.concept.engine/-hit-result/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [hitResult](hit-result.md) | `val hitResult: `[`HitResult`](../../../mozilla.components.concept.engine/-hit-result/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
