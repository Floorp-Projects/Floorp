[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [HitResult](./index.md)

# HitResult

`sealed class HitResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/HitResult.kt#L12)

Represents all the different supported types of data that can be found from long clicking
an element.

### Types

| Name | Summary |
|---|---|
| [AUDIO](-a-u-d-i-o/index.md) | `data class AUDIO : `[`HitResult`](./index.md)<br>If the HTML element was of type 'HTMLAudioElement'. |
| [EMAIL](-e-m-a-i-l/index.md) | `data class EMAIL : `[`HitResult`](./index.md)<br>The type used if the URI is prepended with 'mailto:'. |
| [GEO](-g-e-o/index.md) | `data class GEO : `[`HitResult`](./index.md)<br>The type used if the URI is prepended with 'geo:'. |
| [IMAGE](-i-m-a-g-e/index.md) | `data class IMAGE : `[`HitResult`](./index.md)<br>If the HTML element was of type 'HTMLImageElement'. |
| [IMAGE_SRC](-i-m-a-g-e_-s-r-c/index.md) | `data class IMAGE_SRC : `[`HitResult`](./index.md)<br>If the HTML element was of type 'HTMLImageElement' and contained a URI. |
| [PHONE](-p-h-o-n-e/index.md) | `data class PHONE : `[`HitResult`](./index.md)<br>The type used if the URI is prepended with 'tel:'. |
| [UNKNOWN](-u-n-k-n-o-w-n/index.md) | `data class UNKNOWN : `[`HitResult`](./index.md)<br>Default type if we're unable to match the type to anything. It may or may not have a src. |
| [VIDEO](-v-i-d-e-o/index.md) | `data class VIDEO : `[`HitResult`](./index.md)<br>If the HTML element was of type 'HTMLVideoElement'. |

### Properties

| Name | Summary |
|---|---|
| [src](src.md) | `open val src: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [AUDIO](-a-u-d-i-o/index.md) | `data class AUDIO : `[`HitResult`](./index.md)<br>If the HTML element was of type 'HTMLAudioElement'. |
| [EMAIL](-e-m-a-i-l/index.md) | `data class EMAIL : `[`HitResult`](./index.md)<br>The type used if the URI is prepended with 'mailto:'. |
| [GEO](-g-e-o/index.md) | `data class GEO : `[`HitResult`](./index.md)<br>The type used if the URI is prepended with 'geo:'. |
| [IMAGE](-i-m-a-g-e/index.md) | `data class IMAGE : `[`HitResult`](./index.md)<br>If the HTML element was of type 'HTMLImageElement'. |
| [IMAGE_SRC](-i-m-a-g-e_-s-r-c/index.md) | `data class IMAGE_SRC : `[`HitResult`](./index.md)<br>If the HTML element was of type 'HTMLImageElement' and contained a URI. |
| [PHONE](-p-h-o-n-e/index.md) | `data class PHONE : `[`HitResult`](./index.md)<br>The type used if the URI is prepended with 'tel:'. |
| [UNKNOWN](-u-n-k-n-o-w-n/index.md) | `data class UNKNOWN : `[`HitResult`](./index.md)<br>Default type if we're unable to match the type to anything. It may or may not have a src. |
| [VIDEO](-v-i-d-e-o/index.md) | `data class VIDEO : `[`HitResult`](./index.md)<br>If the HTML element was of type 'HTMLVideoElement'. |
