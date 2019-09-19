[android-components](../../index.md) / [mozilla.components.feature.customtabs.store](../index.md) / [SaveCreatorPackageNameAction](./index.md)

# SaveCreatorPackageNameAction

`data class SaveCreatorPackageNameAction : `[`CustomTabsAction`](../-custom-tabs-action/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/store/CustomTabsAction.kt#L22)

Saves the package name corresponding to a custom tab token.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SaveCreatorPackageNameAction(token: CustomTabsSessionToken, packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Saves the package name corresponding to a custom tab token. |

### Properties

| Name | Summary |
|---|---|
| [packageName](package-name.md) | `val packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Package name of the app that created the custom tab. |
| [token](token.md) | `val token: CustomTabsSessionToken`<br>Token of the custom tab. |
