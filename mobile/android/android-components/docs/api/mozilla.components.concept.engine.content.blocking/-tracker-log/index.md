[android-components](../../index.md) / [mozilla.components.concept.engine.content.blocking](../index.md) / [TrackerLog](./index.md)

# TrackerLog

`data class TrackerLog` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/content/blocking/TrackerLog.kt#L15)

Represents a blocked content tracker.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TrackerLog(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, loadedCategories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackingCategory`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/-tracking-category/index.md)`> = emptyList(), blockedCategories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackingCategory`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/-tracking-category/index.md)`> = emptyList(), cookiesHasBeenBlocked: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Represents a blocked content tracker. |

### Properties

| Name | Summary |
|---|---|
| [blockedCategories](blocked-categories.md) | `val blockedCategories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackingCategory`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/-tracking-category/index.md)`>`<br>A list of tracking categories blocked for this tracker. |
| [cookiesHasBeenBlocked](cookies-has-been-blocked.md) | `val cookiesHasBeenBlocked: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [loadedCategories](loaded-categories.md) | `val loadedCategories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackingCategory`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/-tracking-category/index.md)`>`<br>A list of tracking categories loaded for this tracker. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL of the tracker. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
