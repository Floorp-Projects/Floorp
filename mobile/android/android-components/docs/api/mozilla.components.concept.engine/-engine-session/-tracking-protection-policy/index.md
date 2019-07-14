[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [TrackingProtectionPolicy](./index.md)

# TrackingProtectionPolicy

`open class TrackingProtectionPolicy` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L92)

Represents a tracking protection policy, which is a combination of
tracker categories that should be blocked. Unless otherwise specified,
a [TrackingProtectionPolicy](./index.md) is applicable to all session types (see
[TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md)).

### Types

| Name | Summary |
|---|---|
| [CookiePolicy](-cookie-policy/index.md) | `enum class CookiePolicy`<br>Indicates how cookies should behave for a given [TrackingProtectionPolicy](./index.md). The ids of each cookiePolicy is aligned with the GeckoView @CookieBehavior constants. |

### Properties

| Name | Summary |
|---|---|
| [categories](categories.md) | `val categories: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [cookiePolicy](cookie-policy.md) | `val cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md) |
| [useForPrivateSessions](use-for-private-sessions.md) | `val useForPrivateSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [useForRegularSessions](use-for-regular-sessions.md) | `val useForRegularSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [contains](contains.md) | `fun contains(category: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [equals](equals.md) | `open fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `open fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [AD](-a-d.md) | `const val AD: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks advertisement trackers. |
| [ALL](-a-l-l.md) | `const val ALL: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [ANALYTICS](-a-n-a-l-y-t-i-c-s.md) | `const val ANALYTICS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks analytics trackers. |
| [CONTENT](-c-o-n-t-e-n-t.md) | `const val CONTENT: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks content trackers. May cause issues with some web sites. |
| [CRYPTOMINING](-c-r-y-p-t-o-m-i-n-i-n-g.md) | `const val CRYPTOMINING: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks cryptocurrency miners. |
| [FINGERPRINTING](-f-i-n-g-e-r-p-r-i-n-t-i-n-g.md) | `const val FINGERPRINTING: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks fingerprinting trackers. |
| [RECOMMENDED](-r-e-c-o-m-m-e-n-d-e-d.md) | `const val RECOMMENDED: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [SAFE_BROWSING_ALL](-s-a-f-e_-b-r-o-w-s-i-n-g_-a-l-l.md) | `const val SAFE_BROWSING_ALL: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks all unsafe sites. |
| [SAFE_BROWSING_HARMFUL](-s-a-f-e_-b-r-o-w-s-i-n-g_-h-a-r-m-f-u-l.md) | `const val SAFE_BROWSING_HARMFUL: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks harmful sites. |
| [SAFE_BROWSING_MALWARE](-s-a-f-e_-b-r-o-w-s-i-n-g_-m-a-l-w-a-r-e.md) | `const val SAFE_BROWSING_MALWARE: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks malware sites. |
| [SAFE_BROWSING_PHISHING](-s-a-f-e_-b-r-o-w-s-i-n-g_-p-h-i-s-h-i-n-g.md) | `const val SAFE_BROWSING_PHISHING: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks phishing sites. |
| [SAFE_BROWSING_UNWANTED](-s-a-f-e_-b-r-o-w-s-i-n-g_-u-n-w-a-n-t-e-d.md) | `const val SAFE_BROWSING_UNWANTED: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks unwanted sites. |
| [SOCIAL](-s-o-c-i-a-l.md) | `const val SOCIAL: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Blocks social trackers. |
| [TEST](-t-e-s-t.md) | `const val TEST: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [all](all.md) | `fun all(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md)<br>Strict policy. Combining the [recommended](recommended.md) categories plus [CRYPTOMINING](-c-r-y-p-t-o-m-i-n-i-n-g.md), [FINGERPRINTING](-f-i-n-g-e-r-p-r-i-n-t-i-n-g.md) and [CONTENT](-c-o-n-t-e-n-t.md). With a cookiePolicy of [ACCEPT_NON_TRACKERS](-cookie-policy/-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md). This is the strictest setting and may cause issues on some web sites. |
| [none](none.md) | `fun none(): `[`TrackingProtectionPolicy`](./index.md) |
| [recommended](recommended.md) | `fun recommended(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md)<br>Recommended policy. Combining the [AD](-a-d.md), [ANALYTICS](-a-n-a-l-y-t-i-c-s.md), [SOCIAL](-s-o-c-i-a-l.md), [TEST](-t-e-s-t.md) categories plus [SAFE_BROWSING_ALL](-s-a-f-e_-b-r-o-w-s-i-n-g_-a-l-l.md). With a [CookiePolicy](-cookie-policy/index.md) of [ACCEPT_NON_TRACKERS](-cookie-policy/-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md). This is the recommended setting. |
| [select](select.md) | `fun select(vararg categories: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md)` = ACCEPT_NON_TRACKERS): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md) |

### Inheritors

| Name | Summary |
|---|---|
| [TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md) | `class TrackingProtectionPolicyForSessionTypes : `[`TrackingProtectionPolicy`](./index.md)<br>Subtype of [TrackingProtectionPolicy](./index.md) to control the type of session this policy should be applied to. By default, a policy will be applied to all sessions. |
