[android-components](../index.md) / [mozilla.components.service.digitalassetlinks](./index.md)

## Package mozilla.components.service.digitalassetlinks

### Types

| Name | Summary |
|---|---|
| [AndroidAssetFinder](-android-asset-finder/index.md) | `class AndroidAssetFinder`<br>Get the SHA256 certificate for an installed Android app. |
| [AssetDescriptor](-asset-descriptor/index.md) | `sealed class AssetDescriptor`<br>Uniquely identifies an asset. |
| [IncludeStatement](-include-statement/index.md) | `data class IncludeStatement : `[`StatementResult`](-statement-result.md)<br>Include statements point to another Digital Asset Links statement file. |
| [Relation](-relation/index.md) | `enum class Relation`<br>Describes the nature of a statement, and consists of a kind and a detail. |
| [RelationChecker](-relation-checker/index.md) | `interface RelationChecker`<br>Verifies that a source is linked to a target. |
| [Statement](-statement/index.md) | `data class Statement : `[`StatementResult`](-statement-result.md)<br>Entry in a Digital Asset Links statement file. |
| [StatementListFetcher](-statement-list-fetcher/index.md) | `interface StatementListFetcher`<br>Lists all statements made by a given source. |
| [StatementResult](-statement-result.md) | `sealed class StatementResult`<br>Represents a statement that can be found in an assetlinks.json file. |
