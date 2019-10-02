[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [Engine](../index.md) / [BrowsingData](./index.md)

# BrowsingData

`class BrowsingData` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L25)

Describes a combination of browsing data types stored by the engine.

### Properties

| Name | Summary |
|---|---|
| [types](types.md) | `val types: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Functions

| Name | Summary |
|---|---|
| [contains](contains.md) | `fun contains(type: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ALL](-a-l-l.md) | `const val ALL: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [ALL_CACHES](-a-l-l_-c-a-c-h-e-s.md) | `const val ALL_CACHES: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [ALL_SITE_DATA](-a-l-l_-s-i-t-e_-d-a-t-a.md) | `const val ALL_SITE_DATA: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [ALL_SITE_SETTINGS](-a-l-l_-s-i-t-e_-s-e-t-t-i-n-g-s.md) | `const val ALL_SITE_SETTINGS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [AUTH_SESSIONS](-a-u-t-h_-s-e-s-s-i-o-n-s.md) | `const val AUTH_SESSIONS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [COOKIES](-c-o-o-k-i-e-s.md) | `const val COOKIES: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [DOM_STORAGES](-d-o-m_-s-t-o-r-a-g-e-s.md) | `const val DOM_STORAGES: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [IMAGE_CACHE](-i-m-a-g-e_-c-a-c-h-e.md) | `const val IMAGE_CACHE: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [NETWORK_CACHE](-n-e-t-w-o-r-k_-c-a-c-h-e.md) | `const val NETWORK_CACHE: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [PERMISSIONS](-p-e-r-m-i-s-s-i-o-n-s.md) | `const val PERMISSIONS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [all](all.md) | `fun all(): `[`BrowsingData`](./index.md) |
| [allCaches](all-caches.md) | `fun allCaches(): `[`BrowsingData`](./index.md) |
| [allSiteData](all-site-data.md) | `fun allSiteData(): `[`BrowsingData`](./index.md) |
| [allSiteSettings](all-site-settings.md) | `fun allSiteSettings(): `[`BrowsingData`](./index.md) |
| [select](select.md) | `fun select(vararg types: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`BrowsingData`](./index.md) |
