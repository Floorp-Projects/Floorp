[android-components](../../index.md) / [mozilla.components.feature.session.behavior](../index.md) / [EngineViewBottomBehavior](./index.md)

# EngineViewBottomBehavior

`class EngineViewBottomBehavior : Behavior<<ERROR CLASS>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/behavior/EngineViewBottomBehavior.kt#L24)

A [CoordinatorLayout.Behavior](#) implementation to be used with [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) when placing a toolbar at the
bottom of the screen.

Using this behavior requires the toolbar to use the BrowserToolbarBottomBehavior.

This implementation will update the vertical clipping of the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) so that bottom-aligned web content will
be drawn above the native toolbar.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EngineViewBottomBehavior(context: <ERROR CLASS>?, attrs: <ERROR CLASS>?)`<br>A [CoordinatorLayout.Behavior](#) implementation to be used with [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) when placing a toolbar at the bottom of the screen. |

### Functions

| Name | Summary |
|---|---|
| [layoutDependsOn](layout-depends-on.md) | `fun layoutDependsOn(parent: CoordinatorLayout, child: <ERROR CLASS>, dependency: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onDependentViewChanged](on-dependent-view-changed.md) | `fun onDependentViewChanged(parent: CoordinatorLayout, child: <ERROR CLASS>, dependency: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Apply vertical clipping to [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md). This requires [EngineViewBottomBehavior](./index.md) to be set in/on the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) or its parent. Must be a direct descending child of [CoordinatorLayout](#). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
