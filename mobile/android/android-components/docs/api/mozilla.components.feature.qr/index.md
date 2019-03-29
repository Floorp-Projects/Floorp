[android-components](../index.md) / [mozilla.components.feature.qr](./index.md)

## Package mozilla.components.feature.qr

### Types

| Name | Summary |
|---|---|
| [QrFeature](-qr-feature/index.md) | `class QrFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../mozilla.components.support.base.feature/-back-handler/index.md)<br>Feature implementation that provides QR scanning functionality via the [QrFragment](-qr-fragment/index.md). |
| [QrFragment](-qr-fragment/index.md) | `class QrFragment : Fragment`<br>A [Fragment](#) that displays a QR scanner. |

### Type Aliases

| Name | Summary |
|---|---|
| [OnNeedToRequestPermissions](-on-need-to-request-permissions.md) | `typealias OnNeedToRequestPermissions = (permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [OnScanResult](-on-scan-result.md) | `typealias OnScanResult = (result: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
