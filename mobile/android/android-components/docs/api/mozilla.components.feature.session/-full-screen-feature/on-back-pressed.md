[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [FullScreenFeature](index.md) / [onBackPressed](./on-back-pressed.md)

# onBackPressed

`open fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/FullScreenFeature.kt#L37)

Overrides [BackHandler.onBackPressed](../../mozilla.components.support.base.feature/-back-handler/on-back-pressed.md)

To be called when the back button is pressed, so that only fullscreen mode closes.

**Return**
Returns true if the fullscreen mode was successfully exited; false if no effect was taken.

