---
title: mozilla.components.support.utils - 
---

[mozilla.components.support.utils](./index.html)

## Package mozilla.components.support.utils

### Types

| [ColorUtils](-color-utils/index.html) | `object ColorUtils` |
| [DownloadUtils](-download-utils/index.html) | `object DownloadUtils` |
| [DrawableUtils](-drawable-utils/index.html) | `object DrawableUtils` |
| [SafeBundle](-safe-bundle/index.html) | `class SafeBundle`<br>See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles, and we need to handle those defensively too. |
| [SafeIntent](-safe-intent/index.html) | `class SafeIntent`<br>External applications can pass values into Intents that can cause us to crash: in defense, we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385 for more. |
| [StatusBarUtils](-status-bar-utils/index.html) | `object StatusBarUtils` |
| [ThreadUtils](-thread-utils/index.html) | `object ThreadUtils` |
| [WebURLFinder](-web-u-r-l-finder/index.html) | `class WebURLFinder`<br>Regular expressions used in this class are taken from Android's Patterns.java. We brought them in to standardize URL matching across Android versions, instead of relying on Android version-dependent built-ins that can vary across Android versions. The original code can be found here: http://androidxref.com/8.0.0_r4/xref/frameworks/base/core/java/android/util/Patterns.java |

