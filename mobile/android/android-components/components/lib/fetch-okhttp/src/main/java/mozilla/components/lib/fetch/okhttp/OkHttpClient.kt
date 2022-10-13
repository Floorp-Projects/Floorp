/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.fetch.okhttp

import android.content.Context
import mozilla.components.concept.fetch.BuildConfig
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isDataUri
import mozilla.components.lib.fetch.okhttp.OkHttpClient.Companion.CACHE_MAX_SIZE
import mozilla.components.lib.fetch.okhttp.OkHttpClient.Companion.getOrCreateCookieManager
import okhttp3.Cache
import okhttp3.CacheControl
import okhttp3.JavaNetCookieJar
import okhttp3.OkHttpClient
import okhttp3.RequestBody
import java.net.CookieHandler
import java.net.CookieManager

typealias RequestBuilder = okhttp3.Request.Builder

/**
 * [Client] implementation using OkHttp.
 */
class OkHttpClient(
    private val client: OkHttpClient = OkHttpClient(),
    private val context: Context? = null,
) : Client() {
    private val defaultHeaders: Headers = MutableHeaders(
        "User-Agent" to "MozacFetch/${BuildConfig.LIBRARY_VERSION}",
        "Accept-Encoding" to "gzip",
    )

    override fun fetch(request: Request): Response {
        if (request.private) {
            throw IllegalArgumentException("Client doesn't support private request")
        }

        if (request.isDataUri()) {
            return fetchDataUri(request)
        }

        val requestClient = client.rebuildFor(request, context)

        val requestBuilder = createRequestBuilderWithBody(request)
        requestBuilder.addHeadersFrom(request, defaultHeaders = defaultHeaders)

        if (!request.useCaches) {
            requestBuilder.cacheControl(CacheControl.FORCE_NETWORK)
        }

        val actualResponse = requestClient.newCall(
            requestBuilder.build(),
        ).execute()

        return actualResponse.toResponse()
    }

    companion object {
        internal const val CACHE_MAX_SIZE: Long = 10L * 1024L * 1024L

        fun getOrCreateCookieManager(): CookieManager {
            if (CookieHandler.getDefault() == null) {
                CookieHandler.setDefault(CookieManager())
            }
            return CookieHandler.getDefault() as CookieManager
        }
    }
}

private fun OkHttpClient.rebuildFor(request: Request, context: Context?): OkHttpClient {
    @Suppress("ComplexCondition")
    if (request.connectTimeout != null ||
        request.readTimeout != null ||
        request.redirect != Request.Redirect.FOLLOW ||
        request.cookiePolicy != Request.CookiePolicy.OMIT
    ) {
        val clientBuilder = newBuilder()

        request.connectTimeout?.let { (timeout, unit) -> clientBuilder.connectTimeout(timeout, unit) }
        request.readTimeout?.let { (timeout, unit) -> clientBuilder.readTimeout(timeout, unit) }

        if (request.redirect == Request.Redirect.MANUAL) {
            clientBuilder.followRedirects(false)
        }

        if (request.cookiePolicy == Request.CookiePolicy.INCLUDE) {
            clientBuilder.cookieJar(JavaNetCookieJar(getOrCreateCookieManager()))
        }

        context?.let {
            clientBuilder.cache(Cache(context.cacheDir, CACHE_MAX_SIZE))
        }

        return clientBuilder.build()
    }

    return this
}

private fun okhttp3.Response.toResponse(): Response {
    val body = body()
    val headers = translateHeaders(headers())

    return Response(
        url = request().url().toString(),
        headers = headers,
        status = code(),
        body = if (body != null) Response.Body(body.byteStream(), headers["Content-Type"]) else Response.Body.empty(),
    )
}

private fun createRequestBuilderWithBody(request: Request): RequestBuilder {
    val requestBody = request.body?.let { body ->
        RequestBody.create(null, body.useStream { it.readBytes() })
    }

    return RequestBuilder()
        .url(request.url)
        .method(request.method.name, requestBody)
}

private fun RequestBuilder.addHeadersFrom(request: Request, defaultHeaders: Headers) {
    defaultHeaders
        .filter { header ->
            request.headers?.contains(header.name) != true
        }.filter { header ->
            header.name != "Accept-Encoding" && header.value != "gzip"
        }.forEach { header ->
            addHeader(header.name, header.value)
        }

    request.headers?.forEach { header -> addHeader(header.name, header.value) }
}

private fun translateHeaders(actualHeaders: okhttp3.Headers): Headers {
    val headers = MutableHeaders()

    for (i in 0 until actualHeaders.size()) {
        headers.append(actualHeaders.name(i), actualHeaders.value(i))
    }

    return headers
}
