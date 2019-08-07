[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [SiteTrackingProtection](./index.md)

# SiteTrackingProtection

`enum class SiteTrackingProtection` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L368)

Indicates which tracking protection status a site has.

### Enum Values

| Name | Summary |
|---|---|
| [ON_NO_TRACKERS_BLOCKED](-o-n_-n-o_-t-r-a-c-k-e-r-s_-b-l-o-c-k-e-d.md) | The site has tracking protection enabled, but none trackers have been blocked or detected. |
| [ON_TRACKERS_BLOCKED](-o-n_-t-r-a-c-k-e-r-s_-b-l-o-c-k-e-d.md) | The site has tracking protection enabled, and trackers have been blocked or detected. |
| [OFF_FOR_A_SITE](-o-f-f_-f-o-r_-a_-s-i-t-e.md) | Tracking protection has been disabled for a specific site. |
| [OFF_GLOBALLY](-o-f-f_-g-l-o-b-a-l-l-y.md) | Tracking protection has been disabled for all sites. |
