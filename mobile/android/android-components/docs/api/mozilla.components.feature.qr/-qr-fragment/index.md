[android-components](../../index.md) / [mozilla.components.feature.qr](../index.md) / [QrFragment](./index.md)

# QrFragment

`class QrFragment : Fragment` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/qr/src/main/java/mozilla/components/feature/qr/QrFragment.kt#L72)

A [Fragment](#) that displays a QR scanner.

This class is based on Camera2BasicFragment from:

https://github.com/googlesamples/android-Camera2Basic
https://github.com/kismkof/camera2basic

### Types

| Name | Summary |
|---|---|
| [OnScanCompleteListener](-on-scan-complete-listener/index.md) | `interface OnScanCompleteListener : `[`Serializable`](https://developer.android.com/reference/java/io/Serializable.html)<br>Listener invoked when the QR scan completed successfully. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `QrFragment()`<br>A [Fragment](#) that displays a QR scanner. |

### Functions

| Name | Summary |
|---|---|
| [onCreateView](on-create-view.md) | `fun onCreateView(inflater: `[`LayoutInflater`](https://developer.android.com/reference/android/view/LayoutInflater.html)`, container: `[`ViewGroup`](https://developer.android.com/reference/android/view/ViewGroup.html)`?, savedInstanceState: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?): `[`View`](https://developer.android.com/reference/android/view/View.html)`?` |
| [onPause](on-pause.md) | `fun onPause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onResume](on-resume.md) | `fun onResume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onViewCreated](on-view-created.md) | `fun onViewCreated(view: `[`View`](https://developer.android.com/reference/android/view/View.html)`, savedInstanceState: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [newInstance](new-instance.md) | `fun newInstance(listener: `[`OnScanCompleteListener`](-on-scan-complete-listener/index.md)`): `[`QrFragment`](./index.md) |
