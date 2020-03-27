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
| [MediaMiddleware](../mozilla.components.feature.media.middleware/-media-middleware/index.md) | `class MediaMiddleware : `[`Middleware`](./-middleware.md)`<`[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../mozilla.components.browser.state.action/-browser-action.md)`>`<br>[Middleware](./-middleware.md) implementation for updating the [MediaState.Aggregate](../mozilla.components.browser.state.state/-media-state/-aggregate/index.md) and launching an [MediaServiceLauncher](#) |
