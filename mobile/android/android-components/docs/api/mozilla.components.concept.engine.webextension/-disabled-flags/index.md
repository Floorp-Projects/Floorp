[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [DisabledFlags](./index.md)

# DisabledFlags

`class DisabledFlags` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L429)

Flags to check for different reasons why an extension is disabled.

### Properties

| Name | Summary |
|---|---|
| [value](value.md) | `val value: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Functions

| Name | Summary |
|---|---|
| [contains](contains.md) | `fun contains(flag: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the provided flag is set. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [APP_SUPPORT](-a-p-p_-s-u-p-p-o-r-t.md) | `const val APP_SUPPORT: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [BLOCKLIST](-b-l-o-c-k-l-i-s-t.md) | `const val BLOCKLIST: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [USER](-u-s-e-r.md) | `const val USER: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [select](select.md) | `fun select(vararg flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`DisabledFlags`](./index.md)<br>Selects a combination of flags. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
