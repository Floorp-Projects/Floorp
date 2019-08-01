[android-components](../index.md) / [mozilla.components.feature.customtabs](index.md) / [isTrustedWebActivityIntent](./is-trusted-web-activity-intent.md)

# isTrustedWebActivityIntent

`fun isTrustedWebActivityIntent(intent: <ERROR CLASS>): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabConfigHelper.kt#L65)

Checks if the provided intent is a trusted web activity intent.

### Parameters

`intent` - the intent to check.

**Return**
true if the intent is a trusted web activity intent, otherwise false.

`fun isTrustedWebActivityIntent(safeIntent: `[`SafeIntent`](../mozilla.components.support.utils/-safe-intent/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabConfigHelper.kt#L73)

Checks if the provided intent is a trusted web activity intent.

### Parameters

`safeIntent` - the intent to check, wrapped as a SafeIntent.

**Return**
true if the intent is a trusted web activity intent, otherwise false.

