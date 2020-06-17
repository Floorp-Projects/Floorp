[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [RelationChecker](./index.md)

# RelationChecker

`interface RelationChecker` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/RelationChecker.kt#L10)

Verifies that a source is linked to a target.

### Functions

| Name | Summary |
|---|---|
| [checkRelationship](check-relationship.md) | `abstract fun checkRelationship(source: `[`Web`](../-asset-descriptor/-web/index.md)`, relation: `[`Relation`](../-relation/index.md)`, target: `[`AssetDescriptor`](../-asset-descriptor/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Performs a check to ensure a directional relationships exists between the specified [source](check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/source) and [target](check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/target) assets. The relationship must match the [relation](check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/relation) type given. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DigitalAssetLinksApi](../../mozilla.components.service.digitalassetlinks.api/-digital-asset-links-api/index.md) | `class DigitalAssetLinksApi : `[`RelationChecker`](./index.md)`, `[`StatementListFetcher`](../-statement-list-fetcher/index.md)<br>Digital Asset Links allows any caller to check pre declared relationships between two assets which can be either web domains or native applications. This class checks for a specific relationship declared by two assets via the online API. |
| [StatementRelationChecker](../../mozilla.components.service.digitalassetlinks.local/-statement-relation-checker/index.md) | `class StatementRelationChecker : `[`RelationChecker`](./index.md)<br>Checks if a matching relationship is present in a remote statement list. |
