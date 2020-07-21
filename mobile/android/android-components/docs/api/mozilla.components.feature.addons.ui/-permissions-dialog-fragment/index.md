[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [PermissionsDialogFragment](./index.md)

# PermissionsDialogFragment

`class PermissionsDialogFragment : AppCompatDialogFragment` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/PermissionsDialogFragment.kt#L40)

A dialog that shows a set of permission required by an [Addon](../../mozilla.components.feature.addons/-addon/index.md).

### Types

| Name | Summary |
|---|---|
| [PromptsStyling](-prompts-styling/index.md) | `data class PromptsStyling`<br>Styling for the permissions dialog. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PermissionsDialogFragment()`<br>A dialog that shows a set of permission required by an [Addon](../../mozilla.components.feature.addons/-addon/index.md). |

### Properties

| Name | Summary |
|---|---|
| [onNegativeButtonClicked](on-negative-button-clicked.md) | `var onNegativeButtonClicked: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A lambda called when the deny button is clicked. |
| [onPositiveButtonClicked](on-positive-button-clicked.md) | `var onPositiveButtonClicked: (`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A lambda called when the allow button is clicked. |

### Functions

| Name | Summary |
|---|---|
| [onCreateDialog](on-create-dialog.md) | `fun onCreateDialog(savedInstanceState: <ERROR CLASS>?): <ERROR CLASS>` |
| [onDismiss](on-dismiss.md) | `fun onDismiss(dialog: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [newInstance](new-instance.md) | `fun newInstance(addon: `[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`, promptsStyling: `[`PromptsStyling`](-prompts-styling/index.md)`? = PromptsStyling(
                gravity = Gravity.BOTTOM,
                shouldWidthMatchParent = true
            ), onPositiveButtonClicked: (`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onNegativeButtonClicked: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`PermissionsDialogFragment`](./index.md)<br>Returns a new instance of [PermissionsDialogFragment](./index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [consumeFrom](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md) | `fun <S : `[`State`](../../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../../mozilla.components.lib.state/-action.md)`> Fragment.consumeFrom(store: `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`S`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#A)`>, block: (`[`S`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#S)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Helper extension method for consuming [State](../../mozilla.components.lib.state/-state.md) from a [Store](../../mozilla.components.lib.state/-store/index.md) sequentially in order inside a [Fragment](#). The [block](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#mozilla.components.lib.state.ext$consumeFrom(androidx.fragment.app.Fragment, mozilla.components.lib.state.Store((mozilla.components.lib.state.ext.consumeFrom.S, mozilla.components.lib.state.ext.consumeFrom.A)), kotlin.Function1((mozilla.components.lib.state.ext.consumeFrom.S, kotlin.Unit)))/block) function will get invoked for every [State](../../mozilla.components.lib.state/-state.md) update. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
