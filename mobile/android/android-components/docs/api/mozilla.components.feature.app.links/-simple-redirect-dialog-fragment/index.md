[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [SimpleRedirectDialogFragment](./index.md)

# SimpleRedirectDialogFragment

`class SimpleRedirectDialogFragment : `[`RedirectDialogFragment`](../-redirect-dialog-fragment/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/SimpleRedirectDialogFragment.kt#L23)

This is the default implementation of the [RedirectDialogFragment](../-redirect-dialog-fragment/index.md).

It provides an [AlertDialog](#) giving the user the choice to allow or deny the opening of a
third party app.

Intents passed are guaranteed to be openable by a non-browser app.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SimpleRedirectDialogFragment()`<br>This is the default implementation of the [RedirectDialogFragment](../-redirect-dialog-fragment/index.md). |

### Inherited Properties

| Name | Summary |
|---|---|
| [onCancelRedirect](../-redirect-dialog-fragment/on-cancel-redirect.md) | `var onCancelRedirect: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`?`<br>A callback to trigger when user dismisses the dialog. For instance, a valid use case can be in confirmation dialog, after the negative button is clicked, this callback must be called. |
| [onConfirmRedirect](../-redirect-dialog-fragment/on-confirm-redirect.md) | `var onConfirmRedirect: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A callback to trigger a download, call it when you are ready to open the linked app. For instance, a valid use case can be in confirmation dialog, after the positive button is clicked, this callback must be called. |

### Functions

| Name | Summary |
|---|---|
| [onCreateDialog](on-create-dialog.md) | `fun onCreateDialog(savedInstanceState: <ERROR CLASS>?): <ERROR CLASS>` |

### Inherited Functions

| Name | Summary |
|---|---|
| [setAppLinkRedirectUrl](../-redirect-dialog-fragment/set-app-link-redirect-url.md) | `fun setAppLinkRedirectUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>add the metadata of this download object to the arguments of this fragment. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [KEY_CANCELABLE](-k-e-y_-c-a-n-c-e-l-a-b-l-e.md) | `const val KEY_CANCELABLE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [KEY_NEGATIVE_TEXT](-k-e-y_-n-e-g-a-t-i-v-e_-t-e-x-t.md) | `const val KEY_NEGATIVE_TEXT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [KEY_POSITIVE_TEXT](-k-e-y_-p-o-s-i-t-i-v-e_-t-e-x-t.md) | `const val KEY_POSITIVE_TEXT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [KEY_THEME_ID](-k-e-y_-t-h-e-m-e_-i-d.md) | `const val KEY_THEME_ID: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [KEY_TITLE_TEXT](-k-e-y_-t-i-t-l-e_-t-e-x-t.md) | `const val KEY_TITLE_TEXT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [newInstance](new-instance.md) | `fun newInstance(dialogTitleText: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.string.mozac_feature_applinks_confirm_dialog_title, positiveButtonText: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.string.mozac_feature_applinks_confirm_dialog_confirm, negativeButtonText: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.string.mozac_feature_applinks_confirm_dialog_deny, themeResId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, cancelable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`RedirectDialogFragment`](../-redirect-dialog-fragment/index.md)<br>A builder method for creating a [SimpleRedirectDialogFragment](./index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [consumeFrom](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md) | `fun <S : `[`State`](../../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../../mozilla.components.lib.state/-action.md)`> Fragment.consumeFrom(store: `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`S`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#A)`>, block: (`[`S`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#S)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Helper extension method for consuming [State](../../mozilla.components.lib.state/-state.md) from a [Store](../../mozilla.components.lib.state/-store/index.md) sequentially in order inside a [Fragment](#). The [block](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#mozilla.components.lib.state.ext$consumeFrom(androidx.fragment.app.Fragment, mozilla.components.lib.state.Store((mozilla.components.lib.state.ext.consumeFrom.S, mozilla.components.lib.state.ext.consumeFrom.A)), kotlin.Function1((mozilla.components.lib.state.ext.consumeFrom.S, kotlin.Unit)))/block) function will get invoked for every [State](../../mozilla.components.lib.state/-state.md) update. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
