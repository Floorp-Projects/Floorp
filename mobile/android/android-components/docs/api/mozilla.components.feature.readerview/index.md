[android-components](../index.md) / [mozilla.components.feature.readerview](./index.md)

## Package mozilla.components.feature.readerview

### Types

| Name | Summary |
|---|---|
| [ReaderViewFeature](-reader-view-feature/index.md) | `class ReaderViewFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../mozilla.components.support.base.feature/-user-interaction-handler/index.md)<br>Feature implementation that provides a reader view for the selected session, based on a web extension. |
| [ReaderViewMiddleware](-reader-view-middleware/index.md) | `class ReaderViewMiddleware : `[`Middleware`](../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../mozilla.components.browser.state.action/-browser-action.md)`>`<br>[Middleware](../mozilla.components.lib.state/-middleware.md) implementation for translating [BrowserAction](../mozilla.components.browser.state.action/-browser-action.md)s to [ReaderAction](../mozilla.components.browser.state.action/-reader-action/index.md)s (e.g. if the URL is updated a new "readerable" check should be executed.) |

### Type Aliases

| Name | Summary |
|---|---|
| [onReaderViewStatusChange](on-reader-view-status-change.md) | `typealias onReaderViewStatusChange = (available: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
