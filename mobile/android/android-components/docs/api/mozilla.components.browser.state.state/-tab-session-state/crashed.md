[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [TabSessionState](index.md) / [crashed](./crashed.md)

# crashed

`val crashed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/TabSessionState.kt#L33)

Overrides [SessionState.crashed](../-session-state/crashed.md)

Whether this session has crashed. In conjunction with a `concept-engine`
implementation that uses a multi-process architecture, single sessions can crash without crashing
the whole app. A crashed session may still be operational (since the underlying engine implementation
has recovered its content process), but further action may be needed to restore the last state
before the session has crashed (if desired).

### Property

`crashed` - Whether this session has crashed. In conjunction with a `concept-engine`
implementation that uses a multi-process architecture, single sessions can crash without crashing
the whole app. A crashed session may still be operational (since the underlying engine implementation
has recovered its content process), but further action may be needed to restore the last state
before the session has crashed (if desired).