[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](index.md) / [updateAddon](./update-addon.md)

# updateAddon

`fun updateAddon(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, onFinish: (`[`Status`](../../mozilla.components.feature.addons.update/-addon-updater/-status/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L68)

Try updates the addon with the provided [id](update-addon.md#mozilla.components.feature.addons.AddonManager$updateAddon(kotlin.String, kotlin.Function1((mozilla.components.feature.addons.update.AddonUpdater.Status, kotlin.Unit)))/id) by updating it in the the [store](#) and re-attaching
all the handlers. If there is no update available nothing will happen.

