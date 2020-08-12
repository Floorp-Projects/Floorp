[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [TrackingProtectionPolicy](./index.md)

# TrackingProtectionPolicy

`open class TrackingProtectionPolicy` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L182)

Represents a tracking protection policy, which is a combination of
tracker categories that should be blocked. Unless otherwise specified,
a [TrackingProtectionPolicy](./index.md) is applicable to all session types (see
[TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md)).

### Types

| Name | Summary |
|---|---|
| [CookiePolicy](-cookie-policy/index.md) | `enum class CookiePolicy`<br>Indicates how cookies should behave for a given [TrackingProtectionPolicy](./index.md). The ids of each cookiePolicy is aligned with the GeckoView @CookieBehavior constants. |
| [TrackingCategory](-tracking-category/index.md) | `enum class TrackingCategory` |

### Properties

| Name | Summary |
|---|---|
| [cookiePolicy](cookie-policy.md) | `val cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md) |
| [cookiePurging](cookie-purging.md) | `val cookiePurging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [strictSocialTrackingProtection](strict-social-tracking-protection.md) | `val strictSocialTrackingProtection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`?` |
| [trackingCategories](tracking-categories.md) | `val trackingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`TrackingCategory`](-tracking-category/index.md)`>` |
| [useForPrivateSessions](use-for-private-sessions.md) | `val useForPrivateSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [useForRegularSessions](use-for-regular-sessions.md) | `val useForRegularSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [contains](contains.md) | `fun contains(category: `[`TrackingCategory`](-tracking-category/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [equals](equals.md) | `open fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `open fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [none](none.md) | `fun none(): `[`TrackingProtectionPolicy`](./index.md) |
| [recommended](recommended.md) | `fun recommended(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md)<br>Recommended policy. Combining the [TrackingCategory.RECOMMENDED](-tracking-category/-r-e-c-o-m-m-e-n-d-e-d.md) plus a [CookiePolicy](-cookie-policy/index.md) of [ACCEPT_NON_TRACKERS](-cookie-policy/-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md). This is the recommended setting. |
| [select](select.md) | `fun select(trackingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`TrackingCategory`](-tracking-category/index.md)`> = arrayOf(TrackingCategory.RECOMMENDED), cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md)` = ACCEPT_NON_TRACKERS, strictSocialTrackingProtection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`? = null, cookiePurging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md)<br>Creates a custom [TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md) using the provide values . |
| [strict](strict.md) | `fun strict(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md)<br>Strict policy. Combining the [TrackingCategory.STRICT](-tracking-category/-s-t-r-i-c-t.md) plus a cookiePolicy of [ACCEPT_NON_TRACKERS](-cookie-policy/-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md) This is the strictest setting and may cause issues on some web sites. |

### Extension Functions

| Name | Summary |
|---|---|
| [toContentBlockingSetting](../../../mozilla.components.browser.engine.gecko.ext/to-content-blocking-setting.md) | `fun `[`TrackingProtectionPolicy`](./index.md)`.toContentBlockingSetting(safeBrowsingPolicy: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`SafeBrowsingPolicy`](../-safe-browsing-policy/index.md)`> = arrayOf(EngineSession.SafeBrowsingPolicy.RECOMMENDED)): `[`Settings`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/ContentBlocking/Settings.html)<br>Converts a [TrackingProtectionPolicy](./index.md) into a GeckoView setting that can be used with [GeckoRuntimeSettings.Builder](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntimeSettings/Builder.html). |

### Inheritors

| Name | Summary |
|---|---|
| [TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md) | `class TrackingProtectionPolicyForSessionTypes : `[`TrackingProtectionPolicy`](./index.md)<br>Subtype of [TrackingProtectionPolicy](./index.md) to control the type of session this policy should be applied to. By default, a policy will be applied to all sessions. |
