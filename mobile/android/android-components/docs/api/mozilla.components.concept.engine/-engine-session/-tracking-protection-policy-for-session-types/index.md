[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [TrackingProtectionPolicyForSessionTypes](./index.md)

# TrackingProtectionPolicyForSessionTypes

`class TrackingProtectionPolicyForSessionTypes : `[`TrackingProtectionPolicy`](../-tracking-protection-policy/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L283)

Subtype of [TrackingProtectionPolicy](../-tracking-protection-policy/index.md) to control the type of session this policy
should be applied to. By default, a policy will be applied to all sessions.

### Inherited Properties

| Name | Summary |
|---|---|
| [cookiePolicy](../-tracking-protection-policy/cookie-policy.md) | `val cookiePolicy: `[`CookiePolicy`](../-tracking-protection-policy/-cookie-policy/index.md) |
| [safeBrowsingCategories](../-tracking-protection-policy/safe-browsing-categories.md) | `val safeBrowsingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`SafeBrowsingCategory`](../-tracking-protection-policy/-safe-browsing-category/index.md)`>` |
| [trackingCategories](../-tracking-protection-policy/tracking-categories.md) | `val trackingCategories: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`TrackingCategory`](../-tracking-protection-policy/-tracking-category/index.md)`>` |
| [useForPrivateSessions](../-tracking-protection-policy/use-for-private-sessions.md) | `val useForPrivateSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [useForRegularSessions](../-tracking-protection-policy/use-for-regular-sessions.md) | `val useForRegularSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [forPrivateSessionsOnly](for-private-sessions-only.md) | `fun forPrivateSessionsOnly(): `[`TrackingProtectionPolicy`](../-tracking-protection-policy/index.md)<br>Marks this policy to be used for private sessions only. |
| [forRegularSessionsOnly](for-regular-sessions-only.md) | `fun forRegularSessionsOnly(): `[`TrackingProtectionPolicy`](../-tracking-protection-policy/index.md)<br>Marks this policy to be used for regular (non-private) sessions only. |

### Inherited Functions

| Name | Summary |
|---|---|
| [contains](../-tracking-protection-policy/contains.md) | `fun contains(category: `[`TrackingCategory`](../-tracking-protection-policy/-tracking-category/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [equals](../-tracking-protection-policy/equals.md) | `open fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](../-tracking-protection-policy/hash-code.md) | `open fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
