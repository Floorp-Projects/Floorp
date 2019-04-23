/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.support.test.InstrumentationRegistry

import android.support.test.filters.MediumTest
import android.support.test.filters.SdkSuppress
import android.support.test.runner.AndroidJUnit4

import java.math.BigInteger

import java.net.URI

import java.nio.ByteBuffer
import java.nio.CharBuffer
import java.nio.charset.Charset

import java.security.MessageDigest

import java.util.concurrent.CountDownLatch

import org.hamcrest.MatcherAssert.assertThat
import org.hamcrest.Matchers.*

import org.json.JSONObject
import org.junit.*

import org.junit.rules.ExpectedException
import org.junit.runner.RunWith

import org.mozilla.geckoview.GeckoWebExecutor
import org.mozilla.geckoview.WebRequest
import org.mozilla.geckoview.WebRequestError
import org.mozilla.geckoview.WebResponse

import org.mozilla.geckoview.test.util.Environment
import org.mozilla.geckoview.test.util.HttpBin
import org.mozilla.geckoview.test.util.RuntimeCreator
import java.net.UnknownHostException
import java.util.*

@MediumTest
@RunWith(AndroidJUnit4::class)
class WebExecutorTest {
    companion object {
        val TEST_ENDPOINT: String = "http://localhost:4242"
    }

    lateinit var executor: GeckoWebExecutor
    lateinit var server: HttpBin
    val env = Environment()

    @get:Rule val thrown = ExpectedException.none()

    @Before
    fun setup() {
        // Using @UiThreadTest here does not seem to block
        // the tests which are not using @UiThreadTest, so we do that
        // ourselves here as GeckoRuntime needs to be initialized
        // on the UI thread.
        val latch = CountDownLatch(1)
        Handler(Looper.getMainLooper()).post {
            executor = GeckoWebExecutor(RuntimeCreator.getRuntime())
            server = HttpBin(InstrumentationRegistry.getTargetContext(), URI.create(TEST_ENDPOINT))
            server.start()
            latch.countDown()
        }

        latch.await()
    }

    @After
    fun cleanup() {
        server.stop()
    }

    private fun fetch(request: WebRequest): WebResponse {
        return fetch(request, GeckoWebExecutor.FETCH_FLAGS_NONE)
    }

    private fun fetch(request: WebRequest, flags: Int): WebResponse {
        return executor.fetch(request, flags).poll(env.defaultTimeoutMillis)!!
    }

    fun String.toDirectByteBuffer(): ByteBuffer {
        val chars = CharBuffer.wrap(this)
        val buffer = ByteBuffer.allocateDirect(this.length)
        Charset.forName("UTF-8").newEncoder().encode(chars, buffer, true)

        return buffer
    }

    fun WebResponse.getBodyBytes(): ByteBuffer {
        return ByteBuffer.wrap(body!!.readBytes())
    }

    fun WebResponse.getJSONBody(): JSONObject {
        val bytes = this.getBodyBytes()
        val bodyString = Charset.forName("UTF-8").decode(bytes).toString()
        return JSONObject(bodyString)
    }

    private fun randomString(count: Int): String {
        val chars = "01234567890abcdefghijklmnopqrstuvwxyz[],./?;'"
        val builder = StringBuilder(count)
        val rand = Random(System.currentTimeMillis())

        for (i in 0 until count) {
            builder.append(chars[rand.nextInt(chars.length)])
        }

        return builder.toString()
    }

