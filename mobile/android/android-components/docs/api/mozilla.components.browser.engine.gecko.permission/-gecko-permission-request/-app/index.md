[android-components](../../../index.md) / [mozilla.components.browser.engine.gecko.permission](../../index.md) / [GeckoPermissionRequest](../index.md) / [App](./index.md)

# App

`data class App : `[`GeckoPermissionRequest`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/permission/GeckoPermissionRequest.kt#L66)

Represents a gecko-based application permission request.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `App(nativePermissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, callback: `[`Callback`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession/PermissionDelegate/Callback.html)`)`<br>Represents a gecko-based application permission request. |

### Properties

| Name | Summary |
|---|---|
| [uri](uri.md) | `val uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the URI of the content requesting the permissions. |

### Inherited Properties

| Name | Summary |
|---|---|
| [permissions](../permissions.md) | `open val permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../../mozilla.components.concept.engine.permission/-permission/index.md)`>`<br>the list of requested permissions. |

### Inherited Functions

| Name | Summary |
|---|---|
| [grant](../grant.md) | `open fun grant(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../../mozilla.components.concept.engine.permission/-permission/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Grants the provided permissions, or all requested permissions, if none are provided. |
| [reject](../reject.md) | `open fun reject(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Rejects the requested permissions. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [permissionsMap](permissions-map.md) | `val permissionsMap: <ERROR CLASS>` |
