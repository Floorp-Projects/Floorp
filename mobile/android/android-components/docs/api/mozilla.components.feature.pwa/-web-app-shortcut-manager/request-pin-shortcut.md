[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](index.md) / [requestPinShortcut](./request-pin-shortcut.md)

# requestPinShortcut

`suspend fun requestPinShortcut(context: <ERROR CLASS>, session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, overrideShortcutName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppShortcutManager.kt#L67)

Request to create a new shortcut on the home screen.

### Parameters

`context` - The current context.

`session` - The session to create the shortcut for.

`overrideShortcutName` - (optional) The name of the shortcut. Ignored for PWAs.