[android-components](../../index.md) / [mozilla.components.feature.contextmenu](../index.md) / [ContextMenuCandidate](index.md) / [showFor](./show-for.md)

# showFor

`val showFor: (`[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`, `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuCandidate.kt#L33)

If this lambda returns true for a given [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) and [HitResult](../../mozilla.components.concept.engine/-hit-result/index.md) then it
will be displayed in the context menu.

### Property

`showFor` - If this lambda returns true for a given [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) and [HitResult](../../mozilla.components.concept.engine/-hit-result/index.md) then it
will be displayed in the context menu.