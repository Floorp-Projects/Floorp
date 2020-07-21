[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [RelationChecker](index.md) / [checkRelationship](./check-relationship.md)

# checkRelationship

`abstract fun checkRelationship(source: `[`Web`](../-asset-descriptor/-web/index.md)`, relation: `[`Relation`](../-relation/index.md)`, target: `[`AssetDescriptor`](../-asset-descriptor/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/RelationChecker.kt#L16)

Performs a check to ensure a directional relationships exists between the specified
[source](check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/source) and [target](check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/target) assets. The relationship must match the [relation](check-relationship.md#mozilla.components.service.digitalassetlinks.RelationChecker$checkRelationship(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web, mozilla.components.service.digitalassetlinks.Relation, mozilla.components.service.digitalassetlinks.AssetDescriptor)/relation) type given.

