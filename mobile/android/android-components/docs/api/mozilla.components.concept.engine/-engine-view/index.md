[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineView](./index.md)

# EngineView

`interface EngineView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineView.kt#L16)

View component that renders web content.

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `open fun asView(): `[`View`](https://developer.android.com/reference/android/view/View.html)<br>Convenience method to cast the implementation of this interface to an Android View object. |
| [canScrollVerticallyDown](can-scroll-vertically-down.md) | `open fun canScrollVerticallyDown(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Check if [EngineView](./index.md) can be scrolled vertically down. true if can and false otherwise. |
| [captureThumbnail](capture-thumbnail.md) | `abstract fun captureThumbnail(onFinish: (`[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Request a screenshot of the visible portion of the web page currently being rendered. |
| [onCreate](on-create.md) | `open fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_CREATE](#). See [EngineView](./index.md) implementations for details. |
| [onDestroy](on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_DESTROY](#). See [EngineView](./index.md) implementations for details. |
| [onPause](on-pause.md) | `open fun onPause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_PAUSE](#). See [EngineView](./index.md) implementations for details. |
| [onResume](on-resume.md) | `open fun onResume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_RESUME](#). See [EngineView](./index.md) implementations for details. |
| [onStart](on-start.md) | `open fun onStart(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_START](#). See [EngineView](./index.md) implementations for details. |
| [onStop](on-stop.md) | `open fun onStop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>To be called in response to [Lifecycle.Event.ON_STOP](#). See [EngineView](./index.md) implementations for details. |
| [render](render.md) | `abstract fun render(session: `[`EngineSession`](../-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Render the content of the given session. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoEngineView](../../mozilla.components.browser.engine.gecko/-gecko-engine-view/index.md) | `class GeckoEngineView : `[`FrameLayout`](https://developer.android.com/reference/android/widget/FrameLayout.html)`, `[`EngineView`](./index.md)<br>Gecko-based EngineView implementation. |
| [ServoEngineView](../../mozilla.components.browser.engine.servo/-servo-engine-view/index.md) | `class ServoEngineView : `[`FrameLayout`](https://developer.android.com/reference/android/widget/FrameLayout.html)`, `[`EngineView`](./index.md)<br>Servo-based implementation of EngineView. |
| [SystemEngineView](../../mozilla.components.browser.engine.system/-system-engine-view/index.md) | `class SystemEngineView : `[`FrameLayout`](https://developer.android.com/reference/android/widget/FrameLayout.html)`, `[`EngineView`](./index.md)`, `[`OnLongClickListener`](https://developer.android.com/reference/android/view/View/OnLongClickListener.html)<br>WebView-based implementation of EngineView. |
