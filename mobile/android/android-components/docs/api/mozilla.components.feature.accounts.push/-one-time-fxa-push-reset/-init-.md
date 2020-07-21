[android-components](../../index.md) / [mozilla.components.feature.accounts.push](../index.md) / [OneTimeFxaPushReset](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`OneTimeFxaPushReset(context: <ERROR CLASS>, pushFeature: `[`AutoPushFeature`](../../mozilla.components.feature.push/-auto-push-feature/index.md)`)`

Resets the fxa push scope (and therefore push subscription) if it does not follow the new format.

This is needed only for our existing push users and can be removed when we're more confident our users are
all migrated.

Implementation Notes: In order to support a new performance fix related to push and
[FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) we need to use a new push scope format. This class checks if we have the old
format, and removes it if so, thereby generating a new push scope with the new format.

