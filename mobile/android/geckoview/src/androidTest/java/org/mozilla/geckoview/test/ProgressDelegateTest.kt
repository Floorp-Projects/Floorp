/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Assume.assumeThat
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission
import org.mozilla.geckoview.GeckoSession.ProgressDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.* // ktlint-disable no-wildcard-imports

@RunWith(AndroidJUnit4::class)
@MediumTest
class ProgressDelegateTest : BaseSessionTest() {

    fun testProgress(path: String) {
        mainSession.loadTestPath(path)
        sessionRule.waitForPageStop()

        var counter = 0
        var lastProgress = -1

        sessionRule.forCallbacksDuringWait(object :
            ProgressDelegate,
            NavigationDelegate {
            @AssertCalled
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<ContentPermission>) {
                assertThat("LocationChange is called", url, endsWith(path))
            }

            @AssertCalled
            override fun onProgressChange(session: GeckoSession, progress: Int) {
                assertThat(
                    "Progress must be strictly increasing",
                    progress,
                    greaterThan(lastProgress),
                )
                lastProgress = progress
                counter++
            }

            @AssertCalled
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("PageStart is called", url, endsWith(path))
            }

            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("PageStop is called", success, equalTo(true))
            }
        })

        assertThat(
            "Callback should be called at least twice",
            counter,
            greaterThanOrEqualTo(2),
        )
        assertThat(
            "Last progress value should be 100",
            lastProgress,
            equalTo(100),
        )
    }

    @Test fun loadProgress() {
        testProgress(HELLO_HTML_PATH)
        // Test that loading the same path again still
        // results in the right progress events
        testProgress(HELLO_HTML_PATH)
        // Test that calling a different path works too
        testProgress(HELLO2_HTML_PATH)
    }

    @Test fun load() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", url, notNullValue())
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onSecurityChange(
                session: GeckoSession,
                securityInfo: GeckoSession.ProgressDelegate.SecurityInformation,
            ) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Security info should not be null", securityInfo, notNullValue())

                assertThat("Should not be secure", securityInfo.isSecure, equalTo(false))
            }

            @AssertCalled(count = 1, order = [3])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    @Ignore
    @Test
    fun multipleLoads() {
        mainSession.loadUri(UNKNOWN_HOST_URI)
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 2, order = [1, 3])
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat(
                    "URL should match",
                    url,
                    endsWith(forEachCall(UNKNOWN_HOST_URI, HELLO_HTML_PATH)),
                )
            }

            @AssertCalled(count = 2, order = [2, 4])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                // The first load is certain to fail because of interruption by the second load
                // or by invalid domain name, whereas the second load is certain to succeed.
                assertThat(
                    "Success flag should match",
                    success,
                    equalTo(forEachCall(false, true)),
                )
            }
        })
    }

    @Test fun reload() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        mainSession.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onSecurityChange(
                session: GeckoSession,
                securityInfo: GeckoSession.ProgressDelegate.SecurityInformation,
            ) {
            }

            @AssertCalled(count = 1, order = [3])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    @Test fun goBackAndForward() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
        mainSession.loadTestPath(HELLO2_HTML_PATH)
        sessionRule.waitForPageStop()

        mainSession.goBack()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onSecurityChange(
                session: GeckoSession,
                securityInfo: GeckoSession.ProgressDelegate.SecurityInformation,
            ) {
            }

            @AssertCalled(count = 1, order = [3])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })

        mainSession.goForward()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onSecurityChange(
                session: GeckoSession,
                securityInfo: GeckoSession.ProgressDelegate.SecurityInformation,
            ) {
            }

            @AssertCalled(count = 1, order = [3])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    @Test fun correctSecurityInfoForValidTLS_automation() {
        assumeThat(sessionRule.env.isAutomation, equalTo(true))

        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSecurityChange(
                session: GeckoSession,
                securityInfo: GeckoSession.ProgressDelegate.SecurityInformation,
            ) {
                assertThat(
                    "Should be secure",
                    securityInfo.isSecure,
                    equalTo(true),
                )
                assertThat(
                    "Should not be exception",
                    securityInfo.isException,
                    equalTo(false),
                )
                assertThat(
                    "Origin should match",
                    securityInfo.origin,
                    equalTo("https://example.com"),
                )
                assertThat(
                    "Host should match",
                    securityInfo.host,
                    equalTo("example.com"),
                )
                assertThat(
                    "Subject should match",
                    securityInfo.certificate?.subjectX500Principal?.name,
                    equalTo("CN=example.com"),
                )
                assertThat(
                    "Issuer should match",
                    securityInfo.certificate?.issuerX500Principal?.name,
                    equalTo("OU=Profile Guided Optimization,O=Mozilla Testing,CN=Temporary Certificate Authority"),
                )
                assertThat(
                    "Security mode should match",
                    securityInfo.securityMode,
                    equalTo(GeckoSession.ProgressDelegate.SecurityInformation.SECURITY_MODE_IDENTIFIED),
                )
                assertThat(
                    "Active mixed mode should match",
                    securityInfo.mixedModeActive,
                    equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN),
                )
                assertThat(
                    "Passive mixed mode should match",
                    securityInfo.mixedModePassive,
                    equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN),
                )
            }
        })
    }

    @LargeTest
    @Test
    fun correctSecurityInfoForValidTLS_local() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))

        mainSession.loadUri("https://mozilla-modern.badssl.com")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSecurityChange(
                session: GeckoSession,
                securityInfo: GeckoSession.ProgressDelegate.SecurityInformation,
            ) {
                assertThat(
                    "Should be secure",
                    securityInfo.isSecure,
                    equalTo(true),
                )
                assertThat(
                    "Should not be exception",
                    securityInfo.isException,
                    equalTo(false),
                )
                assertThat(
                    "Origin should match",
                    securityInfo.origin,
                    equalTo("https://mozilla-modern.badssl.com"),
                )
                assertThat(
                    "Host should match",
                    securityInfo.host,
                    equalTo("mozilla-modern.badssl.com"),
                )
                assertThat(
                    "Subject should match",
                    securityInfo.certificate?.subjectX500Principal?.name,
                    equalTo("CN=*.badssl.com,O=Lucas Garron,L=Walnut Creek,ST=California,C=US"),
                )
                assertThat(
                    "Issuer should match",
                    securityInfo.certificate?.issuerX500Principal?.name,
                    equalTo("CN=DigiCert SHA2 Secure Server CA,O=DigiCert Inc,C=US"),
                )
                assertThat(
                    "Security mode should match",
                    securityInfo.securityMode,
                    equalTo(GeckoSession.ProgressDelegate.SecurityInformation.SECURITY_MODE_IDENTIFIED),
                )
                assertThat(
                    "Active mixed mode should match",
                    securityInfo.mixedModeActive,
                    equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN),
                )
                assertThat(
                    "Passive mixed mode should match",
                    securityInfo.mixedModePassive,
                    equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN),
                )
            }
        })
    }

    @LargeTest
    @Test
    fun noSecurityInfoForExpiredTLS() {
        mainSession.loadUri(
            if (sessionRule.env.isAutomation) {
                "https://expired.example.com"
            } else {
                "https://expired.badssl.com"
            },
        )
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should fail", success, equalTo(false))
            }

            @AssertCalled(false)
            override fun onSecurityChange(
                session: GeckoSession,
                securityInfo: GeckoSession.ProgressDelegate.SecurityInformation,
            ) {
            }
        })
    }

    val errorEpsilon = 0.1

    private fun waitForScroll(offset: Double, timeout: Double, param: String) {
        mainSession.evaluateJS(
            """
           new Promise((resolve, reject) => {
             const start = Date.now();
             function step() {
               if (window.visualViewport.$param >= ($offset - $errorEpsilon)) {
                 resolve();
               } else if ($timeout < (Date.now() - start)) {
                 reject();
               } else {
                 window.requestAnimationFrame(step);
               }
             }
             window.requestAnimationFrame(step);
           });
            """.trimIndent(),
        )
    }

    private fun waitForVerticalScroll(offset: Double, timeout: Double) {
        waitForScroll(offset, timeout, "pageTop")
    }

    fun collectState(vararg uris: String): GeckoSession.SessionState {
        for (uri in uris) {
            mainSession.loadUri(uri)
            sessionRule.waitForPageStop()
        }

        mainSession.evaluateJS("document.querySelector('#name').value = 'the name';")
        mainSession.evaluateJS("document.querySelector('#name').dispatchEvent(new Event('input'));")

        mainSession.evaluateJS("window.scrollBy(0, 100);")
        waitForVerticalScroll(100.0, sessionRule.env.defaultTimeoutMillis.toDouble())

        var savedState: GeckoSession.SessionState? = null
        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSessionStateChange(session: GeckoSession, state: GeckoSession.SessionState) {
                savedState = state

                val serialized = state.toString()
                val deserialized = GeckoSession.SessionState.fromString(serialized)
                assertThat("Deserialized session state should match", deserialized, equalTo(state))
            }
        })

        assertThat("State should not be null", savedState, notNullValue())
        return savedState!!
    }

    @WithDisplay(width = 400, height = 400)
    @Test
    fun containsFormData() {
        val startUri = createTestUrl(SAVE_STATE_PATH)
        mainSession.loadUri(startUri)
        sessionRule.waitForPageStop()

        val formData = mainSession.containsFormData()
        sessionRule.waitForResult(formData).let {
            assertThat("There should be no form data", it, equalTo(false))
        }

        mainSession.evaluateJS("document.querySelector('#name').value = 'the name';")
        mainSession.evaluateJS("document.querySelector('#name').dispatchEvent(new Event('input'));")

        val formData2 = mainSession.containsFormData()
        sessionRule.waitForResult(formData2).let {
            assertThat("There should be form data", it, equalTo(true))
        }
    }

    @WithDisplay(width = 400, height = 400)
    @Test
    fun saveAndRestoreStateNewSession() {
        // TODO: Bug 1837551
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val helloUri = createTestUrl(HELLO_HTML_PATH)
        val startUri = createTestUrl(SAVE_STATE_PATH)

        val savedState = collectState(helloUri, startUri)

        val session = sessionRule.createOpenSession()
        session.addDisplay(400, 400)

        session.restoreState(savedState)
        session.waitForPageStop()

        session.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled
            override fun onLocationChange(
                session: GeckoSession,
                url: String?,
                perms: MutableList<ContentPermission>,
            ) {
                assertThat("URI should match", url, equalTo(startUri))
            }
        })

        /* TODO: Reenable when we have a workaround for ContentSessionStore not
                 saving in response to JS-driven formdata changes.
        assertThat("'name' field should match",
                mainSession.evaluateJS("$('#name').value").toString(),
                equalTo("the name"))*/

        assertThat(
            "Scroll position should match",
            session.evaluateJS("window.visualViewport.pageTop") as Double,
            closeTo(100.0, .5),
        )

        session.goBack()

        session.waitUntilCalled(object : NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<ContentPermission>) {
                assertThat("History should be preserved", url, equalTo(helloUri))
            }
        })
    }

    @WithDisplay(width = 400, height = 400)
    @Test
    fun saveAndRestoreState() {
        // Bug 1662035 - disable to reduce intermittent failures
        assumeThat(sessionRule.env.isX86, equalTo(false))
        // TODO: Bug 1837551
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val startUri = createTestUrl(SAVE_STATE_PATH)
        val savedState = collectState(startUri)

        mainSession.loadUri("about:blank")
        sessionRule.waitForPageStop()

        mainSession.restoreState(savedState)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
            @AssertCalled
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<ContentPermission>) {
                assertThat("URI should match", url, equalTo(startUri))
            }
        })

        /* TODO: Reenable when we have a workaround for ContentSessionStore not
                 saving in response to JS-driven formdata changes.
        assertThat("'name' field should match",
                mainSession.evaluateJS("$('#name').value").toString(),
                equalTo("the name"))*/

        assertThat(
            "Scroll position should match",
            mainSession.evaluateJS("window.visualViewport.pageTop") as Double,
            closeTo(100.0, .5),
        )
    }

    @WithDisplay(width = 400, height = 400)
    @Test
    fun flushSessionState() {
        // TODO: Bug 1837551
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val startUri = createTestUrl(SAVE_STATE_PATH)
        mainSession.loadUri(startUri)
        sessionRule.waitForPageStop()

        var oldState: GeckoSession.SessionState? = null

        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSessionStateChange(session: GeckoSession, sessionState: GeckoSession.SessionState) {
                oldState = sessionState
            }
        })

        assertThat("State should not be null", oldState, notNullValue())

        mainSession.setActive(false)

        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSessionStateChange(session: GeckoSession, sessionState: GeckoSession.SessionState) {
                assertThat("Old session state and new should match", sessionState, equalTo(oldState))
            }
        })
    }

    @Test fun nullState() {
        val stateFromNull: GeckoSession.SessionState? = GeckoSession.SessionState.fromString(null)
        val nullState: GeckoSession.SessionState? = null
        assertThat("Null string should result in null state", stateFromNull, equalTo(nullState))
    }

    @NullDelegate(GeckoSession.HistoryDelegate::class)
    @Test
    fun noHistoryDelegateOnSessionStateChange() {
        // TODO: Bug 1837551
        assumeThat(sessionRule.env.isFission, equalTo(false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSessionStateChange(session: GeckoSession, sessionState: GeckoSession.SessionState) {
            }
        })
    }
}
