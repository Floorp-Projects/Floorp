[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](./index.md)

# PromptFeature

`class PromptFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](../../mozilla.components.support.base.feature/-permissions-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/PromptFeature.kt#L72)

Feature for displaying native dialogs for html elements like: input type
date, file, time, color, option, menu, authentication, confirmation and alerts.

There are some requests that are handled with intents instead of dialogs,
like file choosers and others. For this reason, you have to keep the feature
aware of the flow of requesting data from other apps, overriding
onActivityResult in your [Activity](#) or [Fragment](#) and forward its calls
to [onActivityResult](on-activity-result.md).

This feature will subscribe to the currently selected [Session](../../mozilla.components.browser.session/-session/index.md) and display
a suitable native dialog based on [Session.Observer.onPromptRequested](../../mozilla.components.browser.session/-session/-observer/on-prompt-requested.md) events.
Once the dialog is closed or the user selects an item from the dialog
the related [PromptRequest](../../mozilla.components.concept.engine.prompt/-prompt-request/index.md) will be consumed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PromptFeature(activity: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)`<br>`PromptFeature(fragment: Fragment, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)``PromptFeature(activity: <ERROR CLASS>? = null, fragment: Fragment? = null, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)`<br>Feature for displaying native dialogs for html elements like: input type date, file, time, color, option, menu, authentication, confirmation and alerts. |

### Properties

| Name | Summary |
|---|---|
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `val onNeedToRequestPermissions: <ERROR CLASS>`<br>a callback invoked when permissions need to be requested before a prompt (e.g. a file picker) can be displayed. Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. |

### Functions

| Name | Summary |
|---|---|
| [onActivityResult](on-activity-result.md) | `fun onActivityResult(requestCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, resultCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, intent: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature of intent results for prompt requests handled by other apps like file chooser requests. |
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permissions request was completed. It will then either process or dismiss the prompt request. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing the selected session to listen for prompt requests and displays a dialog when needed. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops observing the selected session for incoming prompt requests. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [FILE_PICKER_ACTIVITY_REQUEST_CODE](-f-i-l-e_-p-i-c-k-e-r_-a-c-t-i-v-i-t-y_-r-e-q-u-e-s-t_-c-o-d-e.md) | `const val FILE_PICKER_ACTIVITY_REQUEST_CODE: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
