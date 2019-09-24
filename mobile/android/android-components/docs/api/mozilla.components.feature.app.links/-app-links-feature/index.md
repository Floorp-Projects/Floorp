[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksFeature](./index.md)

# AppLinksFeature

`class AppLinksFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinksFeature.kt#L45)

This feature implements use cases for detecting and handling redirects to external apps. The user
is asked to confirm her intention before leaving the app. These include the Android Intents,
custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs.

In the case of Android Intents that are not installed, and with no fallback, the user is prompted
to search the installed market place.

It provides use cases to detect and open links openable in third party non-browser apps.

It provides a [RequestInterceptor](../../mozilla.components.concept.engine.request/-request-interceptor/index.md) to do the detection and asking of consent.

It requires: a [Context](#), and a [FragmentManager](#).

A [Boolean](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) flag is provided at construction to allow the feature and use cases to be landed without
adjoining UI. The UI will be activated in https://github.com/mozilla-mobile/android-components/issues/2974
and https://github.com/mozilla-mobile/android-components/issues/2975.

### Parameters

`alwaysAllowedSchemes` - List of schemes that will always be allowed to be opened in a third-party
app even if [interceptLinkClicks](#) is `false`.

`alwaysDeniedSchemes` - List of schemes that will never be opened in a third-party app even if
[interceptLinkClicks](#) is `true`.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AppLinksFeature(context: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, interceptLinkClicks: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, alwaysAllowedSchemes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = setOf("mailto", "market", "sms", "tel"), alwaysDeniedSchemes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = setOf("javascript"), fragmentManager: FragmentManager? = null, dialog: `[`RedirectDialogFragment`](../-redirect-dialog-fragment/index.md)` = SimpleRedirectDialogFragment.newInstance(), useCases: `[`AppLinksUseCases`](../-app-links-use-cases/index.md)` = AppLinksUseCases(context))`<br>This feature implements use cases for detecting and handling redirects to external apps. The user is asked to confirm her intention before leaving the app. These include the Android Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing app links on the selected session. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
