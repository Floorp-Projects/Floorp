[android-components](../../index.md) / [mozilla.components.browser.session.tab](../index.md) / [CustomTabConfig](index.md) / [createFromIntent](./create-from-intent.md)

# createFromIntent

`fun ~~createFromIntent~~(intent: `[`SafeIntent`](../../mozilla.components.support.utils/-safe-intent/index.md)`, displayMetrics: <ERROR CLASS>? = null): `[`CustomTabConfig`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/tab/CustomTabConfig.kt#L125)
**Deprecated:** Use createCustomTabConfigFromIntent in feature-customtabs

Creates a CustomTabConfig instance based on the provided intent.

### Parameters

`intent` - the intent, wrapped as a SafeIntent, which is processed
to extract configuration data.

`displayMetrics` - needed in-order to verify that icons of a max size are only provided.

**Return**
the CustomTabConfig instance.

