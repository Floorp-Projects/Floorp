[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [IncludeStatement](./index.md)

# IncludeStatement

`data class IncludeStatement : `[`StatementResult`](../-statement-result.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/Statement.kt#L23)

Include statements point to another Digital Asset Links statement file.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `IncludeStatement(include: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Include statements point to another Digital Asset Links statement file. |

### Properties

| Name | Summary |
|---|---|
| [include](include.md) | `val include: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
