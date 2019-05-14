[android-components](../../index.md) / [mozilla.components.feature.session.behavior](../index.md) / [EngineViewBottomBehavior](./index.md)

# EngineViewBottomBehavior

`class EngineViewBottomBehavior : Behavior<`[`View`](https://developer.android.com/reference/android/view/View.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/behavior/EngineViewBottomBehavior.kt#L23)

A [CoordinatorLayout.Behavior](#) implementation to be used with [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) when placing a toolbar at the
bottom of the screen.

Using this behavior requires the toolbar to use the BrowserToolbarBottomBehavior.

This implementation will update the vertical clipping of the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) so that bottom-aligned web content will
be drawn above the native toolbar.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EngineViewBottomBehavior(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`?, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`?)`<br>A [CoordinatorLayout.Behavior](#) implementation to be used with [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) when placing a toolbar at the bottom of the screen. |

### Functions

| Name | Summary |
|---|---|
| [layoutDependsOn](layout-depends-on.md) | `fun layoutDependsOn(parent: CoordinatorLayout, child: `[`View`](https://developer.android.com/reference/android/view/View.html)`, dependency: `[`View`](https://developer.android.com/reference/android/view/View.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onDependentViewChanged](on-dependent-view-changed.md) | `fun onDependentViewChanged(parent: CoordinatorLayout, child: `[`View`](https://developer.android.com/reference/android/view/View.html)`, dependency: `[`View`](https://developer.android.com/reference/android/view/View.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
