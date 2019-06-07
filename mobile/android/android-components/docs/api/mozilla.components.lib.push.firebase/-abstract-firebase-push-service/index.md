[android-components](../../index.md) / [mozilla.components.lib.push.firebase](../index.md) / [AbstractFirebasePushService](./index.md)

# AbstractFirebasePushService

`abstract class AbstractFirebasePushService : FirebaseMessagingService, `[`PushService`](../../mozilla.components.concept.push/-push-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/push-firebase/src/main/java/mozilla/components/lib/push/firebase/AbstractFirebasePushService.kt#L22)

A Firebase Cloud Messaging implementation of the [PushService](../../mozilla.components.concept.push/-push-service/index.md) for Android devices that support Google Play Services.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractFirebasePushService()`<br>A Firebase Cloud Messaging implementation of the [PushService](../../mozilla.components.concept.push/-push-service/index.md) for Android devices that support Google Play Services. |

### Functions

| Name | Summary |
|---|---|
| [onMessageReceived](on-message-received.md) | `open fun onMessageReceived(remoteMessage: RemoteMessage?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onNewToken](on-new-token.md) | `open fun onNewToken(newToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.md) | `open fun start(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initializes Firebase and starts the messaging service if not already started and enables auto-start as well. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the Firebase messaging service and disables auto-start. |

### Extension Properties

| Name | Summary |
|---|---|
| [appName](../../mozilla.components.support.ktx.android.content/android.content.-context/app-name.md) | `val `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.appName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name (label) of the application or the package name as a fallback. |
| [appVersionName](../../mozilla.components.support.ktx.android.content/android.content.-context/app-version-name.md) | `val `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.appVersionName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The (visible) version name of the application, as specified by the  tag's versionName attribute. E.g. "2.0". |

### Extension Functions

| Name | Summary |
|---|---|
| [hasCamera](../../mozilla.components.support.ktx.android.content/android.content.-context/has-camera.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.hasCamera(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether or not the device has a camera. |
| [isMainProcess](../../mozilla.components.support.ktx.android.content/android.content.-context/is-main-process.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isMainProcess(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if we are running in the main process false otherwise. |
| [isOSOnLowMemory](../../mozilla.components.support.ktx.android.content/android.content.-context/is-o-s-on-low-memory.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isOSOnLowMemory(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not the operating system is under low memory conditions. |
| [isPermissionGranted](../../mozilla.components.support.ktx.android.content/android.content.-context/is-permission-granted.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isPermissionGranted(vararg permission: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns if a list of permission have been granted, if all the permission have been granted returns true otherwise false. |
| [runOnlyInMainProcess](../../mozilla.components.support.ktx.android.content/android.content.-context/run-only-in-main-process.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.runOnlyInMainProcess(block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Takes a function runs it only it if we are running in the main process, otherwise the function will not be executed. |
| [share](../../mozilla.components.support.ktx.android.content/android.content.-context/share.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.share(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, subject: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = getString(R.string.mozac_support_ktx_share_dialog_title)): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Shares content via [ACTION_SEND](https://developer.android.com/reference/android/content/Intent.html#ACTION_SEND) intent. |
| [systemService](../../mozilla.components.support.ktx.android.content/android.content.-context/system-service.md) | `fun <T> `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.systemService(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](../../mozilla.components.support.ktx.android.content/android.content.-context/system-service.md#T)<br>Returns the handle to a system-level service by name. |
