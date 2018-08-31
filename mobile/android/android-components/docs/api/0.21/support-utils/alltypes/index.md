---
title: alltypes - 
---

### All Types

| [mozilla.components.support.utils.ColorUtils](../mozilla.components.support.utils/-color-utils/index.html) |  |
| [mozilla.components.support.utils.DownloadUtils](../mozilla.components.support.utils/-download-utils/index.html) |  |
| [mozilla.components.support.utils.DrawableUtils](../mozilla.components.support.utils/-drawable-utils/index.html) |  |
| [mozilla.components.support.utils.SafeBundle](../mozilla.components.support.utils/-safe-bundle/index.html) | See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles, and we need to handle those defensively too. |
| [mozilla.components.support.utils.SafeIntent](../mozilla.components.support.utils/-safe-intent/index.html) | External applications can pass values into Intents that can cause us to crash: in defense, we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385 for more. |
| [mozilla.components.support.utils.StatusBarUtils](../mozilla.components.support.utils/-status-bar-utils/index.html) |  |
| [mozilla.components.support.utils.ThreadUtils](../mozilla.components.support.utils/-thread-utils/index.html) |  |
| [mozilla.components.support.utils.WebURLFinder](../mozilla.components.support.utils/-web-u-r-l-finder/index.html) | Regular expressions used in this class are taken from Android's Patterns.java. We brought them in to standardize URL matching across Android versions, instead of relying on Android version-dependent built-ins that can vary across Android versions. The original code can be found here: http://androidxref.com/8.0.0_r4/xref/frameworks/base/core/java/android/util/Patterns.java |

