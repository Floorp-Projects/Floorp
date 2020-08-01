[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateFirstContentfulPaintStateAction](./index.md)

# UpdateFirstContentfulPaintStateAction

`data class UpdateFirstContentfulPaintStateAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L281)

Updates the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) of the given [sessionId](session-id.md) to indicate whether the first contentful paint has happened.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateFirstContentfulPaintStateAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, firstContentfulPaint: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Updates the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) of the given [sessionId](session-id.md) to indicate whether the first contentful paint has happened. |

### Properties

| Name | Summary |
|---|---|
| [firstContentfulPaint](first-contentful-paint.md) | `val firstContentfulPaint: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
