[android-components](../../index.md) / [mozilla.components.feature.session.behavior](../index.md) / [EngineViewBottomBehavior](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`EngineViewBottomBehavior(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`?, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`?)`

A [CoordinatorLayout.Behavior](#) implementation to be used with [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) when placing a toolbar at the
bottom of the screen.

Using this behavior requires the toolbar to use the BrowserToolbarBottomBehavior.

This implementation will update the vertical clipping of the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) so that bottom-aligned web content will
be drawn above the native toolbar.

