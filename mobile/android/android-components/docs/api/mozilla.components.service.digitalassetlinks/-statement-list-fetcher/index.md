[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [StatementListFetcher](./index.md)

# StatementListFetcher

`interface StatementListFetcher` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/StatementListFetcher.kt#L10)

Lists all statements made by a given source.

### Functions

| Name | Summary |
|---|---|
| [listStatements](list-statements.md) | `abstract fun listStatements(source: `[`Web`](../-asset-descriptor/-web/index.md)`): `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`Statement`](../-statement/index.md)`>`<br>Retrieves a list of all statements from a given [source](list-statements.md#mozilla.components.service.digitalassetlinks.StatementListFetcher$listStatements(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web)/source). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DigitalAssetLinksApi](../../mozilla.components.service.digitalassetlinks.api/-digital-asset-links-api/index.md) | `class DigitalAssetLinksApi : `[`RelationChecker`](../-relation-checker/index.md)`, `[`StatementListFetcher`](./index.md)<br>Digital Asset Links allows any caller to check pre declared relationships between two assets which can be either web domains or native applications. This class checks for a specific relationship declared by two assets via the online API. |
| [StatementApi](../../mozilla.components.service.digitalassetlinks.local/-statement-api/index.md) | `class StatementApi : `[`StatementListFetcher`](./index.md)<br>Builds a list of statements by sending HTTP requests to the given website. |
