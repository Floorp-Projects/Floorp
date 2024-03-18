/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch.interceptor

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class InterceptorTest {
    @Test
    fun `Interceptors are invoked`() {
        var interceptorInvoked1 = false
        var interceptorInvoked2 = false

        val interceptor1 = object : Interceptor {
            override fun intercept(chain: Interceptor.Chain): Response {
                interceptorInvoked1 = true
                return chain.proceed(chain.request)
            }
        }

        val interceptor2 = object : Interceptor {
            override fun intercept(chain: Interceptor.Chain): Response {
                interceptorInvoked2 = true
                return chain.proceed(chain.request)
            }
        }

        val fake = FakeClient()
        val client = fake.withInterceptors(interceptor1, interceptor2)

        assertFalse(interceptorInvoked1)
        assertFalse(interceptorInvoked2)

        val response = client.fetch(Request(url = "https://www.mozilla.org"))
        assertTrue(fake.resourceFetched)
        assertTrue(response.isSuccess)

        assertTrue(interceptorInvoked1)
        assertTrue(interceptorInvoked2)
    }

    @Test
    fun `Interceptors are invoked in order`() {
        val order = mutableListOf<String>()

        val fake = FakeClient()
        val client = fake.withInterceptors(
            object : Interceptor {
                override fun intercept(chain: Interceptor.Chain): Response {
                    assertEquals("https://www.mozilla.org", chain.request.url)
                    order.add("A")
                    return chain.proceed(
                        chain.request.copy(
                            url = chain.request.url + "/a",
                        ),
                    )
                }
            },
            object : Interceptor {
                override fun intercept(chain: Interceptor.Chain): Response {
                    assertEquals("https://www.mozilla.org/a", chain.request.url)
                    order.add("B")
                    return chain.proceed(
                        chain.request.copy(
                            url = chain.request.url + "/b",
                        ),
                    )
                }
            },
            object : Interceptor {
                override fun intercept(chain: Interceptor.Chain): Response {
                    assertEquals("https://www.mozilla.org/a/b", chain.request.url)
                    order.add("C")
                    return chain.proceed(
                        chain.request.copy(
                            url = chain.request.url + "/c",
                        ),
                    )
                }
            },
        )

        val response = client.fetch(Request(url = "https://www.mozilla.org"))
        assertTrue(fake.resourceFetched)
        assertTrue(response.isSuccess)

        assertEquals("https://www.mozilla.org/a/b/c", response.url)
        assertEquals(listOf("A", "B", "C"), order)
    }

    @Test
    fun `Intercepted request is never fetched`() {
        val fake = FakeClient()
        val client = fake.withInterceptors(
            object : Interceptor {
                override fun intercept(chain: Interceptor.Chain): Response {
                    return Response("https://www.firefox.com", 203, MutableHeaders(), Response.Body.empty())
                }
            },
        )

        val response = client.fetch(Request(url = "https://www.mozilla.org"))
        assertFalse(fake.resourceFetched)
        assertTrue(response.isSuccess)
        assertEquals(203, response.status)
    }
}

private class FakeClient(
    val response: Response? = null,
) : Client() {
    var resourceFetched = false

    override fun fetch(request: Request): Response {
        resourceFetched = true
        return response ?: Response(
            url = request.url,
            status = 200,
            body = Response.Body.empty(),
            headers = MutableHeaders(),
        )
    }
}
