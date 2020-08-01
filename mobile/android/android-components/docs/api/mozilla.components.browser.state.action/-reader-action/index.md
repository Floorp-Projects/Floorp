[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [ReaderAction](./index.md)

# ReaderAction

`sealed class ReaderAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L456)

[BrowserAction](../-browser-action.md) implementations related to updating the [ReaderState](../../mozilla.components.browser.state.state/-reader-state/index.md) of a single [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) inside
[BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md).

### Types

| Name | Summary |
|---|---|
| [ClearReaderActiveUrlAction](-clear-reader-active-url-action/index.md) | `data class ClearReaderActiveUrlAction : `[`ReaderAction`](./index.md)<br>Clears the [ReaderState.activeUrl](../../mozilla.components.browser.state.state/-reader-state/active-url.md). |
| [UpdateReaderActiveAction](-update-reader-active-action/index.md) | `data class UpdateReaderActiveAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.active](../../mozilla.components.browser.state.state/-reader-state/active.md) flag. |
| [UpdateReaderActiveUrlAction](-update-reader-active-url-action/index.md) | `data class UpdateReaderActiveUrlAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.activeUrl](../../mozilla.components.browser.state.state/-reader-state/active-url.md). |
| [UpdateReaderBaseUrlAction](-update-reader-base-url-action/index.md) | `data class UpdateReaderBaseUrlAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.readerBaseUrl](#). |
| [UpdateReaderConnectRequiredAction](-update-reader-connect-required-action/index.md) | `data class UpdateReaderConnectRequiredAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.connectRequired](../../mozilla.components.browser.state.state/-reader-state/connect-required.md) flag. |
| [UpdateReaderableAction](-update-readerable-action/index.md) | `data class UpdateReaderableAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.readerable](../../mozilla.components.browser.state.state/-reader-state/readerable.md) flag. |
| [UpdateReaderableCheckRequiredAction](-update-readerable-check-required-action/index.md) | `data class UpdateReaderableCheckRequiredAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.checkRequired](../../mozilla.components.browser.state.state/-reader-state/check-required.md) flag. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [ClearReaderActiveUrlAction](-clear-reader-active-url-action/index.md) | `data class ClearReaderActiveUrlAction : `[`ReaderAction`](./index.md)<br>Clears the [ReaderState.activeUrl](../../mozilla.components.browser.state.state/-reader-state/active-url.md). |
| [UpdateReaderActiveAction](-update-reader-active-action/index.md) | `data class UpdateReaderActiveAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.active](../../mozilla.components.browser.state.state/-reader-state/active.md) flag. |
| [UpdateReaderActiveUrlAction](-update-reader-active-url-action/index.md) | `data class UpdateReaderActiveUrlAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.activeUrl](../../mozilla.components.browser.state.state/-reader-state/active-url.md). |
| [UpdateReaderBaseUrlAction](-update-reader-base-url-action/index.md) | `data class UpdateReaderBaseUrlAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.readerBaseUrl](#). |
| [UpdateReaderConnectRequiredAction](-update-reader-connect-required-action/index.md) | `data class UpdateReaderConnectRequiredAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.connectRequired](../../mozilla.components.browser.state.state/-reader-state/connect-required.md) flag. |
| [UpdateReaderableAction](-update-readerable-action/index.md) | `data class UpdateReaderableAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.readerable](../../mozilla.components.browser.state.state/-reader-state/readerable.md) flag. |
| [UpdateReaderableCheckRequiredAction](-update-readerable-check-required-action/index.md) | `data class UpdateReaderableCheckRequiredAction : `[`ReaderAction`](./index.md)<br>Updates the [ReaderState.checkRequired](../../mozilla.components.browser.state.state/-reader-state/check-required.md) flag. |
