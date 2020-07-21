[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SelectionAwareSessionObserver](index.md) / [observeSelected](./observe-selected.md)

# observeSelected

`fun observeSelected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SelectionAwareSessionObserver.kt#L41)

Starts observing changes to the selected session (see
[SessionManager.selectedSession](../-session-manager/selected-session.md)). If a different session is selected
the observer will automatically be switched over and only notified of
changes to the newly selected session.

