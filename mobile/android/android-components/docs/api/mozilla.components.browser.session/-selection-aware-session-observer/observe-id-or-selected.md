[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SelectionAwareSessionObserver](index.md) / [observeIdOrSelected](./observe-id-or-selected.md)

# observeIdOrSelected

`fun observeIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SelectionAwareSessionObserver.kt#L53)

Starts observing changes to the session matching the [sessionId](observe-id-or-selected.md#mozilla.components.browser.session.SelectionAwareSessionObserver$observeIdOrSelected(kotlin.String)/sessionId). If
the session does not exist, then observe the selected session.

### Parameters

`sessionId` - the session ID to observe.