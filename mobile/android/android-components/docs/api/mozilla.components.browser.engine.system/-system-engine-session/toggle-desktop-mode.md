[android-components](../../index.md) / [mozilla.components.browser.engine.system](../index.md) / [SystemEngineSession](index.md) / [toggleDesktopMode](./toggle-desktop-mode.md)

# toggleDesktopMode

`fun toggleDesktopMode(enable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, reload: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/SystemEngineSession.kt#L380)

Overrides [EngineSession.toggleDesktopMode](../../mozilla.components.concept.engine/-engine-session/toggle-desktop-mode.md)

See [EngineSession.toggleDesktopMode](../../mozilla.components.concept.engine/-engine-session/toggle-desktop-mode.md)

Precondition:
If settings.useWideViewPort = true, then webSettings.useWideViewPort is always on
If settings.useWideViewPort = false or null, then webSettings.useWideViewPort can be on/off

