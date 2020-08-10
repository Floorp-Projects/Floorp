[android-components](../index.md) / [mozilla.components.service.digitalassetlinks](index.md) / [StatementResult](./-statement-result.md)

# StatementResult

`sealed class StatementResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/Statement.kt#L10)

Represents a statement that can be found in an assetlinks.json file.

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [IncludeStatement](-include-statement/index.md) | `data class IncludeStatement : `[`StatementResult`](./-statement-result.md)<br>Include statements point to another Digital Asset Links statement file. |
| [Statement](-statement/index.md) | `data class Statement : `[`StatementResult`](./-statement-result.md)<br>Entry in a Digital Asset Links statement file. |
