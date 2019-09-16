[android-components](../../index.md) / [mozilla.components.feature.contextmenu](../index.md) / [ContextMenuFragment](./index.md)

# ContextMenuFragment

`class ContextMenuFragment : DialogFragment` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuFragment.kt#L31)

[DialogFragment](#) implementation to display the actual context menu dialog.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContextMenuFragment()`<br>[DialogFragment](#) implementation to display the actual context menu dialog. |

### Functions

| Name | Summary |
|---|---|
| [onCancel](on-cancel.md) | `fun onCancel(dialog: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreateDialog](on-create-dialog.md) | `fun onCreateDialog(savedInstanceState: <ERROR CLASS>?): <ERROR CLASS>` |

### Companion Object Functions

| Name | Summary |
|---|---|
| [create](create.md) | `fun create(tab: `[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, ids: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, labels: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`ContextMenuFragment`](./index.md)<br>Create a new [ContextMenuFragment](./index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [consumeFrom](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md) | `fun <S : `[`State`](../../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../../mozilla.components.lib.state/-action.md)`> Fragment.consumeFrom(store: `[`Store`](../../mozilla.components.lib.state/-store/index.md)`<`[`S`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#S)`, `[`A`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#A)`>, block: (`[`S`](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#S)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Helper extension method for consuming [State](../../mozilla.components.lib.state/-state.md) from a [Store](../../mozilla.components.lib.state/-store/index.md) sequentially in order inside a [Fragment](#). The [block](../../mozilla.components.lib.state.ext/androidx.fragment.app.-fragment/consume-from.md#mozilla.components.lib.state.ext$consumeFrom(androidx.fragment.app.Fragment, mozilla.components.lib.state.Store((mozilla.components.lib.state.ext.consumeFrom.S, mozilla.components.lib.state.ext.consumeFrom.A)), kotlin.Function1((mozilla.components.lib.state.ext.consumeFrom.S, kotlin.Unit)))/block) function will get invoked for every [State](../../mozilla.components.lib.state/-state.md) update. |
