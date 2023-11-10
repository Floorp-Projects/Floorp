/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.Bitmap
import android.os.Looper
import android.os.SystemClock
import android.util.Base64
import android.view.KeyEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.json.JSONObject
import org.junit.Assume.assumeThat
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.* // ktlint-disable no-wildcard-imports
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.GeckoSession.HistoryDelegate
import org.mozilla.geckoview.GeckoSession.Loader
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.GeckoSession.PermissionDelegate
import org.mozilla.geckoview.GeckoSession.ProgressDelegate
import org.mozilla.geckoview.GeckoSession.TextInputDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.* // ktlint-disable no-wildcard-imports
import org.mozilla.geckoview.test.util.UiThreadUtils
import java.io.ByteArrayOutputStream
import java.util.concurrent.ThreadLocalRandom
import kotlin.concurrent.thread

@RunWith(AndroidJUnit4::class)
@MediumTest
class NavigationDelegateTest : BaseSessionTest() {

    // Provides getters for Loader
    class TestLoader : Loader() {
        var mUri: String? = null
        override fun uri(uri: String): TestLoader {
            mUri = uri
            super.uri(uri)
            return this
        }
        fun getUri(): String? {
            return mUri
        }
        override fun flags(f: Int): TestLoader {
            super.flags(f)
            return this
        }
    }

    fun testLoadErrorWithErrorPage(
        testLoader: TestLoader,
        expectedCategory: Int,
        expectedError: Int,
        errorPageUrl: String?,
    ) {
        sessionRule.delegateDuringNextWait(
            object : ProgressDelegate, NavigationDelegate, ContentDelegate {
                @AssertCalled(count = 1, order = [1])
                override fun onLoadRequest(
                    session: GeckoSession,
                    request: LoadRequest,
                ):
                    GeckoResult<AllowOrDeny>? {
                    assertThat(
                        "URI should be " + testLoader.getUri(),
                        request.uri,
                        equalTo(testLoader.getUri()),
                    )
                    assertThat(
                        "App requested this load",
                        request.isDirectNavigation,
                        equalTo(true),
                    )
                    return null
                }

                @AssertCalled(count = 1, order = [2])
                override fun onPageStart(session: GeckoSession, url: String) {
                    assertThat(
                        "URI should be " + testLoader.getUri(),
                        url,
                        equalTo(testLoader.getUri()),
                    )
                }

                @AssertCalled(count = 1, order = [3])
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    assertThat(
                        "Error category should match",
                        error.category,
                        equalTo(expectedCategory),
                    )
                    assertThat(
                        "Error code should match",
                        error.code,
                        equalTo(expectedError),
                    )
                    return GeckoResult.fromValue(errorPageUrl)
                }

                @AssertCalled(count = 1, order = [4])
                override fun onPageStop(session: GeckoSession, success: Boolean) {
                    assertThat("Load should fail", success, equalTo(false))
                }
            },
        )

        mainSession.load(testLoader)
        sessionRule.waitForPageStop()

