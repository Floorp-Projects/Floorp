[android-components](../index.md) / [mozilla.components.feature.customtabs](index.md) / [createCustomTabConfigFromIntent](./create-custom-tab-config-from-intent.md)

# createCustomTabConfigFromIntent

`fun createCustomTabConfigFromIntent(intent: <ERROR CLASS>, resources: <ERROR CLASS>?): `[`CustomTabConfig`](../mozilla.components.browser.state.state/-custom-tab-config/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabConfigHelper.kt#L84)

Creates a [CustomTabConfig](../mozilla.components.browser.state.state/-custom-tab-config/index.md) instance based on the provided intent.

### Parameters

`intent` - the intent, wrapped as a SafeIntent, which is processed to extract configuration data.

`resources` - needed in-order to verify that icons of a max size are only provided.

**Return**
the CustomTabConfig instance.

