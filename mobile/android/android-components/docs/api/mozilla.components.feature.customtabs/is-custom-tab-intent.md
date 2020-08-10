[android-components](../index.md) / [mozilla.components.feature.customtabs](index.md) / [isCustomTabIntent](./is-custom-tab-intent.md)

# isCustomTabIntent

`fun isCustomTabIntent(intent: <ERROR CLASS>): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabConfigHelper.kt#L50)

Checks if the provided intent is a custom tab intent.

### Parameters

`intent` - the intent to check.

**Return**
true if the intent is a custom tab intent, otherwise false.

`fun isCustomTabIntent(safeIntent: `[`SafeIntent`](../mozilla.components.support.utils/-safe-intent/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabConfigHelper.kt#L58)

Checks if the provided intent is a custom tab intent.

### Parameters

`safeIntent` - the intent to check, wrapped as a SafeIntent.

**Return**
true if the intent is a custom tab intent, otherwise false.

