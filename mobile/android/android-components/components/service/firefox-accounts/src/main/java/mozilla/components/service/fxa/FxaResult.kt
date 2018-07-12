package mozilla.components.service.fxa

import java.util.ArrayList

/**
 * FxaResult is a class that represents an asynchronous result.
 *
 * @param <T> The type of the value delivered via the FxaResult.
 */
class FxaResult<T> {

    private var mComplete: Boolean = false
    private var mValue: T? = null
    private var mError: Exception? = null

    private val mListeners: ArrayList<Listener<T>> = ArrayList()

    private interface Listener<T> {
        fun onValue(value: T)

        fun onException(exception: Exception)
    }

    /**
     * Completes this result based on another result.
     *
     * @param other The result that this result should mirror
     */
    private fun completeFrom(other: FxaResult<T>?) {
        if (other == null) {
            return
        }

        other.then(object : OnValueListener<T, Void> {
            override fun onValue(value: T): FxaResult<Void>? {
                complete(value)
                return null
            }
        }, object : OnExceptionListener<Void> {
            override fun onException(exception: Exception): FxaResult<Void>? {
                completeExceptionally(exception)
                return null
            }
        })
    }

    /**
     * Adds a value listener to be called when the [FxaResult] is completed with
     * a value. Listeners will be invoked on the same thread in which the
     * [FxaResult] was completed.
     *
     * @param fn A lambda expression with the same method signature as [OnValueListener],
     * called when the [FxaResult] is completed with a value.
     */
    fun <U> then(fn: (value: T) -> FxaResult<U>?): FxaResult<U> {
        val listener = object : OnValueListener<T, U> {
            override fun onValue(value: T): FxaResult<U>? = fn(value)
        }
        return then(listener, null)
    }

    /**
     * Adds listeners to be called when the [FxaResult] is completed either with
     * a value or [Exception]. Listeners will be invoked on the same thread in which the
     * [FxaResult] was completed.
     *
     * @param vfn A lambda expression with the same method signature as [OnValueListener],
     * called when the [FxaResult] is completed with a value.
     * @param efn A lambda expression with the same method signature as [OnExceptionListener],
     * called when the [FxaResult] is completed with an exception.
     */
    fun <U> then(vfn: (value: T) -> FxaResult<U>?, efn: (exception: Exception) -> FxaResult<U>?): FxaResult<U> {
        val valueListener = object : OnValueListener<T, U> {
            override fun onValue(value: T): FxaResult<U>? = vfn(value)
        }

        val exceptionListener = object : OnExceptionListener<U> {
            override fun onException(exception: Exception): FxaResult<U>? = efn(exception)
        }
        return then(valueListener, exceptionListener)
    }

    /**
     * Adds listeners to be called when the [FxaResult] is completed either with
     * a value or [Exception]. Listeners will be invoked on the same thread in which the
     * [FxaResult] was completed.
     *
     * @param valueListener An instance of [OnValueListener], called when the
     * [FxaResult] is completed with a value.
     * @param exceptionListener An instance of [OnExceptionListener], called when the
     * [FxaResult] is completed with an [Exception].
     */
    @Synchronized
    @Suppress("ComplexMethod", "TooGenericExceptionCaught")
    fun <U> then(valueListener: OnValueListener<T, U>, exceptionListener: OnExceptionListener<U>?): FxaResult<U> {
        val result = FxaResult<U>()
        val listener = object : Listener<T> {
            override fun onValue(value: T) {
                try {
                    result.completeFrom(valueListener.onValue(value))
                } catch (ex: Exception) {
                    result.completeFrom(FxaResult.fromException(ex))
                }
            }

            override fun onException(exception: Exception) {
                if (exceptionListener == null) {
                    // Do not swallow thrown exceptions if a listener is not present
                    throw exception
                }

                result.completeFrom(exceptionListener.onException(exception))
            }
        }

        if (mComplete) {
            mError?.let { listener.onException(it) }
            mValue?.let { listener.onValue(it) }
        } else {
            mListeners.add(listener)
        }

        return result
    }

    /**
     * Adds a value listener to be called when the [FxaResult] and the whole chain of [then]
     * calls is completed with a value. Listeners will be invoked on the same thread in
     * which the [FxaResult] was completed.
     *
     * @param fn A lambda expression with the same method signature as [OnValueListener],
     * called when the [FxaResult] is completed with a value.
     */
    fun whenComplete(fn: (value: T) -> Unit) {
        val listener = object : OnValueListener<T, Void> {
            override fun onValue(value: T): FxaResult<Void>? {
                fn(value)
                return FxaResult()
            }
        }
        then(listener, null)
    }

    /**
     * This completes the result with the specified value. IllegalStateException is thrown
     * if the result is already complete.
     *
     * @param value The value used to complete the result.
     * @throws IllegalStateException
     */
    @Synchronized
    fun complete(value: T) {
        if (mComplete) {
            throw IllegalStateException("result is already complete")
        }

        mValue = value
        mComplete = true

        ArrayList(mListeners).forEach { it.onValue(value) }
    }

    /**
     * This completes the result with the specified [Exception]. IllegalStateException is thrown
     * if the result is already complete.
     *
     * @param exception The [Exception] used to complete the result.
     * @throws IllegalStateException
     */
    @Synchronized
    fun completeExceptionally(exception: Exception) {
        if (mComplete) {
            throw IllegalStateException("result is already complete")
        }

        mError = exception
        mComplete = true

        ArrayList(mListeners).forEach { it.onException(exception) }
    }

    /**
     * An interface used to deliver values to listeners of a [FxaResult]
     *
     * @param <T> This is the type of the value delivered via [.onValue]
     * @param <U> This is the type of the value for the result returned from [.onValue]
     */
    @FunctionalInterface
    interface OnValueListener<T, U> {
        /**
         * Called when a [FxaResult] is completed with a value. This will be
         * called on the same thread in which the result was completed.
         *
         * @param value The value of the [FxaResult]
         * @return A new [FxaResult], used for chaining results together.
         * May be null.
         */
        fun onValue(value: T): FxaResult<U>?
    }

    /**
     * An interface used to deliver exceptions to listeners of a [FxaResult]
     *
     * @param <V> This is the type of the vale for the result returned from [.onException]
     */
    @FunctionalInterface
    interface OnExceptionListener<V> {
        fun onException(exception: Exception): FxaResult<V>?
    }

    companion object {
        /**
         * This constructs a result that is fulfilled with the specified value.
         *
         * @param value The value used to complete the newly created result.
         * @return The completed [FxaResult]
         */
        fun <U> fromValue(value: U): FxaResult<U> {
            val result = FxaResult<U>()
            result.complete(value)
            return result
        }

        /**
         * This constructs a result that is completed with the specified [Exception].
         * May not be null.
         *
         * @param exception The exception used to complete the newly created result.
         * @return The completed [FxaResult]
         */
        fun <T> fromException(exception: Exception): FxaResult<T> {
            val result = FxaResult<T>()
            result.completeExceptionally(exception)
            return result
        }
    }
}
