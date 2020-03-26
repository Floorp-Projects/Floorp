[android-components](../../index.md) / [mozilla.components.feature.addons.update](../index.md) / [AddonUpdater](index.md) / [registerForFutureUpdates](./register-for-future-updates.md)

# registerForFutureUpdates

`abstract fun registerForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L58)

Registers the given [addonId](register-for-future-updates.md#mozilla.components.feature.addons.update.AddonUpdater$registerForFutureUpdates(kotlin.String)/addonId) for periodically check for new updates.

### Parameters

`addonId` - The unique id of the addon.`open fun registerForFutureUpdates(extensions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L96)

Registers the [extensions](register-for-future-updates.md#mozilla.components.feature.addons.update.AddonUpdater$registerForFutureUpdates(kotlin.collections.List((mozilla.components.concept.engine.webextension.WebExtension)))/extensions) for periodic updates, if applicable. Built-in and
unsupported extensions will not update automatically.

### Parameters

`extensions` - The extensions to be registered for updates.