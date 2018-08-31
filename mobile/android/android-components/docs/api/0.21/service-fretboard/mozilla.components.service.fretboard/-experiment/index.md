---
title: Experiment - 
---

[mozilla.components.service.fretboard](../index.html) / [Experiment](./index.html)

# Experiment

`data class Experiment`

Represents an A/B test experiment,
independent of the underlying
storage mechanism

### Types

| [Bucket](-bucket/index.html) | `data class Bucket` |
| [Matcher](-matcher/index.html) | `data class Matcher` |

### Constructors

| [&lt;init&gt;](-init-.html) | `Experiment(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, description: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, match: `[`Matcher`](-matcher/index.html)`? = null, bucket: `[`Bucket`](-bucket/index.html)`? = null, lastModified: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null, payload: `[`ExperimentPayload`](../-experiment-payload/index.html)`? = null, schema: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null)`<br>Represents an A/B test experiment, independent of the underlying storage mechanism |

### Properties

| [bucket](bucket.html) | `val bucket: `[`Bucket`](-bucket/index.html)`?`<br>Experiment buckets |
| [description](description.html) | `val description: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Detailed description of the experiment |
| [id](id.html) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Unique identifier of the experiment |
| [lastModified](last-modified.html) | `val lastModified: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Last modified date, as a UNIX timestamp |
| [match](match.html) | `val match: `[`Matcher`](-matcher/index.html)`?`<br>Filters for enabling the experiment |
| [name](name.html) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Human-readable name of the experiment |
| [payload](payload.html) | `val payload: `[`ExperimentPayload`](../-experiment-payload/index.html)`?`<br>Experiment associated metadata |
| [schema](schema.html) | `val schema: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Last time the experiment schema was modified (as a UNIX timestamp) |

### Functions

| [equals](equals.html) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Compares experiments by their id |
| [hashCode](hash-code.html) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

