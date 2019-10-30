[android-components](../../index.md) / [mozilla.components.feature.customtabs.store](../index.md) / [ValidateRelationshipAction](./index.md)

# ValidateRelationshipAction

`data class ValidateRelationshipAction : `[`CustomTabsAction`](../-custom-tabs-action/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/store/CustomTabsAction.kt#L35)

Marks the state of a custom tabs [Relation](#) verification.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ValidateRelationshipAction(token: CustomTabsSessionToken, relation: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, origin: <ERROR CLASS>, status: `[`VerificationStatus`](../-verification-status/index.md)`)`<br>Marks the state of a custom tabs [Relation](#) verification. |

### Properties

| Name | Summary |
|---|---|
| [origin](origin.md) | `val origin: <ERROR CLASS>`<br>Origin to verify. |
| [relation](relation.md) | `val relation: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Relationship type to verify. |
| [status](status.md) | `val status: `[`VerificationStatus`](../-verification-status/index.md)<br>State of the verification process. |
| [token](token.md) | `val token: CustomTabsSessionToken`<br>Token of the custom tab to verify. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
