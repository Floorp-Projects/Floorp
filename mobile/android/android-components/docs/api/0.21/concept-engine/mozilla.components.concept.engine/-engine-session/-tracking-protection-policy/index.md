---
title: EngineSession.TrackingProtectionPolicy - 
---

[mozilla.components.concept.engine](../../index.html) / [EngineSession](../index.html) / [TrackingProtectionPolicy](./index.html)

# TrackingProtectionPolicy

`class TrackingProtectionPolicy`

Represents a tracking protection policy which is a combination of
tracker categories that should be blocked.

### Constructors

| [&lt;init&gt;](-init-.html) | `TrackingProtectionPolicy(categories: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Represents a tracking protection policy which is a combination of tracker categories that should be blocked. |

### Properties

| [categories](categories.html) | `val categories: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Functions

| [equals](equals.html) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.html) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Properties

| [AD](-a-d.html) | `const val AD: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [ANALYTICS](-a-n-a-l-y-t-i-c-s.html) | `const val ANALYTICS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [CONTENT](-c-o-n-t-e-n-t.html) | `const val CONTENT: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [SOCIAL](-s-o-c-i-a-l.html) | `const val SOCIAL: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| [all](all.html) | `fun all(): `[`TrackingProtectionPolicy`](./index.md) |
| [none](none.html) | `fun none(): `[`TrackingProtectionPolicy`](./index.md) |
| [select](select.html) | `fun select(vararg categories: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TrackingProtectionPolicy`](./index.md) |

