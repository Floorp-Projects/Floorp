[android-components](../../index.md) / [mozilla.components.feature.customtabs.store](../index.md) / [CustomTabState](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`CustomTabState(creatorPackageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, relationships: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`OriginRelationPair`](../-origin-relation-pair/index.md)`, `[`VerificationStatus`](../-verification-status/index.md)`> = emptyMap())`

Value type that represents the state of a single custom tab
accessible from both the service and activity.

This data is meant to supplement [mozilla.components.browser.session.tab.CustomTabConfig](#),
not replace it. It only contains data that the service also needs to work with.

