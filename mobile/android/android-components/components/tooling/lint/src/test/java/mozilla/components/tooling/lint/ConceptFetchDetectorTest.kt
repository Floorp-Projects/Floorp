/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.checks.infrastructure.TestFiles.gradle
import com.android.tools.lint.checks.infrastructure.TestFiles.java
import com.android.tools.lint.checks.infrastructure.TestFiles.kotlin
import com.android.tools.lint.checks.infrastructure.TestLintTask.lint
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class ConceptFetchDetectorTest {

    @Test
    fun `should report when close is not invoked on a Response instance`() {
        lint()
            .files(
                kotlin(
                    """
                        package test

                        import mozilla.components.concept.fetch.*

                        val client = Client()

                        fun isSuccessful() : Boolean {
                            val response = client.fetch(Request("https://mozilla.org"))
                            return response.isSuccess
                        }
                    """.trimIndent(),
                ),
                responseClassfileStub,
                clientClassFileStub,
            )
            .issues(ConceptFetchDetector.ISSUE_FETCH_RESPONSE_CLOSE)
            .run()
            .expect(
                """
                src/test/test.kt:8: Error: Response created but not closed: did you forget to call close()? [FetchResponseClose]
                    val response = client.fetch(Request("https://mozilla.org"))
                                   ~~~~~~~~~~~~
                1 errors, 0 warnings
                """.trimIndent(),
            )
    }

    @Test
    fun `should not report from a result that is closed in another function`() {
        lint()
            .files(
                kotlin(
                    """
                        package test

                        import mozilla.components.concept.fetch.*

                        val client = Client()

                        fun getResult() {
                            return try {
                                client.fetch(request).toResult()
                            } catch (e: IOException) {
                                Logger.debug(message = "Could not fetch region from location service", throwable = e)
                                null
                            }
                        }

                        data class Result(
                            val name: String,
                        )

                        private fun Response.toResult(): Region? {
                            if (!this.isSuccess) {
                                close()
                                return null
                            }

                            use {
                                return try {
                                    Result("{}")
                                } catch (e: JSONException) {
                                    Logger.debug(message = "Could not parse JSON returned from service", throwable = e)
                                    null
                                }
                            }
                        }
                    """.trimIndent(),
                ),
                responseClassfileStub,
                clientClassFileStub,
            )
            .issues(ConceptFetchDetector.ISSUE_FETCH_RESPONSE_CLOSE)
            .run()
            .expectClean()
    }

    @Test
    fun `should pass when auto-closeable 'use' function is invoked`() {
        lint()
            .files(
                kotlin(
                    """
                        package test

                        import mozilla.components.concept.fetch.*
                        import kotlin.io.*

                        val client = Client()

                        fun getResult() {
                            client.fetch(request).use { response ->
                                response.hashCode()
                            }
                        }
                    """.trimIndent(),
                ),
                responseClassfileStub,
                clientClassFileStub,
            )
            .issues(ConceptFetchDetector.ISSUE_FETCH_RESPONSE_CLOSE)
            .run()
            .expectClean()
    }

    @Test
    fun `should pass if body (auto-closeable methods) is used from the response`() {
        lint()
            .files(
                kotlin(
                    """
                        package test

                        import mozilla.components.concept.fetch.*
                        import kotlin.io.*

                        val client = Client()

                        fun getResult() { // OK
                            val response = client.fetch(request)
                            response?.body.string(Charset.UTF_8)
                        }

                        fun getResult2() { // OK
                            client.fetch(request).body.useStream()
                        }

                        fun getResult3() { // OK; escaped.
                            val response = client.fetch(request)
                            process(response)
                        }

                        fun process(response: Response) {
                            response.hashCode()
                        }
                    """.trimIndent(),
                ),
                responseClassfileStub,
                clientClassFileStub,
            )
            .issues(ConceptFetchDetector.ISSUE_FETCH_RESPONSE_CLOSE)
            .run()
            .expectClean()
    }

    @Test
    fun `should pass if try-with-resources is used from the response`() {
        lint()
            .files(
                gradle(
                    // For `try (cursor)` (without declaration) we'll need level 9
                    // or PSI/UAST will return an empty variable list
                    """
                android {
                    compileOptions {
                        sourceCompatibility JavaVersion.VERSION_1_9
                        targetCompatibility JavaVersion.VERSION_1_9
                    }
                }
                """,
                ).indented(),
                java(
                    """
                        package test;

                        import mozilla.components.concept.fetch.Client;
                        import mozilla.components.concept.fetch.Response;
                        import mozilla.components.concept.fetch.Response.Body;
                        import mozilla.components.concept.fetch.Request;

                        public class TryWithResources {
                            public void test(Client client, Request request) {
                                try(Response response = client.fetch(request)) {
                                    if (response != null) {
                                        //noinspection StatementWithEmptyBody
                                        while (response.hashCode()) {
                                            // ..
                                        }
                                    }
                                } catch (Exception e) {
                                    // do nothing
                                }
                            }
                        }
                    """.trimIndent(),
                ),
                responseClassfileStub,
                clientClassFileStub,
            )
            .issues(ConceptFetchDetector.ISSUE_FETCH_RESPONSE_CLOSE)
            .run()
            .expectClean()
    }

    private val clientClassFileStub = kotlin(
        """
        package mozilla.components.concept.fetch

        data class Request(
            val url: String,
            val method: Method = Method.GET,
            val headers: MutableHeaders? = MutableHeaders(),
            val connectTimeout: Pair<Long, TimeUnit>? = null,
            val readTimeout: Pair<Long, TimeUnit>? = null,
            val body: Body? = null,
            val redirect: Redirect = Redirect.FOLLOW,
            val cookiePolicy: CookiePolicy = CookiePolicy.INCLUDE,
            val useCaches: Boolean = true,
            val private: Boolean = false,
        )

        class Client {
            fun fetch(request: Request): Response {
                return Response(
                    url = "https://mozilla.org",
                )
            }
        }
        """.trimIndent(),
    )
    private val responseClassfileStub = kotlin(
        """
        package mozilla.components.concept.fetch

        data class Response(
            val url: String,
            val status: Int,
            val headers: Headers,
            val body: Body,
        ) : Closeable {
            override fun close() {
                body.close()
            }

            open class Body(
                private val stream: InputStream,
                contentType: String? = null,
            ) {
                fun <R> useStream(block: (InputStream) -> R): R {
                }

                fun <R> useBufferedReader(charset: Charset? = null, block: (BufferedReader) -> R): R = use {
                }

                fun string(charset: Charset? = null): String = use {
                }
            }
        }

        val Response.isSuccess: Boolean
            get() = status in SUCCESS_STATUS_RANGE
        """,
    ).indented()
}
