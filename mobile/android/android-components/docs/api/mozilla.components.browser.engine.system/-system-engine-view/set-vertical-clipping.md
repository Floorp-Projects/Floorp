[android-components](../../index.md) / [mozilla.components.browser.engine.system](../index.md) / [SystemEngineView](index.md) / [setVerticalClipping](./set-vertical-clipping.md)

# setVerticalClipping

`fun setVerticalClipping(clippingHeight: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/SystemEngineView.kt#L642)

Overrides [EngineView.setVerticalClipping](../../mozilla.components.concept.engine/-engine-view/set-vertical-clipping.md)

Updates the amount of vertical space that is clipped or visibly obscured in the bottom portion of the view.
Tells the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) where to put bottom fixed elements so they are fully visible.

### Parameters

`clippingHeight` - The height of the bottom clipped space in screen pixels.