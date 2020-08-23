[android-components](../../index.md) / [mozilla.components.feature.prompts.login](../index.md) / [LoginPickerView](./index.md)

# LoginPickerView

`interface LoginPickerView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/login/LoginPickerView.kt#L13)

An interface for views that can display multiple fillable logins for a site.

### Types

| Name | Summary |
|---|---|
| [Listener](-listener/index.md) | `interface Listener`<br>Interface to allow a class to listen to login selection prompt events. |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `abstract var listener: `[`Listener`](-listener/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this [LoginPickerView](./index.md) interface to an actual Android [View](#) object. |
| [hidePicker](hide-picker.md) | `abstract fun hidePicker(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes the UI controls invisible. |
| [showPicker](show-picker.md) | `abstract fun showPicker(list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes the UI controls visible. |
| [tryInflate](try-inflate.md) | `abstract fun tryInflate(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tries to inflate the view if needed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [LoginSelectBar](../-login-select-bar/index.md) | `class LoginSelectBar : ConstraintLayout, `[`LoginPickerView`](./index.md)<br>A customizable multiple login selection bar implementing [LoginPickerView](./index.md). |
