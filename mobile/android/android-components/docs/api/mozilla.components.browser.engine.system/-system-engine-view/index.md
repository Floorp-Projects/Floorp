[android-components](../../index.md) / [mozilla.components.browser.engine.system](../index.md) / [SystemEngineView](./index.md)

# SystemEngineView

`class SystemEngineView : `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/SystemEngineView.kt#L73)

WebView-based implementation of EngineView.

### Types

| Name | Summary |
|---|---|
| [ImageHandler](-image-handler/index.md) | `class ImageHandler` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SystemEngineView(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>WebView-based implementation of EngineView. |

### Properties

| Name | Summary |
|---|---|
| [selectionActionDelegate](selection-action-delegate.md) | `var selectionActionDelegate: `[`SelectionActionDelegate`](../../mozilla.components.concept.engine.selection/-selection-action-delegate/index.md)`?`<br>A delegate that will handle interactions with text selection context menus. |

### Functions

| Name | Summary |
|---|---|
| [canScrollVerticallyDown](can-scroll-vertically-down.md) | `fun canScrollVerticallyDown(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Check if [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) can be scrolled vertically down. true if can and false otherwise. |
| [canScrollVerticallyUp](can-scroll-vertically-up.md) | `fun canScrollVerticallyUp(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Check if [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) can be scrolled vertically up. true if can and false otherwise. |
| [captureThumbnail](capture-thumbnail.md) | `fun captureThumbnail(onFinish: (<ERROR CLASS>?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [clearSelection](clear-selection.md) | `fun clearSelection(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the current selection if possible. |
| [getInputResult](get-input-result.md) | `fun getInputResult(): `[`InputResult`](../../mozilla.components.concept.engine/-engine-view/-input-result/index.md) |
| [onDestroy](on-destroy.md) | `fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_DESTROY](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onLongClick](on-long-click.md) | `fun onLongClick(view: <ERROR CLASS>?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onPause](on-pause.md) | `fun onPause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_PAUSE](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onResume](on-resume.md) | `fun onResume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_RESUME](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [release](release.md) | `fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Releases an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) that is currently rendered by this view (after calling [render](../../mozilla.components.concept.engine/-engine-view/render.md)). |
| [render](render.md) | `fun render(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Render the content of the given session. |
| [setDynamicToolbarMaxHeight](set-dynamic-toolbar-max-height.md) | `fun setDynamicToolbarMaxHeight(height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the maximum height of the dynamic toolbar(s). |
| [setVerticalClipping](set-vertical-clipping.md) | `fun setVerticalClipping(clippingHeight: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the amount of vertical space that is clipped or visibly obscured in the bottom portion of the view. Tells the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) where to put bottom fixed elements so they are fully visible. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../../mozilla.components.concept.engine/-engine-view/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Convenience method to cast the implementation of this interface to an Android View object. |
| [canClearSelection](../../mozilla.components.concept.engine/-engine-view/can-clear-selection.md) | `open fun canClearSelection(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Check if [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) can clear the selection. true if can and false otherwise. |
| [captureThumbnail](../../mozilla.components.concept.engine/-engine-view/capture-thumbnail.md) | `abstract fun captureThumbnail(onFinish: (<ERROR CLASS>?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Request a screenshot of the visible portion of the web page currently being rendered. |
| [onCreate](../../mozilla.components.concept.engine/-engine-view/on-create.md) | `open fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_CREATE](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onStart](../../mozilla.components.concept.engine/-engine-view/on-start.md) | `open fun onStart(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_START](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onStop](../../mozilla.components.concept.engine/-engine-view/on-stop.md) | `open fun onStop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_STOP](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
