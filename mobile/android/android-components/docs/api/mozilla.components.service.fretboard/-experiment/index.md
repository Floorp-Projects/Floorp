[android-components](../../index.md) / [mozilla.components.service.fretboard](../index.md) / [Experiment](./index.md)

# Experiment

`data class Experiment` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/Experiment.kt#L12)

Represents an A/B test experiment,
independent of the underlying
storage mechanism

### Types

| Name | Summary |
|---|---|
| [Bucket](-bucket/index.md) | `data class Bucket` |
| [Matcher](-matcher/index.md) | `data class Matcher` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Experiment(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, description: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, match: `[`Matcher`](-matcher/index.md)`? = null, bucket: `[`Bucket`](-bucket/index.md)`? = null, lastModified: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null, payload: `[`ExperimentPayload`](../-experiment-payload/index.md)`? = null, schema: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null)`<br>Represents an A/B test experiment, independent of the underlying storage mechanism |

### Properties

| Name | Summary |
|---|---|
| [bucket](bucket.md) | `val bucket: `[`Bucket`](-bucket/index.md)`?`<br>Experiment buckets |
| [description](description.md) | `val description: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Detailed description of the experiment |
| [lastModified](last-modified.md) | `val lastModified: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Last modified date, as a UNIX timestamp |
| [match](match.md) | `val match: `[`Matcher`](-matcher/index.md)`?`<br>Filters for enabling the experiment |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Human-readable name of the experiment |
| [payload](payload.md) | `val payload: `[`ExperimentPayload`](../-experiment-payload/index.md)`?`<br>Experiment associated metadata |
| [schema](schema.md) | `val schema: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Last time the experiment schema was modified (as a UNIX timestamp) |

### Functions

| Name | Summary |
|---|---|
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Compares experiments by their id |
| [hashCode](hash-code.md) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
