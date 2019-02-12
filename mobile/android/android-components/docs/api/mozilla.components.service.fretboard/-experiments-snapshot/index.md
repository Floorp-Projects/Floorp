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
