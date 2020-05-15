[android-components](../../index.md) / [mozilla.components.feature.qr](../index.md) / [QrFeature](./index.md)

# QrFeature

`class QrFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md)`, `[`PermissionsFeature`](../../mozilla.components.support.base.feature/-permissions-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/qr/src/main/java/mozilla/components/feature/qr/QrFeature.kt#L36)

Feature implementation that provides QR scanning functionality via the [QrFragment](../-qr-fragment/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `QrFeature(context: <ERROR CLASS>, fragmentManager: FragmentManager, onScanResult: `[`OnScanResult`](../-on-scan-result.md)` = { }, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)` = { }, scanMessage: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null)`<br>Feature implementation that provides QR scanning functionality via the [QrFragment](../-qr-fragment/index.md). |

### Properties

| Name | Summary |
|---|---|
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `val onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)<br>a callback invoked when permissions need to be requested before a QR scan can be performed. Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. This feature will request [android.Manifest.permission.CAMERA](#). |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Called when this [UserInteractionHandler](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md) gets the option to handle the user pressing the back key. |
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permission request was completed. If the requested permissions were granted it will open the QR scanner. |
| [scan](scan.md) | `fun scan(containerViewId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = android.R.id.content): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Starts the QR scanner fragment and listens for scan results. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [onHomePressed](../../mozilla.components.support.base.feature/-user-interaction-handler/on-home-pressed.md) | `open fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>In most cases, when the home button is pressed, we invoke this callback to inform the app that the user is going to leave the app. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
