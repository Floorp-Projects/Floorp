[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks.local](../index.md) / [StatementApi](index.md) / [listStatements](./list-statements.md)

# listStatements

`fun listStatements(source: `[`Web`](../../mozilla.components.service.digitalassetlinks/-asset-descriptor/-web/index.md)`): `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`Statement`](../../mozilla.components.service.digitalassetlinks/-statement/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/local/StatementApi.kt#L36)

Overrides [StatementListFetcher.listStatements](../../mozilla.components.service.digitalassetlinks/-statement-list-fetcher/list-statements.md)

Lazily list all the statements in the given [source](list-statements.md#mozilla.components.service.digitalassetlinks.local.StatementApi$listStatements(mozilla.components.service.digitalassetlinks.AssetDescriptor.Web)/source) website.
If include statements are present, they will be resolved lazily.

