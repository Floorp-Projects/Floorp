[android-components](../index.md) / [mozilla.components.lib.state](index.md) / [Middleware](./-middleware.md)

# Middleware

`typealias Middleware<S, A> = (store: `[`MiddlewareStore`](-middleware-store/index.md)`<`[`S`](-middleware.md#S)`, `[`A`](-middleware.md#A)`>, next: (`[`A`](-middleware.md#A)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, action: `[`A`](-middleware.md#A)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Middleware.kt#L18)

A [Middleware](./-middleware.md) sits between the store and the reducer. It provides an extension point between
dispatching an action, and the moment it reaches the reducer.

A [Middleware](./-middleware.md) can rewrite an [Action](-action.md), it can intercept an [Action](-action.md), dispatch additional
[Action](-action.md)s or perform side-effects when an [Action](-action.md) gets dispatched.

The [Store](-store/index.md) will create a chain of [Middleware](./-middleware.md) instances and invoke them in order. Every
[Middleware](./-middleware.md) can decide to continue the chain (by calling `next`), intercept the chain (by not
invoking `next`). A [Middleware](./-middleware.md) has no knowledge of what comes before or after it in the chain.

### Inheritors

| Name | Summary |
|---|---|
| [ContainerMiddleware](../mozilla.components.feature.containers/-container-middleware/index.md) | `class ContainerMiddleware : `[`Middleware`](./-middleware.md)`<`[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../mozilla.components.browser.state.action/-browser-action.md)`>`<br>[Middleware](./-middleware.md) implementation for handling [ContainerAction](../mozilla.components.browser.state.action/-container-action/index.md) and syncing the containers in [BrowserState.containers](../mozilla.components.browser.state.state/-browser-state/containers.md) with the [ContainerStorage](#). |
| [DownloadMiddleware](../mozilla.components.feature.downloads/-download-middleware/index.md) | `class DownloadMiddleware : `[`Middleware`](./-middleware.md)`<`[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../mozilla.components.browser.state.action/-browser-action.md)`>`<br>[Middleware](./-middleware.md) implementation for managing downloads via the provided download service. Its purpose is to react to global download state changes (e.g. of [BrowserState.downloads](../mozilla.components.browser.state.state/-browser-state/downloads.md)) and notify the download service, as needed. |
| [MediaMiddleware](../mozilla.components.feature.media.middleware/-media-middleware/index.md) | `class MediaMiddleware : `[`Middleware`](./-middleware.md)`<`[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../mozilla.components.browser.state.action/-browser-action.md)`>`<br>[Middleware](./-middleware.md) implementation for updating the [MediaState.Aggregate](../mozilla.components.browser.state.state/-media-state/-aggregate/index.md) and launching an [MediaServiceLauncher](#) |
| [ReaderViewMiddleware](../mozilla.components.feature.readerview/-reader-view-middleware/index.md) | `class ReaderViewMiddleware : `[`Middleware`](./-middleware.md)`<`[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../mozilla.components.browser.state.action/-browser-action.md)`>`<br>[Middleware](./-middleware.md) implementation for translating [BrowserAction](../mozilla.components.browser.state.action/-browser-action.md)s to [ReaderAction](../mozilla.components.browser.state.action/-reader-action/index.md)s (e.g. if the URL is updated a new "readerable" check should be executed.) |
| [ThumbnailsMiddleware](../mozilla.components.browser.thumbnails/-thumbnails-middleware/index.md) | `class ThumbnailsMiddleware : `[`Middleware`](./-middleware.md)`<`[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../mozilla.components.browser.state.action/-browser-action.md)`>`<br>[Middleware](./-middleware.md) implementation for handling [ContentAction.UpdateThumbnailAction](../mozilla.components.browser.state.action/-content-action/-update-thumbnail-action/index.md) and storing the thumbnail to the disk cache. |
