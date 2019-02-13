[android-components](../../index.md) / [mozilla.components.ui.doorhanger](../index.md) / [DoorhangerPrompt](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`DoorhangerPrompt(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, icon: `[`Drawable`](https://developer.android.com/reference/android/graphics/drawable/Drawable.html)`? = null, controlGroups: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ControlGroup`](-control-group/index.md)`> = listOf(), buttons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Button`](-button/index.md)`> = listOf(), onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null)`

Builder for creating a prompt [Doorhanger](../-doorhanger/index.md) providing a way to present decisions to users.

### Parameters

`title` - Title text for this prompt.

`icon` - (Optional) icon to be displayed next to the title.

`controlGroups` - A list of control groups to be displayed in the prompt.

`buttons` - A list of buttons to be displayed in the prompt.

`onDismiss` - that is called when the doorhanger is dismissed.