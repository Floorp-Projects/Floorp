[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [TrackingProtectionPolicy](./index.md)

# TrackingProtectionPolicy

`open class TrackingProtectionPolicy` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L96)

Represents a tracking protection policy, which is a combination of
tracker categories that should be blocked. Unless otherwise specified,
a [TrackingProtectionPolicy](./index.md) is applicable to all session types (see
[TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md)).

### Types

| Name | Summary |
|---|---|
| [CookiePolicy](-cookie-policy/index.md) | `enum class CookiePolicy`<br>Indicates how cookies should behave for a given [TrackingProtectionPolicy](./index.md). The ids of each cookiePolicy is aligned with the GeckoView @CookieBehavior constants. |
| [SafeBrowsingCategory](-safe-browsing-category/index.md) | `enum class SafeBrowsingCategory` |
| [TrackingCategory](-tracking-category/index.md) | `enum class TrackingCategory` |

### Properties

| Name | Summary |
|---|---|
| [cookiePolicy](cookie-policy.md) | `val cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md) |
| [safeBrowsingCategories](safe-browsing-categories.md) | `val safeBrowsingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`SafeBrowsingCategory`](-safe-browsing-category/index.md)`>` |
| [trackingCategories](tracking-categories.md) | `val trackingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`TrackingCategory`](-tracking-category/index.md)`>` |
| [useForPrivateSessions](use-for-private-sessions.md) | `val useForPrivateSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [useForRegularSessions](use-for-regular-sessions.md) | `val useForRegularSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [equals](equals.md) | `open fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `open fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [none](none.md) | `fun none(): `[`TrackingProtectionPolicy`](./index.md) |
| [recommended](recommended.md) | `fun recommended(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md)<br>Recommended policy. Combining the [TrackingCategory.RECOMMENDED](-tracking-category/-r-e-c-o-m-m-e-n-d-e-d.md) plus [SafeBrowsingCategory.RECOMMENDED](-safe-browsing-category/-r-e-c-o-m-m-e-n-d-e-d.md). With a [CookiePolicy](-cookie-policy/index.md) of [ACCEPT_NON_TRACKERS](-cookie-policy/-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md). This is the recommended setting. |
| [select](select.md) | `fun select(trackingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`TrackingCategory`](-tracking-category/index.md)`> = arrayOf(TrackingCategory.RECOMMENDED), safeBrowsingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`SafeBrowsingCategory`](-safe-browsing-category/index.md)`> = arrayOf(SafeBrowsingCategory.RECOMMENDED), cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md)` = ACCEPT_NON_TRACKERS): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md) |
| [strict](strict.md) | `fun strict(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md)<br>Strict policy. Combining the [TrackingCategory.STRICT](-tracking-category/-s-t-r-i-c-t.md) plus [SafeBrowsingCategory.RECOMMENDED](-safe-browsing-category/-r-e-c-o-m-m-e-n-d-e-d.md) With a cookiePolicy of [ACCEPT_NON_TRACKERS](-cookie-policy/-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md). This is the strictest setting and may cause issues on some web sites. |

### Inheritors

| Name | Summary |
|---|---|
| [TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md) | `class TrackingProtectionPolicyForSessionTypes : `[`TrackingProtectionPolicy`](./index.md)<br>Subtype of [TrackingProtectionPolicy](./index.md) to control the type of session this policy should be applied to. By default, a policy will be applied to all sessions. |
