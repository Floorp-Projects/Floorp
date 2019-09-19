[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [TrackingProtectionPolicy](index.md) / [strict](./strict.md)

# strict

`fun strict(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L246)

Strict policy.
Combining the [TrackingCategory.STRICT](-tracking-category/-s-t-r-i-c-t.md) plus a cookiePolicy of [ACCEPT_NON_TRACKERS](-cookie-policy/-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md)
This is the strictest setting and may cause issues on some web sites.

