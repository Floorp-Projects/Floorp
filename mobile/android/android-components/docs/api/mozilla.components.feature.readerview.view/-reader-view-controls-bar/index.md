[android-components](../../index.md) / [mozilla.components.feature.readerview.view](../index.md) / [ReaderViewControlsBar](./index.md)

# ReaderViewControlsBar

`class ReaderViewControlsBar : ConstraintLayout, `[`ReaderViewControlsView`](../-reader-view-controls-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/readerview/src/main/java/mozilla/components/feature/readerview/view/ReaderViewControlsBar.kt#L25)

A customizable ReaderView control bar implementing [ReaderViewControlsView](../-reader-view-controls-view/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ReaderViewControlsBar(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable ReaderView control bar implementing [ReaderViewControlsView](../-reader-view-controls-view/index.md). |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `var listener: `[`Listener`](../-reader-view-controls-view/-listener/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [hideControls](hide-controls.md) | `fun hideControls(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates visibility to [View.GONE](#) of the UI controls. |
| [onFocusChanged](on-focus-changed.md) | `fun onFocusChanged(gainFocus: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, direction: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, previouslyFocusedRect: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setColorScheme](set-color-scheme.md) | `fun setColorScheme(scheme: `[`ColorScheme`](../../mozilla.components.feature.readerview/-reader-view-feature/-color-scheme/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the color scheme of the current and future ReaderView sessions. |
| [setFont](set-font.md) | `fun setFont(font: `[`FontType`](../../mozilla.components.feature.readerview/-reader-view-feature/-font-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the font type of the current and future ReaderView sessions. |
| [setFontSize](set-font-size.md) | `fun setFontSize(size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the font size of the current and future ReaderView sessions. |
| [showControls](show-controls.md) | `fun showControls(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates visibility to [View.VISIBLE](#) and requests focus for the UI controls. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../-reader-view-controls-view/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this [ReaderViewControlsView](../-reader-view-controls-view/index.md) interface to an actual Android [View](#) object. |
