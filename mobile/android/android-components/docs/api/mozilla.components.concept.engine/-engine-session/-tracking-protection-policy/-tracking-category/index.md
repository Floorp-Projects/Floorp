[android-components](../../../../index.md) / [mozilla.components.concept.engine](../../../index.md) / [EngineSession](../../index.md) / [TrackingProtectionPolicy](../index.md) / [TrackingCategory](./index.md)

# TrackingCategory

`enum class TrackingCategory` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L141)

### Enum Values

| Name | Summary |
|---|---|
| [NONE](-n-o-n-e.md) |  |
| [AD](-a-d.md) | Blocks advertisement trackers. |
| [ANALYTICS](-a-n-a-l-y-t-i-c-s.md) | Blocks analytics trackers. |
| [SOCIAL](-s-o-c-i-a-l.md) | Blocks social trackers. |
| [CONTENT](-c-o-n-t-e-n-t.md) | Blocks content trackers. May cause issues with some web sites. |
| [TEST](-t-e-s-t.md) |  |
| [CRYPTOMINING](-c-r-y-p-t-o-m-i-n-i-n-g.md) | Blocks cryptocurrency miners. |
| [FINGERPRINTING](-f-i-n-g-e-r-p-r-i-n-t-i-n-g.md) | Blocks fingerprinting trackers. |
| [RECOMMENDED](-r-e-c-o-m-m-e-n-d-e-d.md) |  |
| [STRICT](-s-t-r-i-c-t.md) | Combining the [RECOMMENDED](-r-e-c-o-m-m-e-n-d-e-d.md) categories plus [CRYPTOMINING](-c-r-y-p-t-o-m-i-n-i-n-g.md), [FINGERPRINTING](-f-i-n-g-e-r-p-r-i-n-t-i-n-g.md) and [CONTENT](-c-o-n-t-e-n-t.md). |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
