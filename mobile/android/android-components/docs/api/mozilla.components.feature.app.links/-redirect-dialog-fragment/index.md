[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [RedirectDialogFragment](./index.md)

# RedirectDialogFragment

`abstract class RedirectDialogFragment : DialogFragment` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/RedirectDialogFragment.kt#L16)

This is a general representation of a dialog meant to be used in collaboration with [AppLinksInterceptor](../-app-links-interceptor/index.md)
to show a dialog before an external link is opened.
If [SimpleRedirectDialogFragment](../-simple-redirect-dialog-fragment/index.md) is not flexible enough for your use case you should inherit for this class.
Be mindful to call [onConfirmRedirect](on-confirm-redirect.md) when you want to open the linked app.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RedirectDialogFragment()`<br>This is a general representation of a dialog meant to be used in collaboration with [AppLinksInterceptor](../-app-links-interceptor/index.md) to show a dialog before an external link is opened. If [SimpleRedirectDialogFragment](../-simple-redirect-dialog-fragment/index.md) is not flexible enough for your use case you should inherit for this class. Be mindful to call [onConfirmRedirect](on-confirm-redirect.md) when you want to open the linked app. |

### Properties

| Name | Summary |
|---|---|
| [onCancelRedirect](on-cancel-redirect.md) | `var onCancelRedirect: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`?`<br>A callback to trigger when user dismisses the dialog. For instance, a valid use case can be in confirmation dialog, after the negative button is clicked, this callback must be called. |
| [onConfirmRedirect](on-confirm-redirect.md) | `var onConfirmRedirect: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A callback to trigger a download, call it when you are ready to open the linked app. For instance, a valid use case can be in confirmation dialog, after the positive button is clicked, this callback must be called. |

### Functions

| Name | Summary |
|---|---|
| [setAppLinkRedirectUrl](set-app-link-redirect-url.md) | `fun setAppLinkRedirectUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>add the metadata of this download object to the arguments of this fragment. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [FRAGMENT_TAG](-f-r-a-g-m-e-n-t_-t-a-g.md) | `const val FRAGMENT_TAG: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [KEY_INTENT_URL](-k-e-y_-i-n-t-e-n-t_-u-r-l.md) | `const val KEY_INTENT_URL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Key for finding the app link. |

### Extension Functions

| Name | Summary |
|---|---|
| [consumeFrom](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md) | `fun <S : `[`State`](../../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../../mozilla.components.lib.state/-action.md)`> Fragment.consumeFrom(store: `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`S`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#A)`>, block: (`[`S`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#S)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Helper extension method for consuming [State](../../mozilla.components.lib.state/-state.md) from a [Store](../../mozilla.components.lib.state/-store/index.md) sequentially in order inside a [Fragment](#). The [block](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#mozilla.components.lib.state.ext$consumeFrom(androidx.fragment.app.Fragment, mozilla.components.lib.state.Store((mozilla.components.lib.state.ext.consumeFrom.S, mozilla.components.lib.state.ext.consumeFrom.A)), kotlin.Function1((mozilla.components.lib.state.ext.consumeFrom.S, kotlin.Unit)))/block) function will get invoked for every [State](../../mozilla.components.lib.state/-state.md) update. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [SimpleRedirectDialogFragment](../-simple-redirect-dialog-fragment/index.md) | `class SimpleRedirectDialogFragment : `[`RedirectDialogFragment`](./index.md)<br>This is the default implementation of the [RedirectDialogFragment](./index.md). |
