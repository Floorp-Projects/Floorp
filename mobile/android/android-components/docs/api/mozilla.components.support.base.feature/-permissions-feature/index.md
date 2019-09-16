[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [PermissionsFeature](./index.md)

# PermissionsFeature

`interface PermissionsFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/PermissionsFeature.kt#L34)

Interface for features that need to request permissions from the user.

Example integration:

```
class MyFragment : Fragment {
    val myFeature = MyPermissionsFeature(
        onNeedToRequestPermissions = { permissions ->
            requestPermissions(permissions, REQUEST_CODE_MY_FEATURE)
        }
    )

    override fun onRequestPermissionsResult(resultCode: Int, permissions: Array<String>, grantResults: IntArray) {
        if (resultCode == REQUEST_CODE_MY_FEATURE) {
            myFeature.onPermissionsResult(permissions, grantResults)
        }
    }

    companion object {
        private const val REQUEST_CODE_MY_FEATURE = 1
    }
}
```

### Properties

| Name | Summary |
|---|---|
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `abstract val onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../-on-need-to-request-permissions.md)<br>A callback invoked when permissions need to be requested by the feature before it can complete its task. Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. |

### Functions

| Name | Summary |
|---|---|
| [onPermissionsResult](on-permissions-result.md) | `abstract fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that a permission request was completed. The feature should react to this and complete its task. |

### Inheritors

| Name | Summary |
|---|---|
| [DownloadsFeature](../../mozilla.components.feature.downloads/-downloads-feature/index.md) | `class DownloadsFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](./index.md)<br>Feature implementation to provide download functionality for the selected session. The feature will subscribe to the selected session and listen for downloads. |
| [PromptFeature](../../mozilla.components.feature.prompts/-prompt-feature/index.md) | `class PromptFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](./index.md)<br>Feature for displaying native dialogs for html elements like: input type date, file, time, color, option, menu, authentication, confirmation and alerts. |
| [QrFeature](../../mozilla.components.feature.qr/-qr-feature/index.md) | `class QrFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../-back-handler/index.md)`, `[`PermissionsFeature`](./index.md)<br>Feature implementation that provides QR scanning functionality via the [QrFragment](../../mozilla.components.feature.qr/-qr-fragment/index.md). |
| [SitePermissionsFeature](../../mozilla.components.feature.sitepermissions/-site-permissions-feature/index.md) | `class SitePermissionsFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](./index.md)<br>This feature will subscribe to the currently selected [Session](../../mozilla.components.browser.session/-session/index.md) and display a suitable dialogs based on [Session.Observer.onAppPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-app-permission-requested.md) or [Session.Observer.onContentPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-content-permission-requested.md)  events. Once the dialog is closed the [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) will be consumed. |
