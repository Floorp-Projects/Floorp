[android-components](../../index.md) / [mozilla.components.browser.toolbar.behavior](../index.md) / [BrowserToolbarBottomBehavior](./index.md)

# BrowserToolbarBottomBehavior

`class BrowserToolbarBottomBehavior : Behavior<`[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/behavior/BrowserToolbarBottomBehavior.kt#L37)

A [CoordinatorLayout.Behavior](#) implementation to be used when placing [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) at the bottom of the screen.

This implementation will:

* Show/Hide the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) automatically when scrolling vertically.
* On showing a [Snackbar](#) position it above the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md).
* Snap the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) to be hidden or visible when the user stops scrolling.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserToolbarBottomBehavior(context: <ERROR CLASS>?, attrs: <ERROR CLASS>?)`<br>A [CoordinatorLayout.Behavior](#) implementation to be used when placing [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) at the bottom of the screen. |

### Properties

| Name | Summary |
|---|---|
| [context](context.md) | `val context: <ERROR CLASS>?` |

### Functions

| Name | Summary |
|---|---|
| [forceExpand](force-expand.md) | `fun forceExpand(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Used to expand the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) upwards |
| [layoutDependsOn](layout-depends-on.md) | `fun layoutDependsOn(parent: CoordinatorLayout, child: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, dependency: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onInterceptTouchEvent](on-intercept-touch-event.md) | `fun onInterceptTouchEvent(parent: CoordinatorLayout, child: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, ev: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onStartNestedScroll](on-start-nested-scroll.md) | `fun onStartNestedScroll(coordinatorLayout: CoordinatorLayout, child: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, directTargetChild: <ERROR CLASS>, target: <ERROR CLASS>, axes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, type: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onStopNestedScroll](on-stop-nested-scroll.md) | `fun onStopNestedScroll(coordinatorLayout: CoordinatorLayout, child: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, target: <ERROR CLASS>, type: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
