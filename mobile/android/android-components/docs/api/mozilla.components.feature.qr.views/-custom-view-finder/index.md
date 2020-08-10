[android-components](../../index.md) / [mozilla.components.feature.qr.views](../index.md) / [CustomViewFinder](./index.md)

# CustomViewFinder

`class CustomViewFinder : AppCompatImageView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/qr/src/main/java/mozilla/components/feature/qr/views/CustomViewFinder.kt#L37)

A [View](#) that shows a ViewFinder positioned in center of the camera view and draws an Overlay

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomViewFinder(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null)`<br>A [View](#) that shows a ViewFinder positioned in center of the camera view and draws an Overlay |

### Functions

| Name | Summary |
|---|---|
| [onConfigurationChanged](on-configuration-changed.md) | `fun onConfigurationChanged(newConfig: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDraw](on-draw.md) | `fun onDraw(canvas: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLayout](on-layout.md) | `fun onLayout(changed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, left: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, top: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, right: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, bottom: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSizeChanged](on-size-changed.md) | `fun onSizeChanged(w: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, h: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, oldw: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, oldh: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setViewFinderColor](set-view-finder-color.md) | `fun setViewFinderColor(color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the stroke color for the ViewFinder. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [setMessage](set-message.md) | `fun setMessage(scanMessageStringRes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the message to be displayed below ViewFinder. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
