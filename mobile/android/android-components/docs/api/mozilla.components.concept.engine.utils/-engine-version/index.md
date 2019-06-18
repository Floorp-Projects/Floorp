[android-components](../../index.md) / [mozilla.components.concept.engine.utils](../index.md) / [EngineVersion](./index.md)

# EngineVersion

`data class EngineVersion` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/utils/EngineVersion.kt#L16)

Data class for engine versions using semantic versioning (major.minor.patch).

### Parameters

`major` - Major version number

`minor` - Minor version number

`patch` - Patch version number

`metadata` - Additional and optional metadata appended to the version number, e.g. for a version number of
"68.0a1" [metadata](metadata.md) will contain "a1".

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EngineVersion(major: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, minor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, patch: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, metadata: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Data class for engine versions using semantic versioning (major.minor.patch). |

### Properties

| Name | Summary |
|---|---|
| [major](major.md) | `val major: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Major version number |
| [metadata](metadata.md) | `val metadata: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Additional and optional metadata appended to the version number, e.g. for a version number of "68.0a1" [metadata](metadata.md) will contain "a1". |
| [minor](minor.md) | `val minor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Minor version number |
| [patch](patch.md) | `val patch: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Patch version number |

### Functions

| Name | Summary |
|---|---|
| [compareTo](compare-to.md) | `operator fun compareTo(other: `[`EngineVersion`](./index.md)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [isAtLeast](is-at-least.md) | `fun isAtLeast(major: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, minor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, patch: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this version number equals or is higher than the provided [major](is-at-least.md#mozilla.components.concept.engine.utils.EngineVersion$isAtLeast(kotlin.Int, kotlin.Int, kotlin.Long)/major), [minor](is-at-least.md#mozilla.components.concept.engine.utils.EngineVersion$isAtLeast(kotlin.Int, kotlin.Int, kotlin.Long)/minor), [patch](is-at-least.md#mozilla.components.concept.engine.utils.EngineVersion$isAtLeast(kotlin.Int, kotlin.Int, kotlin.Long)/patch) version numbers. |
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [parse](parse.md) | `fun parse(version: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`EngineVersion`](./index.md)`?`<br>Parses the given [version](parse.md#mozilla.components.concept.engine.utils.EngineVersion.Companion$parse(kotlin.String)/version) string and returns an [EngineVersion](./index.md). Returns null if the [version](parse.md#mozilla.components.concept.engine.utils.EngineVersion.Companion$parse(kotlin.String)/version) string could not be parsed successfully. |
