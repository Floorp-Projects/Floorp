[android-components](../../index.md) / [mozilla.components.browser.session.action](../index.md) / [SessionAction](./index.md)

# SessionAction

`sealed class SessionAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/action/BrowserAction.kt#L39)

[BrowserAction](../-browser-action.md) implementations related to updating the a single [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) inside [BrowserState](../../mozilla.components.browser.session.state/-browser-state/index.md).

### Types

| Name | Summary |
|---|---|
| [AddHitResultAction](-add-hit-result-action/index.md) | `data class AddHitResultAction : `[`SessionAction`](./index.md)<br>Sets the [HitResult](../../mozilla.components.concept.engine/-hit-result/index.md) state of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-add-hit-result-action/session-id.md). |
| [CanGoBackAction](-can-go-back-action/index.md) | `data class CanGoBackAction : `[`SessionAction`](./index.md)<br>Updates the "canGoBack" state of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-can-go-back-action/session-id.md). |
| [CanGoForward](-can-go-forward/index.md) | `data class CanGoForward : `[`SessionAction`](./index.md)<br>Updates the "canGoForward" state of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-can-go-forward/session-id.md). |
| [ConsumeHitResultAction](-consume-hit-result-action/index.md) | `data class ConsumeHitResultAction : `[`SessionAction`](./index.md)<br>Clears the [HitResult](../../mozilla.components.concept.engine/-hit-result/index.md) state of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-consume-hit-result-action/session-id.md). |
| [UpdateProgressAction](-update-progress-action/index.md) | `data class UpdateProgressAction : `[`SessionAction`](./index.md)<br>Updates the progress of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-update-progress-action/session-id.md). |
| [UpdateUrlAction](-update-url-action/index.md) | `data class UpdateUrlAction : `[`SessionAction`](./index.md)<br>Updates the URL of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-update-url-action/session-id.md). |

### Inheritors

| Name | Summary |
|---|---|
| [AddHitResultAction](-add-hit-result-action/index.md) | `data class AddHitResultAction : `[`SessionAction`](./index.md)<br>Sets the [HitResult](../../mozilla.components.concept.engine/-hit-result/index.md) state of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-add-hit-result-action/session-id.md). |
| [CanGoBackAction](-can-go-back-action/index.md) | `data class CanGoBackAction : `[`SessionAction`](./index.md)<br>Updates the "canGoBack" state of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-can-go-back-action/session-id.md). |
| [CanGoForward](-can-go-forward/index.md) | `data class CanGoForward : `[`SessionAction`](./index.md)<br>Updates the "canGoForward" state of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-can-go-forward/session-id.md). |
| [ConsumeHitResultAction](-consume-hit-result-action/index.md) | `data class ConsumeHitResultAction : `[`SessionAction`](./index.md)<br>Clears the [HitResult](../../mozilla.components.concept.engine/-hit-result/index.md) state of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-consume-hit-result-action/session-id.md). |
| [UpdateProgressAction](-update-progress-action/index.md) | `data class UpdateProgressAction : `[`SessionAction`](./index.md)<br>Updates the progress of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-update-progress-action/session-id.md). |
| [UpdateUrlAction](-update-url-action/index.md) | `data class UpdateUrlAction : `[`SessionAction`](./index.md)<br>Updates the URL of the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-update-url-action/session-id.md). |
