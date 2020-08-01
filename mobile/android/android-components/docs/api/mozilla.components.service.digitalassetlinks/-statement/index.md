[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [Statement](./index.md)

# Statement

`data class Statement : `[`StatementResult`](../-statement-result.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/Statement.kt#L15)

Entry in a Digital Asset Links statement file.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Statement(relation: `[`Relation`](../-relation/index.md)`, target: `[`AssetDescriptor`](../-asset-descriptor/index.md)`)`<br>Entry in a Digital Asset Links statement file. |

### Properties

| Name | Summary |
|---|---|
| [relation](relation.md) | `val relation: `[`Relation`](../-relation/index.md) |
| [target](target.md) | `val target: `[`AssetDescriptor`](../-asset-descriptor/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
