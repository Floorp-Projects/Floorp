/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoResponse
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.filters.LargeTest
import android.support.test.runner.AndroidJUnit4

import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class ProgressDelegateTest : BaseSessionTest() {

    @Test fun load() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", url, notNullValue())
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Security info should not be null", securityInfo, notNullValue())

                assertThat("Should not be secure", securityInfo.isSecure, equalTo(false))
                assertThat("Tracking mode should match",
                           securityInfo.trackingMode,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN))
            }

            @AssertCalled(count = 1, order = intArrayOf(3))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    fun loadExpectNetError(testUri: String) {
        sessionRule.session.loadUri(testUri);
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate, Callbacks.NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int,
                                       flags: Int,
                                       response: GeckoResponse<Boolean>) {
                if (sessionRule.currentCall.counter == 1) {
                    assertThat("URI should be " + testUri, uri, equalTo(testUri));
                } else {
                    assertThat("URI should be about:neterror", uri, startsWith("about:neterror"));
                }
                response.respond(false)
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should fail", success, equalTo(false))
            }
        })
    }

    @Test fun loadUnknownHost() {
        loadExpectNetError(INVALID_URI)
    }

    @Test fun loadBadPort() {
        loadExpectNetError("http://localhost:1/")
    }

    @Ignore
    @Test fun multipleLoads() {
        sessionRule.session.loadUri(INVALID_URI)
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 2, order = intArrayOf(1, 3))
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url,
                           endsWith(forEachCall(INVALID_URI, HELLO_HTML_PATH)))
            }

            @AssertCalled(count = 2, order = intArrayOf(2, 4))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                // The first load is certain to fail because of interruption by the second load
                // or by invalid domain name, whereas the second load is certain to succeed.
                assertThat("Success flag should match", success,
                           equalTo(forEachCall(false, true)))
            };
        })
    }

    @Test fun reload() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
            }

            @AssertCalled(count = 1, order = intArrayOf(3))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    @Test fun goBackAndForward() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
        sessionRule.session.loadTestPath(HELLO2_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.goBack()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
            }

            @AssertCalled(count = 1, order = intArrayOf(3))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })

        sessionRule.session.goForward()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
            }

            @AssertCalled(count = 1, order = intArrayOf(3))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    @Test fun correctSecurityInfoForValidTLS_automation() {
        assumeThat(sessionRule.env.isAutomation, equalTo(true))

        sessionRule.session.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
                assertThat("Should be secure",
                           securityInfo.isSecure, equalTo(true))
                assertThat("Should not be exception",
                           securityInfo.isException, equalTo(false))
                assertThat("Origin should match",
                           securityInfo.origin,
                           equalTo("https://example.com"))
                assertThat("Host should match",
                           securityInfo.host,
                           equalTo("example.com"))
                assertThat("Organization should match",
                           securityInfo.organization,
                           equalTo(""))
                assertThat("Subject name should match",
                           securityInfo.subjectName,
                           equalTo("CN=example.com"))
                assertThat("Issuer common name should match",
                           securityInfo.issuerCommonName,
                           equalTo("Temporary Certificate Authority"))
                assertThat("Issuer organization should match",
                           securityInfo.issuerOrganization,
                           equalTo("Mozilla Testing"))
                assertThat("Security mode should match",
                           securityInfo.securityMode,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.SECURITY_MODE_IDENTIFIED))
                assertThat("Active mixed mode should match",
                           securityInfo.mixedModeActive,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN))
                assertThat("Passive mixed mode should match",
                           securityInfo.mixedModePassive,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN))
                assertThat("Tracking mode should match",
                           securityInfo.trackingMode,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN))
            }
        })
    }

    @LargeTest
    @Test fun correctSecurityInfoForValidTLS_local() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))

        sessionRule.session.loadUri("https://mozilla-modern.badssl.com")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
                assertThat("Should be secure",
                           securityInfo.isSecure, equalTo(true))
                assertThat("Should not be exception",
                           securityInfo.isException, equalTo(false))
                assertThat("Origin should match",
                           securityInfo.origin,
                           equalTo("https://mozilla-modern.badssl.com"))
                assertThat("Host should match",
                           securityInfo.host,
                           equalTo("mozilla-modern.badssl.com"))
                assertThat("Organization should match",
                           securityInfo.organization,
                           equalTo("Lucas Garron"))
                assertThat("Subject name should match",
                           securityInfo.subjectName,
                           equalTo("CN=*.badssl.com,O=Lucas Garron,L=Walnut Creek,ST=California,C=US"))
                assertThat("Issuer common name should match",
                           securityInfo.issuerCommonName,
                           equalTo("DigiCert SHA2 Secure Server CA"))
                assertThat("Issuer organization should match",
                           securityInfo.issuerOrganization,
                           equalTo("DigiCert Inc"))
                assertThat("Security mode should match",
                           securityInfo.securityMode,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.SECURITY_MODE_IDENTIFIED))
                assertThat("Active mixed mode should match",
                           securityInfo.mixedModeActive,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN))
                assertThat("Passive mixed mode should match",
                           securityInfo.mixedModePassive,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN))
                assertThat("Tracking mode should match",
                           securityInfo.trackingMode,
                           equalTo(GeckoSession.ProgressDelegate.SecurityInformation.CONTENT_UNKNOWN))
            }
        })
    }

    @LargeTest
    @Test fun noSecurityInfoForExpiredTLS() {
        sessionRule.session.loadUri(if (sessionRule.env.isAutomation)
                                        "https://expired.example.com"
                                    else
                                        "https://expired.badssl.com")
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should fail", success, equalTo(false))
            }

            @AssertCalled(false)
            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
            }
        })
    }
}
