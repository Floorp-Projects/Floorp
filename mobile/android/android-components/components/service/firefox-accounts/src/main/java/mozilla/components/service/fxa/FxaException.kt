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
        fun fromConsuming(e: Error): FxaException {
            val message = e.consumeMessage() ?: ""
            return when (e.code) {
                FxaClient.ErrorCode.AuthenticationError.ordinal -> Unauthorized(message)
                FxaClient.ErrorCode.InternalPanic.ordinal -> Panic(message)
                else -> Unspecified(message)
            }
        }
    }
}