    @Test
    fun smoke() {
        val uri = "$TEST_ENDPOINT/anything"
        val bodyString = randomString(8192)
        val referrer = "http://foo/bar"

        val request = WebRequest.Builder(uri)
                .method("POST")
                .header("Header1", "Clobbered")
                .header("Header1", "Value")
                .addHeader("Header2", "Value1")
                .addHeader("Header2", "Value2")
                .referrer(referrer)
                .header("Content-Type", "text/plain")
                .body(bodyString.toDirectByteBuffer())
                .build()

        val response = fetch(request)

        assertThat("URI should match", response.uri, equalTo(uri))
        assertThat("Status could should match", response.statusCode, equalTo(200))
        assertThat("Content type should match", response.headers["Content-Type"], equalTo("application/json"))
        assertThat("Redirected should match", response.redirected, equalTo(false))

        val body = response.getJSONBody()
        assertThat("Method should match", body.getString("method"), equalTo("POST"))
        assertThat("Headers should match", body.getJSONObject("headers").getString("Header1"), equalTo("Value"))
        assertThat("Headers should match", body.getJSONObject("headers").getString("Header2"), equalTo("Value1, Value2"))
        assertThat("Headers should match", body.getJSONObject("headers").getString("Content-Type"), equalTo("text/plain"))
        assertThat("Referrer should match", body.getJSONObject("headers").getString("Referer"), equalTo(referrer))
        assertThat("Data should match", body.getString("data"), equalTo(bodyString));
    }

    @Test
    fun testFetchAsset() {
        val response = fetch(WebRequest("$TEST_ENDPOINT/assets/www/hello.html"))
        assertThat("Status should match", response.statusCode, equalTo(200))
        assertThat("Body should have bytes", response.getBodyBytes().remaining(), greaterThan(0))
    }

    @Test
    fun test404() {
        val response = fetch(WebRequest("$TEST_ENDPOINT/status/404"))
        assertThat("Status code should match", response.statusCode, equalTo(404))
    }

    @Test
    fun testRedirect() {
        val response = fetch(WebRequest("$TEST_ENDPOINT/redirect-to?url=/status/200"))

        assertThat("URI should match", response.uri, equalTo(TEST_ENDPOINT +"/status/200"))
        assertThat("Redirected should match", response.redirected, equalTo(true))
        assertThat("Status code should match", response.statusCode, equalTo(200))
    }

    @Test
    fun testDisallowRedirect() {
        val response = fetch(WebRequest("$TEST_ENDPOINT/redirect-to?url=/status/200"), GeckoWebExecutor.FETCH_FLAGS_NO_REDIRECTS)

        assertThat("URI should match", response.uri, equalTo("$TEST_ENDPOINT/redirect-to?url=/status/200"))
        assertThat("Redirected should match", response.redirected, equalTo(false))
        assertThat("Status code should match", response.statusCode, equalTo(302))
    }

    @Test
    fun testRedirectLoop() {
        thrown.expect(equalTo(WebRequestError(WebRequestError.ERROR_REDIRECT_LOOP, WebRequestError.ERROR_CATEGORY_NETWORK)))
        fetch(WebRequest("$TEST_ENDPOINT/redirect/100"))
    }

    @Test
    fun testAuth() {
        // We don't support authentication yet, but want to make sure it doesn't do anything
        // silly like try to prompt the user.
        val response = fetch(WebRequest("$TEST_ENDPOINT/basic-auth/foo/bar"))
        assertThat("Status code should match", response.statusCode, equalTo(401))
    }

    @Test
    fun testSslError() {
        val uri = if (env.isAutomation) {
            "https://expired.example.com/"
        } else {
            "https://expired.badssl.com/"
        }

        thrown.expect(equalTo(WebRequestError(WebRequestError.ERROR_SECURITY_BAD_CERT, WebRequestError.ERROR_CATEGORY_SECURITY)))
        fetch(WebRequest(uri))
    }

    @Test
    fun testCookies() {
        val uptimeMillis = SystemClock.uptimeMillis()
        val response = fetch(WebRequest("$TEST_ENDPOINT/cookies/set/uptimeMillis/$uptimeMillis"))

        // We get redirected to /cookies which returns the cookies that were sent in the request
        assertThat("URI should match", response.uri, equalTo("$TEST_ENDPOINT/cookies"))
        assertThat("Status code should match", response.statusCode, equalTo(200))

        val body = response.getJSONBody()
        assertThat("Body should match",
                body.getJSONObject("cookies").getString("uptimeMillis"),
                equalTo(uptimeMillis.toString()))

        val anotherBody = fetch(WebRequest("$TEST_ENDPOINT/cookies")).getJSONBody()
        assertThat("Body should match",
                anotherBody.getJSONObject("cookies").getString("uptimeMillis"),
                equalTo(uptimeMillis.toString()))
    }

