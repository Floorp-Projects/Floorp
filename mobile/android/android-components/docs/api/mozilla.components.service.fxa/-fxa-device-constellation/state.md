[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [state](./state.md)

# state

`fun state(): `[`ConstellationState`](../../mozilla.components.concept.sync/-constellation-state/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L154)

Overrides [DeviceConstellation.state](../../mozilla.components.concept.sync/-device-constellation/state.md)

Current state of the constellation. May be missing if state was never queried.

**Return**
[ConstellationState](../../mozilla.components.concept.sync/-constellation-state/index.md) describes current and other known devices in the constellation.

