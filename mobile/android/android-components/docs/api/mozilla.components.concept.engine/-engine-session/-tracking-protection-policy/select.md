[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [TrackingProtectionPolicy](index.md) / [select](./select.md)

# select

`fun select(trackingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`TrackingCategory`](-tracking-category/index.md)`> = arrayOf(TrackingCategory.RECOMMENDED), cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md)` = ACCEPT_NON_TRACKERS, strictSocialTrackingProtection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`? = null, cookiePurging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L334)

Creates a custom [TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md) using the provide values .

### Parameters

`trackingCategories` - a list of tracking categories to apply.

`cookiePolicy` - indicate how cookies should behave for this policy.

`strictSocialTrackingProtection` - indicate  if content should be blocked from the
social-tracking-protection-digest256 list, when given a null value,
it is only applied when the [EngineSession.TrackingProtectionPolicy.TrackingCategory.STRICT](-tracking-category/-s-t-r-i-c-t.md)
is set.

`cookiePurging` - Whether or not to automatically purge tracking cookies. This will
purge cookies from tracking sites that do not have recent user interaction provided.