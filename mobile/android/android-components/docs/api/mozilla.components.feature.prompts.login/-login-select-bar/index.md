[android-components](../../index.md) / [mozilla.components.feature.prompts.login](../index.md) / [LoginSelectBar](./index.md)

# LoginSelectBar

`class LoginSelectBar : ConstraintLayout, `[`LoginPickerView`](../-login-picker-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/login/LoginSelectBar.kt#L27)

A customizable multiple login selection bar implementing [LoginPickerView](../-login-picker-view/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LoginSelectBar(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable multiple login selection bar implementing [LoginPickerView](../-login-picker-view/index.md). |

### Properties

| Name | Summary |
|---|---|
| [headerTextStyle](header-text-style.md) | `var headerTextStyle: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?` |
| [listener](listener.md) | `var listener: `[`Listener`](../-login-picker-view/-listener/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `fun asView(): <ERROR CLASS>`<br>Casts this [LoginPickerView](../-login-picker-view/index.md) interface to an actual Android [View](#) object. |
| [hidePicker](hide-picker.md) | `fun hidePicker(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes the UI controls invisible. |
| [showPicker](show-picker.md) | `fun showPicker(list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes the UI controls visible. |
| [tryInflate](try-inflate.md) | `fun tryInflate(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tries to inflate the view if needed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
