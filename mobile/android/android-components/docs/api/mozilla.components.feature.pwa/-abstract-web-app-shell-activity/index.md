[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [AbstractWebAppShellActivity](./index.md)

# AbstractWebAppShellActivity

`abstract class AbstractWebAppShellActivity : AppCompatActivity` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/AbstractWebAppShellActivity.kt#L21)

Activity for "standalone" and "fullscreen" web applications.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractWebAppShellActivity()`<br>Activity for "standalone" and "fullscreen" web applications. |

### Properties

| Name | Summary |
|---|---|
| [engine](engine.md) | `abstract val engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md) |
| [engineView](engine-view.md) | `lateinit var engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md) |
| [session](session.md) | `lateinit var session: `[`Session`](../../mozilla.components.browser.session/-session/index.md) |
| [sessionManager](session-manager.md) | `abstract val sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md) |

### Functions

| Name | Summary |
|---|---|
| [onCreate](on-create.md) | `open fun onCreate(savedInstanceState: `[`Bundle`](https://developer.android.com/reference/android/os/Bundle.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDestroy](on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [INTENT_ACTION](-i-n-t-e-n-t_-a-c-t-i-o-n.md) | `const val INTENT_ACTION: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Properties

| Name | Summary |
|---|---|
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
