[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngineView](./index.md)

# GeckoEngineView

`class GeckoEngineView : `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngineView.kt#L21)

Gecko-based EngineView implementation.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoEngineView(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>Gecko-based EngineView implementation. |

### Functions

| Name | Summary |
|---|---|
| [canScrollVerticallyDown](can-scroll-vertically-down.md) | `fun canScrollVerticallyDown(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Check if [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) can be scrolled vertically down. true if can and false otherwise. |
| [canScrollVerticallyUp](can-scroll-vertically-up.md) | `fun canScrollVerticallyUp(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Check if [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) can be scrolled vertically up. true if can and false otherwise. |
| [captureThumbnail](capture-thumbnail.md) | `fun captureThumbnail(onFinish: (<ERROR CLASS>?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDetachedFromWindow](on-detached-from-window.md) | `fun onDetachedFromWindow(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [release](release.md) | `fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Releases an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) that is currently rendered by this view (after calling [render](../../mozilla.components.concept.engine/-engine-view/render.md)). |
| [render](render.md) | `fun render(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Render the content of the given session. |
| [setVerticalClipping](set-vertical-clipping.md) | `fun setVerticalClipping(clippingHeight: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the amount of vertical space that is clipped or visibly obscured in the bottom portion of the view. Tells the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) where to put bottom fixed elements so they are fully visible. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../../mozilla.components.concept.engine/-engine-view/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Convenience method to cast the implementation of this interface to an Android View object. |
| [captureThumbnail](../../mozilla.components.concept.engine/-engine-view/capture-thumbnail.md) | `abstract fun captureThumbnail(onFinish: (<ERROR CLASS>?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Request a screenshot of the visible portion of the web page currently being rendered. |
| [onCreate](../../mozilla.components.concept.engine/-engine-view/on-create.md) | `open fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_CREATE](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onDestroy](../../mozilla.components.concept.engine/-engine-view/on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_DESTROY](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onPause](../../mozilla.components.concept.engine/-engine-view/on-pause.md) | `open fun onPause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_PAUSE](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onResume](../../mozilla.components.concept.engine/-engine-view/on-resume.md) | `open fun onResume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_RESUME](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onStart](../../mozilla.components.concept.engine/-engine-view/on-start.md) | `open fun onStart(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_START](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
| [onStop](../../mozilla.components.concept.engine/-engine-view/on-stop.md) | `open fun onStop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_STOP](#). See [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) implementations for details. |
