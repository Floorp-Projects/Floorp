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

### Inheritors

| Name | Summary |
|---|---|
| [ReaderViewControlsBar](../-reader-view-controls-bar/index.md) | `class ReaderViewControlsBar : ConstraintLayout, `[`ReaderViewControlsView`](./index.md)<br>A customizable ReaderView control bar implementing [ReaderViewControlsView](./index.md). |
