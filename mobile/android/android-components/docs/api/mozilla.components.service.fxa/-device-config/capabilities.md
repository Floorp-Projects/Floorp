[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [DeviceConfig](index.md) / [capabilities](./capabilities.md)

# capabilities

`val capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Config.kt#L30)

A set of device capabilities, such as SEND_TAB. This set can be expanded by
re-initializing [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) with a new set (e.g. on app restart).
Shrinking a set of capabilities is currently not supported.

### Property

`capabilities` - A set of device capabilities, such as SEND_TAB. This set can be expanded by
re-initializing [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) with a new set (e.g. on app restart).
Shrinking a set of capabilities is currently not supported.