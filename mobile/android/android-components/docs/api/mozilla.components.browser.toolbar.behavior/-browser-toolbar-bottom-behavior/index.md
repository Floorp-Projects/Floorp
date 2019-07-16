[android-components](../../index.md) / [mozilla.components.browser.toolbar.behavior](../index.md) / [BrowserToolbarBottomBehavior](./index.md)

# BrowserToolbarBottomBehavior

`class BrowserToolbarBottomBehavior : Behavior<`[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/behavior/BrowserToolbarBottomBehavior.kt#L33)

A [CoordinatorLayout.Behavior](#) implementation to be used when placing [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) at the bottom of the screen.

This implementation will:

* Show/Hide the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) automatically when scrolling vertically.
* On showing a [Snackbar](#) position it above the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md).
* Snap the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) to be hidden or visible when the user stops scrolling.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserToolbarBottomBehavior(context: <ERROR CLASS>?, attrs: <ERROR CLASS>?)`<br>A [CoordinatorLayout.Behavior](#) implementation to be used when placing [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) at the bottom of the screen. |

### Functions

| Name | Summary |
|---|---|
| [layoutDependsOn](layout-depends-on.md) | `fun layoutDependsOn(parent: CoordinatorLayout, child: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, dependency: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onNestedPreScroll](on-nested-pre-scroll.md) | `fun onNestedPreScroll(coordinatorLayout: CoordinatorLayout, child: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, target: <ERROR CLASS>, dx: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, dy: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, consumed: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`, type: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStartNestedScroll](on-start-nested-scroll.md) | `fun onStartNestedScroll(coordinatorLayout: CoordinatorLayout, child: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, directTargetChild: <ERROR CLASS>, target: <ERROR CLASS>, axes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, type: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onStopNestedScroll](on-stop-nested-scroll.md) | `fun onStopNestedScroll(coordinatorLayout: CoordinatorLayout, child: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, target: <ERROR CLASS>, type: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
