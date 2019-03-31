[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [TrackingProtectionPolicy](./index.md)

# TrackingProtectionPolicy

`open class TrackingProtectionPolicy` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L75)

Represents a tracking protection policy, which is a combination of
tracker categories that should be blocked. Unless otherwise specified,
a [TrackingProtectionPolicy](./index.md) is applicable to all session types (see
[TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md)).

### Properties

| Name | Summary |
|---|---|
| [categories](categories.md) | `val categories: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [useForPrivateSessions](use-for-private-sessions.md) | `var useForPrivateSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [useForRegularSessions](use-for-regular-sessions.md) | `var useForRegularSessions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [contains](contains.md) | `fun contains(category: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [equals](equals.md) | `open fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `open fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [AD](-a-d.md) | `const val AD: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [ANALYTICS](-a-n-a-l-y-t-i-c-s.md) | `const val ANALYTICS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [CONTENT](-c-o-n-t-e-n-t.md) | `const val CONTENT: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [CRYPTOMINING](-c-r-y-p-t-o-m-i-n-i-n-g.md) | `const val CRYPTOMINING: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [SOCIAL](-s-o-c-i-a-l.md) | `const val SOCIAL: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [TEST](-t-e-s-t.md) | `const val TEST: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [all](all.md) | `fun all(): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md) |
| [none](none.md) | `fun none(): `[`TrackingProtectionPolicy`](./index.md) |
| [select](select.md) | `fun select(vararg categories: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TrackingProtectionPolicyForSessionTypes`](../-tracking-protection-policy-for-session-types/index.md) |

### Inheritors

| Name | Summary |
|---|---|
| [TrackingProtectionPolicyForSessionTypes](../-tracking-protection-policy-for-session-types/index.md) | `class TrackingProtectionPolicyForSessionTypes : `[`TrackingProtectionPolicy`](./index.md)<br>Subtype of [TrackingProtectionPolicy](./index.md) to control the type of session this policy should be applied to. By default, a policy will be applied to all sessions. |
