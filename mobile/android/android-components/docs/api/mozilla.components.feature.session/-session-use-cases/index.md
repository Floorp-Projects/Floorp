[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [SessionUseCases](./index.md)

# SessionUseCases

`class SessionUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L20)

Contains use cases related to the session feature.

### Parameters

`sessionManager` - the application's [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md).

`onNoSession` - When invoking a use case that requires a (selected) [Session](../../mozilla.components.browser.session/-session/index.md) and when no [Session](../../mozilla.components.browser.session/-session/index.md) is available
this (optional) lambda will be invoked to create a [Session](../../mozilla.components.browser.session/-session/index.md). The default implementation creates a [Session](../../mozilla.components.browser.session/-session/index.md) and adds
it to the [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md).

### Types

| Name | Summary |
|---|---|
| [ClearDataUseCase](-clear-data-use-case/index.md) | `class ClearDataUseCase` |
| [CrashRecoveryUseCase](-crash-recovery-use-case/index.md) | `class CrashRecoveryUseCase`<br>Tries to recover from a crash by restoring the last know state. |
| [DefaultLoadUrlUseCase](-default-load-url-use-case/index.md) | `class DefaultLoadUrlUseCase : `[`LoadUrlUseCase`](-load-url-use-case/index.md) |
| [ExitFullScreenUseCase](-exit-full-screen-use-case/index.md) | `class ExitFullScreenUseCase` |
| [GoBackUseCase](-go-back-use-case/index.md) | `class GoBackUseCase` |
| [GoForwardUseCase](-go-forward-use-case/index.md) | `class GoForwardUseCase` |
| [GoToHistoryIndexUseCase](-go-to-history-index-use-case/index.md) | `class GoToHistoryIndexUseCase`<br>Use case to jump to an arbitrary history index in a session's backstack. |
| [LoadDataUseCase](-load-data-use-case/index.md) | `class LoadDataUseCase` |
| [LoadUrlUseCase](-load-url-use-case/index.md) | `interface LoadUrlUseCase`<br>Contract for use cases that load a provided URL. |
| [ReloadUrlUseCase](-reload-url-use-case/index.md) | `class ReloadUrlUseCase` |
| [RequestDesktopSiteUseCase](-request-desktop-site-use-case/index.md) | `class RequestDesktopSiteUseCase` |
| [StopLoadingUseCase](-stop-loading-use-case/index.md) | `class StopLoadingUseCase` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SessionUseCases(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, onNoSession: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Session`](../../mozilla.components.browser.session/-session/index.md)` = { url ->
        Session(url).apply { sessionManager.add(this) }
    })`<br>Contains use cases related to the session feature. |

### Properties

| Name | Summary |
|---|---|
| [clearData](clear-data.md) | `val clearData: `[`ClearDataUseCase`](-clear-data-use-case/index.md) |
| [crashRecovery](crash-recovery.md) | `val crashRecovery: `[`CrashRecoveryUseCase`](-crash-recovery-use-case/index.md) |
| [exitFullscreen](exit-fullscreen.md) | `val exitFullscreen: `[`ExitFullScreenUseCase`](-exit-full-screen-use-case/index.md) |
| [goBack](go-back.md) | `val goBack: `[`GoBackUseCase`](-go-back-use-case/index.md) |
| [goForward](go-forward.md) | `val goForward: `[`GoForwardUseCase`](-go-forward-use-case/index.md) |
| [goToHistoryIndex](go-to-history-index.md) | `val goToHistoryIndex: `[`GoToHistoryIndexUseCase`](-go-to-history-index-use-case/index.md) |
| [loadData](load-data.md) | `val loadData: `[`LoadDataUseCase`](-load-data-use-case/index.md) |
| [loadUrl](load-url.md) | `val loadUrl: `[`DefaultLoadUrlUseCase`](-default-load-url-use-case/index.md) |
| [reload](reload.md) | `val reload: `[`ReloadUrlUseCase`](-reload-url-use-case/index.md) |
| [requestDesktopSite](request-desktop-site.md) | `val requestDesktopSite: `[`RequestDesktopSiteUseCase`](-request-desktop-site-use-case/index.md) |
| [stopLoading](stop-loading.md) | `val stopLoading: `[`StopLoadingUseCase`](-stop-loading-use-case/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
