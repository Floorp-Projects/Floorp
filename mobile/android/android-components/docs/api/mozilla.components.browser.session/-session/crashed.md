[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [Session](index.md) / [crashed](./crashed.md)

# crashed

`var crashed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/Session.kt#L438)

Whether this [Session](index.md) has crashed.

In conjunction with a `concept-engine` implementation that uses a multi-process architecture, single sessions
can crash without crashing the whole app.

A crashed session may still be operational (since the underlying engine implementation has recovered its content
process), but further action may be needed to restore the last state before the session has crashed (if desired).

**Getter**

Whether this [Session](index.md) has crashed.

In conjunction with a `concept-engine` implementation that uses a multi-process architecture, single sessions
can crash without crashing the whole app.

A crashed session may still be operational (since the underlying engine implementation has recovered its content
process), but further action may be needed to restore the last state before the session has crashed (if desired).

**Setter**

Whether this [Session](index.md) has crashed.

In conjunction with a `concept-engine` implementation that uses a multi-process architecture, single sessions
can crash without crashing the whole app.

A crashed session may still be operational (since the underlying engine implementation has recovered its content
process), but further action may be needed to restore the last state before the session has crashed (if desired).

