[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksFeature](./index.md)

# AppLinksFeature

`class AppLinksFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinksFeature.kt#L34)

This feature implements observer for handling redirects to external apps. The users are asked to
confirm their intention before leaving the app if in private session.  These include the Android
Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs.

It requires: a [Context](#), and a [FragmentManager](#).

### Parameters

`context` - Context the feature is associated with.

`sessionManager` - Provides access to a centralized registry of all active sessions.

`sessionId` - the session ID to observe.

`fragmentManager` - FragmentManager for interacting with fragments.

`dialog` - The dialog for redirect.

`launchInApp` - If {true} then launch app links in third party app(s). Default to false because
of security concerns.

`useCases` - These use cases allow for the detection of, and opening of links that other apps
have registered to open.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AppLinksFeature(context: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager? = null, dialog: `[`RedirectDialogFragment`](../-redirect-dialog-fragment/index.md)`? = null, launchInApp: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, useCases: `[`AppLinksUseCases`](../-app-links-use-cases/index.md)` = AppLinksUseCases(context, launchInApp), failedToLaunchAction: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`<br>This feature implements observer for handling redirects to external apps. The users are asked to confirm their intention before leaving the app if in private session.  These include the Android Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing app links on the selected session. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
