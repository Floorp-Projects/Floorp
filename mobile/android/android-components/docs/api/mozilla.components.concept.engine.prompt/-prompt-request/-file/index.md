[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [File](./index.md)

# File

`data class File : `[`PromptRequest`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L107)

Value type that represents a request for a selecting one or multiple files.

### Types

| Name | Summary |
|---|---|
| [FacingMode](-facing-mode/index.md) | `enum class FacingMode` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `File(mimeTypes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<out `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, isMultipleFilesSelection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, onSingleFileSelected: (<ERROR CLASS>, <ERROR CLASS>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onMultipleFilesSelected: (<ERROR CLASS>, `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<<ERROR CLASS>>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)``File(mimeTypes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<out `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, isMultipleFilesSelection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, captureMode: `[`FacingMode`](-facing-mode/index.md)` = FacingMode.NONE, onSingleFileSelected: (<ERROR CLASS>, <ERROR CLASS>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onMultipleFilesSelected: (<ERROR CLASS>, `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<<ERROR CLASS>>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request for a selecting one or multiple files. |

### Properties

| Name | Summary |
|---|---|
| [captureMode](capture-mode.md) | `val captureMode: `[`FacingMode`](-facing-mode/index.md)<br>indicates if the local media capturing capabilities should be used, such as the camera or microphone. |
| [isMultipleFilesSelection](is-multiple-files-selection.md) | `val isMultipleFilesSelection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>true if the user can select more that one file false otherwise. |
| [mimeTypes](mime-types.md) | `val mimeTypes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<out `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>a set of allowed mime types. Only these file types can be selected. |
| [onDismiss](on-dismiss.md) | `val onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user has canceled the file selection. |
| [onMultipleFilesSelected](on-multiple-files-selected.md) | `val onMultipleFilesSelected: (<ERROR CLASS>, `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<<ERROR CLASS>>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user has selected multiple files. |
| [onSingleFileSelected](on-single-file-selected.md) | `val onSingleFileSelected: (<ERROR CLASS>, <ERROR CLASS>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user has selected a single file. |
