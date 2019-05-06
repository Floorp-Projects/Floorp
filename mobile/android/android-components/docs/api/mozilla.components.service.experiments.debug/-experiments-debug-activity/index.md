[android-components](../../index.md) / [mozilla.components.service.experiments.debug](../index.md) / [ExperimentsDebugActivity](./index.md)

# ExperimentsDebugActivity

`class ExperimentsDebugActivity : `[`Activity`](https://developer.android.com/reference/android/app/Activity.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/debug/ExperimentsDebugActivity.kt#L19)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ExperimentsDebugActivity()` |

### Functions

| Name | Summary |
|---|---|
| [onCreate](on-create.md) | `fun onCreate(savedInstanceState: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [OVERRIDE_BRANCH_EXTRA_KEY](-o-v-e-r-r-i-d-e_-b-r-a-n-c-h_-e-x-t-r-a_-k-e-y.md) | `const val OVERRIDE_BRANCH_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [OVERRIDE_CLEAR_ALL_EXTRA_KEY](-o-v-e-r-r-i-d-e_-c-l-e-a-r_-a-l-l_-e-x-t-r-a_-k-e-y.md) | `const val OVERRIDE_CLEAR_ALL_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [OVERRIDE_EXPERIMENT_EXTRA_KEY](-o-v-e-r-r-i-d-e_-e-x-p-e-r-i-m-e-n-t_-e-x-t-r-a_-k-e-y.md) | `const val OVERRIDE_EXPERIMENT_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [SET_KINTO_INSTANCE_EXTRA_KEY](-s-e-t_-k-i-n-t-o_-i-n-s-t-a-n-c-e_-e-x-t-r-a_-k-e-y.md) | `const val SET_KINTO_INSTANCE_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [UPDATE_EXPERIMENTS_EXTRA_KEY](-u-p-d-a-t-e_-e-x-p-e-r-i-m-e-n-t-s_-e-x-t-r-a_-k-e-y.md) | `const val UPDATE_EXPERIMENTS_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Properties

| Name | Summary |
|---|---|
| [appName](../../mozilla.components.support.ktx.android.content/android.content.-context/app-name.md) | `val `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.appName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name (label) of the application or the package name as a fallback. |
| [appVersionName](../../mozilla.components.support.ktx.android.content/android.content.-context/app-version-name.md) | `val `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.appVersionName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The (visible) version name of the application, as specified by the  tag's versionName attribute. E.g. "2.0". |

### Extension Functions

| Name | Summary |
|---|---|
| [applyOrientation](../../mozilla.components.feature.pwa.ext/android.app.-activity/apply-orientation.md) | `fun `[`Activity`](https://developer.android.com/reference/android/app/Activity.html)`.applyOrientation(manifest: `[`WebAppManifest`](../../mozilla.components.browser.session.manifest/-web-app-manifest/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the requested orientation of the [Activity](https://developer.android.com/reference/android/app/Activity.html) to the orientation provided by the given [WebAppManifest](../../mozilla.components.browser.session.manifest/-web-app-manifest/index.md) (See [WebAppManifest.orientation](../../mozilla.components.browser.session.manifest/-web-app-manifest/orientation.md) and [WebAppManifest.Orientation](../../mozilla.components.browser.session.manifest/-web-app-manifest/-orientation/index.md). |
| [enterToImmersiveMode](../../mozilla.components.support.ktx.android.view/android.app.-activity/enter-to-immersive-mode.md) | `fun `[`Activity`](https://developer.android.com/reference/android/app/Activity.html)`.enterToImmersiveMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Attempts to call immersive mode using the View to hide the status bar and navigation buttons. |
| [exitImmersiveModeIfNeeded](../../mozilla.components.support.ktx.android.view/android.app.-activity/exit-immersive-mode-if-needed.md) | `fun `[`Activity`](https://developer.android.com/reference/android/app/Activity.html)`.exitImmersiveModeIfNeeded(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Attempts to come out from immersive mode using the View. |
| [isMainProcess](../../mozilla.components.support.ktx.android.content/android.content.-context/is-main-process.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isMainProcess(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if we are running in the main process false otherwise. |
| [isOSOnLowMemory](../../mozilla.components.support.ktx.android.content/android.content.-context/is-o-s-on-low-memory.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isOSOnLowMemory(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not the operating system is under low memory conditions. |
| [isPermissionGranted](../../mozilla.components.support.ktx.android.content/android.content.-context/is-permission-granted.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isPermissionGranted(vararg permission: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns if a list of permission have been granted, if all the permission have been granted returns true otherwise false. |
| [runOnlyInMainProcess](../../mozilla.components.support.ktx.android.content/android.content.-context/run-only-in-main-process.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.runOnlyInMainProcess(block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Takes a function runs it only it if we are running in the main process, otherwise the function will not be executed. |
| [share](../../mozilla.components.support.ktx.android.content/android.content.-context/share.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.share(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, subject: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = getString(R.string.mozac_support_ktx_share_dialog_title)): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Shares content via [ACTION_SEND](https://developer.android.com/reference/android/content/Intent.html#ACTION_SEND) intent. |
| [systemService](../../mozilla.components.support.ktx.android.content/android.content.-context/system-service.md) | `fun <T> `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.systemService(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](../../mozilla.components.support.ktx.android.content/android.content.-context/system-service.md#T)<br>Returns the handle to a system-level service by name. |
