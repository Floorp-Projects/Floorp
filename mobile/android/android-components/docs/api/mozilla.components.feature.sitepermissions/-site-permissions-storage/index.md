[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsStorage](./index.md)

# SitePermissionsStorage

`class SitePermissionsStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsStorage.kt#L33)

A storage implementation to save [SitePermissions](../-site-permissions/index.md).

### Types

| Name | Summary |
|---|---|
| [Permission](-permission/index.md) | `enum class Permission` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SitePermissionsStorage(context: <ERROR CLASS>, dataCleanable: `[`DataCleanable`](../../mozilla.components.concept.engine/-data-cleanable/index.md)`? = null)`<br>A storage implementation to save [SitePermissions](../-site-permissions/index.md). |

### Functions

| Name | Summary |
|---|---|
| [findAllSitePermissionsGroupedByPermission](find-all-site-permissions-grouped-by-permission.md) | `fun findAllSitePermissionsGroupedByPermission(): `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`Permission`](-permission/index.md)`, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SitePermissions`](../-site-permissions/index.md)`>>`<br>Finds all SitePermissions grouped by [Permission](-permission/index.md). |
| [findSitePermissionsBy](find-site-permissions-by.md) | `fun findSitePermissionsBy(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SitePermissions`](../-site-permissions/index.md)`?`<br>Finds all SitePermissions that match the [origin](find-site-permissions-by.md#mozilla.components.feature.sitepermissions.SitePermissionsStorage$findSitePermissionsBy(kotlin.String)/origin). |
| [getSitePermissionsPaged](get-site-permissions-paged.md) | `fun getSitePermissionsPaged(): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`SitePermissions`](../-site-permissions/index.md)`>`<br>Returns all saved [SitePermissions](../-site-permissions/index.md) instances as a [DataSource.Factory](#). |
| [remove](remove.md) | `fun remove(sitePermissions: `[`SitePermissions`](../-site-permissions/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes all sitePermissions that match the sitePermissions provided as a parameter. |
| [removeAll](remove-all.md) | `fun removeAll(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes all sitePermissions sitePermissions. |
| [save](save.md) | `fun save(sitePermissions: `[`SitePermissions`](../-site-permissions/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Persists the [sitePermissions](save.md#mozilla.components.feature.sitepermissions.SitePermissionsStorage$save(mozilla.components.feature.sitepermissions.SitePermissions)/sitePermissions) provided as a parameter. |
| [update](update.md) | `fun update(sitePermissions: `[`SitePermissions`](../-site-permissions/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Replaces an existing SitePermissions with the values of [sitePermissions](update.md#mozilla.components.feature.sitepermissions.SitePermissionsStorage$update(mozilla.components.feature.sitepermissions.SitePermissions)/sitePermissions) provided as a parameter. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
