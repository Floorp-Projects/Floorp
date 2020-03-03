[android-components](../index.md) / [mozilla.components.feature.accounts.push](./index.md)

## Package mozilla.components.feature.accounts.push

### Types

| Name | Summary |
|---|---|
| [FxaPushSupportFeature](-fxa-push-support-feature/index.md) | `class FxaPushSupportFeature`<br>A feature used for supporting FxA and push integration where needed. One of the main functions is when FxA notifies the device during a sync, that it's unable to reach the device via push messaging; triggering a push registration renewal. |
| [OneTimeFxaPushReset](-one-time-fxa-push-reset/index.md) | `class OneTimeFxaPushReset`<br>Resets the fxa push scope (and therefore push subscription) if it does not follow the new format. |
| [SendTabFeature](-send-tab-feature/index.md) | `class SendTabFeature`<br>A feature that uses the [FxaAccountManager](../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) to receive tabs. |
| [SendTabUseCases](-send-tab-use-cases/index.md) | `class SendTabUseCases`<br>Contains use cases for sending tabs to devices related to the firefox-accounts. |
