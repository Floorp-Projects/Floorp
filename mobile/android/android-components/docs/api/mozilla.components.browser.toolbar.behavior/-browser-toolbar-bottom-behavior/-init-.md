[android-components](../../index.md) / [mozilla.components.browser.toolbar.behavior](../index.md) / [BrowserToolbarBottomBehavior](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserToolbarBottomBehavior(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`?, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`?)`

A [CoordinatorLayout.Behavior](#) implementation to be used when placing [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) at the bottom of the screen.

This implementation will:

* Show/Hide the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) automatically when scrolling vertically.
* On showing a [Snackbar](#) position it above the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md).
* Snap the [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) to be hidden or visible when the user stops scrolling.
