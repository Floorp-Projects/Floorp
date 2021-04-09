/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.Bitmap
import android.os.SystemClock
import android.view.KeyEvent
import android.util.Base64
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import java.io.ByteArrayOutputStream
import java.util.concurrent.ThreadLocalRandom
import org.hamcrest.Matchers.*
import org.json.JSONObject
import org.junit.Assume.assumeThat
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*
import org.mozilla.geckoview.GeckoSession.*
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.*
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.test.util.UiThreadUtils

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
        fun getUri() : String? {
            return mUri
        }
        override fun flags(f: Int): TestLoader {
            super.flags(f)
            return this
        }
    }

    fun testLoadErrorWithErrorPage(testLoader: TestLoader, expectedCategory: Int,
                                   expectedError: Int,
                                   errorPageUrl: String?) {
        sessionRule.delegateDuringNextWait(
                object : Callbacks.ProgressDelegate, Callbacks.NavigationDelegate, Callbacks.ContentDelegate {
                    @AssertCalled(count = 1, order = [1])
                    override fun onLoadRequest(session: GeckoSession,
                                               request: LoadRequest):
                                               GeckoResult<AllowOrDeny>? {
                        assertThat("URI should be " + testLoader.getUri(), request.uri,
                                equalTo(testLoader.getUri()))
                        assertThat("App requested this load", request.isDirectNavigation,
                                equalTo(true))
                        return null
                    }

                    @AssertCalled(count = 1, order = [2])
                    override fun onPageStart(session: GeckoSession, url: String) {
                        assertThat("URI should be " + testLoader.getUri(), url,
                                equalTo(testLoader.getUri()))
                    }

                    @AssertCalled(count = 1, order = [3])
                    override fun onLoadError(session: GeckoSession, uri: String?,
                                             error: WebRequestError): GeckoResult<String>? {
                        assertThat("Error category should match", error.category,
                                equalTo(expectedCategory))
                        assertThat("Error code should match", error.code,
                                equalTo(expectedError))
                        return GeckoResult.fromValue(errorPageUrl)
                    }

                    @AssertCalled(count = 1, order = [4])
                    override fun onPageStop(session: GeckoSession, success: Boolean) {
                        assertThat("Load should fail", success, equalTo(false))
                    }
                })

        sessionRule.session.load(testLoader)
        sessionRule.waitForPageStop()

        if (errorPageUrl != null) {
            sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate, Callbacks.NavigationDelegate {
                @AssertCalled(count = 1, order = [1])
                override fun onLocationChange(session: GeckoSession, url: String?) {
                    assertThat("URL should match", url, equalTo(testLoader.getUri()))
                }

                @AssertCalled(count = 1, order = [2])
                override fun onTitleChange(session: GeckoSession, title: String?) {
                    assertThat("Title should not be empty", title, not(isEmptyOrNullString()))
                }
            })
        }
    }

    fun testLoadExpectError(testUri: String, expectedCategory: Int,
                            expectedError: Int) {
        testLoadExpectError(TestLoader().uri(testUri), expectedCategory, expectedError)
    }

    fun testLoadExpectError(testLoader: TestLoader, expectedCategory: Int,
                            expectedError: Int) {
        testLoadErrorWithErrorPage(testLoader, expectedCategory,
                expectedError, createTestUrl(HELLO_HTML_PATH))
        testLoadErrorWithErrorPage(testLoader, expectedCategory,
                expectedError, null)
    }

    fun testLoadEarlyErrorWithErrorPage(testUri: String, expectedCategory: Int,
                                        expectedError: Int,
                                        errorPageUrl: String?) {
        sessionRule.delegateDuringNextWait(
                object : Callbacks.ProgressDelegate, Callbacks.NavigationDelegate, Callbacks.ContentDelegate {

                    @AssertCalled(false)
                    override fun onPageStart(session: GeckoSession, url: String) {
                        assertThat("URI should be " + testUri, url, equalTo(testUri))
                    }

                    @AssertCalled(count = 1, order = [1])
                    override fun onLoadError(session: GeckoSession, uri: String?,
                                             error: WebRequestError): GeckoResult<String>? {
                        assertThat("Error category should match", error.category,
                                equalTo(expectedCategory))
                        assertThat("Error code should match", error.code,
                                equalTo(expectedError))
                        return GeckoResult.fromValue(errorPageUrl)
                    }

                    @AssertCalled(false)
                    override fun onPageStop(session: GeckoSession, success: Boolean) {
                    }
                })

        sessionRule.session.loadUri(testUri)
        sessionRule.waitUntilCalled(Callbacks.NavigationDelegate::class, "onLoadError")

        if (errorPageUrl != null) {
            sessionRule.waitUntilCalled(object: Callbacks.ContentDelegate {
                @AssertCalled(count = 1)
                override fun onTitleChange(session: GeckoSession, title: String?) {
                    assertThat("Title should not be empty", title, not(isEmptyOrNullString()))
                }
            })
        }
    }

    fun testLoadEarlyError(testUri: String, expectedCategory: Int,
                           expectedError: Int) {
        testLoadEarlyErrorWithErrorPage(testUri, expectedCategory, expectedError, createTestUrl(HELLO_HTML_PATH))
        testLoadEarlyErrorWithErrorPage(testUri, expectedCategory, expectedError, null)
    }

    @Test fun loadFileNotFound() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        testLoadExpectError("file:///test.mozilla",
                WebRequestError.ERROR_CATEGORY_URI,
                WebRequestError.ERROR_FILE_NOT_FOUND)

        val promise = mainSession.evaluatePromiseJS("document.addCertException(false)")
        var exceptionCaught = false
        try {
            val result = promise.value as Boolean
            assertThat("Promise should not resolve", result, equalTo(false))
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            exceptionCaught = true;
        }
        assertThat("document.addCertException failed with exception", exceptionCaught, equalTo(true))
    }

    @Test fun loadUnknownHost() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        testLoadExpectError(UNKNOWN_HOST_URI,
                WebRequestError.ERROR_CATEGORY_URI,
                WebRequestError.ERROR_UNKNOWN_HOST)
    }

    // External loads should not have access to privileged protocols
    @Test fun loadExternalDenied() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        testLoadExpectError(TestLoader().uri("file:///").flags(LOAD_FLAGS_EXTERNAL),
                WebRequestError.ERROR_CATEGORY_UNKNOWN,
                WebRequestError.ERROR_UNKNOWN)
        testLoadExpectError(TestLoader().uri("resource://gre/").flags(LOAD_FLAGS_EXTERNAL),
                WebRequestError.ERROR_CATEGORY_UNKNOWN,
                WebRequestError.ERROR_UNKNOWN)
        testLoadExpectError(TestLoader().uri("about:about").flags(LOAD_FLAGS_EXTERNAL),
                WebRequestError.ERROR_CATEGORY_UNKNOWN,
                WebRequestError.ERROR_UNKNOWN)
    }

    @Test fun loadInvalidUri() {
        testLoadEarlyError(INVALID_URI,
                WebRequestError.ERROR_CATEGORY_URI,
                WebRequestError.ERROR_MALFORMED_URI)
    }

    @Test fun loadBadPort() {
        testLoadEarlyError("http://localhost:1/",
                WebRequestError.ERROR_CATEGORY_NETWORK,
                WebRequestError.ERROR_PORT_BLOCKED)
    }

    @Test fun loadUntrusted() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val host = if (sessionRule.env.isAutomation) {
            "expired.example.com"
        } else {
            "expired.badssl.com"
        }
        val uri = "https://$host/"
        testLoadExpectError(uri,
                WebRequestError.ERROR_CATEGORY_SECURITY,
                WebRequestError.ERROR_SECURITY_BAD_CERT)

        mainSession.waitForJS("document.addCertException(false)")
        mainSession.delegateDuringNextWait(
                object : Callbacks.ProgressDelegate, Callbacks.NavigationDelegate, Callbacks.ContentDelegate {
                    @AssertCalled(count = 1, order = [1])
                    override fun onPageStart(session: GeckoSession, url: String) {
                        assertThat("URI should be " + uri, url, equalTo(uri))
                    }

                    @AssertCalled(count = 1, order = [2])
                    override fun onPageStop(session: GeckoSession, success: Boolean) {
                        assertThat("Load should succeed", success, equalTo(true))
                        sessionRule.removeCertOverride(host, -1)
                    }
                })
        mainSession.evaluateJS("location.reload()")
        mainSession.waitForPageStop()
    }

    @Test fun loadDeprecatedTls() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        // Load an initial generic error page in order to ensure 'allowDeprecatedTls' is false
        testLoadExpectError(UNKNOWN_HOST_URI,
                WebRequestError.ERROR_CATEGORY_URI,
                WebRequestError.ERROR_UNKNOWN_HOST)
        mainSession.evaluateJS("document.allowDeprecatedTls = false")

        val uri = if (sessionRule.env.isAutomation) {
            "https://tls1.example.com/"
        } else {
            "https://tls-v1-0.badssl.com:1010/"
        }
        testLoadExpectError(uri,
                WebRequestError.ERROR_CATEGORY_SECURITY,
                WebRequestError.ERROR_SECURITY_SSL)

        mainSession.delegateDuringNextWait(object : Callbacks.ProgressDelegate, Callbacks.NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should be successful", success, equalTo(true))
            }
        })

        mainSession.evaluateJS("document.allowDeprecatedTls = true")
        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @Test fun loadWithHTTPSOnlyMode() {
        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.HTTPS_ONLY)

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

        mainSession.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                assertThat("categories should match", error.category, equalTo(WebRequestError.ERROR_CATEGORY_SECURITY))
                assertThat("codes should match", error.code, equalTo(WebRequestError.ERROR_SECURITY_BAD_CERT))
                return null
            }
        })

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.ALLOW_ALL)

        mainSession.loadUri(secureUri)
        mainSession.waitForPageStop()

        mainSession.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }

            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return null
            }
        })

        val privateSession = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder(mainSession.settings)
                        .usePrivateMode(true)
                        .build())

        privateSession.loadUri(secureUri)
        privateSession.waitForPageStop()

        privateSession.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }

            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return null
            }
        })

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.HTTPS_ONLY_PRIVATE)

        privateSession.loadUri(insecureUri)
        privateSession.waitForPageStop()

        privateSession.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                assertThat("categories should match", error.category, equalTo(WebRequestError.ERROR_CATEGORY_SECURITY))
                assertThat("codes should match", error.code, equalTo(WebRequestError.ERROR_SECURITY_BAD_CERT))
                return null
            }
        })

        mainSession.loadUri(secureUri)
        mainSession.waitForPageStop()

        mainSession.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }

            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return null
            }
        })

        sessionRule.runtime.settings.setAllowInsecureConnections(GeckoRuntimeSettings.ALLOW_ALL)
    }

    @Ignore // Disabled for bug 1619344.
    @Test fun loadUnknownProtocol() {
        testLoadEarlyError(UNKNOWN_PROTOCOL_URI,
                WebRequestError.ERROR_CATEGORY_URI,
                WebRequestError.ERROR_UNKNOWN_PROTOCOL)
    }

    @Test fun loadUnknownProtocolIframe() {
        // Should match iframe URI from IFRAME_UNKNOWN_PROTOCOL
        val iframeUri = "foo://bar"
        sessionRule.session.loadTestPath(IFRAME_UNKNOWN_PROTOCOL)
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest) : GeckoResult<AllowOrDeny>? {
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URI should match", request.uri, endsWith(IFRAME_UNKNOWN_PROTOCOL))
                return null
            }

            @AssertCalled(count = 1)
            override fun onSubframeLoadRequest(session: GeckoSession,
                                               request: LoadRequest):
                                               GeckoResult<AllowOrDeny>? {
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URI should match", request.uri, endsWith(iframeUri))
                return null
            }
        })
    }

    @Setting(key = Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Ignore // TODO: Bug 1564373
    @Test fun trackingProtection() {
        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)
        sessionRule.session.loadTestPath(TRACKERS_PATH)

        sessionRule.waitUntilCalled(
                object : Callbacks.ContentBlockingDelegate {
            @AssertCalled(count = 3)
            override fun onContentBlocked(session: GeckoSession,
                                          event: ContentBlocking.BlockEvent) {
                assertThat("Category should be set",
                           event.antiTrackingCategory,
                           equalTo(category))
                assertThat("URI should not be null", event.uri, notNullValue())
                assertThat("URI should match", event.uri, endsWith("tracker.js"))
            }

            @AssertCalled(false)
            override fun onContentLoaded(session: GeckoSession, event: ContentBlocking.BlockEvent) {
            }
        })

        sessionRule.session.settings.useTrackingProtection = false

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.ContentBlockingDelegate {
            @AssertCalled(false)
            override fun onContentBlocked(session: GeckoSession,
                                          event: ContentBlocking.BlockEvent) {
            }

            @AssertCalled(count = 3)
            override fun onContentLoaded(session: GeckoSession, event: ContentBlocking.BlockEvent) {
                assertThat("Category should be set",
                           event.antiTrackingCategory,
                           equalTo(category))
                assertThat("URI should not be null", event.uri, notNullValue())
                assertThat("URI should match", event.uri, endsWith("tracker.js"))
            }
        })
    }

    @Test fun redirectLoad() {
        val redirectUri = if (sessionRule.env.isAutomation) {
            "http://example.org/tests/junit/hello.html"
        } else {
            "http://jigsaw.w3.org/HTTP/300/Overview.html"
        }
        val uri = if (sessionRule.env.isAutomation) {
            "http://example.org/tests/junit/simple_redirect.sjs?$redirectUri"
        } else {
            "http://jigsaw.w3.org/HTTP/300/301.html"
        }

        sessionRule.session.loadUri(uri)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2, order = [1, 2])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URL should match", request.uri,
                        equalTo(forEachCall(request.uri, redirectUri)))
                assertThat("Trigger URL should be null", request.triggerUri,
                           nullValue())
                assertThat("From app should be correct", request.isDirectNavigation,
                        equalTo(forEachCall(true, false)))
                assertThat("Target should not be null", request.target, notNullValue())
                assertThat("Target should match", request.target,
                        equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                assertThat("Redirect flag is set", request.isRedirect,
                        equalTo(forEachCall(false, true)))
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

        sessionRule.session.loadTestPath(path)
        sessionRule.waitForPageStop()

        // We shouldn't be firing onLoadRequest for iframes, including redirects.
        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                    GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("App requested this load", request.isDirectNavigation, equalTo(true))
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URI should match", request.uri, endsWith(path))
                assertThat("isRedirect should match", request.isRedirect, equalTo(false))
                return null
            }

            @AssertCalled(count = 2)
            override fun onSubframeLoadRequest(session: GeckoSession,
                                               request: LoadRequest):
                    GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("App did not request this load", request.isDirectNavigation, equalTo(false))
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("isRedirect should match", request.isRedirect,
                        equalTo(forEachCall(false, true)))
                return null
            }
        })
    }

    @Test fun redirectDenyLoad() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val redirectUri = if (sessionRule.env.isAutomation) {
            "http://example.org/tests/junit/hello.html"
        } else {
            "http://jigsaw.w3.org/HTTP/300/Overview.html"
        }
        val uri = if (sessionRule.env.isAutomation) {
            "http://example.org/tests/junit/simple_redirect.sjs?$redirectUri"
        } else {
            "http://jigsaw.w3.org/HTTP/300/301.html"
        }

        sessionRule.delegateDuringNextWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2, order = [1, 2])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URL should match", request.uri,
                        equalTo(forEachCall(request.uri, redirectUri)))
                assertThat("Trigger URL should be null", request.triggerUri,
                           nullValue())
                assertThat("From app should be correct", request.isDirectNavigation,
                        equalTo(forEachCall(true, false)))
                assertThat("Target should not be null", request.target, notNullValue())
                assertThat("Target should match", request.target,
                        equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                assertThat("Redirect flag is set", request.isRedirect,
                        equalTo(forEachCall(false, true)))

                return forEachCall(GeckoResult.allow(), GeckoResult.deny())
            }
        })

        sessionRule.session.loadUri(uri)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, equalTo(uri))
            }
        })
    }

    @Test fun redirectIntentLoad() {
        assumeThat(sessionRule.env.isAutomation, equalTo(true))
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))

        val redirectUri = "intent://test"
        val uri = "http://example.org/tests/junit/simple_redirect.sjs?$redirectUri"

        sessionRule.session.loadUri(uri)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2, order = [1, 2])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("URL should match", request.uri, equalTo(forEachCall(uri, redirectUri)))
                assertThat("From app should be correct", request.isDirectNavigation,
                        equalTo(forEachCall(true, false)))
                return null
            }
        })
    }


    @Test fun bypassClassifier() {
        val phishingUri = "https://www.itisatrap.org/firefox/its-a-trap.html"
        val category = ContentBlocking.SafeBrowsing.PHISHING

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        sessionRule.session.load(Loader()
            .uri(phishingUri + "?bypass=true")
            .flags(GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER))
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })
    }

    @Test fun safebrowsingPhishing() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val phishingUri = "https://www.itisatrap.org/firefox/its-a-trap.html"
        val category = ContentBlocking.SafeBrowsing.PHISHING

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        // Add query string to avoid bypassing classifier check because of cache.
        testLoadExpectError(phishingUri + "?block=true",
                        WebRequestError.ERROR_CATEGORY_SAFEBROWSING,
                        WebRequestError.ERROR_SAFEBROWSING_PHISHING_URI)

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(ContentBlocking.SafeBrowsing.NONE)

        sessionRule.session.loadUri(phishingUri + "?block=false")
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })
    }

    @Test fun safebrowsingMalware() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val malwareUri = "https://www.itisatrap.org/firefox/its-an-attack.html"
        val category = ContentBlocking.SafeBrowsing.MALWARE

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        testLoadExpectError(malwareUri + "?block=true",
                        WebRequestError.ERROR_CATEGORY_SAFEBROWSING,
                        WebRequestError.ERROR_SAFEBROWSING_MALWARE_URI)

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(ContentBlocking.SafeBrowsing.NONE)

        sessionRule.session.loadUri(malwareUri + "?block=false")
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })
    }

    @Test fun safebrowsingUnwanted() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val unwantedUri = "https://www.itisatrap.org/firefox/unwanted.html"
        val category = ContentBlocking.SafeBrowsing.UNWANTED

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        testLoadExpectError(unwantedUri + "?block=true",
                        WebRequestError.ERROR_CATEGORY_SAFEBROWSING,
                        WebRequestError.ERROR_SAFEBROWSING_UNWANTED_URI)

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(ContentBlocking.SafeBrowsing.NONE)

        sessionRule.session.loadUri(unwantedUri + "?block=false")
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })
    }

    @Test fun safebrowsingHarmful() {
        // TODO: Bug 1673954
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val harmfulUri = "https://www.itisatrap.org/firefox/harmful.html"
        val category = ContentBlocking.SafeBrowsing.HARMFUL

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(category)

        testLoadExpectError(harmfulUri + "?block=true",
                        WebRequestError.ERROR_CATEGORY_SAFEBROWSING,
                        WebRequestError.ERROR_SAFEBROWSING_HARMFUL_URI)

        sessionRule.runtime.settings.contentBlocking.setSafeBrowsing(ContentBlocking.SafeBrowsing.NONE)

        sessionRule.session.loadUri(harmfulUri + "?block=false")
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })
    }

    // Checks that the User Agent matches the user agent built in
    // nsHttpHandler::BuildUserAgent
    @Test fun defaultUserAgentMatchesActualUserAgent() {
        var userAgent = sessionRule.waitForResult(sessionRule.session.userAgent)
        assertThat("Mobile user agent should match the default user agent",
                userAgent, equalTo(GeckoSession.getDefaultUserAgent()))
    }

    @Test fun desktopMode() {
        sessionRule.session.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        val mobileSubStr = "Mobile"
        val desktopSubStr = "X11"

        assertThat("User agent should be set to mobile",
                   getUserAgent(),
                   containsString(mobileSubStr))

        var userAgent = sessionRule.waitForResult(sessionRule.session.userAgent)
        assertThat("User agent should be reported as mobile",
                    userAgent, containsString(mobileSubStr))

        sessionRule.session.settings.userAgentMode = GeckoSessionSettings.USER_AGENT_MODE_DESKTOP

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should be set to desktop",
                   getUserAgent(),
                   containsString(desktopSubStr))

        userAgent = sessionRule.waitForResult(sessionRule.session.userAgent)
        assertThat("User agent should be reported as desktop",
                    userAgent, containsString(desktopSubStr))

        sessionRule.session.settings.userAgentMode = GeckoSessionSettings.USER_AGENT_MODE_MOBILE

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should be set to mobile",
                   getUserAgent(),
                   containsString(mobileSubStr))

        userAgent = sessionRule.waitForResult(sessionRule.session.userAgent)
        assertThat("User agent should be reported as mobile",
                    userAgent, containsString(mobileSubStr))

        val vrSubStr = "Mobile VR"
        sessionRule.session.settings.userAgentMode = GeckoSessionSettings.USER_AGENT_MODE_VR

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should be set to VR",
                getUserAgent(),
                containsString(vrSubStr))

        userAgent = sessionRule.waitForResult(sessionRule.session.userAgent)
        assertThat("User agent should be reported as VR",
                userAgent, containsString(vrSubStr))

    }

    private fun getUserAgent(session: GeckoSession = sessionRule.session): String {
        return session.evaluateJS("window.navigator.userAgent") as String
    }

    @Test fun uaOverrideNewSession() {
        val newSession = sessionRule.createClosedSession()
        newSession.settings.userAgentOverride = "Test user agent override"

        newSession.open()
        newSession.loadUri("https://example.com")
        newSession.waitForPageStop()

        assertThat("User agent should match override", getUserAgent(newSession),
                equalTo("Test user agent override"))
    }

    @Test fun uaOverride() {
        sessionRule.session.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        val mobileSubStr = "Mobile"
        val vrSubStr = "Mobile VR"
        val overrideUserAgent = "This is the override user agent"

        assertThat("User agent should be reported as mobile",
                getUserAgent(), containsString(mobileSubStr))

        sessionRule.session.settings.userAgentOverride = overrideUserAgent

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should be reported as override",
                getUserAgent(), equalTo(overrideUserAgent))

        sessionRule.session.settings.userAgentMode = GeckoSessionSettings.USER_AGENT_MODE_VR

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should still be reported as override even when USER_AGENT_MODE is set",
                getUserAgent(), equalTo(overrideUserAgent))

        sessionRule.session.settings.userAgentOverride = null

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should now be reported as VR",
                getUserAgent(), containsString(vrSubStr))

        sessionRule.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                sessionRule.session.settings.userAgentOverride = overrideUserAgent
                return null
            }
        })

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should be reported as override after being set in onLoadRequest",
                getUserAgent(), equalTo(overrideUserAgent))

        sessionRule.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                sessionRule.session.settings.userAgentOverride = null
                return null
            }
        })

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should again be reported as VR after disabling override in onLoadRequest",
                getUserAgent(), containsString(vrSubStr))
    }

    @WithDisplay(width = 600, height = 200)
    @Test fun viewportMode() {
        sessionRule.session.loadTestPath(VIEWPORT_PATH)
        sessionRule.waitForPageStop()

        val desktopInnerWidth = 980.0
        val physicalWidth = 600.0
        val pixelRatio = sessionRule.session.evaluateJS("window.devicePixelRatio") as Double
        val mobileInnerWidth = physicalWidth / pixelRatio
        val innerWidthJs = "window.innerWidth"

        var innerWidth = sessionRule.session.evaluateJS(innerWidthJs) as Double
        assertThat("innerWidth should be equal to $mobileInnerWidth",
                innerWidth, closeTo(mobileInnerWidth, 0.1))

        sessionRule.session.settings.viewportMode = GeckoSessionSettings.VIEWPORT_MODE_DESKTOP

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        innerWidth = sessionRule.session.evaluateJS(innerWidthJs) as Double
        assertThat("innerWidth should be equal to $desktopInnerWidth", innerWidth,
                closeTo(desktopInnerWidth, 0.1))

        sessionRule.session.settings.viewportMode = GeckoSessionSettings.VIEWPORT_MODE_MOBILE

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        innerWidth = sessionRule.session.evaluateJS(innerWidthJs) as Double
        assertThat("innerWidth should be equal to $mobileInnerWidth again",
                innerWidth, closeTo(mobileInnerWidth, 0.1))
    }

    @Test fun load() {
        sessionRule.session.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URI should not be null", request.uri, notNullValue())
                assertThat("URI should match", request.uri, endsWith(HELLO_HTML_PATH))
                assertThat("Trigger URL should be null", request.triggerUri,
                           nullValue())
                assertThat("App requested this load", request.isDirectNavigation,
                        equalTo(true))
                assertThat("Target should not be null", request.target, notNullValue())
                assertThat("Target should match", request.target,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                assertThat("Redirect flag is not set", request.isRedirect, equalTo(false))
                assertThat("Should not have a user gesture", request.hasUserGesture, equalTo(false))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(session: GeckoSession, url: String?) {
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
        sessionRule.session.loadUri(dataUrl)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?) {
                assertThat("URL should match the provided data URL", url, equalTo(dataUrl))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @NullDelegate(GeckoSession.NavigationDelegate::class)
    @Test fun load_withoutNavigationDelegate() {
        // Test that when navigation delegate is disabled, we can still perform loads.
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()
    }

    @NullDelegate(GeckoSession.NavigationDelegate::class)
    @Test fun load_canUnsetNavigationDelegate() {
        // Test that if we unset the navigation delegate during a load, the load still proceeds.
        var onLocationCount = 0
        sessionRule.session.navigationDelegate = object : Callbacks.NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, url: String?) {
                onLocationCount++
            }
        }
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()

        assertThat("Should get callback for first load",
                   onLocationCount, equalTo(1))

        sessionRule.session.reload()
        sessionRule.session.navigationDelegate = null
        sessionRule.session.waitForPageStop()

        assertThat("Should not get callback for second load",
                   onLocationCount, equalTo(1))
    }

    @Test fun loadString() {
        val dataString = "<html><head><title>TheTitle</title></head><body>TheBody</body></html>"
        val mimeType = "text/html"
        sessionRule.session.load(Loader().data(dataString, mimeType))
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate, Callbacks.ContentDelegate {
            @AssertCalled
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Title should match", title, equalTo("TheTitle"))
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?) {
                assertThat("URL should be a data URL", url,
                           equalTo(createDataUri(dataString, mimeType)))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @Test fun loadString_noMimeType() {
        sessionRule.session.load(Loader().data("Hello, World!", null))
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?) {
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

        sessionRule.session.load(Loader().data(bytes, "text/html"))
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate, Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Title should match", title, equalTo("Hello, world!"))
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?) {
                assertThat("URL should match", url, equalTo(createDataUri(bytes, "text/html")))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    private fun createDataUri(data: String,
                              mimeType: String?): String {
        return String.format("data:%s,%s", mimeType ?: "", data)
    }

    private fun createDataUri(bytes: ByteArray,
                              mimeType: String?): String {
        return String.format("data:%s;base64,%s", mimeType ?: "",
                Base64.encodeToString(bytes, Base64.NO_WRAP))
    }

    fun loadDataHelper(assetPath: String, mimeType: String? = null) {
        val bytes = getTestBytes(assetPath)
        assertThat("test data should have bytes", bytes.size, greaterThan(0))

        sessionRule.session.load(Loader().data(bytes, mimeType))
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?) {
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
        sessionRule.session.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitForPageStop()

        sessionRule.session.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("URI should match", request.uri, endsWith(HELLO_HTML_PATH))
                assertThat("Trigger URL should be null", request.triggerUri,
                           nullValue())
                assertThat("Target should match", request.target,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                assertThat("Load should not be direct", request.isDirectNavigation,
                        equalTo(false))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(session: GeckoSession, url: String?) {
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
        sessionRule.session.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitForPageStop()

        sessionRule.session.loadUri("$TEST_ENDPOINT$HELLO2_HTML_PATH")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?, perms : MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }
        })

        sessionRule.session.goBack()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 0, order = [1])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("Load should not be direct", request.isDirectNavigation,
                        equalTo(false))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(session: GeckoSession, url: String?, perms : MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
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

        sessionRule.session.goForward()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 0, order = [1])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("Load should not be direct", request.isDirectNavigation,
                        equalTo(false))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(session: GeckoSession, url: String?, perms : MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
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
        sessionRule.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                if (request.uri.endsWith(HELLO_HTML_PATH)) {
                    return GeckoResult.deny()
                } else {
                    return GeckoResult.allow()
                }
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.loadTestPath(HELLO2_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("window.open('newSession_child.html', '_blank')")

        sessionRule.session.waitUntilCalled(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("URI should be correct", request.uri, endsWith(NEW_SESSION_CHILD_HTML_PATH))
                assertThat("Trigger URL should match", request.triggerUri,
                           endsWith(NEW_SESSION_HTML_PATH))
                assertThat("Target should be correct", request.target,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW))
                assertThat("Load should not be direct", request.isDirectNavigation,
                        equalTo(false))
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

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("window.open('file:///data/local/tmp', '_blank')")
    }

    @Test fun onNewSession_calledForTargetBlankLink() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("document.querySelector('#targetBlankLink').click()")

        sessionRule.session.waitUntilCalled(object : Callbacks.NavigationDelegate {
            // We get two onLoadRequest calls for the link click,
            // one when loading the URL and one when opening a new window.
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("URI should be correct", request.uri, endsWith(NEW_SESSION_CHILD_HTML_PATH))
                assertThat("Trigger URL should be null", request.triggerUri,
                           endsWith(NEW_SESSION_HTML_PATH))
                assertThat("Target should be correct", request.target,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW))
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

        sessionRule.session.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
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

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        val newSession = delegateNewSession()
        sessionRule.session.evaluateJS("document.querySelector('#targetBlankLink').click()")
        // Initial about:blank
        newSession.waitForPageStop()
        // NEW_SESSION_CHILD_HTML_PATH
        newSession.waitForPageStop()

        newSession.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        val newSession = delegateNewSession()
        sessionRule.session.evaluateJS("document.querySelector('#targetBlankLink').click()")
        newSession.waitForPageStop()

        assertThat("window.opener should be set",
                   newSession.evaluateJS("window.opener.location.pathname") as String,
                   equalTo(NEW_SESSION_HTML_PATH))
    }

    @Test fun onNewSession_supportNoOpener() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        val newSession = delegateNewSession()
        sessionRule.session.evaluateJS("document.querySelector('#noOpenerLink').click()")
        newSession.waitForPageStop()

        assertThat("window.opener should not be set",
                   newSession.evaluateJS("window.opener"),
                   equalTo(JSONObject.NULL))
    }

    @Test fun onNewSession_notCalledForHandledLoads() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                // Pretend we handled the target="_blank" link click.
                if (request.uri.endsWith(NEW_SESSION_CHILD_HTML_PATH)) {
                    return GeckoResult.deny()
                } else {
                    return GeckoResult.allow()
                }
            }
        })

        sessionRule.session.evaluateJS("document.querySelector('#targetBlankLink').click()")

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        // Assert that onNewSession was not called for the link click.
        sessionRule.session.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("URI must match", request.uri,
                           endsWith(forEachCall(NEW_SESSION_CHILD_HTML_PATH, NEW_SESSION_HTML_PATH)))
                assertThat("Load should not be direct", request.isDirectNavigation,
                        equalTo(false))
                return null
            }

            @AssertCalled(count = 0)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }
        })
    }

    @Test fun onNewSession_submitFormWithTargetBlank() {
        sessionRule.session.loadTestPath(FORM_BLANK_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.evaluateJS("""
            document.querySelector('input[type=text]').focus()
        """)
        sessionRule.session.waitUntilCalled(GeckoSession.TextInputDelegate::class,
                                            "restartInput")

        val time = SystemClock.uptimeMillis()
        val keyEvent = KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER, 0)
        sessionRule.session.textInput.onKeyDown(KeyEvent.KEYCODE_ENTER, keyEvent)
        sessionRule.session.textInput.onKeyUp(KeyEvent.KEYCODE_ENTER,
                                              KeyEvent.changeAction(keyEvent,
                                                                    KeyEvent.ACTION_UP))

        sessionRule.session.waitUntilCalled(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("URL should be correct", request.uri,
                           endsWith("form_blank.html?"))
                assertThat("Trigger URL should match", request.triggerUri,
                           endsWith("form_blank.html"))
                assertThat("Target should be correct", request.target,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW))
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

        sessionRule.session.load(Loader()
            .uri(uri)
            .referrer(referrer)
            .flags(GeckoSession.LOAD_FLAGS_NONE))
        sessionRule.session.waitForPageStop()

        assertThat("Referrer should match",
                   sessionRule.session.evaluateJS("document.referrer") as String,
                   equalTo(referrer))
    }

    @Test fun loadUriReferrerSession() {
        val uri = "https://example.com/bar"
        val referrer = "https://example.org/"

        sessionRule.session.loadUri(referrer)
        sessionRule.session.waitForPageStop()

        val newSession = sessionRule.createOpenSession()
        newSession.load(Loader()
            .uri(uri)
            .referrer(sessionRule.session)
            .flags(GeckoSession.LOAD_FLAGS_NONE))
        newSession.waitForPageStop()

        assertThat("Referrer should match",
                newSession.evaluateJS("document.referrer") as String,
                equalTo(referrer))
    }

    @Test fun loadUriReferrerSessionFileUrl() {
        val uri = "file:///system/etc/fonts.xml"
        val referrer = "https://example.org"

        sessionRule.session.loadUri(referrer)
        sessionRule.session.waitForPageStop()

        val newSession = sessionRule.createOpenSession()
        newSession.load(Loader()
            .uri(uri)
            .referrer(sessionRule.session)
            .flags(GeckoSession.LOAD_FLAGS_NONE))
        newSession.waitUntilCalled(object : Callbacks.NavigationDelegate {
            @AssertCalled
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })
    }

    private fun loadUriHeaderTest(headers: Map<String?,String?>,
                                  additional: Map<String?, String?>,
                                  filter: Int = GeckoSession.HEADER_FILTER_CORS_SAFELISTED) {
        // First collect default headers with no override
        sessionRule.session.loadUri("$TEST_ENDPOINT/anything")
        sessionRule.session.waitForPageStop()

        val defaultContent = sessionRule.session.evaluateJS("document.body.children[0].innerHTML") as String
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
        sessionRule.session.load(Loader()
            .uri("$TEST_ENDPOINT/anything")
            .additionalHeaders(headers)
            .headerFilter(filter))
        sessionRule.session.waitForPageStop()

        val content = sessionRule.session.evaluateJS("document.body.children[0].innerHTML") as String
        val body = JSONObject(content)
        val actualHeaders = body.getJSONObject("headers").asMap<String>()

        assertThat("Headers should match", expected as Map<String?, String?>,
                equalTo(actualHeaders))
    }

    private fun testLoaderEquals(a: Loader, b: Loader, shouldBeEqual: Boolean) {
        assertThat("Equal test", a == b, equalTo(shouldBeEqual))
        assertThat("HashCode test", a.hashCode() == b.hashCode(),
                equalTo(shouldBeEqual))
    }

    @Test fun loaderEquals() {
        testLoaderEquals(
                Loader().uri("http://test-uri-equals.com"),
                Loader().uri("http://test-uri-equals.com"),
                true)
        testLoaderEquals(
                Loader().uri("http://test-uri-equals.com"),
                Loader().uri("http://test-uri-equalsx.com"),
                false)

        testLoaderEquals(
                Loader().uri("http://test-uri-equals.com")
                        .flags(LOAD_FLAGS_BYPASS_CLASSIFIER)
                        .headerFilter(HEADER_FILTER_UNRESTRICTED_UNSAFE)
                        .referrer("test-referrer"),
                Loader().uri("http://test-uri-equals.com")
                        .flags(LOAD_FLAGS_BYPASS_CLASSIFIER)
                        .headerFilter(HEADER_FILTER_UNRESTRICTED_UNSAFE)
                        .referrer("test-referrer"),
                true)
        testLoaderEquals(
                Loader().uri("http://test-uri-equals.com")
                        .flags(LOAD_FLAGS_BYPASS_CLASSIFIER)
                        .headerFilter(HEADER_FILTER_UNRESTRICTED_UNSAFE)
                        .referrer(sessionRule.session),
                Loader().uri("http://test-uri-equals.com")
                        .flags(LOAD_FLAGS_BYPASS_CLASSIFIER)
                        .headerFilter(HEADER_FILTER_UNRESTRICTED_UNSAFE)
                        .referrer("test-referrer"),
                false)

        testLoaderEquals(
                Loader().referrer(sessionRule.session)
                        .data("testtest", "text/plain"),
                Loader().referrer(sessionRule.session)
                        .data("testtest", "text/plain"),
                true)
        testLoaderEquals(
                Loader().referrer(sessionRule.session)
                        .data("testtest", "text/plain"),
                Loader().referrer("test-referrer")
                        .data("testtest", "text/plain"),
                false)
    }

    @Test fun loadUriHeader() {
        // Basic test
        loadUriHeaderTest(
                mapOf("Header1" to "Value", "Header2" to "Value1, Value2"),
                mapOf()
        )
        loadUriHeaderTest(
                mapOf("Header1" to "Value", "Header2" to "Value1, Value2"),
                mapOf("Header1" to "Value", "Header2" to "Value1, Value2"),
                GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE
        )

        // Empty value headers are ignored
        loadUriHeaderTest(
                mapOf("ValueLess1" to "", "ValueLess2" to null),
                mapOf()
        )

        // Null key or special headers are ignored
        loadUriHeaderTest(
                mapOf(null to "BadNull",
                      "Connection" to "BadConnection",
                      "Host" to "BadHost"),
                mapOf()
        )

        // Key or value cannot contain '\r\n'
        loadUriHeaderTest(
                mapOf("Header1" to "Value",
                      "Header2" to "Value1, Value2",
                      "this\r\nis invalid" to "test value",
                      "test key" to "this\r\n is a no-no",
                      "what" to "what\r\nhost:amazon.com",
                      "Header3" to "Value1, Value2, Value3"
                ),
                mapOf()
        )
        loadUriHeaderTest(
                mapOf("Header1" to "Value",
                        "Header2" to "Value1, Value2",
                        "this\r\nis invalid" to "test value",
                        "test key" to "this\r\n is a no-no",
                        "what" to "what\r\nhost:amazon.com",
                        "Header3" to "Value1, Value2, Value3"
                ),
                mapOf("Header1" to "Value",
                        "Header2" to "Value1, Value2",
                        "Header3" to "Value1, Value2, Value3"),
                GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE
        )

        loadUriHeaderTest(
                mapOf("Header1" to "Value",
                        "Header2" to "Value1, Value2",
                        "what" to "what\r\nhost:amazon.com"),
                mapOf()
        )
        loadUriHeaderTest(
                mapOf("Header1" to "Value",
                      "Header2" to "Value1, Value2",
                      "what" to "what\r\nhost:amazon.com"),
                mapOf("Header1" to "Value", "Header2" to "Value1, Value2"),
                GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE
        )

        loadUriHeaderTest(
                mapOf("what" to "what\r\nhost:amazon.com"),
                mapOf()
        )

        loadUriHeaderTest(
                mapOf("this\r\n" to "yes"),
                mapOf()
        )

        // Connection and Host cannot be overriden, no matter the case spelling
        loadUriHeaderTest(
                mapOf("Header1" to "Value1", "ConnEction" to "test", "connection" to "test2"),
                mapOf()
        )
        loadUriHeaderTest(
                mapOf("Header1" to "Value1", "ConnEction" to "test", "connection" to "test2"),
                mapOf("Header1" to "Value1"),
                GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE
        )

        loadUriHeaderTest(
                mapOf("Header1" to "Value1", "connection" to "test2"),
                mapOf()
        )
        loadUriHeaderTest(
                mapOf("Header1" to "Value1", "connection" to "test2"),
                mapOf("Header1" to "Value1"),
                GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE
        )

        loadUriHeaderTest(
                mapOf("Header1   " to "Value1", "host" to "test2"),
                mapOf()
        )
        loadUriHeaderTest(
                mapOf("Header1   " to "Value1", "host" to "test2"),
                mapOf("Header1" to "Value1"),
                GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE
        )

        loadUriHeaderTest(
                mapOf("Header1" to "Value1", "host" to "test2"),
                mapOf()
        )
        loadUriHeaderTest(
                mapOf("Header1" to "Value1", "host" to "test2"),
                mapOf("Header1" to "Value1"),
                GeckoSession.HEADER_FILTER_UNRESTRICTED_UNSAFE
        )

        // Adding white space at the end of a forbidden header still prevents override
        loadUriHeaderTest(
                mapOf("host" to "amazon.com",
                      "host " to "amazon.com",
                      "host\r" to "amazon.com",
                      "host\r\n" to "amazon.com"),
                mapOf()
        )

        // '\r' or '\n' are forbidden character even when not following each other
        loadUriHeaderTest(
                mapOf("abc\ra\n" to "amazon.com"),
                mapOf()
        )

        // CORS Safelist test
        loadUriHeaderTest(
                mapOf("Accept-Language" to "fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
                      "Accept" to "text/html",
                      "Content-Language" to "de-DE, en-CA",
                      "Content-Type" to "multipart/form-data; boundary=something"),
                mapOf("Accept-Language" to "fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
                      "Accept" to "text/html",
                      "Content-Language" to "de-DE, en-CA",
                      "Content-Type" to "multipart/form-data; boundary=something"),
                GeckoSession.HEADER_FILTER_CORS_SAFELISTED
        )

        // CORS safelist doesn't allow Content-type image/svg
        loadUriHeaderTest(
                mapOf("Accept-Language" to "fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
                      "Accept" to "text/html",
                      "Content-Language" to "de-DE, en-CA",
                      "Content-Type" to "image/svg; boundary=something"),
                mapOf("Accept-Language" to "fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
                      "Accept" to "text/html",
                      "Content-Language" to "de-DE, en-CA"),
                GeckoSession.HEADER_FILTER_CORS_SAFELISTED
        )
    }

    @Test(expected = GeckoResult.UncaughtException::class)
    fun onNewSession_doesNotAllowOpened() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession> {
                return GeckoResult.fromValue(sessionRule.createOpenSession())
            }
        })

        sessionRule.session.evaluateJS("document.querySelector('#targetBlankLink').click()")

        sessionRule.session.waitUntilCalled(GeckoSession.NavigationDelegate::class,
                                            "onNewSession")
        UiThreadUtils.loopUntilIdle(sessionRule.env.defaultTimeoutMillis)
    }

    @Test
    fun extensionProcessSwitching() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))

        val controller = sessionRule.runtime.webExtensionController

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtensionController.PromptDelegate::class,
                controller::setPromptDelegate,
                { controller.promptDelegate = null },
                object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        val extension = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/page-history.xpi"))

        assertThat("baseUrl should be a valid extension URL",
                extension.metaData.baseUrl, startsWith("moz-extension://"))

        val url = extension.metaData.baseUrl + "page.html"
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
        mainSession.delegateUntilTestEnd(object: GeckoSession.NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, url: String?) {
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

        assertThat("docShell should start out active", mainSession.active,
            equalTo(true))

        // This loads in the parent process
        mainSession.loadUri(url)
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, equalTo(url))

        // This will load a page in the child
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, endsWith(HELLO_HTML_PATH))
        assertThat("docShell should be active after switching process",
                mainSession.active,
                equalTo(true))

        mainSession.loadUri(url)
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, equalTo(url))

        sessionRule.session.goBack()
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, endsWith(HELLO_HTML_PATH))
        assertThat("docShell should be active after switching process",
                mainSession.active,
                equalTo(true))

        sessionRule.session.goBack()
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, equalTo(url))

        sessionRule.session.goBack()
        sessionRule.waitForPageStop()

        assertThat("URL should match", currentUrl!!, endsWith(HELLO2_HTML_PATH))
        assertThat("docShell should be active after switching process",
                mainSession.active,
                equalTo(true))

        settings.aboutConfigEnabled = aboutConfigEnabled
    }

    @Test fun setLocationHash() {
        sessionRule.session.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitForPageStop()

        sessionRule.session.evaluateJS("location.hash = 'test1';")

        sessionRule.session.waitUntilCalled(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                assertThat("Load should not be direct", request.isDirectNavigation,
                        equalTo(false))
                return null
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?, perms : MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
                assertThat("URI should match", url, endsWith("#test1"))
            }
        })

        sessionRule.session.evaluateJS("location.hash = 'test2';")

        sessionRule.session.waitUntilCalled(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 0)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                return null
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?, perms : MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
                assertThat("URI should match", url, endsWith("#test2"))
            }
        })
    }

    @Test fun purgeHistory() {
        // TODO: Bug 1648158
        assumeThat(sessionRule.env.isFission, equalTo(false))
        sessionRule.session.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
        sessionRule.waitUntilCalled(object : Callbacks.NavigationDelegate {
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
        sessionRule.session.loadUri("$TEST_ENDPOINT$HELLO2_HTML_PATH")
        sessionRule.waitUntilCalled(object : Callbacks.All {
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
            override fun onHistoryStateChange(session: GeckoSession, state: GeckoSession.HistoryDelegate.HistoryList) {
                assertThat("History should have two entries", state.size, equalTo(2))
            }
        })
        sessionRule.session.purgeHistory()
        sessionRule.waitUntilCalled(object : Callbacks.All {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: GeckoSession.HistoryDelegate.HistoryList) {
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
    @Test fun userGesture() {
        mainSession.loadUri("$TEST_ENDPOINT$CLICK_TO_RELOAD_HTML_PATH")
        mainSession.waitForPageStop()

        mainSession.synthesizeTap(50, 50)

        sessionRule.waitUntilCalled(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("Should have a user gesture", request.hasUserGesture, equalTo(true))
                assertThat("Load should not be direct", request.isDirectNavigation,
                        equalTo(false))
                return GeckoResult.allow()
            }
        })
    }

    @Test fun loadAfterLoad() {
        // TODO: Bug 1657028
        assumeThat(sessionRule.env.isFission, equalTo(false))
        sessionRule.session.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
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

        sessionRule.session.delegateUntilTestEnd(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("URLs should match", request.uri, equalTo(expectedUri))
                return GeckoResult.allow()
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     error: WebRequestError): GeckoResult<String>? {
                assertThat("Error category should match", error.category,
                        equalTo(WebRequestError.ERROR_CATEGORY_URI))
                assertThat("Error code should match", error.code,
                        equalTo(WebRequestError.ERROR_DATA_URI_TOO_LONG))
                assertThat("URLs should match", uri, equalTo(expectedUri))
                return null
            }
        })

        sessionRule.session.load(loader)
        sessionRule.waitUntilCalled(Callbacks.NavigationDelegate::class, "onLoadError")
    }

    @Test
    fun loadLongDataUriToplevelIndirect() {
        val dataBytes = ByteArray(3 * 1024 * 1024)
        val dataUri = createDataUri(dataBytes, "*/*")

        sessionRule.session.loadTestPath(DATA_URI_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.delegateUntilTestEnd(object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return GeckoResult.deny()
            }
        })

        sessionRule.session.evaluateJS("document.querySelector('#largeLink').href = \"$dataUri\"")
        sessionRule.session.evaluateJS("document.querySelector('#largeLink').click()")
        sessionRule.session.waitForPageStop()
    }

    @Test
    fun loadShortDataUriToplevelIndirect() {
        sessionRule.session.delegateUntilTestEnd(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return GeckoResult.allow()
            }

            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })

        val dataBytes = this.getTestBytes("/assets/www/images/test.gif")
        val uri = createDataUri(dataBytes, "image/*")

        sessionRule.session.loadTestPath(DATA_URI_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("document.querySelector('#smallLink').href = \"$uri\"")
        sessionRule.session.evaluateJS("document.querySelector('#smallLink').click()")
        sessionRule.session.waitForPageStop()
    }

    fun createLargeHighEntropyImageDataUri() : String {
        val desiredMinSize = (2 * 1024 * 1024) + 1

        val width = 768;
        val height = 768;

        val bitmap = Bitmap.createBitmap(ThreadLocalRandom.current().ints(width.toLong() * height.toLong()).toArray(),
                                         width, height, Bitmap.Config.ARGB_8888)

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

        sessionRule.session.delegateUntilTestEnd(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return GeckoResult.allow()
            }

            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     error: WebRequestError): GeckoResult<String>? {
                return null
            }
        })

        sessionRule.session.loadTestPath(DATA_URI_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("document.querySelector('#image').onload = () => { imageLoaded = true; }")
        sessionRule.session.evaluateJS("document.querySelector('#image').src = \"$dataUri\"")
        UiThreadUtils.waitForCondition({
          sessionRule.session.evaluateJS("document.querySelector('#image').complete") as Boolean
        }, sessionRule.env.defaultTimeoutMillis)
        sessionRule.session.evaluateJS("if (!imageLoaded) throw imageLoaded")
    }
}
