[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks.local](../index.md) / [StatementRelationChecker](./index.md)

# StatementRelationChecker

`class StatementRelationChecker : `[`RelationChecker`](../../mozilla.components.service.digitalassetlinks/-relation-checker/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/local/StatementRelationChecker.kt#L16)

Checks if a matching relationship is present in a remote statement list.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `StatementRelationChecker(listFetcher: `[`StatementListFetcher`](../../mozilla.components.service.digitalassetlinks/-statement-list-fetcher/index.md)`)`<br>Checks if a matching relationship is present in a remote statement list. |

### Functions

| Name | Summary |
|---|---|
| [checkRelationship](check-relationship.md) | `fun checkRelationship(source: `[`Web`](../../mozilla.components.service.digitalassetlinks/-asset-descriptor/-web/index.md)`, relation: `[`Relation`](../../mozilla.components.service.digitalassetlinks/-relation/index.md)`, target: `[`AssetDescriptor`](../../mozilla.components.service.digitalassetlinks/-asset-descriptor/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Performs a check to ensure a directional relationships exists between the specified [source](../../mozilla.components.service.digitalassetlinks/-relation-checker/check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/source) and [target](../../mozilla.components.service.digitalassetlinks/-relation-checker/check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/target) assets. The relationship must match the [relation](../../mozilla.components.service.digitalassetlinks/-relation-checker/check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/relation) type given. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [checkLink](check-link.md) | `fun checkLink(statements: `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`Statement`](../../mozilla.components.service.digitalassetlinks/-statement/index.md)`>, relation: `[`Relation`](../../mozilla.components.service.digitalassetlinks/-relation/index.md)`, target: `[`AssetDescriptor`](../../mozilla.components.service.digitalassetlinks/-asset-descriptor/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Check if any of the given [Statement](../../mozilla.components.service.digitalassetlinks/-statement/index.md)s are linked to the given [target](check-link.md#mozilla.components.service.digitalassetlinks.local.StatementRelationChecker.Companion$checkLink(kotlin.sequences.Sequence((mozilla.components.service.digitalassetlinks.Statement)), mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/target). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
