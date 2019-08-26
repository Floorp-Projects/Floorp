[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [LoadUrlFlags](./index.md)

# LoadUrlFlags

`class LoadUrlFlags` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L318)

Describes a combination of flags provided to the engine when loading a URL.

### Properties

| Name | Summary |
|---|---|
| [value](value.md) | `val value: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Functions

| Name | Summary |
|---|---|
| [contains](contains.md) | `fun contains(flag: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ALLOW_POPUPS](-a-l-l-o-w_-p-o-p-u-p-s.md) | `const val ALLOW_POPUPS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [BYPASS_CACHE](-b-y-p-a-s-s_-c-a-c-h-e.md) | `const val BYPASS_CACHE: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [BYPASS_CLASSIFIER](-b-y-p-a-s-s_-c-l-a-s-s-i-f-i-e-r.md) | `const val BYPASS_CLASSIFIER: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [BYPASS_PROXY](-b-y-p-a-s-s_-p-r-o-x-y.md) | `const val BYPASS_PROXY: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [EXTERNAL](-e-x-t-e-r-n-a-l.md) | `const val EXTERNAL: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [NONE](-n-o-n-e.md) | `const val NONE: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [all](all.md) | `fun all(): `[`LoadUrlFlags`](./index.md) |
| [external](external.md) | `fun external(): `[`LoadUrlFlags`](./index.md) |
| [none](none.md) | `fun none(): `[`LoadUrlFlags`](./index.md) |
| [select](select.md) | `fun select(vararg types: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`LoadUrlFlags`](./index.md) |
