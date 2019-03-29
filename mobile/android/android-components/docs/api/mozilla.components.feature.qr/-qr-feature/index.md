[android-components](../../index.md) / [mozilla.components.feature.qr](../index.md) / [QrFeature](./index.md)

# QrFeature

`class QrFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../../mozilla.components.support.base.feature/-back-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/qr/src/main/java/mozilla/components/feature/qr/QrFeature.kt#L32)

Feature implementation that provides QR scanning functionality via the [QrFragment](../-qr-fragment/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `QrFeature(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, fragmentManager: FragmentManager, onScanResult: `[`OnScanResult`](../-on-scan-result.md)` = { }, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../-on-need-to-request-permissions.md)` = { })`<br>Feature implementation that provides QR scanning functionality via the [QrFragment](../-qr-fragment/index.md). |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Called when this [BackHandler](../../mozilla.components.support.base.feature/-back-handler/index.md) gets the option to handle the user pressing the back key. |
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permission request was completed. If the requested permissions were granted it will open the QR scanner. |
| [scan](scan.md) | `fun scan(containerViewId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = android.R.id.content): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Starts the QR scanner fragment and listens for scan results. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
