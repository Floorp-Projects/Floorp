[android-components](../../../index.md) / [mozilla.components.feature.addons.update](../../index.md) / [DefaultAddonUpdater](../index.md) / [UpdateAttemptStorage](./index.md)

# UpdateAttemptStorage

`class UpdateAttemptStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L505)

A storage implementation to persist [AddonUpdater.UpdateAttempt](../../-addon-updater/-update-attempt/index.md)s.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateAttemptStorage(context: <ERROR CLASS>)`<br>A storage implementation to persist [AddonUpdater.UpdateAttempt](../../-addon-updater/-update-attempt/index.md)s. |

### Functions

| Name | Summary |
|---|---|
| [findUpdateAttemptBy](find-update-attempt-by.md) | `fun findUpdateAttemptBy(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`UpdateAttempt`](../../-addon-updater/-update-attempt/index.md)`?`<br>Finds the [AddonUpdater.UpdateAttempt](../../-addon-updater/-update-attempt/index.md) that match the [addonId](find-update-attempt-by.md#mozilla.components.feature.addons.update.DefaultAddonUpdater.UpdateAttemptStorage$findUpdateAttemptBy(kotlin.String)/addonId) otherwise returns null. |
