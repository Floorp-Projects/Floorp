[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [Session](index.md) / [hasParentSession](./has-parent-session.md)

# hasParentSession

`val hasParentSession: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/Session.kt#L492)

Returns true if this [Session](index.md) has a parent [Session](index.md).

A [Session](index.md) can have a parent [Session](index.md) if one was provided when calling [SessionManager.add](../-session-manager/add.md). The parent
[Session](index.md) is usually the [Session](index.md) the new [Session](index.md) was opened from - like opening a new tab from a link
context menu ("Open in new tab").

