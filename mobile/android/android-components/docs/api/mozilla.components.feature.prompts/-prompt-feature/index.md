[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](./index.md)

# PromptFeature

`class PromptFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](../../mozilla.components.support.base.feature/-permissions-feature/index.md)`, Prompter` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/PromptFeature.kt#L113)

Feature for displaying native dialogs for html elements like: input type
date, file, time, color, option, menu, authentication, confirmation and alerts.

There are some requests that are handled with intents instead of dialogs,
like file choosers and others. For this reason, you have to keep the feature
aware of the flow of requesting data from other apps, overriding
onActivityResult in your [Activity](#) or [Fragment](#) and forward its calls
to [onActivityResult](on-activity-result.md).

This feature will subscribe to the currently selected session and display
a suitable native dialog based on [Session.Observer.onPromptRequested](#) events.
Once the dialog is closed or the user selects an item from the dialog
the related [PromptRequest](../../mozilla.components.concept.engine.prompt/-prompt-request/index.md) will be consumed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PromptFeature(activity: <ERROR CLASS>, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, shareDelegate: `[`ShareDelegate`](../../mozilla.components.feature.prompts.share/-share-delegate/index.md)` = DefaultShareDelegate(), loginValidationDelegate: `[`LoginValidationDelegate`](../../mozilla.components.concept.storage/-login-validation-delegate/index.md)`? = null, isSaveLoginEnabled: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, loginExceptionStorage: `[`LoginExceptions`](../-login-exceptions/index.md)`? = null, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)`<br>`PromptFeature(fragment: Fragment, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, shareDelegate: `[`ShareDelegate`](../../mozilla.components.feature.prompts.share/-share-delegate/index.md)` = DefaultShareDelegate(), loginValidationDelegate: `[`LoginValidationDelegate`](../../mozilla.components.concept.storage/-login-validation-delegate/index.md)`? = null, isSaveLoginEnabled: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, loginExceptionStorage: `[`LoginExceptions`](../-login-exceptions/index.md)`? = null, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)`<br>`PromptFeature(activity: <ERROR CLASS>? = null, fragment: Fragment? = null, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)` |

### Properties

| Name | Summary |
|---|---|
| [loginExceptionStorage](login-exception-storage.md) | `val loginExceptionStorage: `[`LoginExceptions`](../-login-exceptions/index.md)`?`<br>An implementation of [LoginExceptions](../-login-exceptions/index.md) that saves and checks origins the user does not want to see a save login dialog for. |
| [loginValidationDelegate](login-validation-delegate.md) | `val loginValidationDelegate: `[`LoginValidationDelegate`](../../mozilla.components.concept.storage/-login-validation-delegate/index.md)`?`<br>Validates whether or not a given Login may be stored. |
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `val onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)<br>A callback invoked when permissions need to be requested before a prompt (e.g. a file picker) can be displayed. Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. |

### Functions

| Name | Summary |
|---|---|
| [onActivityResult](on-activity-result.md) | `fun onActivityResult(requestCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, resultCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, intent: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature of intent results for prompt requests handled by other apps like file chooser requests. |
| [onCancel](on-cancel.md) | `fun onCancel(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a dialog is dismissed. This consumes the [PromptFeature](./index.md) value from the session indicated by [sessionId](on-cancel.md#mozilla.components.feature.prompts.PromptFeature$onCancel(kotlin.String)/sessionId). |
| [onClear](on-clear.md) | `fun onClear(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when the user is requesting to clear the selected value from the dialog. This consumes the [PromptFeature](./index.md) value from the [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) indicated by [sessionId](on-clear.md#mozilla.components.feature.prompts.PromptFeature$onClear(kotlin.String)/sessionId). |
| [onConfirm](on-confirm.md) | `fun onConfirm(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when the user confirms the action on the dialog. This consumes the [PromptFeature](./index.md) value from the [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) indicated by [sessionId](on-confirm.md#mozilla.components.feature.prompts.PromptFeature$onConfirm(kotlin.String, kotlin.Any)/sessionId). |
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permissions request was completed. It will then either process or dismiss the prompt request. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing the selected session to listen for prompt requests and displays a dialog when needed. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops observing the selected session for incoming prompt requests. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
