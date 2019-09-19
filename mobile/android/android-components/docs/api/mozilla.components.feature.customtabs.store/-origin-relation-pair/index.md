[android-components](../../index.md) / [mozilla.components.feature.customtabs.store](../index.md) / [OriginRelationPair](./index.md)

# OriginRelationPair

`data class OriginRelationPair` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/store/CustomTabsServiceState.kt#L42)

Pair of origin and relation type used as key in [CustomTabState.relationships](../-custom-tab-state/relationships.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `OriginRelationPair(origin: <ERROR CLASS>, relation: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Pair of origin and relation type used as key in [CustomTabState.relationships](../-custom-tab-state/relationships.md). |

### Properties

| Name | Summary |
|---|---|
| [origin](origin.md) | `val origin: <ERROR CLASS>`<br>URL that contains only the scheme, host, and port. https://html.spec.whatwg.org/multipage/origin.html#concept-origin |
| [relation](relation.md) | `val relation: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Enum that indicates the relation type. |
