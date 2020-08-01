[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks.api](../index.md) / [DigitalAssetLinksApi](./index.md)

# DigitalAssetLinksApi

`class DigitalAssetLinksApi : `[`RelationChecker`](../../mozilla.components.service.digitalassetlinks/-relation-checker/index.md)`, `[`StatementListFetcher`](../../mozilla.components.service.digitalassetlinks/-statement-list-fetcher/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/api/DigitalAssetLinksApi.kt#L28)

Digital Asset Links allows any caller to check pre declared relationships between
two assets which can be either web domains or native applications.
This class checks for a specific relationship declared by two assets via the online API.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DigitalAssetLinksApi(httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, apiKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?)`<br>Digital Asset Links allows any caller to check pre declared relationships between two assets which can be either web domains or native applications. This class checks for a specific relationship declared by two assets via the online API. |

### Functions

| Name | Summary |
|---|---|
| [checkRelationship](check-relationship.md) | `fun checkRelationship(source: `[`Web`](../../mozilla.components.service.digitalassetlinks/-asset-descriptor/-web/index.md)`, relation: `[`Relation`](../../mozilla.components.service.digitalassetlinks/-relation/index.md)`, target: `[`AssetDescriptor`](../../mozilla.components.service.digitalassetlinks/-asset-descriptor/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Performs a check to ensure a directional relationships exists between the specified [source](../../mozilla.components.service.digitalassetlinks/-relation-checker/check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/source) and [target](../../mozilla.components.service.digitalassetlinks/-relation-checker/check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/target) assets. The relationship must match the [relation](../../mozilla.components.service.digitalassetlinks/-relation-checker/check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/relation) type given. |
| [listStatements](list-statements.md) | `fun listStatements(source: `[`Web`](../../mozilla.components.service.digitalassetlinks/-asset-descriptor/-web/index.md)`): `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`Statement`](../../mozilla.components.service.digitalassetlinks/-statement/index.md)`>`<br>Retrieves a list of all statements from a given [source](../../mozilla.components.service.digitalassetlinks/-statement-list-fetcher/list-statements.md#mozilla.components.service.digitalassetlinks.StatementListFetcher$listStatements(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web)/source). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
