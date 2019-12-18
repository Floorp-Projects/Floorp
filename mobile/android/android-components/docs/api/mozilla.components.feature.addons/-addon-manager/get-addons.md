[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](index.md) / [getAddons](./get-addons.md)

# getAddons

`suspend fun getAddons(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../-addon/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L46)

Returns the list of all installed and recommended add-ons.

### Exceptions

`AddonManagerException` - in case of a problem reading from
the [addonsProvider](#) or querying web extension state from the engine / store.

**Return**
list of all [Addon](../-addon/index.md)s with up-to-date [Addon.installedState](../-addon/installed-state.md).

