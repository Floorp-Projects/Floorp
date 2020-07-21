[android-components](../../index.md) / [mozilla.components.feature.readerview.view](../index.md) / [ReaderViewControlsView](./index.md)

# ReaderViewControlsView

`interface ReaderViewControlsView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/readerview/src/main/java/mozilla/components/feature/readerview/view/ReaderViewControlsView.kt#L14)

An interface for views that can display ReaderView appearance controls (e.g. font size, font type).

### Types

| Name | Summary |
|---|---|
| [Listener](-listener/index.md) | `interface Listener` |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `abstract var listener: `[`Listener`](-listener/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this [ReaderViewControlsView](./index.md) interface to an actual Android [View](#) object. |
| [hideControls](hide-controls.md) | `abstract fun hideControls(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes the UI controls invisible. |
| [setColorScheme](set-color-scheme.md) | `abstract fun setColorScheme(scheme: `[`ColorScheme`](../../mozilla.components.feature.readerview/-reader-view-feature/-color-scheme/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the selected color scheme. |
| [setFont](set-font.md) | `abstract fun setFont(font: `[`FontType`](../../mozilla.components.feature.readerview/-reader-view-feature/-font-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the selected font option. |
| [setFontSize](set-font-size.md) | `abstract fun setFontSize(size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the selected font size. |
| [showControls](show-controls.md) | `abstract fun showControls(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Makes the UI controls visible and requests focus. |
| [tryInflate](try-inflate.md) | `abstract fun tryInflate(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tries to inflate the view if needed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [ReaderViewControlsBar](../-reader-view-controls-bar/index.md) | `class ReaderViewControlsBar : ConstraintLayout, `[`ReaderViewControlsView`](./index.md)<br>A customizable ReaderView control bar implementing [ReaderViewControlsView](./index.md). |
