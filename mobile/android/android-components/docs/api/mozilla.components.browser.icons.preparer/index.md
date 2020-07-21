[android-components](../index.md) / [mozilla.components.browser.icons.preparer](./index.md)

## Package mozilla.components.browser.icons.preparer

### Types

| Name | Summary |
|---|---|
| [DiskIconPreparer](-disk-icon-preparer/index.md) | `class DiskIconPreparer : `[`IconPreprarer`](-icon-preprarer/index.md)<br>[IconPreprarer](-icon-preprarer/index.md) implementation implementation that will add known resource URLs (from a disk cache) to the request if the request doesn't contain a list of resources yet. |
| [IconPreprarer](-icon-preprarer/index.md) | `interface IconPreprarer`<br>An [IconPreparer](#) implementation receives an [IconRequest](../mozilla.components.browser.icons/-icon-request/index.md) before it is getting loaded. The preparer has the option to rewrite the [IconRequest](../mozilla.components.browser.icons/-icon-request/index.md) and return a new instance. |
| [MemoryIconPreparer](-memory-icon-preparer/index.md) | `class MemoryIconPreparer : `[`IconPreprarer`](-icon-preprarer/index.md)<br>An [IconPreprarer](-icon-preprarer/index.md) implementation that will add known resource URLs (from an in-memory cache) to the request if the request doesn't contain a list of resources yet. |
| [TippyTopIconPreparer](-tippy-top-icon-preparer/index.md) | `class TippyTopIconPreparer : `[`IconPreprarer`](-icon-preprarer/index.md)<br>[IconPreprarer](-icon-preprarer/index.md) implementation that looks up the host in our "tippy top" list. If it can find a match then it inserts the icon URL into the request. |
