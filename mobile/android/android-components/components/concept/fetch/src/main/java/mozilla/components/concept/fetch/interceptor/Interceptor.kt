/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch.interceptor

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response

/**
 * An [Interceptor] for a [Client] implementation.
 *
 * Interceptors can monitor, modify, retry, redirect or record requests as well as responses going through a [Client].
 */
interface Interceptor {
    /**
     * Allows an [Interceptor] to intercept a request and modify request or response.
     *
     * An interceptor can retrieve the request by calling [Chain.request].
     *
     * If the interceptor wants to continue executing the chain (which will execute potentially other interceptors and
     * may eventually perform the request) it can call [Chain.proceed] and pass along the original or a modified
     * request.
     *
     * Finally the interceptor needs to return a [Response]. This can either be the [Response] from calling
     * [Chain.proceed] - modified or unmodified - or a [Response] the interceptor created manually or obtained from
     * a different source.
     */
    fun intercept(chain: Chain): Response

    /**
     * The request interceptor chain.
     */
    interface Chain {
        /**
         * The current request. May be modified by a previously executed interceptor.
         */
        val request: Request

        /**
         * Proceed executing the interceptor chain and eventually perform the request.
         */
        fun proceed(request: Request): Response
    }
}

/**
 * Creates a new [Client] instance that will use the provided list of [Interceptor] instances.
 */
fun Client.withInterceptors(
    vararg interceptors: Interceptor,
): Client = InterceptorClient(this, interceptors.toList())

/**
 * A [Client] instance that will wrap the provided [actualClient] and call the interceptor chain before executing
 * the request.
 */
private class InterceptorClient(
    private val actualClient: Client,
    private val interceptors: List<Interceptor>,
) : Client() {
    override fun fetch(request: Request): Response =
        InterceptorChain(actualClient, interceptors.toList(), request)
            .proceed(request)
}

/**
 * [InterceptorChain] implementation that keeps track of executing the chain of interceptors before executing the
 * request on the provided [client].
 */
private class InterceptorChain(
    private val client: Client,
    private val interceptors: List<Interceptor>,
    private var currentRequest: Request,
) : Interceptor.Chain {
    private var index = 0

    override val request: Request
        get() = currentRequest

    override fun proceed(request: Request): Response {
        currentRequest = request

        return if (index < interceptors.size) {
            val interceptor = interceptors[index]
            index++
            interceptor.intercept(this)
        } else {
            client.fetch(request)
        }
    }
}
