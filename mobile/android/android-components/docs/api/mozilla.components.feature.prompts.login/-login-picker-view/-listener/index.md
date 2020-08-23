[android-components](../../../index.md) / [mozilla.components.feature.prompts.login](../../index.md) / [LoginPickerView](../index.md) / [Listener](./index.md)

# Listener

`interface Listener` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/login/LoginPickerView.kt#L44)

Interface to allow a class to listen to login selection prompt events.

### Functions

| Name | Summary |
|---|---|
| [onLoginSelected](on-login-selected.md) | `abstract fun onLoginSelected(login: `[`Login`](../../../mozilla.components.concept.storage/-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when a user selects a login to fill from the options. |
| [onManageLogins](on-manage-logins.md) | `abstract fun onManageLogins(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when the user invokes the "manage logins" option. |
