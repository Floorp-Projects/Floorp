[android-components](../../../index.md) / [mozilla.components.feature.addons.update](../../index.md) / [DefaultAddonUpdater](../index.md) / [UpdateAttemptStorage](index.md) / [findUpdateAttemptBy](./find-update-attempt-by.md)

# findUpdateAttemptBy

`@WorkerThread fun findUpdateAttemptBy(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`UpdateAttempt`](../../-addon-updater/-update-attempt/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L526)

Finds the [AddonUpdater.UpdateAttempt](../../-addon-updater/-update-attempt/index.md) that match the [addonId](find-update-attempt-by.md#mozilla.components.feature.addons.update.DefaultAddonUpdater.UpdateAttemptStorage$findUpdateAttemptBy(kotlin.String)/addonId) otherwise returns null.

### Parameters

`addonId` - the id to be used as filter in the search.