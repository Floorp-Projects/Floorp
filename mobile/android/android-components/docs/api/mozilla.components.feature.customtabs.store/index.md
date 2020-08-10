[android-components](../index.md) / [mozilla.components.feature.customtabs.store](./index.md)

## Package mozilla.components.feature.customtabs.store

### Types

| Name | Summary |
|---|---|
| [CustomTabState](-custom-tab-state/index.md) | `data class CustomTabState`<br>Value type that represents the state of a single custom tab accessible from both the service and activity. |
| [CustomTabsAction](-custom-tabs-action/index.md) | `sealed class CustomTabsAction : `[`Action`](../mozilla.components.lib.state/-action.md) |
| [CustomTabsServiceState](-custom-tabs-service-state/index.md) | `data class CustomTabsServiceState : `[`State`](../mozilla.components.lib.state/-state.md)<br>Value type that represents the custom tabs state accessible from both the service and activity. |
| [CustomTabsServiceStore](-custom-tabs-service-store/index.md) | `class CustomTabsServiceStore : `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`CustomTabsServiceState`](-custom-tabs-service-state/index.md)`, `[`CustomTabsAction`](-custom-tabs-action/index.md)`>` |
| [OriginRelationPair](-origin-relation-pair/index.md) | `data class OriginRelationPair`<br>Pair of origin and relation type used as key in [CustomTabState.relationships](-custom-tab-state/relationships.md). |
| [SaveCreatorPackageNameAction](-save-creator-package-name-action/index.md) | `data class SaveCreatorPackageNameAction : `[`CustomTabsAction`](-custom-tabs-action/index.md)<br>Saves the package name corresponding to a custom tab token. |
| [ValidateRelationshipAction](-validate-relationship-action/index.md) | `data class ValidateRelationshipAction : `[`CustomTabsAction`](-custom-tabs-action/index.md)<br>Marks the state of a custom tabs [Relation](#) verification. |
| [VerificationStatus](-verification-status/index.md) | `enum class VerificationStatus`<br>Different states of Digital Asset Link verification. |
