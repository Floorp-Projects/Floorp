[android-components](../index.md) / [mozilla.components.lib.state](index.md) / [State](./-state.md)

# State

`interface State` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/State.kt#L10)

Generic interface for a [State](./-state.md) maintained by a [Store](-store/index.md).

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md) | `data class BrowserState : `[`State`](./-state.md)<br>Value type that represents the complete state of the browser/engine. |
| [CustomTabsServiceState](../mozilla.components.feature.customtabs.store/-custom-tabs-service-state/index.md) | `data class CustomTabsServiceState : `[`State`](./-state.md)<br>Value type that represents the custom tabs state accessible from both the service and activity. |
| [MigrationState](../mozilla.components.support.migration.state/-migration-state/index.md) | `data class MigrationState : `[`State`](./-state.md)<br>Value type that represents the state of the migration. |
