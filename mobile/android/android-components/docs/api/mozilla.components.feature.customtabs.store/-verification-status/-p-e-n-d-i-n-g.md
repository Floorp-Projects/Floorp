[android-components](../../index.md) / [mozilla.components.feature.customtabs.store](../index.md) / [VerificationStatus](index.md) / [PENDING](./-p-e-n-d-i-n-g.md)

# PENDING

`PENDING` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/store/CustomTabsServiceState.kt#L57)

Indicates verification has started and hasn't returned yet.

To avoid flashing the toolbar, we choose to hide it when a Digital Asset Link is being verified.
We only show the toolbar when the verification fails, or an origin never requested to be verified.