    @Test
    fun testAnonymous() {
        // Ensure a cookie is set for the test server
        testCookies();

        val response = fetch(WebRequest("$TEST_ENDPOINT/cookies"),
                GeckoWebExecutor.FETCH_FLAGS_ANONYMOUS)

        assertThat("Status code should match", response.statusCode, equalTo(200))
        val cookies = response.getJSONBody().getJSONObject("cookies")
        assertThat("Cookies should be empty", cookies.length(), equalTo(0))
    }

    @Test
    fun testSpeculativeConnect() {
        // We don't have a way to know if it succeeds or not, but at least we can ensure
        // it doesn't explode.
        executor.speculativeConnect("http://localhost")

        // This is just a fence to ensure the above actually ran.
        fetch(WebRequest("$TEST_ENDPOINT/cookies"))
    }

    @Test
    fun testResolveV4() {
        val addresses = executor.resolve("localhost").poll()!!
        assertThat("Addresses should not be null",
                addresses, notNullValue())
        assertThat("First address should be loopback",
                addresses.first().isLoopbackAddress, equalTo(true))
        assertThat("First address size should be 4",
                addresses.first().address.size, equalTo(4))
    }

    @Test
    @SdkSuppress(minSdkVersion = Build.VERSION_CODES.LOLLIPOP)
    fun testResolveV6() {
        val addresses = executor.resolve("ip6-localhost").poll()!!
        assertThat("Addresses should not be null",
                addresses, notNullValue())
        assertThat("First address should be loopback",
                addresses.first().isLoopbackAddress, equalTo(true))
        assertThat("First address size should be 16",
                addresses.first().address.size, equalTo(16))
    }

    @Test
    fun testFetchUnknownHost() {
        thrown.expect(equalTo(WebRequestError(WebRequestError.ERROR_UNKNOWN_HOST, WebRequestError.ERROR_CATEGORY_URI)))
        fetch(WebRequest("https://this.should.not.resolve"))
    }

    @Test(expected = UnknownHostException::class)
    fun testResolveError() {
        executor.resolve("this.should.not.resolve").poll()
    }

    @Test
    fun testFetchStream() {
        val expectedCount = 1 * 1024 * 1024 // 1MB
        val response = executor.fetch(WebRequest("$TEST_ENDPOINT/bytes/$expectedCount")).poll(env.defaultTimeoutMillis)!!

        assertThat("Status code should match", response.statusCode, equalTo(200))
        assertThat("Content-Length should match", response.headers["Content-Length"]!!.toInt(), equalTo(expectedCount))

        val stream = response.body!!
        val bytes = stream.readBytes(expectedCount)
        stream.close()

        assertThat("Byte counts should match", bytes.size, equalTo(expectedCount))

        val digest = MessageDigest.getInstance("SHA-256").digest(bytes)
        assertThat("Hashes should match", response.headers["X-SHA-256"],
                equalTo(String.format("%064x", BigInteger(1, digest))))
    }

    @Test
    fun testFetchStreamCancel() {
        val expectedCount = 1 * 1024 * 1024 // 1MB
        val response = executor.fetch(WebRequest("$TEST_ENDPOINT/bytes/$expectedCount")).poll(env.defaultTimeoutMillis)!!

        assertThat("Status code should match", response.statusCode, equalTo(200))
        assertThat("Content-Length should match", response.headers["Content-Length"]!!.toInt(), equalTo(expectedCount))

        val stream = response.body!!;

        assertThat("Stream should have 0 bytes available", stream.available(), equalTo(0))

        // Wait a second. Not perfect, but should be enough time for at least one buffer
        // to be appended if things are not going as they should.
        SystemClock.sleep(1000);

        assertThat("Stream should still have 0 bytes available", stream.available(), equalTo(0));

        stream.close()
    }
}
