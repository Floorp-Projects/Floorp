[android-components](../../../index.md) / [mozilla.components.browser.toolbar.display](../../index.md) / [DisplayToolbar](../index.md) / [Icons](./index.md)

# Icons

`data class Icons` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/display/DisplayToolbar.kt#L122)

Data class holding the customizable icons in "display mode".

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Icons(emptyIcon: <ERROR CLASS>?, trackingProtectionTrackersBlocked: <ERROR CLASS>, trackingProtectionNothingBlocked: <ERROR CLASS>, trackingProtectionException: <ERROR CLASS>)`<br>Data class holding the customizable icons in "display mode". |

### Properties

| Name | Summary |
|---|---|
| [emptyIcon](empty-icon.md) | `val emptyIcon: <ERROR CLASS>?`<br>An icon that is shown in front of the URL if the URL is empty. |
| [trackingProtectionException](tracking-protection-exception.md) | `val trackingProtectionException: <ERROR CLASS>`<br>An icon that is shown if tracking protection is enabled but the current page is in the "exception list". |
| [trackingProtectionNothingBlocked](tracking-protection-nothing-blocked.md) | `val trackingProtectionNothingBlocked: <ERROR CLASS>`<br>An icon that is shown if tracking protection is enabled and no trackers have been blocked. |
| [trackingProtectionTrackersBlocked](tracking-protection-trackers-blocked.md) | `val trackingProtectionTrackersBlocked: <ERROR CLASS>`<br>An icon that is shown if tracking protection is enabled and trackers have been blocked. |
