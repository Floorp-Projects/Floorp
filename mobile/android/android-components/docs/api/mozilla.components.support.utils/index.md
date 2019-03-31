[android-components](../index.md) / [mozilla.components.support.utils](./index.md)

## Package mozilla.components.support.utils

### Types

| Name | Summary |
|---|---|
| [Browsers](-browsers/index.md) | `class Browsers`<br>Helpful tools for dealing with other browsers on this device. |
| [ColorUtils](-color-utils/index.md) | `object ColorUtils` |
| [DomainMatch](-domain-match/index.md) | `data class DomainMatch` |
| [DownloadUtils](-download-utils/index.md) | `object DownloadUtils` |
| [DrawableUtils](-drawable-utils/index.md) | `object DrawableUtils` |
| [SafeBundle](-safe-bundle/index.md) | `class SafeBundle`<br>See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles, and we need to handle those defensively too. |
| [SafeIntent](-safe-intent/index.md) | `class SafeIntent`<br>External applications can pass values into Intents that can cause us to crash: in defense, we wrap [Intent](https://developer.android.com/reference/android/content/Intent.html) and catch the exceptions they may force us to throw. See bug 1090385 for more. |
| [StatusBarUtils](-status-bar-utils/index.md) | `object StatusBarUtils` |
| [ThreadUtils](-thread-utils/index.md) | `object ThreadUtils` |
| [WebURLFinder](-web-u-r-l-finder/index.md) | `class WebURLFinder`<br>Regular expressions used in this class are taken from Android's Patterns.java. We brought them in to standardize URL matching across Android versions, instead of relying on Android version-dependent built-ins that can vary across Android versions. The original code can be found here: http://androidxref.com/8.0.0_r4/xref/frameworks/base/core/java/android/util/Patterns.java |

### Extensions for External Classes

| Name | Summary |
|---|---|
| [android.content.Intent](android.content.-intent/index.md) |  |

### Functions

| Name | Summary |
|---|---|
| [segmentAwareDomainMatch](segment-aware-domain-match.md) | `fun segmentAwareDomainMatch(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, urls: `[`Iterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-iterable/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`DomainMatch`](-domain-match/index.md)`?` |
