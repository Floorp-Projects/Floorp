[android-components](../../index.md) / [mozilla.components.feature.customtabs](../index.md) / [CustomTabsToolbarFeature](index.md) / [onBackPressed](./on-back-pressed.md)

# onBackPressed

`fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabsToolbarFeature.kt#L236)

Overrides [BackHandler.onBackPressed](../../mozilla.components.support.base.feature/-back-handler/on-back-pressed.md)

When the back button is pressed if not initialized returns false,
when initialized removes the current Custom Tabs session and returns true.
Should be called when the back button is pressed.

