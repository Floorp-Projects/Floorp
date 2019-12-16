[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](./index.md)

# AddonManager

`class AddonManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L23)

Provides access to installed and recommended [Addon](../-addon/index.md)s and manages their states.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddonManager(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, addonsProvider: `[`AddonsProvider`](../-addons-provider/index.md)`, addonUpdater: `[`AddonUpdater`](../../mozilla.components.feature.addons.update/-addon-updater/index.md)`)`<br>Provides access to installed and recommended [Addon](../-addon/index.md)s and manages their states. |

### Functions

| Name | Summary |
|---|---|
| [getAddons](get-addons.md) | `suspend fun getAddons(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../-addon/index.md)`>`<br>Returns the list of all installed and recommended add-ons. |
| [updateAddon](update-addon.md) | `fun updateAddon(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, onFinish: (`[`Status`](../../mozilla.components.feature.addons.update/-addon-updater/-status/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Try updates the addon with the provided [id](update-addon.md#mozilla.components.feature.addons.AddonManager$updateAddon(kotlin.String, kotlin.Function1((mozilla.components.feature.addons.update.AddonUpdater.Status, kotlin.Unit)))/id) by updating it in the the [store](#) and re-attaching all the handlers. If there is no update available nothing will happen. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
