[android-components](../../../index.md) / [mozilla.components.browser.state.state.content](../../index.md) / [DownloadState](../index.md) / [Status](./index.md)

# Status

`enum class Status` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/content/DownloadState.kt#L52)

Status that represents every state that a download can be in.

### Enum Values

| Name | Summary |
|---|---|
| [INITIATED](-i-n-i-t-i-a-t-e-d.md) | Indicates that the download is in the first state after creation but not yet [DOWNLOADING](-d-o-w-n-l-o-a-d-i-n-g.md). |
| [DOWNLOADING](-d-o-w-n-l-o-a-d-i-n-g.md) | Indicates that an [INITIATED](-i-n-i-t-i-a-t-e-d.md) download is now actively being downloaded. |
| [PAUSED](-p-a-u-s-e-d.md) | Indicates that the download that has been [DOWNLOADING](-d-o-w-n-l-o-a-d-i-n-g.md) has been paused. |
| [CANCELLED](-c-a-n-c-e-l-l-e-d.md) | Indicates that the download that has been [DOWNLOADING](-d-o-w-n-l-o-a-d-i-n-g.md) has been cancelled. |
| [FAILED](-f-a-i-l-e-d.md) | Indicates that the download that has been [DOWNLOADING](-d-o-w-n-l-o-a-d-i-n-g.md) has moved to failed because something unexpected has happened. |
| [COMPLETED](-c-o-m-p-l-e-t-e-d.md) | Indicates that the [DOWNLOADING](-d-o-w-n-l-o-a-d-i-n-g.md) download has been completed. |
