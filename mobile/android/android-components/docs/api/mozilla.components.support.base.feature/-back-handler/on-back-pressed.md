[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [BackHandler](index.md) / [onBackPressed](./on-back-pressed.md)

# onBackPressed

`abstract fun ~~onBackPressed~~(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/BackHandler.kt#L24)
**Deprecated:** Use `UserInteractionHandler` instead.

Called when this [UserInteractionHandler](../-user-interaction-handler/index.md) gets the option to handle the user pressing the back key.

Returns true if this [UserInteractionHandler](../-user-interaction-handler/index.md) consumed the event and no other components need to be notified.

