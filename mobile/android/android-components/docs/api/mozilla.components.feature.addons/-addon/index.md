[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [Addon](./index.md)

# Addon

`data class Addon` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/Addon.kt#L35)

Represents an add-on based on the AMO store:
https://addons.mozilla.org/en-US/firefox/

### Types

| Name | Summary |
|---|---|
| [Author](-author/index.md) | `data class Author`<br>Represents an add-on author. |
| [InstalledState](-installed-state/index.md) | `data class InstalledState`<br>Returns a list of id resources per each item on the [Addon.permissions](permissions.md) list. Holds the state of the installed web extension of this add-on. |
| [Rating](-rating/index.md) | `data class Rating`<br>Holds all the rating information of this add-on. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Addon(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, authors: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Author`](-author/index.md)`> = emptyList(), categories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyList(), downloadUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", version: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyList(), translatableName: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyMap(), translatableDescription: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyMap(), translatableSummary: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyMap(), iconUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", siteUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", rating: `[`Rating`](-rating/index.md)`? = null, createdAt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", updatedAt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", installedState: `[`InstalledState`](-installed-state/index.md)`? = null)`<br>Represents an add-on based on the AMO store: https://addons.mozilla.org/en-US/firefox/ |

### Properties

| Name | Summary |
|---|---|
| [authors](authors.md) | `val authors: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Author`](-author/index.md)`>`<br>List holding information about the add-on authors. |
| [categories](categories.md) | `val categories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>List of categories the add-on belongs to. |
| [createdAt](created-at.md) | `val createdAt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The date the add-on was created. |
| [downloadUrl](download-url.md) | `val downloadUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The (absolute) URL to download the add-on file (eg xpi). |
| [iconUrl](icon-url.md) | `val iconUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL to icon for the add-on. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The unique ID of this add-on. |
| [installedState](installed-state.md) | `val installedState: `[`InstalledState`](-installed-state/index.md)`?`<br>Holds the state of the installed web extension for this add-on. Null, if the [Addon](./index.md) is not installed. |
| [permissions](permissions.md) | `val permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>List of the add-on permissions for this File. |
| [rating](rating.md) | `val rating: `[`Rating`](-rating/index.md)`?`<br>The rating information of this add-on. |
| [siteUrl](site-url.md) | `val siteUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The (absolute) add-on detail URL. |
| [translatableDescription](translatable-description.md) | `val translatableDescription: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>A map containing the different translations for the add-on description, where the key is the language and the value is the actual translated text. |
| [translatableName](translatable-name.md) | `val translatableName: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>A map containing the different translations for the add-on name, where the key is the language and the value is the actual translated text. |
| [translatableSummary](translatable-summary.md) | `val translatableSummary: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>A map containing the different translations for the add-on name, where the key is the language and the value is the actual translated text. |
| [updatedAt](updated-at.md) | `val updatedAt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The date of the last time the add-on was updated by its developer(s). |
| [version](version.md) | `val version: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The add-on version e.g "1.23.0". |

### Functions

| Name | Summary |
|---|---|
| [filterTranslations](filter-translations.md) | `fun filterTranslations(locales: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Addon`](./index.md)<br>Returns a copy of this [Addon](./index.md) containing only translations (description, name, summary) of the provided locales. All other translations will be removed. |
| [isDisabledAsUnsupported](is-disabled-as-unsupported.md) | `fun isDisabledAsUnsupported(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this [Addon](./index.md) is currently disabled because it is not supported. This is based on the installed extension state in the engine. An addon can be disabled as unsupported and later become supported, so both [isSupported](is-supported.md) and [isDisabledAsUnsupported](is-disabled-as-unsupported.md) can be true. |
| [isEnabled](is-enabled.md) | `fun isEnabled(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this [Addon](./index.md) is currently enabled. |
| [isInstalled](is-installed.md) | `fun isInstalled(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this [Addon](./index.md) is currently installed. |
| [isSupported](is-supported.md) | `fun isSupported(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this [Addon](./index.md) is currently supported by the browser. |
| [translatePermissions](translate-permissions.md) | `fun translatePermissions(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`>`<br>Returns a list of id resources per each item on the [permissions](permissions.md) list. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [permissionToTranslation](permission-to-translation.md) | `val permissionToTranslation: <ERROR CLASS>`<br>A map of permissions to translation string ids. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [localizePermissions](localize-permissions.md) | `fun localizePermissions(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`>`<br>Takes a list of [permissions](localize-permissions.md#mozilla.components.feature.addons.Addon.Companion$localizePermissions(kotlin.collections.List((kotlin.String)))/permissions) and returns a list of id resources per each item. |

### Extension Properties

| Name | Summary |
|---|---|
| [translatedName](../../mozilla.components.feature.addons.ui/translated-name.md) | `val `[`Addon`](./index.md)`.translatedName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A shortcut to get the localized name of an add-on. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
