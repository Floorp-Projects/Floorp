[android-components](../../index.md) / [mozilla.components.service.fretboard](../index.md) / [ExperimentsSnapshot](./index.md)

# ExperimentsSnapshot

`data class ExperimentsSnapshot` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/ExperimentsSnapshot.kt#L10)

Represents an experiment sync result

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ExperimentsSnapshot(experiments: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Experiment`](../-experiment/index.md)`>, lastModified: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?)`<br>Represents an experiment sync result |

### Properties

| Name | Summary |
|---|---|
| [experiments](experiments.md) | `val experiments: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Experiment`](../-experiment/index.md)`>`<br>Downloaded list of experiments |
| [lastModified](last-modified.md) | `val lastModified: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Last time experiments were modified on the server, as a UNIX timestamp |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
