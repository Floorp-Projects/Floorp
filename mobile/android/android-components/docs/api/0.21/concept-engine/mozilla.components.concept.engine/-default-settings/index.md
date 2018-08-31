---
title: DefaultSettings - 
---

[mozilla.components.concept.engine](../index.html) / [DefaultSettings](./index.html)

# DefaultSettings

`data class DefaultSettings : `[`Settings`](../-settings/index.html)

[Settings](../-settings/index.html) implementation used to set defaults for [Engine](../-engine/index.html) and [EngineSession](../-engine-session/index.html).

### Constructors

| [&lt;init&gt;](-init-.html) | `DefaultSettings(javascriptEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, domStorageEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, trackingProtectionPolicy: `[`TrackingProtectionPolicy`](../-engine-session/-tracking-protection-policy/index.html)`? = null, requestInterceptor: `[`RequestInterceptor`](../../mozilla.components.concept.engine.request/-request-interceptor/index.html)`? = null)`<br>[Settings](../-settings/index.html) implementation used to set defaults for [Engine](../-engine/index.html) and [EngineSession](../-engine-session/index.html). |

### Properties

| [domStorageEnabled](dom-storage-enabled.html) | `var domStorageEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Setting to control whether or not DOM Storage is enabled. |
| [javascriptEnabled](javascript-enabled.html) | `var javascriptEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Setting to control whether or not JavaScript is enabled. |
| [requestInterceptor](request-interceptor.html) | `var requestInterceptor: `[`RequestInterceptor`](../../mozilla.components.concept.engine.request/-request-interceptor/index.html)`?`<br>Setting to intercept and override requests. |
| [trackingProtectionPolicy](tracking-protection-policy.html) | `var trackingProtectionPolicy: `[`TrackingProtectionPolicy`](../-engine-session/-tracking-protection-policy/index.html)`?`<br>Setting to control tracking protection. |

