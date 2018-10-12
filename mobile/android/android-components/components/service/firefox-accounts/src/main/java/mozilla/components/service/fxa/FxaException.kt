package mozilla.components.service.fxa

/**
 * Wrapper class for the exceptions thrown in the Rust library, which ensures that the
 * error messages will be consumed and freed properly in Rust.
 */
open class FxaException(message: String) : Exception(message) {
    class Unspecified(msg: String) : FxaException(msg)
    class Unauthorized(msg: String) : FxaException(msg)
    class Panic(msg: String) : FxaException(msg)

    companion object {
        // These numbers come from `ffi::error_codes` in the fxa-client rust code.
        const val CODE_UNAUTHORIZED: Int = 2
        const val CODE_PANIC: Int = -1
        fun fromConsuming(e: Error): FxaException {
            val message = e.consumeMessage() ?: ""
            return when (e.code) {
                CODE_UNAUTHORIZED -> Unauthorized(message)
                CODE_PANIC -> Panic(message)
                else -> Unspecified(message)
            }
        }
    }
}
