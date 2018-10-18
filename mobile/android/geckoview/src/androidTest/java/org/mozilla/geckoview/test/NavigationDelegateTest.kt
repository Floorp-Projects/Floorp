/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.GeckoSession.TrackingProtectionDelegate;
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.Setting
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
class NavigationDelegateTest : BaseSessionTest() {

    fun testLoadErrorWithErrorPage(testUri: String, expectedCategory: Int,
                                   expectedError: Int,
                                   errorPageUrl: String?) {
        sessionRule.delegateDuringNextWait(
                object : Callbacks.ProgressDelegate, Callbacks.NavigationDelegate, Callbacks.ContentDelegate {
                    @AssertCalled(count = 1, order = [1])
                    override fun onLoadRequest(session: GeckoSession, uri: String,
                                               where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                        assertThat("URI should be " + testUri, uri, equalTo(testUri))
                        return null
                    }

                    @AssertCalled(count = 1, order = [2])
                    override fun onPageStart(session: GeckoSession, url: String) {
                        assertThat("URI should be " + testUri, url, equalTo(testUri))
                    }

                    @AssertCalled(count = 1, order = [3])
                    override fun onLoadError(session: GeckoSession, uri: String?,
                                             category: Int, error: Int): GeckoResult<String>? {
                        assertThat("Error category should match", category,
                                equalTo(expectedCategory))
                        assertThat("Error should match", error,
                                equalTo(expectedError))
                        return GeckoResult.fromValue(errorPageUrl)
                    }

                    @AssertCalled(count = 1, order = [4])
                    override fun onPageStop(session: GeckoSession, success: Boolean) {
                        assertThat("Load should fail", success, equalTo(false))
                    }
                })

        sessionRule.session.loadUri(testUri);
        sessionRule.waitForPageStop()

        if (errorPageUrl != null) {
            sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate, Callbacks.NavigationDelegate {
                @AssertCalled(count = 1, order = [1])
                override fun onLocationChange(session: GeckoSession, url: String) {
                    assertThat("URL should match", url, equalTo(testUri))
                }

                @AssertCalled(count = 1, order = [2])
                override fun onTitleChange(session: GeckoSession, title: String) {
                    assertThat("Title should not be empty", title, not(isEmptyOrNullString()))
                }
            })
        }
    }

    fun testLoadExpectError(testUri: String, expectedCategory: Int,
                            expectedError: Int) {
        testLoadErrorWithErrorPage(testUri, expectedCategory,
                expectedError, createTestUrl(HELLO_HTML_PATH))
        testLoadErrorWithErrorPage(testUri, expectedCategory,
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
                                             category: Int, error: Int): GeckoResult<String>? {
                        assertThat("Error category should match", category,
                                equalTo(expectedCategory))
                        assertThat("Error should match", error,
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
                override fun onTitleChange(session: GeckoSession, title: String) {
                    assertThat("Title should not be empty", title, not(isEmptyOrNullString()));
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
        testLoadExpectError("file:///test.mozilla",
                GeckoSession.NavigationDelegate.ERROR_CATEGORY_URI,
                GeckoSession.NavigationDelegate.ERROR_FILE_NOT_FOUND)
    }

    @Test fun loadUnknownHost() {
        testLoadExpectError(UNKNOWN_HOST_URI,
                GeckoSession.NavigationDelegate.ERROR_CATEGORY_URI,
                GeckoSession.NavigationDelegate.ERROR_UNKNOWN_HOST)
    }

    @Test fun loadInvalidUri() {
        testLoadEarlyError(INVALID_URI,
                GeckoSession.NavigationDelegate.ERROR_CATEGORY_URI,
                GeckoSession.NavigationDelegate.ERROR_MALFORMED_URI)
    }

    @Test fun loadBadPort() {
        testLoadEarlyError("http://localhost:1/",
                GeckoSession.NavigationDelegate.ERROR_CATEGORY_NETWORK,
                GeckoSession.NavigationDelegate.ERROR_PORT_BLOCKED)
    }

    @Test fun loadUntrusted() {
        val uri = if (sessionRule.env.isAutomation) {
            "https://expired.example.com/"
        } else {
            "https://expired.badssl.com/"
        }
        testLoadExpectError(uri,
                GeckoSession.NavigationDelegate.ERROR_CATEGORY_SECURITY,
                GeckoSession.NavigationDelegate.ERROR_SECURITY_BAD_CERT);
    }

    @Setting(key = Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Test fun trackingProtection() {
        val category = TrackingProtectionDelegate.CATEGORY_TEST;
        sessionRule.runtime.settings.trackingProtectionCategories = category
        sessionRule.session.loadTestPath(TRACKERS_PATH)

        sessionRule.waitUntilCalled(
                object : Callbacks.TrackingProtectionDelegate {
            @AssertCalled(count = 1)
            override fun onTrackerBlocked(session: GeckoSession, uri: String,
                                          categories: Int) {
                assertThat("Category should be set",
                           categories,
                           equalTo(category))
                assertThat("URI should not be null", uri, notNullValue())
                assertThat("URI should match", uri, endsWith("trackertest.org/tracker.js"))
            }
        })

        sessionRule.session.settings.setBoolean(
            GeckoSessionSettings.USE_TRACKING_PROTECTION, false)

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.TrackingProtectionDelegate {
            @AssertCalled(false)
            override fun onTrackerBlocked(session: GeckoSession, uri: String,
                                          categories: Int) {
            }
        })
    }

    @Ignore
    @Test fun redirectLoad() {
        val redirectUri = if (sessionRule.env.isAutomation) {
            "http://example.org/tests/robocop/robocop_blank_02.html"
        } else {
            "http://jigsaw.w3.org/HTTP/300/Overview.html"
        }
        val uri = if (sessionRule.env.isAutomation) {
            "http://example.org/tests/robocop/simple_redirect.sjs?$redirectUri"
        } else {
            "http://jigsaw.w3.org/HTTP/300/301.html"
        }

        sessionRule.session.loadUri(uri)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2, order = [1, 2])
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URI should not be null", uri, notNullValue())
                assertThat("URL should match", uri,
                        equalTo(forEachCall(uri, redirectUri)))
                assertThat("Where should not be null", where, notNullValue())
                assertThat("Where should match", where,
                        equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return null
            }
        })
    }

    @Test fun safebrowsingPhishing() {
        val phishingUri = "https://www.itisatrap.org/firefox/its-a-trap.html"

        sessionRule.runtime.settings.blockPhishing = true

        testLoadExpectError(phishingUri,
                        GeckoSession.NavigationDelegate.ERROR_CATEGORY_SAFEBROWSING,
                        GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_PHISHING_URI)

        sessionRule.runtime.settings.blockPhishing = false

        sessionRule.session.loadUri(phishingUri)
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     category: Int, error: Int): GeckoResult<String>? {
                return null
            }
        })
    }

    @Test fun safebrowsingMalware() {
        val malwareUri = "https://www.itisatrap.org/firefox/its-an-attack.html"

        sessionRule.runtime.settings.blockMalware = true

        testLoadExpectError(malwareUri,
                        GeckoSession.NavigationDelegate.ERROR_CATEGORY_SAFEBROWSING,
                        GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_MALWARE_URI)

        sessionRule.runtime.settings.blockMalware = false

        sessionRule.session.loadUri(malwareUri)
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     category: Int, error: Int): GeckoResult<String>? {
                return null
            }
        })
    }

    @Test fun safebrowsingUnwanted() {
        val unwantedUri = "https://www.itisatrap.org/firefox/unwanted.html"

        sessionRule.runtime.settings.blockMalware = true

        testLoadExpectError(unwantedUri,
                        GeckoSession.NavigationDelegate.ERROR_CATEGORY_SAFEBROWSING,
                        GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_UNWANTED_URI)

        sessionRule.runtime.settings.blockMalware = false

        sessionRule.session.loadUri(unwantedUri)
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     category: Int, error: Int): GeckoResult<String>? {
                return null
            }
        })
    }

    @Test fun safebrowsingHarmful() {
        val harmfulUri = "https://www.itisatrap.org/firefox/harmful.html"

        sessionRule.runtime.settings.blockMalware = true

        testLoadExpectError(harmfulUri,
                        GeckoSession.NavigationDelegate.ERROR_CATEGORY_SAFEBROWSING,
                        GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_HARMFUL_URI)

        sessionRule.runtime.settings.blockMalware = false

        sessionRule.session.loadUri(harmfulUri)
        sessionRule.session.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.NavigationDelegate {
            @AssertCalled(false)
            override fun onLoadError(session: GeckoSession, uri: String?,
                                     category: Int, error: Int): GeckoResult<String>? {
                return null
            }
        })
    }

    @WithDevToolsAPI
    @Test fun desktopMode() {
        sessionRule.session.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        val userAgentJs = "window.navigator.userAgent"
        val mobileSubStr = "Mobile"
        val desktopSubStr = "X11"

        assertThat("User agent should be set to mobile",
                   sessionRule.session.evaluateJS(userAgentJs) as String,
                   containsString(mobileSubStr))

        var userAgent = sessionRule.waitForResult(sessionRule.session.getUserAgent());
        assertThat("User agent should be reported as mobile",
                    userAgent, containsString(mobileSubStr));

        sessionRule.session.settings.setInt(
            GeckoSessionSettings.USER_AGENT_MODE, GeckoSessionSettings.USER_AGENT_MODE_DESKTOP)

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should be set to desktop",
                   sessionRule.session.evaluateJS(userAgentJs) as String,
                   containsString(desktopSubStr))

        userAgent = sessionRule.waitForResult(sessionRule.session.getUserAgent());
        assertThat("User agent should be reported as desktop",
                    userAgent, containsString(desktopSubStr));

        sessionRule.session.settings.setInt(
            GeckoSessionSettings.USER_AGENT_MODE, GeckoSessionSettings.USER_AGENT_MODE_MOBILE)

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("User agent should be set to mobile",
                   sessionRule.session.evaluateJS(userAgentJs) as String,
                   containsString(mobileSubStr))

        userAgent = sessionRule.waitForResult(sessionRule.session.getUserAgent());
        assertThat("User agent should be reported as mobile",
                    userAgent, containsString(mobileSubStr));

    }

    @Test fun telemetrySnapshots() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        val telemetry = sessionRule.runtime.telemetry
        val result = sessionRule.waitForResult(telemetry.getSnapshots(false))

        assertThat("Snapshots should not be null",
                   result?.get("parent"), notNullValue())

        if (sessionRule.env.isMultiprocess) {
            assertThat("Snapshots should not be null",
                       result?.get("content"), notNullValue())
        }
    }

    @Test fun load() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URI should not be null", uri, notNullValue())
                assertThat("URI should match", uri, endsWith(HELLO_HTML_PATH))
                assertThat("Where should not be null", where, notNullValue())
                assertThat("Where should match", where,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(session: GeckoSession, url: String) {
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
        sessionRule.session.loadUri(dataUrl);
        sessionRule.waitForPageStop();

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String) {
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
            override fun onLocationChange(session: GeckoSession, url: String) {
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
        sessionRule.session.loadString(dataString, mimeType)
        sessionRule.waitForPageStop();

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate, Callbacks.ContentDelegate {
            @AssertCalled
            override fun onTitleChange(session: GeckoSession, title: String) {
                assertThat("Title should match", title, equalTo("TheTitle"));
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URL should be a data URL", url,
                           equalTo(GeckoSession.createDataUri(dataString, mimeType)))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @Test fun loadString_noMimeType() {
        sessionRule.session.loadString("Hello, World!", null)
        sessionRule.waitForPageStop();

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String) {
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

        sessionRule.session.loadData(bytes, "text/html");
        sessionRule.waitForPageStop();

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate, Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onTitleChange(session: GeckoSession, title: String) {
                assertThat("Title should match", title, equalTo("Hello, world!"))
            }

            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URL should match", url, equalTo(GeckoSession.createDataUri(bytes, "text/html")))
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    fun loadDataHelper(assetPath: String, mimeType: String? = null) {
        val bytes = getTestBytes(assetPath)
        assertThat("test data should have bytes", bytes.size, greaterThan(0))

        sessionRule.session.loadData(bytes, mimeType);
        sessionRule.waitForPageStop();

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URL should match", url, equalTo(GeckoSession.createDataUri(bytes, mimeType)))
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
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("URI should match", uri, endsWith(HELLO_HTML_PATH))
                assertThat("Where should match", where,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(session: GeckoSession, url: String) {
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
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.loadTestPath(HELLO2_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }
        })

        sessionRule.session.goBack()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("URI should match", uri, endsWith(HELLO_HTML_PATH))
                assertThat("Where should match", where,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(session: GeckoSession, url: String) {
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
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("URI should match", uri, endsWith(HELLO2_HTML_PATH))
                assertThat("Where should match", where,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return null
            }

            @AssertCalled(count = 1, order = [2])
            override fun onLocationChange(session: GeckoSession, url: String) {
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
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny> {
                val res : AllowOrDeny
                if (uri.endsWith(HELLO_HTML_PATH)) {
                    res = AllowOrDeny.DENY
                } else {
                    res = AllowOrDeny.ALLOW
                }
                return GeckoResult.fromValue(res)
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

    @WithDevToolsAPI
    @Test fun onNewSession_calledForWindowOpen() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("window.open('newSession_child.html', '_blank')")

        sessionRule.session.waitUntilCalled(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("URI should be correct", uri, endsWith(NEW_SESSION_CHILD_HTML_PATH))
                assertThat("Where should be correct", where,
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

    @WithDevToolsAPI
    @Test fun onNewSession_calledForTargetBlankLink() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("$('#targetBlankLink').click()")

        sessionRule.session.waitUntilCalled(object : Callbacks.NavigationDelegate {
            // We get two onLoadRequest calls for the link click,
            // one when loading the URL and one when opening a new window.
            @AssertCalled(count = 2, order = [1])
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("URI should be correct", uri, endsWith(NEW_SESSION_CHILD_HTML_PATH))
                assertThat("Where should be correct", where,
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

    private fun delegateNewSession(): GeckoSession {
        val newSession = sessionRule.createClosedSession()

        sessionRule.session.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession> {
                return GeckoResult.fromValue(newSession)
            }
        })

        return newSession
    }

    @WithDevToolsAPI
    @Test fun onNewSession_childShouldLoad() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        val newSession = delegateNewSession()
        sessionRule.session.evaluateJS("$('#targetBlankLink').click()")
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

    @WithDevToolsAPI
    @Test fun onNewSession_setWindowOpener() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        val newSession = delegateNewSession()
        sessionRule.session.evaluateJS("$('#targetBlankLink').click()")
        newSession.waitForPageStop()

        assertThat("window.opener should be set",
                   newSession.evaluateJS("window.opener.location.pathname") as String,
                   equalTo(NEW_SESSION_HTML_PATH))
    }

    @WithDevToolsAPI
    @Test fun onNewSession_supportNoOpener() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        val newSession = delegateNewSession()
        sessionRule.session.evaluateJS("$('#noOpenerLink').click()")
        newSession.waitForPageStop()

        assertThat("window.opener should not be set",
                   newSession.evaluateJS("window.opener"), nullValue())
    }

    @WithDevToolsAPI
    @Test fun onNewSession_notCalledForHandledLoads() {
        // Disable popup blocker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny> {
                // Pretend we handled the target="_blank" link click.
                val res : AllowOrDeny
                if (uri.endsWith(NEW_SESSION_CHILD_HTML_PATH)) {
                    res = AllowOrDeny.DENY
                } else {
                    res = AllowOrDeny.ALLOW
                }
                return GeckoResult.fromValue(res)
            }
        })

        sessionRule.session.evaluateJS("$('#targetBlankLink').click()")

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        // Assert that onNewSession was not called for the link click.
        sessionRule.session.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("URI must match", uri,
                           endsWith(forEachCall(NEW_SESSION_CHILD_HTML_PATH, NEW_SESSION_HTML_PATH)))
                return null
            }

            @AssertCalled(count = 0)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }
        })
    }

    @WithDevToolsAPI
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

        sessionRule.session.evaluateJS("$('#targetBlankLink').click()")

        sessionRule.session.waitUntilCalled(GeckoSession.NavigationDelegate::class,
                                            "onNewSession")
    }
}
