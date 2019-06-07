[android-components](../../index.md) / [mozilla.components.ui.doorhanger](../index.md) / [DoorhangerPrompt](./index.md)

# DoorhangerPrompt

`class ~~DoorhangerPrompt~~` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/doorhanger/src/main/java/mozilla/components/ui/doorhanger/DoorhangerPrompt.kt#L34)
**Deprecated:** The ui-doorhanger component is getting removed.

Builder for creating a prompt [Doorhanger](../-doorhanger/index.md) providing a way to present decisions to users.

### Parameters

`title` - Title text for this prompt.

`icon` - (Optional) icon to be displayed next to the title.

`controlGroups` - A list of control groups to be displayed in the prompt.

`buttons` - A list of buttons to be displayed in the prompt.

`onDismiss` - that is called when the doorhanger is dismissed.

### Types

| Name | Summary |
|---|---|
| [Button](-button/index.md) | `data class Button` |
| [Control](-control/index.md) | `sealed class Control` |
| [ControlGroup](-control-group/index.md) | `data class ControlGroup`<br>A group of controls to be displayed in a [DoorhangerPrompt](./index.md). |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DoorhangerPrompt(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, icon: `[`Drawable`](https://developer.android.com/reference/android/graphics/drawable/Drawable.html)`? = null, controlGroups: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ControlGroup`](-control-group/index.md)`> = listOf(), buttons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Button`](-button/index.md)`> = listOf(), onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null)`<br>Builder for creating a prompt [Doorhanger](../-doorhanger/index.md) providing a way to present decisions to users. |

### Properties

| Name | Summary |
|---|---|
| [buttons](buttons.md) | `val buttons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Button`](-button/index.md)`>`<br>A list of buttons to be displayed in the prompt. |
| [controlGroups](control-groups.md) | `val controlGroups: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ControlGroup`](-control-group/index.md)`>`<br>A list of control groups to be displayed in the prompt. |

### Functions

| Name | Summary |
|---|---|
| [createDoorhanger](create-doorhanger.md) | `fun createDoorhanger(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`Doorhanger`](../-doorhanger/index.md)<br>Creates a [Doorhanger](../-doorhanger/index.md) from the [DoorhangerPrompt](./index.md) configuration. |
