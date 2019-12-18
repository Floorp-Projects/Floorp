[android-components](../index.md) / [mozilla.components.feature.app.links](./index.md)

## Package mozilla.components.feature.app.links

### Types

| Name | Summary |
|---|---|
| [AppLinkRedirect](-app-link-redirect/index.md) | `data class AppLinkRedirect`<br>Data class for the external Intent or fallback URL a given URL encodes for. |
| [AppLinksFeature](-app-links-feature/index.md) | `class AppLinksFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>This feature implements observer for handling redirects to external apps. The users are asked to confirm their intention before leaving the app if in private session.  These include the Android Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs. |
| [AppLinksInterceptor](-app-links-interceptor/index.md) | `class AppLinksInterceptor : `[`RequestInterceptor`](../mozilla.components.concept.engine.request/-request-interceptor/index.md)<br>This feature implements use cases for detecting and handling redirects to external apps. The user is asked to confirm her intention before leaving the app. These include the Android Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs. |
| [AppLinksUseCases](-app-links-use-cases/index.md) | `class AppLinksUseCases`<br>These use cases allow for the detection of, and opening of links that other apps have registered an [IntentFilter](#)s to open. |
| [RedirectDialogFragment](-redirect-dialog-fragment/index.md) | `abstract class RedirectDialogFragment : DialogFragment`<br>This is a general representation of a dialog meant to be used in collaboration with [AppLinksInterceptor](-app-links-interceptor/index.md) to show a dialog before an external link is opened. If [SimpleRedirectDialogFragment](-simple-redirect-dialog-fragment/index.md) is not flexible enough for your use case you should inherit for this class. Be mindful to call [onConfirmRedirect](-redirect-dialog-fragment/on-confirm-redirect.md) when you want to open the linked app. |
| [SimpleRedirectDialogFragment](-simple-redirect-dialog-fragment/index.md) | `class SimpleRedirectDialogFragment : `[`RedirectDialogFragment`](-redirect-dialog-fragment/index.md)<br>This is the default implementation of the [RedirectDialogFragment](-redirect-dialog-fragment/index.md). |
