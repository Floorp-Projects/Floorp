[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [TrackingProtectionPolicy](index.md) / [all](./all.md)

# all

`fun all(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L198)

Strict policy.
Combining the [recommended](recommended.md) categories plus [CRYPTOMINING](-c-r-y-p-t-o-m-i-n-i-n-g.md), [FINGERPRINTING](-f-i-n-g-e-r-p-r-i-n-t-i-n-g.md) and [CONTENT](-c-o-n-t-e-n-t.md).
With a cookiePolicy of [ACCEPT_NON_TRACKERS](-cookie-policy/-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md).
This is the strictest setting and may cause issues on some web sites.

