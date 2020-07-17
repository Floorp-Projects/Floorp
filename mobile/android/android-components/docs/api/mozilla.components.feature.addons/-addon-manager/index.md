[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](./index.md)

# AddonManager

`class AddonManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L34)

Provides access to installed and recommended [Addon](../-addon/index.md)s and manages their states.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddonManager(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, runtime: `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, addonsProvider: `[`AddonsProvider`](../-addons-provider/index.md)`, addonUpdater: `[`AddonUpdater`](../../mozilla.components.feature.addons.update/-addon-updater/index.md)`)`<br>Provides access to installed and recommended [Addon](../-addon/index.md)s and manages their states. |

### Functions

| Name | Summary |
|---|---|
| [disableAddon](disable-addon.md) | `fun disableAddon(addon: `[`Addon`](../-addon/index.md)`, source: `[`EnableSource`](../../mozilla.components.concept.engine.webextension/-enable-source/index.md)` = EnableSource.USER, onSuccess: (`[`Addon`](../-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disables the provided [Addon](../-addon/index.md). |
| [enableAddon](enable-addon.md) | `fun enableAddon(addon: `[`Addon`](../-addon/index.md)`, source: `[`EnableSource`](../../mozilla.components.concept.engine.webextension/-enable-source/index.md)` = EnableSource.USER, onSuccess: (`[`Addon`](../-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables the provided [Addon](../-addon/index.md). |
| [getAddons](get-addons.md) | `suspend fun getAddons(waitForPendingActions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../-addon/index.md)`>`<br>Returns the list of all installed and recommended add-ons. |
| [installAddon](install-addon.md) | `fun installAddon(addon: `[`Addon`](../-addon/index.md)`, onSuccess: (`[`Addon`](../-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`CancellableOperation`](../../mozilla.components.concept.engine/-cancellable-operation/index.md)<br>Installs the provided [Addon](../-addon/index.md). |
| [setAddonAllowedInPrivateBrowsing](set-addon-allowed-in-private-browsing.md) | `fun setAddonAllowedInPrivateBrowsing(addon: `[`Addon`](../-addon/index.md)`, allowed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, onSuccess: (`[`Addon`](../-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets whether to allow/disallow the provided [Addon](../-addon/index.md) in private browsing mode. |
| [uninstallAddon](uninstall-addon.md) | `fun uninstallAddon(addon: `[`Addon`](../-addon/index.md)`, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Uninstalls the provided [Addon](../-addon/index.md). |
| [updateAddon](update-addon.md) | `fun updateAddon(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, onFinish: (`[`Status`](../../mozilla.components.feature.addons.update/-addon-updater/-status/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the addon with the provided [id](update-addon.md#mozilla.components.feature.addons.AddonManager$updateAddon(kotlin.String, kotlin.Function1((mozilla.components.feature.addons.update.AddonUpdater.Status, kotlin.Unit)))/id) if an update is available. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [BLOCKED_PERMISSIONS](-b-l-o-c-k-e-d_-p-e-r-m-i-s-s-i-o-n-s.md) | `val BLOCKED_PERMISSIONS: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
