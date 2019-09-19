/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.launch
import org.mozilla.geckoview.GeckoResult
import kotlin.coroutines.CoroutineContext
import kotlin.coroutines.EmptyCoroutineContext
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlin.coroutines.suspendCoroutine

/**
 * Wait for a GeckoResult to be complete in a co-routine.
 */
suspend fun <T> GeckoResult<T>.await() = suspendCoroutine<T?> { continuation ->
    then({
        continuation.resume(it)
        GeckoResult<Void>()
    }, {
        continuation.resumeWithException(it)
        GeckoResult<Void>()
    })
}

/**
 * Create a GeckoResult from a co-routine.
 */
@Suppress("TooGenericExceptionCaught")
fun <T> CoroutineScope.launchGeckoResult(
    context: CoroutineContext = EmptyCoroutineContext,
    start: CoroutineStart = CoroutineStart.DEFAULT,
    block: suspend CoroutineScope.() -> T
) = GeckoResult<T>().apply {
    launch(context, start) {
        try {
            val value = block()
            complete(value)
        } catch (exception: Throwable) {
            completeExceptionally(exception)
        }
    }
}
