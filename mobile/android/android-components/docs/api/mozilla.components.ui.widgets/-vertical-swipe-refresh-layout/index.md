[android-components](../../index.md) / [mozilla.components.ui.widgets](../index.md) / [VerticalSwipeRefreshLayout](./index.md)

# VerticalSwipeRefreshLayout

`class VerticalSwipeRefreshLayout : SwipeRefreshLayout` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/widgets/src/main/java/mozilla/components/ui/widgets/VerticalSwipeRefreshLayout.kt#L28)

[SwipeRefreshLayout](#) that filters only vertical scrolls for triggering pull to refresh.

Following situations will not trigger pull to refresh:

* a scroll happening more on the horizontal axis
* a scale in/out gesture
* a quick scale gesture

To control responding to scrolls and showing the pull to refresh throbber or not
use the [View.isEnabled](#) property.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `VerticalSwipeRefreshLayout(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null)`<br>[SwipeRefreshLayout](#) that filters only vertical scrolls for triggering pull to refresh. |

### Functions

| Name | Summary |
|---|---|
| [onInterceptTouchEvent](on-intercept-touch-event.md) | `fun onInterceptTouchEvent(event: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onStartNestedScroll](on-start-nested-scroll.md) | `fun onStartNestedScroll(child: <ERROR CLASS>, target: <ERROR CLASS>, nestedScrollAxes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
