[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](index.md) / [uninstallShortcuts](./uninstall-shortcuts.md)

# uninstallShortcuts

`suspend fun uninstallShortcuts(context: <ERROR CLASS>, startUrls: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, disabledMessage: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppShortcutManager.kt#L178)

Uninstalls a set of PWAs from the user's device by disabling their
shortcuts and removing the associated manifest data.

### Parameters

`startUrls` - List of manifest startUrls to remove.

`disabledMessage` - Message to display when a disable shortcut is tapped.