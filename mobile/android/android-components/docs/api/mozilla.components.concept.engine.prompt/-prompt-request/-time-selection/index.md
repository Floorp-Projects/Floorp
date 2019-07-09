[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [TimeSelection](./index.md)

# TimeSelection

`class TimeSelection : `[`PromptRequest`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L83)

Value type that represents a request for a date prompt for picking a year, month, and day.

### Types

| Name | Summary |
|---|---|
| [Type](-type/index.md) | `enum class Type` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TimeSelection(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, initialDate: `[`Date`](https://developer.android.com/reference/java/util/Date.html)`, minimumDate: `[`Date`](https://developer.android.com/reference/java/util/Date.html)`?, maximumDate: `[`Date`](https://developer.android.com/reference/java/util/Date.html)`?, type: `[`Type`](-type/index.md)` = Type.DATE, onConfirm: (`[`Date`](https://developer.android.com/reference/java/util/Date.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onClear: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request for a date prompt for picking a year, month, and day. |

### Properties

| Name | Summary |
|---|---|
| [initialDate](initial-date.md) | `val initialDate: `[`Date`](https://developer.android.com/reference/java/util/Date.html)<br>date that dialog should be set by default. |
| [maximumDate](maximum-date.md) | `val maximumDate: `[`Date`](https://developer.android.com/reference/java/util/Date.html)`?`<br>date allow to be selected. |
| [minimumDate](minimum-date.md) | `val minimumDate: `[`Date`](https://developer.android.com/reference/java/util/Date.html)`?`<br>date allow to be selected. |
| [onClear](on-clear.md) | `val onClear: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback that is called when the user requests the picker to be clear up. |
| [onConfirm](on-confirm.md) | `val onConfirm: (`[`Date`](https://developer.android.com/reference/java/util/Date.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback that is called when the date is selected. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>of the dialog. |
| [type](type.md) | `val type: `[`Type`](-type/index.md)<br>indicate which [Type](-type/index.md) of selection de user wants. |
