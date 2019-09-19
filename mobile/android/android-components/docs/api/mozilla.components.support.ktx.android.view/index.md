[android-components](../index.md) / [mozilla.components.support.ktx.android.view](./index.md)

## Package mozilla.components.support.ktx.android.view

### Properties

| Name | Summary |
|---|---|
| [isLTR](is-l-t-r.md) | `val <ERROR CLASS>.isLTR: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Left to Right? |
| [isRTL](is-r-t-l.md) | `val <ERROR CLASS>.isRTL: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Right to Left? |

### Functions

| Name | Summary |
|---|---|
| [enterToImmersiveMode](enter-to-immersive-mode.md) | `fun <ERROR CLASS>.enterToImmersiveMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Attempts to call immersive mode using the View to hide the status bar and navigation buttons. |
| [exitImmersiveModeIfNeeded](exit-immersive-mode-if-needed.md) | `fun <ERROR CLASS>.exitImmersiveModeIfNeeded(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Attempts to come out from immersive mode using the View. |
| [getRectWithViewLocation](get-rect-with-view-location.md) | `fun <ERROR CLASS>.getRectWithViewLocation(): <ERROR CLASS>`<br>Fills the given [Rect](#) with data about location view in the window. |
| [hideKeyboard](hide-keyboard.md) | `fun <ERROR CLASS>.hideKeyboard(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Hides the soft input window. |
| [onNextGlobalLayout](on-next-global-layout.md) | `fun <ERROR CLASS>.onNextGlobalLayout(callback: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a one-time callback to be invoked when the global layout state or the visibility of views within the view tree changes. |
| [putCompoundDrawablesRelative](put-compound-drawables-relative.md) | `fun <ERROR CLASS>.putCompoundDrawablesRelative(start: <ERROR CLASS>? = null, top: <ERROR CLASS>? = null, end: <ERROR CLASS>? = null, bottom: <ERROR CLASS>? = null): <ERROR CLASS>`<br>Sets the [Drawable](#)s (if any) to appear to the start of, above, to the end of, and below the text. Use `null` if you do not want a Drawable there. The Drawables must already have had [Drawable.setBounds](#) called. |
| [putCompoundDrawablesRelativeWithIntrinsicBounds](put-compound-drawables-relative-with-intrinsic-bounds.md) | `fun <ERROR CLASS>.putCompoundDrawablesRelativeWithIntrinsicBounds(start: <ERROR CLASS>? = null, top: <ERROR CLASS>? = null, end: <ERROR CLASS>? = null, bottom: <ERROR CLASS>? = null): <ERROR CLASS>`<br>Sets the [Drawable](#)s (if any) to appear to the start of, above, to the end of, and below the text. Use `null` if you do not want a Drawable there. The Drawables' bounds will be set to their intrinsic bounds. |
| [setNavigationBarTheme](set-navigation-bar-theme.md) | `fun <ERROR CLASS>.setNavigationBarTheme(color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Colors the navigation bar. If the color is light enough, a light navigation bar with dark icons will be used. |
| [setPadding](set-padding.md) | `fun <ERROR CLASS>.setPadding(padding: `[`Padding`](../mozilla.components.support.base.android/-padding/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set a padding using [Padding](../mozilla.components.support.base.android/-padding/index.md) object. |
| [setStatusBarTheme](set-status-bar-theme.md) | `fun <ERROR CLASS>.setStatusBarTheme(color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Colors the status bar. If the color is light enough, a light status bar with dark icons will be used. |
| [showKeyboard](show-keyboard.md) | `fun <ERROR CLASS>.showKeyboard(flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = InputMethodManager.SHOW_IMPLICIT): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tries to focus this view and show the soft input window for it. |
| [toScope](to-scope.md) | `fun <ERROR CLASS>.toScope(): CoroutineScope`<br>Creates a [CoroutineScope](#) that is active as long as this [View](#) is attached. Once this [View](#) gets detached this [CoroutineScope](#) gets cancelled automatically. |
| [use](use.md) | `fun <R> <ERROR CLASS>.use(functionBlock: (<ERROR CLASS>) -> `[`R`](use.md#R)`): `[`R`](use.md#R)<br>Executes the given [functionBlock](use.md#mozilla.components.support.ktx.android.view$use(, kotlin.Function1((, mozilla.components.support.ktx.android.view.use.R)))/functionBlock) function on this resource and then closes it down correctly whether an exception is thrown or not. This is inspired by [java.lang.AutoCloseable.use](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/use.html). |
