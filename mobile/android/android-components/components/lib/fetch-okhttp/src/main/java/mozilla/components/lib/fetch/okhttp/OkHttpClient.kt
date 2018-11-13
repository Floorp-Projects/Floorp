/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.fetch.okhttp

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import okhttp3.OkHttpClient
import okhttp3.RequestBody

typealias RequestBuilder = okhttp3.Request.Builder

/**
 * [Client] implementation using OkHttp.
 */
class OkHttpClient(
    private val client: OkHttpClient = OkHttpClient()
) : Client() {
    override fun fetch(request: Request): Response {
        val requestClient = client.rebuildFor(request)

        val requestBuilder = createRequestBuilderWithBody(request)
        requestBuilder.addHeadersFrom(request, defaultHeaders = defaultHeaders)

        val actualResponse = requestClient.newCall(
            requestBuilder.build()
        ).execute()

        return actualResponse.toResponse()
    }
}

private fun okhttp3.OkHttpClient.rebuildFor(request: Request): okhttp3.OkHttpClient {
    if (request.connectTimeout != null ||
        request.readTimeout != null ||
        request.redirect != Request.Redirect.FOLLOW
    ) {
        val clientBuilder = newBuilder()

        request.connectTimeout?.let { (timeout, unit) -> clientBuilder.connectTimeout(timeout, unit) }
        request.readTimeout?.let { (timeout, unit) -> clientBuilder.readTimeout(timeout, unit) }

        if (request.redirect == Request.Redirect.MANUAL) {
            clientBuilder.followRedirects(false)
        }

        return clientBuilder.build()
    }

    return this
}

private fun okhttp3.Response.toResponse(): Response {
    val body = body()

    return Response(
        url = request().url().toString(),
        headers = translateHeaders(headers()),
        status = code(),
        body = if (body != null) Response.Body(body.byteStream()) else Response.Body.empty()
    )
}

private fun createRequestBuilderWithBody(request: Request): RequestBuilder {
    val requestBody = request.body?.let { body ->
        RequestBody.create(null, body.useStream { it.readBytes() })
    }

    val requestBuilder = RequestBuilder()
        .url(request.url)
        .method(request.method.name, requestBody)

    return requestBuilder
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
