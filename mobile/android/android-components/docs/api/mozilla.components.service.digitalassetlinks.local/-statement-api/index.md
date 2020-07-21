[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks.local](../index.md) / [StatementApi](./index.md)

# StatementApi

`class StatementApi : `[`StatementListFetcher`](../../mozilla.components.service.digitalassetlinks/-statement-list-fetcher/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/local/StatementApi.kt#L30)

Builds a list of statements by sending HTTP requests to the given website.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `StatementApi(httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`)`<br>Builds a list of statements by sending HTTP requests to the given website. |

### Functions

| Name | Summary |
|---|---|
| [listStatements](list-statements.md) | `fun listStatements(source: `[`Web`](../../mozilla.components.service.digitalassetlinks/-asset-descriptor/-web/index.md)`): `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`Statement`](../../mozilla.components.service.digitalassetlinks/-statement/index.md)`>`<br>Lazily list all the statements in the given [source](list-statements.md#mozilla.components.service.digitalassetlinks.local.StatementApi$listStatements(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web)/source) website. If include statements are present, they will be resolved lazily. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
