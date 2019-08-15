[android-components](../../../../index.md) / [mozilla.components.concept.engine](../../../index.md) / [EngineSession](../../index.md) / [TrackingProtectionPolicy](../index.md) / [CookiePolicy](./index.md)

# CookiePolicy

`enum class CookiePolicy` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L109)

Indicates how cookies should behave for a given [TrackingProtectionPolicy](../index.md).
The ids of each cookiePolicy is aligned with the GeckoView @CookieBehavior constants.

### Enum Values

| Name | Summary |
|---|---|
| [ACCEPT_ALL](-a-c-c-e-p-t_-a-l-l.md) | Accept first-party and third-party cookies and site data. |
| [ACCEPT_ONLY_FIRST_PARTY](-a-c-c-e-p-t_-o-n-l-y_-f-i-r-s-t_-p-a-r-t-y.md) | Accept only first-party cookies and site data to block cookies which are not associated with the domain of the visited site. |
| [ACCEPT_NONE](-a-c-c-e-p-t_-n-o-n-e.md) | Do not store any cookies and site data. |
| [ACCEPT_VISITED](-a-c-c-e-p-t_-v-i-s-i-t-e-d.md) | Accept first-party and third-party cookies and site data only from sites previously visited in a first-party context. |
| [ACCEPT_NON_TRACKERS](-a-c-c-e-p-t_-n-o-n_-t-r-a-c-k-e-r-s.md) | Accept only first-party and non-tracking third-party cookies and site data to block cookies which are not associated with the domain of the visited site set by known trackers. |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
