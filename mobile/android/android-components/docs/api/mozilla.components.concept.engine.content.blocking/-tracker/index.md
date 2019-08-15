[android-components](../../index.md) / [mozilla.components.concept.engine.content.blocking](../index.md) / [Tracker](./index.md)

# Tracker

`class Tracker` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/content/blocking/Tracker.kt#L16)

Represents a blocked content tracker.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Tracker(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, trackingCategories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackingCategory`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/-tracking-category/index.md)`> = emptyList(), cookiePolicies: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CookiePolicy`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/-cookie-policy/index.md)`> = emptyList())`<br>Represents a blocked content tracker. |

### Properties

| Name | Summary |
|---|---|
| [cookiePolicies](cookie-policies.md) | `val cookiePolicies: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CookiePolicy`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/-cookie-policy/index.md)`>`<br>The cookie types of the blocked resource. |
| [trackingCategories](tracking-categories.md) | `val trackingCategories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackingCategory`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/-tracking-category/index.md)`>`<br>The anti-tracking category types of the blocked resource. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL of the tracker. |
