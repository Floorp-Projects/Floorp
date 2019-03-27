[android-components](../../index.md) / [mozilla.components.feature.customtabs](../index.md) / [AbstractCustomTabsService](./index.md)

# AbstractCustomTabsService

`abstract class AbstractCustomTabsService : CustomTabsService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/AbstractCustomTabsService.kt#L26)

[Service](https://developer.android.com/reference/android/app/Service.html) providing Custom Tabs related functionality.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractCustomTabsService()`<br>[Service](https://developer.android.com/reference/android/app/Service.html) providing Custom Tabs related functionality. |

### Properties

| Name | Summary |
|---|---|
| [engine](engine.md) | `abstract val engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md) |

### Functions

| Name | Summary |
|---|---|
| [extraCommand](extra-command.md) | `open fun extraCommand(commandName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, args: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?): `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?` |
| [mayLaunchUrl](may-launch-url.md) | `open fun mayLaunchUrl(sessionToken: CustomTabsSessionToken?, url: `[`Uri`](https://developer.android.com/reference/android/net/Uri.html)`?, extras: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?, otherLikelyBundles: `[`MutableList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-list/index.html)`<`[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`>?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [newSession](new-session.md) | `open fun newSession(sessionToken: CustomTabsSessionToken?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [postMessage](post-message.md) | `open fun postMessage(sessionToken: CustomTabsSessionToken?, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, extras: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [requestPostMessageChannel](request-post-message-channel.md) | `open fun requestPostMessageChannel(sessionToken: CustomTabsSessionToken?, postMessageOrigin: `[`Uri`](https://developer.android.com/reference/android/net/Uri.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [updateVisuals](update-visuals.md) | `open fun updateVisuals(sessionToken: CustomTabsSessionToken?, bundle: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [validateRelationship](validate-relationship.md) | `open fun validateRelationship(sessionToken: CustomTabsSessionToken?, relation: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, origin: `[`Uri`](https://developer.android.com/reference/android/net/Uri.html)`?, extras: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [warmup](warmup.md) | `open fun warmup(flags: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Extension Properties

| Name | Summary |
|---|---|
| [appName](../../mozilla.components.support.ktx.android.content/android.content.-context/app-name.md) | `val `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.appName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name (label) of the application or the package name as a fallback. |
| [appVersionName](../../mozilla.components.support.ktx.android.content/android.content.-context/app-version-name.md) | `val `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.appVersionName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The (visible) version name of the application, as specified by the  tag's versionName attribute. E.g. "2.0". |

### Extension Functions

| Name | Summary |
|---|---|
| [isMainProcess](../../mozilla.components.support.ktx.android.content/android.content.-context/is-main-process.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isMainProcess(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if we are running in the main process false otherwise. |
| [isOSOnLowMemory](../../mozilla.components.support.ktx.android.content/android.content.-context/is-o-s-on-low-memory.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isOSOnLowMemory(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not the operating system is under low memory conditions. |
| [isPermissionGranted](../../mozilla.components.support.ktx.android.content/android.content.-context/is-permission-granted.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isPermissionGranted(vararg permission: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns if a list of permission have been granted, if all the permission have been granted returns true otherwise false. |
| [runOnlyInMainProcess](../../mozilla.components.support.ktx.android.content/android.content.-context/run-only-in-main-process.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.runOnlyInMainProcess(block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Takes a function runs it only it if we are running in the main process, otherwise the function will not be executed. |
| [share](../../mozilla.components.support.ktx.android.content/android.content.-context/share.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.share(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, subject: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = getString(R.string.mozac_support_ktx_share_dialog_title)): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Shares content via [ACTION_SEND](https://developer.android.com/reference/android/content/Intent.html#ACTION_SEND) intent. |
| [systemService](../../mozilla.components.support.ktx.android.content/android.content.-context/system-service.md) | `fun <T> `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.systemService(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](../../mozilla.components.support.ktx.android.content/android.content.-context/system-service.md#T)<br>Returns the handle to a system-level service by name. |
