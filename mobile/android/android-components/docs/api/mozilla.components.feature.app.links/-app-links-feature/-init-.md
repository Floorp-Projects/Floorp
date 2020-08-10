[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AppLinksFeature(context: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager? = null, dialog: `[`RedirectDialogFragment`](../-redirect-dialog-fragment/index.md)`? = null, launchInApp: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, useCases: `[`AppLinksUseCases`](../-app-links-use-cases/index.md)` = AppLinksUseCases(context, launchInApp), failedToLaunchAction: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, loadUrlUseCase: `[`DefaultLoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-default-load-url-use-case/index.md)`? = null)`

This feature implements observer for handling redirects to external apps. The users are asked to
confirm their intention before leaving the app if in private session.  These include the Android
Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs.

It requires: a [Context](#), and a [FragmentManager](#).

### Parameters

`context` - Context the feature is associated with.

`sessionManager` - Provides access to a centralized registry of all active sessions.

`sessionId` - The session ID to observe.

`fragmentManager` - FragmentManager for interacting with fragments.

`dialog` - The dialog for redirect.

`launchInApp` - If {true} then launch app links in third party app(s). Default to false because
of security concerns.

`useCases` - These use cases allow for the detection of, and opening of links that other apps
have registered to open.

`failedToLaunchAction` - Action to perform when failing to launch in third party app.

`loadUrlUseCase` - Used to load URL if user decides not to launch in third party app.