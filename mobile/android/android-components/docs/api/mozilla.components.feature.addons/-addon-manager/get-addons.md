[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](index.md) / [getAddons](./get-addons.md)

# getAddons

`suspend fun getAddons(waitForPendingActions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../-addon/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L56)

Returns the list of all installed and recommended add-ons.

### Parameters

`waitForPendingActions` - whether or not to wait (suspend, but not
block) until all pending add-on actions (install/uninstall/enable/disable)
are completed in either success or failure.

### Exceptions

`AddonManagerException` - in case of a problem reading from
the [addonsProvider](#) or querying web extension state from the engine / store.

**Return**
list of all [Addon](../-addon/index.md)s with up-to-date [Addon.installedState](../-addon/installed-state.md).

