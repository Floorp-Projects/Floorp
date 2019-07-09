[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`PromptFeature(activity: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)`
`PromptFeature(fragment: Fragment, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)``PromptFeature(activity: <ERROR CLASS>? = null, fragment: Fragment? = null, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)`

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

