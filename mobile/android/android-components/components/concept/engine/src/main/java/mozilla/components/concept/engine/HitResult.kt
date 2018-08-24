package mozilla.components.concept.engine

/**
 * Represents all the different supported types of data that can be found from long clicking
 * an element.
 */
@Suppress("ClassNaming", "ClassName")
sealed class HitResult(val src: String) {
    /**
     * Default type if we're unable to match the type to anything. It may or may not have a src.
     */
    class UNKNOWN(src: String) : HitResult(src)

    /**
     * If the HTML element was of type 'HTMLImageElement'.
     */
    class IMAGE(src: String) : HitResult(src)

    /**
     * If the HTML element was of type 'HTMLVideoElement'.
     */
    class VIDEO(src: String) : HitResult(src)

    /**
     * If the HTML element was of type 'HTMLAudioElement'.
     */
    class AUDIO(src: String) : HitResult(src)

    /**
     * If the HTML element was of type 'HTMLImageElement' and contained a URI.
     */
    class IMAGE_SRC(src: String, val uri: String) : HitResult(src)

    /**
     * The type used if the URI is prepended with 'tel:'.
     */
    class PHONE(src: String) : HitResult(src)

    /**
     * The type used if the URI is prepended with 'mailto:'.
     */
    class EMAIL(src: String) : HitResult(src)

    /**
     * The type used if the URI is prepended with 'geo:'.
     */
    class GEO(src: String) : HitResult(src)
}
