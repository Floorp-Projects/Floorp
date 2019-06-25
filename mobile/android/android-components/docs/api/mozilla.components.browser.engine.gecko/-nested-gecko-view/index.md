[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [NestedGeckoView](./index.md)

# NestedGeckoView

`open class NestedGeckoView : `[`GeckoView`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoView.html)`, NestedScrollingChild` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/NestedGeckoView.kt#L27)

geckoView that supports nested scrolls (for using in a CoordinatorLayout).

This code is a simplified version of the NestedScrollView implementation
which can be found in the support library:
[android.support.v4.widget.NestedScrollView](#)

Based on:
https://github.com/takahirom/webview-in-coordinatorlayout

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `NestedGeckoView(context: <ERROR CLASS>)`<br>geckoView that supports nested scrolls (for using in a CoordinatorLayout). |

### Functions

| Name | Summary |
|---|---|
| [dispatchNestedFling](dispatch-nested-fling.md) | `open fun dispatchNestedFling(velocityX: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`, velocityY: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`, consumed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [dispatchNestedPreFling](dispatch-nested-pre-fling.md) | `open fun dispatchNestedPreFling(velocityX: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`, velocityY: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [dispatchNestedPreScroll](dispatch-nested-pre-scroll.md) | `open fun dispatchNestedPreScroll(dx: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, dy: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, consumed: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?, offsetInWindow: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [dispatchNestedScroll](dispatch-nested-scroll.md) | `open fun dispatchNestedScroll(dxConsumed: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, dyConsumed: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, dxUnconsumed: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, dyUnconsumed: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, offsetInWindow: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hasNestedScrollingParent](has-nested-scrolling-parent.md) | `open fun hasNestedScrollingParent(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [isNestedScrollingEnabled](is-nested-scrolling-enabled.md) | `open fun isNestedScrollingEnabled(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onTouchEvent](on-touch-event.md) | `open fun onTouchEvent(ev: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [setNestedScrollingEnabled](set-nested-scrolling-enabled.md) | `open fun setNestedScrollingEnabled(enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [startNestedScroll](start-nested-scroll.md) | `open fun startNestedScroll(axes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [stopNestedScroll](stop-nested-scroll.md) | `open fun stopNestedScroll(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