        if (errorPageUrl != null) {
            sessionRule.waitUntilCalled(object : ContentDelegate, NavigationDelegate {
                @AssertCalled(count = 1, order = [1])
                override fun onLocationChange(
                    session: GeckoSession,
                    url: String?,
                    perms: MutableList<PermissionDelegate.ContentPermission>,
                ) {
                    assertThat("URL should match", url, equalTo(testLoader.getUri()))
                }

                @AssertCalled(count = 1, order = [2])
                override fun onTitleChange(session: GeckoSession, title: String?) {
                    if (!errorPageUrl.startsWith("about:")) {
                        assertThat("Title should not be empty", title, not(isEmptyOrNullString()))
                    }
                }
            })
        }
    }

    fun testLoadExpectError(
        testUri: String,
        expectedCategory: Int,
        expectedError: Int,
    ) {
        testLoadExpectError(TestLoader().uri(testUri), expectedCategory, expectedError)
    }

    fun testLoadExpectError(
        testLoader: TestLoader,
        expectedCategory: Int,
        expectedError: Int,
    ) {
        testLoadErrorWithErrorPage(
            testLoader,
            expectedCategory,
            expectedError,
            "about:blank",
        )
        testLoadErrorWithErrorPage(
            testLoader,
            expectedCategory,
            expectedError,
            "about:blank",
        )
    }

    fun testLoadEarlyErrorWithErrorPage(
        testUri: String,
        expectedCategory: Int,
        expectedError: Int,
        errorPageUrl: String?,
    ) {
        sessionRule.delegateDuringNextWait(
            object : ProgressDelegate, NavigationDelegate, ContentDelegate {

                @AssertCalled(false)
                override fun onPageStart(session: GeckoSession, url: String) {
                    assertThat("URI should be " + testUri, url, equalTo(testUri))
                }

                @AssertCalled(count = 1, order = [1])
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    assertThat(
                        "Error category should match",
                        error.category,
                        equalTo(expectedCategory),
                    )
                    assertThat(
                        "Error code should match",
                        error.code,
                        equalTo(expectedError),
                    )
                    return GeckoResult.fromValue(errorPageUrl)
                }

                @AssertCalled(false)
                override fun onPageStop(session: GeckoSession, success: Boolean) {
                }
            },
        )

        mainSession.loadUri(testUri)
        sessionRule.waitUntilCalled(NavigationDelegate::class, "onLoadError")

        if (errorPageUrl != null) {
            sessionRule.waitUntilCalled(object : ContentDelegate {
                @AssertCalled(count = 1)
                override fun onTitleChange(session: GeckoSession, title: String?) {}
            })
        }
    }

    fun testLoadEarlyError(
        testUri: String,
        expectedCategory: Int,
        expectedError: Int,
    ) {
        testLoadEarlyErrorWithErrorPage(testUri, expectedCategory, expectedError, "about:blank")
        testLoadEarlyErrorWithErrorPage(testUri, expectedCategory, expectedError, null)
    }

    @Test fun loadFileNotFound() {
        testLoadExpectError(
            "file:///test.mozilla",
            WebRequestError.ERROR_CATEGORY_URI,
            WebRequestError.ERROR_FILE_NOT_FOUND,
        )

        val promise = mainSession.evaluatePromiseJS("document.addCertException(false)")
        var exceptionCaught = false
        try {
            val result = promise.value as Boolean
            assertThat("Promise should not resolve", result, equalTo(false))
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            exceptionCaught = true
        }
        assertThat("document.addCertException failed with exception", exceptionCaught, equalTo(true))
    }

    @Test fun loadUnknownHost() {
        testLoadExpectError(
            UNKNOWN_HOST_URI,
            WebRequestError.ERROR_CATEGORY_URI,
            WebRequestError.ERROR_UNKNOWN_HOST,
        )
    }

    // External loads should not have access to privileged protocols
    @Test fun loadExternalDenied() {
        testLoadExpectError(
            TestLoader()
                .uri("file:///")
                .flags(GeckoSession.LOAD_FLAGS_EXTERNAL),
            WebRequestError.ERROR_CATEGORY_UNKNOWN,
            WebRequestError.ERROR_UNKNOWN,
        )
        testLoadExpectError(
            TestLoader()
                .uri("resource://gre/")
                .flags(GeckoSession.LOAD_FLAGS_EXTERNAL),
            WebRequestError.ERROR_CATEGORY_UNKNOWN,
            WebRequestError.ERROR_UNKNOWN,
        )
        testLoadExpectError(
            TestLoader()
                .uri("about:about")
                .flags(GeckoSession.LOAD_FLAGS_EXTERNAL),
            WebRequestError.ERROR_CATEGORY_UNKNOWN,
            WebRequestError.ERROR_UNKNOWN,
        )
        testLoadExpectError(
            TestLoader()
                .uri("resource://android/assets/web_extensions/")
                .flags(GeckoSession.LOAD_FLAGS_EXTERNAL),
            WebRequestError.ERROR_CATEGORY_UNKNOWN,
            WebRequestError.ERROR_UNKNOWN,
        )
    }

    @Test fun loadInvalidUri() {
        testLoadEarlyError(
            INVALID_URI,
            WebRequestError.ERROR_CATEGORY_URI,
            WebRequestError.ERROR_MALFORMED_URI,
        )
    }

    @Test fun loadBadPort() {
        testLoadEarlyError(
            "http://localhost:1/",
            WebRequestError.ERROR_CATEGORY_NETWORK,
            WebRequestError.ERROR_PORT_BLOCKED,
        )
    }

    @Test fun loadUntrusted() {
        val host = if (sessionRule.env.isAutomation) {
            "expired.example.com"
        } else {
            "expired.badssl.com"
        }
        val uri = "https://$host/"
        testLoadExpectError(
            uri,
            WebRequestError.ERROR_CATEGORY_SECURITY,
            WebRequestError.ERROR_SECURITY_BAD_CERT,
        )

        if (!sessionRule.env.isFission) { // todo: Bug 1673954
            mainSession.waitForJS("document.addCertException(false)")
            mainSession.delegateDuringNextWait(
                object : ProgressDelegate, NavigationDelegate, ContentDelegate {
                    @AssertCalled(count = 1, order = [1])
                    override fun onPageStart(session: GeckoSession, url: String) {
                        assertThat("URI should be " + uri, url, equalTo(uri))
                    }

                    @AssertCalled(count = 1, order = [2])
                    override fun onSecurityChange(
                        session: GeckoSession,
                        securityInfo: ProgressDelegate.SecurityInformation,
                    ) {
                        assertThat("Should be exception", securityInfo.isException, equalTo(true))
                        assertThat("Should not be secure", securityInfo.isSecure, equalTo(false))
                    }

                    @AssertCalled(count = 1, order = [3])
                    override fun onPageStop(session: GeckoSession, success: Boolean) {
                        assertThat("Load should succeed", success, equalTo(true))
                        sessionRule.removeAllCertOverrides()
                    }
                },
            )
            mainSession.evaluateJS("location.reload()")
            mainSession.waitForPageStop()
        }
    }

    @Test fun loadWithHTTPSOnlyMode() {
        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.HTTPS_ONLY)

        val httpsFirstPref = "dom.security.https_first"
        val httpsFirstPrefValue = (sessionRule.getPrefs(httpsFirstPref)[0] as Boolean)

        val httpsFirstPBMPref = "dom.security.https_first_pbm"
        val httpsFirstPBMPrefValue = (sessionRule.getPrefs(httpsFirstPBMPref)[0] as Boolean)

        val insecureUri = if (sessionRule.env.isAutomation) {
            "http://nocert.example.com/"
        } else {
            "http://neverssl.com"
        }

        val secureUri = if (sessionRule.env.isAutomation) {
            "http://example.com/"
        } else {
            "http://neverssl.com"
        }

        mainSession.loadUri(insecureUri)
        mainSession.waitForPageStop()

        mainSession.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                assertThat("categories should match", error.category, equalTo(WebRequestError.ERROR_CATEGORY_NETWORK))
                assertThat("codes should match", error.code, equalTo(WebRequestError.ERROR_HTTPS_ONLY))
                return null
            }
        })

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.ALLOW_ALL)

        mainSession.loadUri(secureUri)
        mainSession.waitForPageStop()

        var onLoadCalledCounter = 0
        mainSession.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }

            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                onLoadCalledCounter++
                return null
            }
        })

        if (httpsFirstPrefValue) {
            // if https-first is enabled we get two calls to onLoadRequest
            // (1) http://example.com/ and  (2) https://example.com/
            assertThat("Assert count mainSession.onLoadRequest", onLoadCalledCounter, equalTo(2))
        } else {
            assertThat("Assert count mainSession.onLoadRequest", onLoadCalledCounter, equalTo(1))
        }

        val privateSession = sessionRule.createOpenSession(
            GeckoSessionSettings.Builder(mainSession.settings)
                .usePrivateMode(true)
                .build(),
        )

        privateSession.loadUri(secureUri)
        privateSession.waitForPageStop()

        onLoadCalledCounter = 0
        privateSession.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }

            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                onLoadCalledCounter++
                return null
            }
        })

        if (httpsFirstPBMPrefValue) {
            // if https-first is enabled we get two calls to onLoadRequest
            // (1) http://example.com/ and  (2) https://example.com/
            assertThat("Assert count privateSession.onLoadRequest", onLoadCalledCounter, equalTo(2))
        } else {
            assertThat("Assert count privateSession.onLoadRequest", onLoadCalledCounter, equalTo(1))
        }

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.HTTPS_ONLY_PRIVATE)

        privateSession.loadUri(insecureUri)
        privateSession.waitForPageStop()

        privateSession.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                assertThat("categories should match", error.category, equalTo(WebRequestError.ERROR_CATEGORY_NETWORK))
                assertThat("codes should match", error.code, equalTo(WebRequestError.ERROR_HTTPS_ONLY))
                return null
            }
        })

        mainSession.loadUri(secureUri)
        mainSession.waitForPageStop()

        onLoadCalledCounter = 0
        mainSession.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }

            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                onLoadCalledCounter++
                return null
            }
        })

        if (httpsFirstPrefValue) {
            // if https-first is enabled we get two calls to onLoadRequest
            // (1) http://example.com/ and  (2) https://example.com/
            assertThat("Assert count mainSession.onLoadRequest", onLoadCalledCounter, equalTo(2))
        } else {
            assertThat("Assert count mainSession.onLoadRequest", onLoadCalledCounter, equalTo(1))
        }

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.ALLOW_ALL)
    }

    // Due to Bug 1692578 we currently cannot test bypassing of the error
    // the URI loading process takes the desktop path for iframes
    @Test fun loadHTTPSOnlyInSubframe() {
        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.HTTPS_ONLY)

        val uri = "http://example.org/tests/junit/iframe_http_only.html"
        val httpsUri = "https://example.org/tests/junit/iframe_http_only.html"
        val iFrameUri = "http://expired.example.com/"
        val iFrameHttpsUri = "https://expired.example.com/"

        val testLoader = TestLoader().uri(uri)

        sessionRule.delegateDuringNextWait(
            object : ProgressDelegate, NavigationDelegate, ContentDelegate {
                @AssertCalled(count = 2)
                override fun onLoadRequest(
                    session: GeckoSession,
                    request: LoadRequest,
                ):
                    GeckoResult<AllowOrDeny>? {
                    assertThat("The URLs must match", request.uri, equalTo(forEachCall(uri, httpsUri)))
                    return null
                }

                @AssertCalled(count = 1)
                override fun onPageStart(session: GeckoSession, url: String) {
                    assertThat(
                        "URI should be " + uri,
                        url,
                        equalTo(uri),
                    )
                }

                @AssertCalled(count = 1)
                override fun onPageStop(session: GeckoSession, success: Boolean) {
                    assertThat("Load should fail", success, equalTo(true))
                }

                @AssertCalled(count = 2)
                override fun onSubframeLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                    assertThat("URI should not be null", request.uri, notNullValue())
                    assertThat("URI should match", request.uri, equalTo(forEachCall(iFrameUri, iFrameHttpsUri)))
                    return GeckoResult.allow()
                }
            },
        )

        mainSession.load(testLoader)
        sessionRule.waitForPageStop()

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.ALLOW_ALL)
    }

    @Test fun bypassHTTPSOnlyError() {
        // Bug 1849060. Hit debug assertion with fission
        assumeThat(sessionRule.env.isFission and sessionRule.env.isDebugBuild, equalTo(false))

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.HTTPS_ONLY)

        val host = if (sessionRule.env.isAutomation) {
            "expired.example.com"
        } else {
            "expired.badssl.com"
        }

        val uri = "http://$host/"
        val httpsUri = "https://$host/"

        val testLoader = TestLoader().uri(uri)

        // The two loads below follow testLoadExpectError(TestLoader, Int, Int) flow

        sessionRule.delegateDuringNextWait(
            object : ProgressDelegate, NavigationDelegate, ContentDelegate {
                @AssertCalled(count = 2)
                override fun onLoadRequest(
                    session: GeckoSession,
                    request: LoadRequest,
                ):
                    GeckoResult<AllowOrDeny>? {
                    assertThat("The URLs must match", request.uri, equalTo(forEachCall(uri, httpsUri)))
                    return null
                }

                @AssertCalled(count = 1)
                override fun onPageStart(session: GeckoSession, url: String) {
                    assertThat(
                        "URI should be " + uri,
                        url,
                        equalTo(uri),
                    )
                }

                @AssertCalled(count = 1)
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    assertThat(
                        "Error code should match",
                        error.code,
                        equalTo(WebRequestError.ERROR_HTTPS_ONLY),
                    )
                    return GeckoResult.fromValue("about:blank")
                }

                @AssertCalled(count = 1)
                override fun onPageStop(session: GeckoSession, success: Boolean) {
                    assertThat("Load should fail", success, equalTo(false))
                }
            },
        )

        mainSession.load(testLoader)
        sessionRule.waitForPageStop()

        sessionRule.waitUntilCalled(object : ContentDelegate, NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, equalTo(httpsUri))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onTitleChange(session: GeckoSession, title: String?) {}
        })

        sessionRule.delegateDuringNextWait(
            object : ProgressDelegate, NavigationDelegate, ContentDelegate {
                @AssertCalled(count = 2, order = [1, 3])
                override fun onLoadRequest(
                    session: GeckoSession,
                    request: LoadRequest,
                ):
                    GeckoResult<AllowOrDeny>? {
                    assertThat("The URLs must match", request.uri, equalTo(forEachCall(uri, httpsUri)))
                    return null
                }

                @AssertCalled(count = 1, order = [4])
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    assertThat(
                        "Error code should match",
                        error.code,
                        equalTo(WebRequestError.ERROR_HTTPS_ONLY),
                    )
                    // When returning null then process is switched, web extension won't be loaded
                    // since there is no document element.
                    // So we shouldn't return null with fission if we want to use `evaluateJS`.
                    return GeckoResult.fromValue("about:blank")
                }

                @AssertCalled(count = 1, order = [5])
                override fun onPageStop(session: GeckoSession, success: Boolean) {
                    assertThat("Load should fail", success, equalTo(false))
                }
            },
        )

        mainSession.load(testLoader)
        sessionRule.waitForPageStop()

        // No good way to wait for loading about:blank error page. Use onLocaitonChange etc.
        sessionRule.waitUntilCalled(object : ContentDelegate, NavigationDelegate {
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, equalTo(httpsUri))
            }

            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Title should not be empty", title, not(isEmptyOrNullString()))
            }
        })

        sessionRule.delegateDuringNextWait(
            object : ProgressDelegate, NavigationDelegate, ContentDelegate {
                @AssertCalled(count = 1)
                override fun onLoadRequest(
                    session: GeckoSession,
                    request: LoadRequest,
                ):
                    GeckoResult<AllowOrDeny>? {
                    // We set http scheme only in case it's not iFrame
                    assertThat("The URLs must match", request.uri, equalTo(uri))
                    return null
                }

                @AssertCalled(count = 0)
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    return null
                }
            },
        )

        // Calling eloadWithHttpsOnlyException may causes that the document will be unloaded
        // immediately before native message isn't handled.
        try {
            mainSession.evaluateJS("document.reloadWithHttpsOnlyException();")
        } catch (ex: RejectedPromiseException) {
            // Communication port for web extensions is immediately disconnected. Re-try.
            mainSession.evaluateJS("document.reloadWithHttpsOnlyException();")
        }
        mainSession.waitForPageStop()

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.ALLOW_ALL)
    }

    @Test fun loadHSTSBadCert() {
        val httpsFirstPref = "dom.security.https_first"
        assertThat("https pref should be false", sessionRule.getPrefs(httpsFirstPref)[0] as Boolean, equalTo(false))

        // load secure url with hsts header
        val uri = "https://example.com/tests/junit/hsts_header.sjs"
        mainSession.loadUri(uri)
        mainSession.waitForPageStop()

        // load insecure subdomain url to see if it gets upgraded to https
        val http_uri = "http://test1.example.com/"
        val https_uri = "https://test1.example.com/"

        mainSession.loadUri(http_uri)
        mainSession.waitForPageStop()

        mainSession.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat(
                    "URI should be HTTP then redirected to HTTPS",
                    request.uri,
                    equalTo(forEachCall(http_uri, https_uri)),
                )
                return null
            }
        })

        // load subdomain that will trigger the cert error
        val no_cert_uri = "https://nocert.example.com/"
        mainSession.loadUri(no_cert_uri)
        mainSession.waitForPageStop()

        mainSession.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                assertThat("categories should match", error.category, equalTo(WebRequestError.ERROR_CATEGORY_NETWORK))
                assertThat("codes should match", error.code, equalTo(WebRequestError.ERROR_BAD_HSTS_CERT))
                return null
            }
        })
        sessionRule.clearHSTSState()
    }

    @Ignore // Disabled for bug 1619344.
    @Test
    fun loadUnknownProtocol() {
        testLoadEarlyError(
            UNKNOWN_PROTOCOL_URI,
            WebRequestError.ERROR_CATEGORY_URI,
            WebRequestError.ERROR_UNKNOWN_PROTOCOL,
        )
    }

    // Due to Bug 1692578 we currently cannot test displaying the error
    // the URI loading process takes the desktop path for iframes
    @Test fun loadUnknownProtocolIframe() {
        // Should match iframe URI from IFRAME_UNKNOWN_PROTOCOL
        val iframeUri = "foo://bar"
        mainSession.loadTestPath(IFRAME_UNKNOWN_PROTOCOL)
        mainSession.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URI should match", request.uri, endsWith(IFRAME_UNKNOWN_PROTOCOL))
                return null
            }

            @AssertCalled(count = 1)
            override fun onSubframeLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URI should match", request.uri, endsWith(iframeUri))
                return null
            }
        })
    }

    @Setting(key = Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Ignore
    // TODO: Bug 1564373
    @Test
    fun trackingProtection() {
        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)
        mainSession.loadTestPath(TRACKERS_PATH)

        sessionRule.waitUntilCalled(
            object : ContentBlocking.Delegate {
                @AssertCalled(count = 3)
                override fun onContentBlocked(
                    session: GeckoSession,
                    event: ContentBlocking.BlockEvent,
                ) {
                    assertThat(
                        "Category should be set",
                        event.antiTrackingCategory,
                        equalTo(category),
                    )
                    assertThat("URI should not be null", event.uri, notNullValue())
                    assertThat("URI should match", event.uri, endsWith("tracker.js"))
                }

                @AssertCalled(false)
                override fun onContentLoaded(session: GeckoSession, event: ContentBlocking.BlockEvent) {
                }
            },
        )

        mainSession.settings.useTrackingProtection = false

        mainSession.reload()
        mainSession.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
            object : ContentBlocking.Delegate {
                @AssertCalled(false)
                override fun onContentBlocked(
                    session: GeckoSession,
                    event: ContentBlocking.BlockEvent,
                ) {
                }

                @AssertCalled(count = 3)
                override fun onContentLoaded(session: GeckoSession, event: ContentBlocking.BlockEvent) {
                    assertThat(
                        "Category should be set",
                        event.antiTrackingCategory,
                        equalTo(category),
                    )
                    assertThat("URI should not be null", event.uri, notNullValue())
                    assertThat("URI should match", event.uri, endsWith("tracker.js"))
                }
            },
        )
    }

    @Test fun redirectLoad() {
        val redirectUri = if (sessionRule.env.isAutomation) {
            "https://example.org/tests/junit/hello.html"
        } else {
            "https://jigsaw.w3.org/HTTP/300/Overview.html"
        }
        val uri = if (sessionRule.env.isAutomation) {
            "https://example.org/tests/junit/simple_redirect.sjs?$redirectUri"
        } else {
            "https://jigsaw.w3.org/HTTP/300/301.html"
        }

        mainSession.loadUri(uri)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 2, order = [1, 2])
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat(
                    "URL should match",
                    request.uri,
                    equalTo(forEachCall(request.uri, redirectUri)),
                )
                assertThat(
                    "Trigger URL should be null",
                    request.triggerUri,
                    nullValue(),
                )
                assertThat(
                    "From app should be correct",
                    request.isDirectNavigation,
                    equalTo(forEachCall(true, false)),
                )
                assertThat("Target should not be null", request.target, notNullValue())
                assertThat(
                    "Target should match",
                    request.target,
                    equalTo(NavigationDelegate.TARGET_WINDOW_CURRENT),
                )
                assertThat(
                    "Redirect flag is set",
                    request.isRedirect,
                    equalTo(forEachCall(false, true)),
                )
                return null
            }
        })
    }

    @Test fun redirectLoadIframe() {
        val path = if (sessionRule.env.isAutomation) {
            IFRAME_REDIRECT_AUTOMATION
        } else {
            IFRAME_REDIRECT_LOCAL
        }

        mainSession.loadTestPath(path)
        sessionRule.waitForPageStop()

        // We shouldn't be firing onLoadRequest for iframes, including redirects.
        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("App requested this load", request.isDirectNavigation, equalTo(true))
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URI should match", request.uri, endsWith(path))
                assertThat("isRedirect should match", request.isRedirect, equalTo(false))
                return null
            }

            @AssertCalled(count = 2)
            override fun onSubframeLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("App did not request this load", request.isDirectNavigation, equalTo(false))
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat(
                    "isRedirect should match",
                    request.isRedirect,
                    equalTo(forEachCall(false, true)),
                )
                return null
            }
        })
    }

    @Test fun redirectDenyLoad() {
        val redirectUri = if (sessionRule.env.isAutomation) {
            "https://example.org/tests/junit/hello.html"
        } else {
            "https://jigsaw.w3.org/HTTP/300/Overview.html"
        }
        val uri = if (sessionRule.env.isAutomation) {
            "https://example.org/tests/junit/simple_redirect.sjs?$redirectUri"
        } else {
            "https://jigsaw.w3.org/HTTP/300/301.html"
        }

        sessionRule.delegateDuringNextWait(
            object : NavigationDelegate {
                @AssertCalled(count = 2, order = [1, 2])
                override fun onLoadRequest(
                    session: GeckoSession,
                    request: LoadRequest,
                ):
                    GeckoResult<AllowOrDeny>? {
                    assertThat("Session should not be null", session, notNullValue())
                    assertThat("URI should not be null", request.uri, notNullValue())
                    assertThat(
                        "URL should match",
                        request.uri,
                        equalTo(forEachCall(request.uri, redirectUri)),
                    )
                    assertThat(
                        "Trigger URL should be null",
                        request.triggerUri,
                        nullValue(),
                    )
                    assertThat(
                        "From app should be correct",
                        request.isDirectNavigation,
                        equalTo(forEachCall(true, false)),
                    )
                    assertThat("Target should not be null", request.target, notNullValue())
                    assertThat(
                        "Target should match",
                        request.target,
                        equalTo(NavigationDelegate.TARGET_WINDOW_CURRENT),
                    )
                    assertThat(
                        "Redirect flag is set",
                        request.isRedirect,
                        equalTo(forEachCall(false, true)),
                    )

                    return forEachCall(GeckoResult.allow(), GeckoResult.deny())
                }
            },
        )

        mainSession.loadUri(uri)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
            object : ProgressDelegate {
                @AssertCalled(count = 1, order = [1])
                override fun onPageStart(session: GeckoSession, url: String) {
                    assertThat("URL should match", url, equalTo(uri))
                }
            },
        )
    }

    @Test fun redirectIntentLoad() {
        assumeThat(sessionRule.env.isAutomation, equalTo(true))

        val redirectUri = "intent://test"
        val uri = "https://example.org/tests/junit/simple_redirect.sjs?$redirectUri"

        mainSession.loadUri(uri)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 2, order = [1, 2])
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("URL should match", request.uri, equalTo(forEachCall(uri, redirectUri)))
                assertThat(
                    "From app should be correct",
                    request.isDirectNavigation,
                    equalTo(forEachCall(true, false)),
                )
                return null
            }
        })
    }

    @Test fun bypassClassifier() {
        val phishingUri = "https://www.itisatrap.org/firefox/its-a-trap.html"
        val category = ContentBlocking.SafeBrowsing.PHISHING

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        mainSession.load(
            Loader()
                .uri(phishingUri + "?bypass=true")
                .flags(GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER),
        )
        mainSession.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
            object : NavigationDelegate {
                @AssertCalled(false)
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    return null
                }
            },
        )
    }

    @Test fun safebrowsingPhishing() {
        val phishingUri = "https://www.itisatrap.org/firefox/its-a-trap.html"
        val category = ContentBlocking.SafeBrowsing.PHISHING

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        // Add query string to avoid bypassing classifier check because of cache.
        testLoadExpectError(
            phishingUri + "?block=true",
            WebRequestError.ERROR_CATEGORY_SAFEBROWSING,
            WebRequestError.ERROR_SAFEBROWSING_PHISHING_URI,
        )

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(ContentBlocking.SafeBrowsing.NONE)

        mainSession.loadUri(phishingUri + "?block=false")
        mainSession.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
            object : NavigationDelegate {
                @AssertCalled(false)
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    return null
                }
            },
        )
    }

    @Test fun safebrowsingMalware() {
        val malwareUri = "https://www.itisatrap.org/firefox/its-an-attack.html"
        val category = ContentBlocking.SafeBrowsing.MALWARE

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        testLoadExpectError(
            malwareUri + "?block=true",
            WebRequestError.ERROR_CATEGORY_SAFEBROWSING,
            WebRequestError.ERROR_SAFEBROWSING_MALWARE_URI,
        )

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(ContentBlocking.SafeBrowsing.NONE)

        mainSession.loadUri(malwareUri + "?block=false")
        mainSession.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
            object : NavigationDelegate {
                @AssertCalled(false)
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    return null
                }
            },
        )
    }

    @Test fun safebrowsingUnwanted() {
        val unwantedUri = "https://www.itisatrap.org/firefox/unwanted.html"
        val category = ContentBlocking.SafeBrowsing.UNWANTED

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        testLoadExpectError(
            unwantedUri + "?block=true",
            WebRequestError.ERROR_CATEGORY_SAFEBROWSING,
            WebRequestError.ERROR_SAFEBROWSING_UNWANTED_URI,
        )

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(ContentBlocking.SafeBrowsing.NONE)

        mainSession.loadUri(unwantedUri + "?block=false")
        mainSession.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
            object : NavigationDelegate {
                @AssertCalled(false)
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    return null
                }
            },
        )
    }

    @Test fun safebrowsingHarmful() {
        val harmfulUri = "https://www.itisatrap.org/firefox/harmful.html"
        val category = ContentBlocking.SafeBrowsing.HARMFUL

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        testLoadExpectError(
            harmfulUri + "?block=true",
            WebRequestError.ERROR_CATEGORY_SAFEBROWSING,
            WebRequestError.ERROR_SAFEBROWSING_HARMFUL_URI,
        )

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(ContentBlocking.SafeBrowsing.NONE)

        mainSession.loadUri(harmfulUri + "?block=false")
        mainSession.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
            object : NavigationDelegate {
                @AssertCalled(false)
                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    error: WebRequestError,
                ): GeckoResult<String>? {
                    return null
                }
            },
        )
    }

    // Checks that the User Agent matches the user agent built in
    // nsHttpHandler::BuildUserAgent
    @Test fun defaultUserAgentMatchesActualUserAgent() {
        var userAgent = sessionRule.waitForResult(mainSession.userAgent)
        assertThat(
            "Mobile user agent should match the default user agent",
            userAgent,
            equalTo(GeckoSession.getDefaultUserAgent()),
        )
    }

    @Test fun desktopMode() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        val mobileSubStr = "Mobile"
        val desktopSubStr = "X11"

        assertThat(
            "User agent should be set to mobile",
            getUserAgent(),
            containsString(mobileSubStr),
        )

        var userAgent = sessionRule.waitForResult(mainSession.userAgent)
        assertThat(
            "User agent should be reported as mobile",
            userAgent,
            containsString(mobileSubStr),
        )

        mainSession.settings.userAgentMode = GeckoSessionSettings.USER_AGENT_MODE_DESKTOP

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat(
            "User agent should be set to desktop",
            getUserAgent(),
            containsString(desktopSubStr),
        )

        userAgent = sessionRule.waitForResult(mainSession.userAgent)
        assertThat(
            "User agent should be reported as desktop",
            userAgent,
            containsString(desktopSubStr),
        )

        mainSession.settings.userAgentMode = GeckoSessionSettings.USER_AGENT_MODE_MOBILE

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat(
            "User agent should be set to mobile",
            getUserAgent(),
            containsString(mobileSubStr),
        )

        userAgent = sessionRule.waitForResult(mainSession.userAgent)
        assertThat(
            "User agent should be reported as mobile",
            userAgent,
            containsString(mobileSubStr),
        )

        val vrSubStr = "Mobile VR"
        mainSession.settings.userAgentMode = GeckoSessionSettings.USER_AGENT_MODE_VR

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat(
            "User agent should be set to VR",
            getUserAgent(),
            containsString(vrSubStr),
        )

        userAgent = sessionRule.waitForResult(mainSession.userAgent)
        assertThat(
            "User agent should be reported as VR",
            userAgent,
            containsString(vrSubStr),
        )
    }

    private fun getUserAgent(session: GeckoSession = mainSession): String {
        return session.evaluateJS("window.navigator.userAgent") as String
    }

    @Test fun uaOverrideNewSession() {
        val newSession = sessionRule.createClosedSession()
        newSession.settings.userAgentOverride = "Test user agent override"

        newSession.open()
        newSession.loadUri("https://example.com")
        newSession.waitForPageStop()

        assertThat(
            "User agent should match override",
            getUserAgent(newSession),
            equalTo("Test user agent override"),
        )
    }

    @Test fun uaOverride() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        val mobileSubStr = "Mobile"
        val vrSubStr = "Mobile VR"
        val overrideUserAgent = "This is the override user agent"

        assertThat(
            "User agent should be reported as mobile",
            getUserAgent(),
            containsString(mobileSubStr),
        )

        mainSession.settings.userAgentOverride = overrideUserAgent

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat(
            "User agent should be reported as override",
            getUserAgent(),
            equalTo(overrideUserAgent),
        )

        mainSession.settings.userAgentMode = GeckoSessionSettings.USER_AGENT_MODE_VR

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat(
            "User agent should still be reported as override even when USER_AGENT_MODE is set",
            getUserAgent(),
            equalTo(overrideUserAgent),
        )

        mainSession.settings.userAgentOverride = null

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat(
            "User agent should now be reported as VR",
            getUserAgent(),
            containsString(vrSubStr),
        )

        sessionRule.delegateDuringNextWait(object : NavigationDelegate {
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                mainSession.settings.userAgentOverride = overrideUserAgent
                return null
            }
        })

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat(
            "User agent should be reported as override after being set in onLoadRequest",
            getUserAgent(),
            equalTo(overrideUserAgent),
        )

        sessionRule.delegateDuringNextWait(object : NavigationDelegate {
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                mainSession.settings.userAgentOverride = null
                return null
            }
        })

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat(
            "User agent should again be reported as VR after disabling override in onLoadRequest",
            getUserAgent(),
            containsString(vrSubStr),
        )
    }

    @WithDisplay(width = 600, height = 200)
    @Test
    fun viewportMode() {
        mainSession.loadTestPath(VIEWPORT_PATH)
        sessionRule.waitForPageStop()

        val desktopInnerWidth = 980.0
        val physicalWidth = 600.0
        val pixelRatio = mainSession.evaluateJS("window.devicePixelRatio") as Double
        val mobileInnerWidth = physicalWidth / pixelRatio
        val innerWidthJs = "window.innerWidth"

        var innerWidth = mainSession.evaluateJS(innerWidthJs) as Double
        assertThat(
            "innerWidth should be equal to $mobileInnerWidth",
            innerWidth,
            closeTo(mobileInnerWidth, 0.1),
        )

        mainSession.settings.viewportMode = GeckoSessionSettings.VIEWPORT_MODE_DESKTOP

        mainSession.reload()
        mainSession.waitForPageStop()

        innerWidth = mainSession.evaluateJS(innerWidthJs) as Double
        assertThat(
            "innerWidth should be equal to $desktopInnerWidth",
            innerWidth,
            closeTo(desktopInnerWidth, 0.1),
        )

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        innerWidth = mainSession.evaluateJS(innerWidthJs) as Double
        assertThat(
            "after navigation innerWidth should be equal to $desktopInnerWidth",
            innerWidth,
            closeTo(desktopInnerWidth, 0.1),
        )

        mainSession.loadTestPath(VIEWPORT_PATH)
        sessionRule.waitForPageStop()

        innerWidth = mainSession.evaluateJS(innerWidthJs) as Double
        assertThat(
            "after navigting back innerWidth should be equal to $desktopInnerWidth",
            innerWidth,
            closeTo(desktopInnerWidth, 0.1),
        )

        mainSession.settings.viewportMode = GeckoSessionSettings.VIEWPORT_MODE_MOBILE

        mainSession.reload()
        mainSession.waitForPageStop()

        innerWidth = mainSession.evaluateJS(innerWidthJs) as Double
        assertThat(
            "innerWidth should be equal to $mobileInnerWidth again",
            innerWidth,
            closeTo(mobileInnerWidth, 0.1),
        )
    }

    @Test fun load() {
        mainSession.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URI should match", request.uri, endsWith(HELLO_HTML_PATH))
                assertThat(
                    "Trigger URL should be null",
                    request.triggerUri,
                    nullValue(),
                )
                assertThat(
                    "App requested this load",
                    request.isDirectNavigation,
                    equalTo(true),
                )
                assertThat("Target should not be null", request.target, notNullValue())
                assertThat(
                    "Target should match",
                    request.target,
                    equalTo(NavigationDelegate.TARGET_WINDOW_CURRENT),
                )
                assertThat("Redirect flag is not set", request.isRedirect, equalTo(false))
                assertThat("Should not have a user gesture", request.hasUserGesture, equalTo(false))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", url, notNullValue())
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go back", canGoBack, equalTo(false))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }
        })
    }

    @Test fun load_dataUri() {
        val dataUrl = "data:,Hello%2C%20World!"
        mainSession.loadUri(dataUrl)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate, ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match the provided data URL", url, equalTo(dataUrl))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @NullDelegate(NavigationDelegate::class)
    @Test
    fun load_withoutNavigationDelegate() {
        // Test that when navigation delegate is disabled, we can still perform loads.
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @NullDelegate(NavigationDelegate::class)
    @Test
    fun load_canUnsetNavigationDelegate() {
        // Test that if we unset the navigation delegate during a load, the load still proceeds.
        var onLocationCount = 0
        mainSession.navigationDelegate = object : NavigationDelegate {
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                onLocationCount++
            }
        }
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        assertThat(
            "Should get callback for first load",
            onLocationCount,
            equalTo(1),
        )

        mainSession.reload()
        mainSession.navigationDelegate = null
        mainSession.waitForPageStop()

        assertThat(
            "Should not get callback for second load",
            onLocationCount,
            equalTo(1),
        )
    }

    @Test fun loadString() {
        val dataString = "<html><head><title>TheTitle</title></head><body>TheBody</body></html>"
        val mimeType = "text/html"
        mainSession.load(Loader().data(dataString, mimeType))
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate, ProgressDelegate, ContentDelegate {
            @AssertCalled
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Title should match", title, equalTo("TheTitle"))
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat(
                    "URL should be a data URL",
                    url,
                    equalTo(createDataUri(dataString, mimeType)),
                )
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @Test fun loadString_noMimeType() {
        mainSession.load(Loader().data("Hello, World!", null))
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate, ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should be a data URL", url, startsWith("data:"))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @Test fun loadData_html() {
        val bytes = getTestBytes(HELLO_HTML_PATH)
        assertThat("test html should have data", bytes.size, greaterThan(0))

        mainSession.load(Loader().data(bytes, "text/html"))
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate, ProgressDelegate, ContentDelegate {
            @AssertCalled(count = 1)
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Title should match", title, equalTo("Hello, world!"))
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, equalTo(createDataUri(bytes, "text/html")))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    private fun createDataUri(
        data: String,
        mimeType: String?,
    ): String {
        return String.format("data:%s,%s", mimeType ?: "", data)
    }

    private fun createDataUri(
        bytes: ByteArray,
        mimeType: String?,
    ): String {
        return String.format(
            "data:%s;base64,%s",
            mimeType ?: "",
            Base64.encodeToString(bytes, Base64.NO_WRAP),
        )
    }

    fun loadDataHelper(assetPath: String, mimeType: String? = null) {
        val bytes = getTestBytes(assetPath)
        assertThat("test data should have bytes", bytes.size, greaterThan(0))

        mainSession.load(Loader().data(bytes, mimeType))
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate, ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, equalTo(createDataUri(bytes, mimeType)))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @Test fun loadData() {
        loadDataHelper("/assets/www/images/test.gif", "image/gif")
    }

    @Test fun loadData_noMimeType() {
        loadDataHelper("/assets/www/images/test.gif")
    }

    @Test fun reload() {
        mainSession.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitForPageStop()

        mainSession.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("URI should match", request.uri, endsWith(HELLO_HTML_PATH))
                assertThat(
                    "Trigger URL should be null",
                    request.triggerUri,
                    nullValue(),
                )
                assertThat(
                    "Target should match",
                    request.target,
                    equalTo(NavigationDelegate.TARGET_WINDOW_CURRENT),
                )
                assertThat(
                    "Load should not be direct",
                    request.isDirectNavigation,
                    equalTo(false),
                )
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Cannot go back", canGoBack, equalTo(false))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }
        })
    }

    @Test fun goBackAndForward() {
        mainSession.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitForPageStop()

        mainSession.loadUri("$TEST_ENDPOINT$HELLO2_HTML_PATH")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }
        })

        mainSession.goBack()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 0, order = [1])
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat(
                    "Load should not be direct",
                    request.isDirectNavigation,
                    equalTo(false),
                )
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Cannot go back", canGoBack, equalTo(false))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Can go forward", canGoForward, equalTo(true))
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }
        })

        mainSession.goForward()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 0, order = [1])
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat(
                    "Load should not be direct",
                    request.isDirectNavigation,
                    equalTo(false),
                )
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Can go back", canGoBack, equalTo(true))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }
        })
    }

    @Test fun onLoadUri_returnTrueCancelsLoad() {
        sessionRule.delegateDuringNextWait(object : NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                if (request.uri.endsWith(HELLO_HTML_PATH)) {
                    return GeckoResult.deny()
                } else {
                    return GeckoResult.allow()
                }
            }
        })

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.loadTestPath(HELLO2_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    @Test fun onNewSession_calledForWindowOpen() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(NEW_SESSION_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("window.open('newSession_child.html', '_blank')")

        mainSession.waitUntilCalled(object : NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("URI should be correct", request.uri, endsWith(NEW_SESSION_CHILD_HTML_PATH))
                assertThat(
                    "Trigger URL should match",
                    request.triggerUri,
                    endsWith(NEW_SESSION_HTML_PATH),
                )
                assertThat(
                    "Target should be correct",
                    request.target,
                    equalTo(NavigationDelegate.TARGET_WINDOW_NEW),
                )
                assertThat(
                    "Load should not be direct",
                    request.isDirectNavigation,
                    equalTo(false),
                )
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                assertThat("URI should be correct", uri, endsWith(NEW_SESSION_CHILD_HTML_PATH))
                return null
            }
        })
    }

    @Test(expected = GeckoSessionTestRule.RejectedPromiseException::class)
    fun onNewSession_rejectLocal() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(NEW_SESSION_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("window.open('file:///data/local/tmp', '_blank')")
    }

    @Test fun onNewSession_calledForTargetBlankLink() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(NEW_SESSION_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("document.querySelector('#targetBlankLink').click()")

        mainSession.waitUntilCalled(object : NavigationDelegate {
            // We get two onLoadRequest calls for the link click,
            // one when loading the URL and one when opening a new window.
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat("URI should be correct", request.uri, endsWith(NEW_SESSION_CHILD_HTML_PATH))
                assertThat(
                    "Trigger URL should be null",
                    request.triggerUri,
                    endsWith(NEW_SESSION_HTML_PATH),
                )
                assertThat(
                    "Target should be correct",
                    request.target,
                    equalTo(NavigationDelegate.TARGET_WINDOW_NEW),
                )
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                assertThat("URI should be correct", uri, endsWith(NEW_SESSION_CHILD_HTML_PATH))
                return null
            }
        })
    }

    private fun delegateNewSession(settings: GeckoSessionSettings = mainSession.settings): GeckoSession {
        val newSession = sessionRule.createClosedSession(settings)

        mainSession.delegateDuringNextWait(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession> {
                return GeckoResult.fromValue(newSession)
            }
        })

        return newSession
    }

    @Test fun onNewSession_childShouldLoad() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(NEW_SESSION_HTML_PATH)
        mainSession.waitForPageStop()

        val newSession = delegateNewSession()
        mainSession.evaluateJS("document.querySelector('#targetBlankLink').click()")
        // Initial about:blank
        newSession.waitForPageStop()
        // NEW_SESSION_CHILD_HTML_PATH
        newSession.waitForPageStop()

        newSession.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(NEW_SESSION_CHILD_HTML_PATH))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    @Test fun onNewSession_setWindowOpener() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(NEW_SESSION_HTML_PATH)
        mainSession.waitForPageStop()

        val newSession = delegateNewSession()
        mainSession.evaluateJS("document.querySelector('#targetBlankLink').click()")
        newSession.waitForPageStop()

        assertThat(
            "window.opener should be set",
            newSession.evaluateJS("window.opener.location.pathname") as String,
            equalTo(NEW_SESSION_HTML_PATH),
        )
    }

    @Test fun onNewSession_supportNoOpener() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(NEW_SESSION_HTML_PATH)
        mainSession.waitForPageStop()

        val newSession = delegateNewSession()
        mainSession.evaluateJS("document.querySelector('#noOpenerLink').click()")
        newSession.waitForPageStop()

        assertThat(
            "window.opener should not be set",
            newSession.evaluateJS("window.opener"),
            equalTo(JSONObject.NULL),
        )
    }

    @Test fun onNewSession_notCalledForHandledLoads() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(NEW_SESSION_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.delegateDuringNextWait(object : NavigationDelegate {
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                // Pretend we handled the target="_blank" link click.
                if (request.uri.endsWith(NEW_SESSION_CHILD_HTML_PATH)) {
                    return GeckoResult.deny()
                } else {
                    return GeckoResult.allow()
                }
            }
        })

        mainSession.evaluateJS("document.querySelector('#targetBlankLink').click()")

        mainSession.reload()
        mainSession.waitForPageStop()

        // Assert that onNewSession was not called for the link click.
        mainSession.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat(
                    "URI must match",
                    request.uri,
                    endsWith(forEachCall(NEW_SESSION_CHILD_HTML_PATH, NEW_SESSION_HTML_PATH)),
                )
                assertThat(
                    "Load should not be direct",
                    request.isDirectNavigation,
                    equalTo(false),
                )
                return null
            }

            @AssertCalled(count = 0)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }
        })
    }

    @Test fun onNewSession_submitFormWithTargetBlank() {
        mainSession.loadTestPath(FORM_BLANK_HTML_PATH)
        sessionRule.waitForPageStop()

        mainSession.evaluateJS(
            """
            document.querySelector('input[type=text]').focus()
        """,
        )
        mainSession.waitUntilCalled(
            TextInputDelegate::class,
            "restartInput",
        )

        val time = SystemClock.uptimeMillis()
        val keyEvent = KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER, 0)
        mainSession.textInput.onKeyDown(KeyEvent.KEYCODE_ENTER, keyEvent)
        mainSession.textInput.onKeyUp(
            KeyEvent.KEYCODE_ENTER,
            KeyEvent.changeAction(
                keyEvent,
                KeyEvent.ACTION_UP,
            ),
        )

        mainSession.waitUntilCalled(object : NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest):
                GeckoResult<AllowOrDeny>? {
                assertThat(
                    "URL should be correct",
                    request.uri,
                    endsWith("form_blank.html?"),
                )
                assertThat(
                    "Trigger URL should match",
                    request.triggerUri,
                    endsWith("form_blank.html"),
                )
                assertThat(
                    "Target should be correct",
                    request.target,
                    equalTo(NavigationDelegate.TARGET_WINDOW_NEW),
                )
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onNewSession(session: GeckoSession, uri: String):
                GeckoResult<GeckoSession>? {
                assertThat("URL should be correct", uri, endsWith("form_blank.html?"))
                return null
            }
        })
    }

    @Test fun loadUriReferrer() {
        val uri = "https://example.com"
        val referrer = "https://foo.org/"

        mainSession.load(
            Loader()
                .uri(uri)
                .referrer(referrer)
                .flags(GeckoSession.LOAD_FLAGS_NONE),
        )
        mainSession.waitForPageStop()

        assertThat(
            "Referrer should match",
            mainSession.evaluateJS("document.referrer") as String,
            equalTo(referrer),
        )
    }

    @Test fun loadUriReferrerSession() {
        val uri = "https://example.com/bar"
        val referrer = "https://example.org/"

        mainSession.loadUri(referrer)
        mainSession.waitForPageStop()

        val newSession = sessionRule.createOpenSession()
        newSession.load(
            Loader()
                .uri(uri)
                .referrer(mainSession)
                .flags(GeckoSession.LOAD_FLAGS_NONE),
        )
        newSession.waitForPageStop()

        assertThat(
            "Referrer should match",
            newSession.evaluateJS("document.referrer") as String,
            equalTo(referrer),
        )
    }

    @Test fun loadUriReferrerSessionFileUrl() {
        val uri = "file:///system/etc/fonts.xml"
        val referrer = "https://example.org"

        mainSession.loadUri(referrer)
        mainSession.waitForPageStop()

        val newSession = sessionRule.createOpenSession()
        newSession.load(
            Loader()
                .uri(uri)
                .referrer(mainSession)
                .flags(GeckoSession.LOAD_FLAGS_NONE),
        )
        newSession.waitUntilCalled(object : NavigationDelegate {
            @AssertCalled
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })
    }

    private fun loadUriHeaderTest(
        headers: Map<String?, String?>,
        additional: Map<String?, String?>,
        filter: Int = GeckoSession.HEADER_FILTER_CORS_SAFELISTED,
    ) {
        // First collect default headers with no override
        mainSession.loadUri("$TEST_ENDPOINT/anything")
        mainSession.waitForPageStop()

        val defaultContent = mainSession.evaluateJS("document.body.children[0].innerHTML") as String
        val defaultBody = JSONObject(defaultContent)
        val defaultHeaders = defaultBody.getJSONObject("headers").asMap<String>()

        val expected = HashMap(additional)
        for (key in defaultHeaders.keys) {
            expected[key] = defaultHeaders[key]
            if (additional.containsKey(key)) {
                // TODO: Bug 1671294, headers should be replaced, not appended
                expected[key] += ", " + additional[key]
            }
        }

        // Now load the page with the header override
        mainSession.load(
            Loader()
                .uri("$TEST_ENDPOINT/anything")
                .additionalHeaders(headers)
                .headerFilter(filter),
        )
        mainSession.waitForPageStop()

        val content = mainSession.evaluateJS("document.body.children[0].innerHTML") as String
        val body = JSONObject(content)
        val actualHeaders = body.getJSONObject("headers").asMap<String>()

        assertThat(
            "Headers should match",
            expected as Map<String?, String?>,
            equalTo(actualHeaders),
        )
    }

    private fun testLoaderEquals(a: Loader, b: Loader, shouldBeEqual: Boolean) {
        assertThat("Equal test", a == b, equalTo(shouldBeEqual))
        assertThat(
            "HashCode test",
            a.hashCode() == b.hashCode(),
            equalTo(shouldBeEqual),
        )
    }

    @Test fun loaderEquals() {
        testLoaderEquals(
            Loader().uri("http://test-uri-equals.com"),
            Loader().uri("http://test-uri-equals.com"),
            true,
        )
        testLoaderEquals(
            Loader().uri("http://test-uri-equals.com"),
            Loader().uri("http://test-uri-equalsx.com"),
            false,
        )

        testLoaderEquals(
            Loader().uri("http://test-uri-equals.com")
                .flags(GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER)
                .headerFilter(GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE)
                .referrer("test-referrer"),
            Loader().uri("http://test-uri-equals.com")
                .flags(GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER)
                .headerFilter(GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE)
                .referrer("test-referrer"),
            true,
        )
        testLoaderEquals(
            Loader().uri("http://test-uri-equals.com")
                .flags(GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER)
                .headerFilter(GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE)
                .referrer(mainSession),
            Loader().uri("http://test-uri-equals.com")
                .flags(GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER)
                .headerFilter(GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE)
                .referrer("test-referrer"),
            false,
        )

        testLoaderEquals(
            Loader().referrer(mainSession)
                .data("testtest", "text/plain"),
            Loader().referrer(mainSession)
                .data("testtest", "text/plain"),
            true,
        )
        testLoaderEquals(
            Loader().referrer(mainSession)
                .data("testtest", "text/plain"),
            Loader().referrer("test-referrer")
                .data("testtest", "text/plain"),
            false,
        )
    }

    @Test fun loadUriHeader() {
        // Basic test
        loadUriHeaderTest(
            mapOf("Header1" to "Value", "Header2" to "Value1, Value2"),
            mapOf(),
        )
        loadUriHeaderTest(
            mapOf("Header1" to "Value", "Header2" to "Value1, Value2"),
            mapOf("Header1" to "Value", "Header2" to "Value1, Value2"),
            GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE,
        )

        // Empty value headers are ignored
        loadUriHeaderTest(
            mapOf("ValueLess1" to "", "ValueLess2" to null),
            mapOf(),
        )

        // Null key or special headers are ignored
        loadUriHeaderTest(
            mapOf(
                null to "BadNull",
                "Connection" to "BadConnection",
                "Host" to "BadHost",
            ),
            mapOf(),
        )

        // Key or value cannot contain '\r\n'
        loadUriHeaderTest(
            mapOf(
                "Header1" to "Value",
                "Header2" to "Value1, Value2",
                "this\r\nis invalid" to "test value",
                "test key" to "this\r\n is a no-no",
                "what" to "what\r\nhost:amazon.com",
                "Header3" to "Value1, Value2, Value3",
            ),
            mapOf(),
        )
        loadUriHeaderTest(
            mapOf(
                "Header1" to "Value",
                "Header2" to "Value1, Value2",
                "this\r\nis invalid" to "test value",
                "test key" to "this\r\n is a no-no",
                "what" to "what\r\nhost:amazon.com",
                "Header3" to "Value1, Value2, Value3",
            ),
            mapOf(
                "Header1" to "Value",
                "Header2" to "Value1, Value2",
                "Header3" to "Value1, Value2, Value3",
            ),
            GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE,
        )

        loadUriHeaderTest(
            mapOf(
                "Header1" to "Value",
                "Header2" to "Value1, Value2",
                "what" to "what\r\nhost:amazon.com",
            ),
            mapOf(),
        )
        loadUriHeaderTest(
            mapOf(
                "Header1" to "Value",
                "Header2" to "Value1, Value2",
                "what" to "what\r\nhost:amazon.com",
            ),
            mapOf("Header1" to "Value", "Header2" to "Value1, Value2"),
            GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE,
        )

        loadUriHeaderTest(
            mapOf("what" to "what\r\nhost:amazon.com"),
            mapOf(),
        )

        loadUriHeaderTest(
            mapOf("this\r\n" to "yes"),
            mapOf(),
        )

        // Connection and Host cannot be overriden, no matter the case spelling
        loadUriHeaderTest(
            mapOf("Header1" to "Value1", "ConnEction" to "test", "connection" to "test2"),
            mapOf(),
        )
        loadUriHeaderTest(
            mapOf("Header1" to "Value1", "ConnEction" to "test", "connection" to "test2"),
            mapOf("Header1" to "Value1"),
            GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE,
        )

        loadUriHeaderTest(
            mapOf("Header1" to "Value1", "connection" to "test2"),
            mapOf(),
        )
        loadUriHeaderTest(
            mapOf("Header1" to "Value1", "connection" to "test2"),
            mapOf("Header1" to "Value1"),
            GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE,
        )

        loadUriHeaderTest(
            mapOf("Header1   " to "Value1", "host" to "test2"),
            mapOf(),
        )
        loadUriHeaderTest(
            mapOf("Header1   " to "Value1", "host" to "test2"),
            mapOf("Header1" to "Value1"),
            GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE,
        )

        loadUriHeaderTest(
            mapOf("Header1" to "Value1", "host" to "test2"),
            mapOf(),
        )
        loadUriHeaderTest(
            mapOf("Header1" to "Value1", "host" to "test2"),
            mapOf("Header1" to "Value1"),
            GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE,
        )

        // Adding white space at the end of a forbidden header still prevents override
        loadUriHeaderTest(
            mapOf(
                "host" to "amazon.com",
                "host " to "amazon.com",
                "host\r" to "amazon.com",
                "host\r\n" to "amazon.com",
            ),
            mapOf(),
        )

        // '\r' or '\n' are forbidden character even when not following each other
        loadUriHeaderTest(
            mapOf("abc\ra\n" to "amazon.com"),
            mapOf(),
        )

        // CORS Safelist test
        loadUriHeaderTest(
            mapOf(
                "Accept-Language" to "fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
                "Accept" to "text/html",
                "Content-Language" to "de-DE, en-CA",
                "Content-Type" to "multipart/form-data; boundary=something",
            ),
            mapOf(
                "Accept-Language" to "fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
                "Accept" to "text/html",
                "Content-Language" to "de-DE, en-CA",
                "Content-Type" to "multipart/form-data; boundary=something",
            ),
            GeckoSession.HEADER_FILTER_CORS_SAFELISTED,
        )

        // CORS safelist doesn't allow Content-type image/svg
        loadUriHeaderTest(
            mapOf(
                "Accept-Language" to "fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
                "Accept" to "text/html",
                "Content-Language" to "de-DE, en-CA",
                "Content-Type" to "image/svg; boundary=something",
            ),
            mapOf(
                "Accept-Language" to "fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
                "Accept" to "text/html",
                "Content-Language" to "de-DE, en-CA",
            ),
            GeckoSession.HEADER_FILTER_CORS_SAFELISTED,
        )
    }

    @Test(expected = GeckoResult.UncaughtException::class)
    fun onNewSession_doesNotAllowOpened() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(NEW_SESSION_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.delegateDuringNextWait(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession> {
                return GeckoResult.fromValue(sessionRule.createOpenSession())
            }
        })

        mainSession.evaluateJS("document.querySelector('#targetBlankLink').click()")

        mainSession.waitUntilCalled(
            NavigationDelegate::class,
            "onNewSession",
        )
        UiThreadUtils.loopUntilIdle(sessionRule.env.defaultTimeoutMillis)
    }

    @Test
    fun extensionProcessSwitching() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false,
            ),
        )

        val controller = sessionRule.runtime.webExtensionController

        sessionRule.delegateUntilTestEnd(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        val onReadyResult = GeckoResult<String>()
        var extBaseUrl = ""
        sessionRule.addExternalDelegateUntilTestEnd(
            WebExtensionController.AddonManagerDelegate::class,
            { delegate -> controller.setAddonManagerDelegate(delegate) },
            { controller.setAddonManagerDelegate(null) },
            object : WebExtensionController.AddonManagerDelegate {
                @AssertCalled(count = 1)
                override fun onReady(extension: WebExtension) {
                    extBaseUrl = extension.metaData.baseUrl
                    onReadyResult.complete(null)
                    super.onReady(extension)
                }
            },
        )

        val extension = sessionRule.waitForResult(
            controller.install("https://example.org/tests/junit/page-history.xpi"),
        )

        // Wait for the extension to have been started before trying to navigate
        // to the test extension page.
        sessionRule.waitForResult(onReadyResult)

        assertThat(
            "baseUrl should be a valid extension URL",
            extBaseUrl,
            startsWith("moz-extension://"),
        )

        val url = extBaseUrl + "page.html"
        processSwitchingTest(url)

        sessionRule.waitForResult(controller.uninstall(extension))
    }

    @Test
    fun mainProcessSwitching() {
        processSwitchingTest("about:config")
    }

    private fun processSwitchingTest(url: String) {
        val settings = sessionRule.runtime.settings
        val aboutConfigEnabled = settings.aboutConfigEnabled
        settings.aboutConfigEnabled = true

        var currentUrl: String? = null
        mainSession.delegateUntilTestEnd(object : NavigationDelegate {
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                currentUrl = url
            }

            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                assertThat("Should not get here", false, equalTo(true))
                return null
            }
        })

        // This will load a page in the child
        mainSession.loadTestPath(HELLO2_HTML_PATH)
        sessionRule.waitForPageStop()

        assertThat(
            "docShell should start out active",
            mainSession.active,
            equalTo(true),
        )

        // This loads in the parent process
        mainSession.loadUri(url)
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, equalTo(url))

        // This will load a page in the child
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, endsWith(HELLO_HTML_PATH))
        assertThat(
            "docShell should be active after switching process",
            mainSession.active,
            equalTo(true),
        )

        mainSession.loadUri(url)
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, equalTo(url))

        mainSession.goBack()
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, endsWith(HELLO_HTML_PATH))
        assertThat(
            "docShell should be active after switching process",
            mainSession.active,
            equalTo(true),
        )

        mainSession.goBack()
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, equalTo(url))

        mainSession.goBack()
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, endsWith(HELLO2_HTML_PATH))
        assertThat(
            "docShell should be active after switching process",
            mainSession.active,
            equalTo(true),
        )

        settings.aboutConfigEnabled = aboutConfigEnabled
    }

    @Test fun setLocationHash() {
        mainSession.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitForPageStop()

        mainSession.evaluateJS("location.hash = 'test1';")

        mainSession.waitUntilCalled(object : NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                assertThat(
                    "Load should not be direct",
                    request.isDirectNavigation,
                    equalTo(false),
                )
                return null
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URI should match", url, endsWith("#test1"))
            }
        })

        mainSession.evaluateJS("location.hash = 'test2';")

        mainSession.waitUntilCalled(object : NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ):
                GeckoResult<AllowOrDeny>? {
                return null
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URI should match", url, endsWith("#test2"))
            }
        })
    }

    @Test fun purgeHistory() {
        // TODO: Bug 1837551
        assumeThat(sessionRule.env.isFission, equalTo(false))

        mainSession.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitUntilCalled(object : HistoryDelegate, NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go back", canGoBack, equalTo(false))
            }

            @AssertCalled(count = 1)
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }

            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: HistoryDelegate.HistoryList) {
                assertThat("History should have one entry", state.size, equalTo(1))
            }
        })
        mainSession.loadUri("$TEST_ENDPOINT$HELLO2_HTML_PATH")
        sessionRule.waitUntilCalled(object : HistoryDelegate, NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go back", canGoBack, equalTo(true))
            }

            @AssertCalled(count = 1)
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }

            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: HistoryDelegate.HistoryList) {
                assertThat("History should have two entries", state.size, equalTo(2))
            }
        })
        mainSession.purgeHistory()
        sessionRule.waitUntilCalled(object : HistoryDelegate, NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: HistoryDelegate.HistoryList) {
                assertThat("History should have one entry", state.size, equalTo(1))
            }

            @AssertCalled(count = 1)
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go back", canGoBack, equalTo(false))
            }

            @AssertCalled(count = 1)
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun userGesture() {
        mainSession.loadUri("$TEST_ENDPOINT$CLICK_TO_RELOAD_HTML_PATH")
        mainSession.waitForPageStop()

        mainSession.synthesizeTap(50, 50)

        sessionRule.waitUntilCalled(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("Should have a user gesture", request.hasUserGesture, equalTo(true))
                assertThat(
                    "Load should not be direct",
                    request.isDirectNavigation,
                    equalTo(false),
                )
                return GeckoResult.allow()
            }
        })
    }

    @Test fun loadAfterLoad() {
        mainSession.delegateDuringNextWait(object : NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("URLs should match", request.uri, endsWith(forEachCall(HELLO_HTML_PATH, HELLO2_HTML_PATH)))
                return GeckoResult.allow()
            }
        })

        mainSession.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        mainSession.loadUri("$TEST_ENDPOINT$HELLO2_HTML_PATH")
        mainSession.waitForPageStop()
    }

    @Test
    fun loadLongDataUriToplevelDirect() {
        val dataBytes = ByteArray(3 * 1024 * 1024)
        val expectedUri = createDataUri(dataBytes, "*/*")
        val loader = Loader().data(dataBytes, "*/*")

        mainSession.delegateUntilTestEnd(object : NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("URLs should match", request.uri, equalTo(expectedUri))
                return GeckoResult.allow()
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLoadError(
                session: GeckoSession,
                uri: String?,
                error: WebRequestError,
            ): GeckoResult<String>? {
                assertThat(
                    "Error category should match",
                    error.category,
                    equalTo(WebRequestError.ERROR_CATEGORY_URI),
                )
                assertThat(
                    "Error code should match",
                    error.code,
                    equalTo(WebRequestError.ERROR_DATA_URI_TOO_LONG),
                )
                assertThat("URLs should match", uri, equalTo(expectedUri))
                return null
            }
        })

        mainSession.load(loader)
        sessionRule.waitUntilCalled(NavigationDelegate::class, "onLoadError")
    }

    @Test
    fun loadLongDataUriToplevelIndirect() {
        val dataBytes = ByteArray(3 * 1024 * 1024)
        val dataUri = createDataUri(dataBytes, "*/*")

        mainSession.loadTestPath(DATA_URI_PATH)
        mainSession.waitForPageStop()

        mainSession.delegateUntilTestEnd(object : NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return GeckoResult.deny()
            }
        })

        mainSession.evaluateJS("document.querySelector('#largeLink').href = \"$dataUri\"")
        mainSession.evaluateJS("document.querySelector('#largeLink').click()")
        mainSession.waitForPageStop()
    }

    @Test
    @NullDelegate(NavigationDelegate::class)
    fun loadOnBackgroundThreadNullNavigationDelegate() {
        thread {
            // Make sure we're running in a thread without a Looper.
            assertThat(
                "We should not have a looper.",
                Looper.myLooper(),
                equalTo(null),
            )
            mainSession.loadTestPath(HELLO_HTML_PATH)
        }

        mainSession.waitUntilCalled(object : ProgressDelegate {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page loaded successfully", success, equalTo(true))
            }
        })
    }

    @Test
    fun invalidScheme() {
        val invalidUri = "tel:#12345678"
        mainSession.loadUri(invalidUri)
        mainSession.waitUntilCalled(object : NavigationDelegate {
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                assertThat("Uri should match", uri, equalTo(invalidUri))
                assertThat(
                    "error should match",
                    error.code,
                    equalTo(WebRequestError.ERROR_MALFORMED_URI),
                )
                assertThat(
                    "error should match",
                    error.category,
                    equalTo(WebRequestError.ERROR_CATEGORY_URI),
                )
                return null
            }
        })
    }

    @Test
    fun loadOnBackgroundThread() {
        mainSession.delegateUntilTestEnd(object : NavigationDelegate {
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return GeckoResult.allow()
            }
        })

        thread {
            // Make sure we're running in a thread without a Looper.
            assertThat(
                "We should not have a looper.",
                Looper.myLooper(),
                equalTo(null),
            )
            mainSession.loadTestPath(HELLO_HTML_PATH)
        }

        mainSession.waitUntilCalled(object : ProgressDelegate {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page loaded successfully", success, equalTo(true))
            }
        })
    }

    @Test
    fun loadShortDataUriToplevelIndirect() {
        mainSession.delegateUntilTestEnd(object : NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return GeckoResult.allow()
            }

            @AssertCalled(false)
            override fun onLoadError(
                session: GeckoSession,
                uri: String?,
                error: WebRequestError,
            ): GeckoResult<String>? {
                return null
            }
        })

        val dataBytes = this.getTestBytes("/assets/www/images/test.gif")
        val uri = createDataUri(dataBytes, "image/*")

        mainSession.loadTestPath(DATA_URI_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("document.querySelector('#smallLink').href = \"$uri\"")
        mainSession.evaluateJS("document.querySelector('#smallLink').click()")
        mainSession.waitForPageStop()
    }

    fun createLargeHighEntropyImageDataUri(): String {
        val desiredMinSize = (2 * 1024 * 1024) + 1

        val width = 768
        val height = 768

        val bitmap = Bitmap.createBitmap(
            ThreadLocalRandom.current().ints(width.toLong() * height.toLong()).toArray(),
            width,
            height,
            Bitmap.Config.ARGB_8888,
        )

        val stream = ByteArrayOutputStream()
        if (!bitmap.compress(Bitmap.CompressFormat.PNG, 0, stream)) {
            throw Exception("Error compressing PNG")
        }

        val uri = createDataUri(stream.toByteArray(), "image/png")

        if (uri.length < desiredMinSize) {
            throw Exception("Test uri is too small, want at least " + desiredMinSize + ", got " + uri.length)
        }

        return uri
    }

    @Test
    fun loadLongDataUriNonToplevel() {
        val dataUri = createLargeHighEntropyImageDataUri()

        mainSession.delegateUntilTestEnd(object : NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return GeckoResult.allow()
            }

            @AssertCalled(false)
            override fun onLoadError(
                session: GeckoSession,
                uri: String?,
                error: WebRequestError,
            ): GeckoResult<String>? {
                return null
            }
        })

        mainSession.loadTestPath(DATA_URI_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("document.querySelector('#image').onload = () => { imageLoaded = true; }")
        mainSession.evaluateJS("document.querySelector('#image').src = \"$dataUri\"")
        UiThreadUtils.waitForCondition({
            mainSession.evaluateJS("document.querySelector('#image').complete") as Boolean
        }, sessionRule.env.defaultTimeoutMillis)
        mainSession.evaluateJS("if (!imageLoaded) throw imageLoaded")
    }

    @Test
    fun bypassLoadUriDelegate() {
        val testUri = "https://www.mozilla.org"

        mainSession.load(
            Loader()
                .uri(testUri)
                .flags(GeckoSession.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE),
        )
        mainSession.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
            object : NavigationDelegate {
                @AssertCalled(false)
                override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                    return null
                }
            },
        )
    }

    @Test fun goBackFromHistory() {
        // TODO: Bug 1837551
        assumeThat(sessionRule.env.isFission, equalTo(false))

        mainSession.loadTestPath(HELLO_HTML_PATH)

        mainSession.waitUntilCalled(object : HistoryDelegate, ContentDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: HistoryDelegate.HistoryList) {
                assertThat("History should have one entry", state.size, equalTo(1))
            }

            @AssertCalled(count = 1)
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Title should match", title, equalTo("Hello, world!"))
            }
        })

        mainSession.loadTestPath(HELLO2_HTML_PATH)

        mainSession.waitUntilCalled(object : HistoryDelegate, NavigationDelegate, ContentDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: HistoryDelegate.HistoryList) {
                assertThat("History should have two entry", state.size, equalTo(2))
            }

            @AssertCalled(count = 1)
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Can go back", canGoBack, equalTo(true))
            }

            @AssertCalled(count = 1)
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Title should match", title, equalTo("Hello, world! Again!"))
            }
        })

        // goBack will be navigated from history.

        var lastTitle: String? = ""
        sessionRule.delegateDuringNextWait(object : NavigationDelegate, ContentDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<PermissionDelegate.ContentPermission>,
            ) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled
            override fun onTitleChange(session: GeckoSession, title: String?) {
                lastTitle = title
            }
        })

        mainSession.goBack()
        sessionRule.waitForPageStop()
        assertThat("Title should match", lastTitle, equalTo("Hello, world!"))
    }

    @Test
    fun loadAndroidAssets() {
        val assetUri = "resource://android/assets/web_extensions/"
        mainSession.loadUri(assetUri)

        mainSession.waitUntilCalled(object : ProgressDelegate {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page loaded successfully", success, equalTo(true))
            }
        })
    }
}
