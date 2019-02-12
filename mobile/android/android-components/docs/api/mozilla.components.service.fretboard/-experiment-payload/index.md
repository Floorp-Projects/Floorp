[android-components](../../index.md) / [mozilla.components.service.fretboard](../index.md) / [ExperimentPayload](./index.md)

# ExperimentPayload

`class ExperimentPayload` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/ExperimentPayload.kt#L10)

Class which represents an experiment associated data

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ExperimentPayload()`<br>Class which represents an experiment associated data |

### Functions

| Name | Summary |
|---|---|
| [get](get.md) | `fun get(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?`<br>Gets a value from the payload |
| [getBooleanList](get-boolean-list.md) | `fun getBooleanList(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>?`<br>Gets a value from the payload as a list of Boolean |
| [getDoubleList](get-double-list.md) | `fun getDoubleList(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`>?`<br>Gets a value from the payload as a list of Double |
| [getIntList](get-int-list.md) | `fun getIntList(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`>?`<br>Gets a value from the payload as a list of Int |
| [getKeys](get-keys.md) | `fun getKeys(): `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Gets all the payload keys |
| [getLongList](get-long-list.md) | `fun getLongList(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`>?`<br>Gets a value from the payload as a list of Long |
| [getStringList](get-string-list.md) | `fun getStringList(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?`<br>Gets a value from the payload as a list of String |
| [put](put.md) | `fun put(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Puts a value into the payload |
