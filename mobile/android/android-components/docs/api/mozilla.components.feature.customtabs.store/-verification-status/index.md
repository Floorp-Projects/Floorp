[android-components](../../index.md) / [mozilla.components.feature.customtabs.store](../index.md) / [VerificationStatus](./index.md)

# VerificationStatus

`enum class VerificationStatus` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/store/CustomTabsServiceState.kt#L50)

Different states of Digital Asset Link verification.

### Enum Values

| Name | Summary |
|---|---|
| [PENDING](-p-e-n-d-i-n-g.md) | Indicates verification has started and hasn't returned yet. |
| [SUCCESS](-s-u-c-c-e-s-s.md) | Indicates that verification has completed and the link was verified. |
| [FAILURE](-f-a-i-l-u-r-e.md) | Indicates that verification has completed and the link was invalid. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
