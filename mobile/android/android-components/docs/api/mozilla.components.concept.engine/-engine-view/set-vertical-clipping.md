[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineView](index.md) / [setVerticalClipping](./set-vertical-clipping.md)

# setVerticalClipping

`abstract fun setVerticalClipping(clippingHeight: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineView.kt#L97)

Updates the amount of vertical space that is clipped or visibly obscured in the bottom portion of the view.
Tells the [EngineView](index.md) where to put bottom fixed elements so they are fully visible.

### Parameters

`clippingHeight` - The height of the bottom clipped space in screen pixels.