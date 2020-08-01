[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [UserInteractionHandler](./index.md)

# UserInteractionHandler

`interface UserInteractionHandler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/UserInteractionHandler.kt#L13)

Generic interface for fragments, features and other components that want to handle user
interactions such as 'back' or 'home' button presses.

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `abstract fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Called when this [UserInteractionHandler](./index.md) gets the option to handle the user pressing the back key. |
| [onHomePressed](on-home-pressed.md) | `open fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>In most cases, when the home button is pressed, we invoke this callback to inform the app that the user is going to leave the app. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [CustomTabsToolbarFeature](../../mozilla.components.feature.customtabs/-custom-tabs-toolbar-feature/index.md) | `class CustomTabsToolbarFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](./index.md)<br>Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig. |
| [FindInPageFeature](../../mozilla.components.feature.findinpage/-find-in-page-feature/index.md) | `class FindInPageFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](./index.md)<br>Feature implementation that will keep a [FindInPageView](../../mozilla.components.feature.findinpage.view/-find-in-page-view/index.md) in sync with a bound [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md). |
| [FullScreenFeature](../../mozilla.components.feature.session/-full-screen-feature/index.md) | `open class FullScreenFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](./index.md)<br>Feature implementation for handling fullscreen mode (exiting and back button presses). |
| [QrFeature](../../mozilla.components.feature.qr/-qr-feature/index.md) | `class QrFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](./index.md)`, `[`PermissionsFeature`](../-permissions-feature/index.md)<br>Feature implementation that provides QR scanning functionality via the [QrFragment](../../mozilla.components.feature.qr/-qr-fragment/index.md). |
| [ReaderViewFeature](../../mozilla.components.feature.readerview/-reader-view-feature/index.md) | `class ReaderViewFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](./index.md)<br>Feature implementation that provides a reader view for the selected session, based on a web extension. |
| [SessionFeature](../../mozilla.components.feature.session/-session-feature/index.md) | `class SessionFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](./index.md)<br>Feature implementation for connecting the engine module with the session module. |
| [ToolbarFeature](../../mozilla.components.feature.toolbar/-toolbar-feature/index.md) | `class ToolbarFeature : `[`LifecycleAwareFeature`](../-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](./index.md)<br>Feature implementation for connecting a toolbar implementation with the session module. |
