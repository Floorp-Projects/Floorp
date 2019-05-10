/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI


import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.util.Callbacks
import org.junit.Ignore

@RunWith(AndroidJUnit4::class)
@MediumTest
class HistoryDelegateTest : BaseSessionTest() {
    companion object {
        // Keep in sync with the styles in `LINKS_HTML_PATH`.
        const val UNVISITED_COLOR = "rgb(0, 0, 255)"
        const val VISITED_COLOR = "rgb(255, 0, 0)"
    }

    @WithDevToolsAPI
    @Test fun getVisited() {
        val testUri = createTestUrl(LINKS_HTML_PATH)
        sessionRule.delegateDuringNextWait(object : GeckoSession.HistoryDelegate {
            @AssertCalled(count = 1)
            override fun onVisited(session: GeckoSession, url: String,
                                   lastVisitedURL: String?,
                                   flags: Int): GeckoResult<Boolean>? {
                assertThat("Should pass visited URL", url, equalTo(testUri))
                assertThat("Should not pass last visited URL", lastVisitedURL, nullValue())
                assertThat("Should set visit flags", flags,
                    equalTo(GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL))
                return GeckoResult.fromValue(true)
            }

            @AssertCalled(count = 1)
            override fun getVisited(session: GeckoSession,
                                    urls: Array<String>) : GeckoResult<BooleanArray>? {
                val expected = arrayOf(
                    "https://mozilla.org/",
                    "https://getfirefox.com/",
                    "https://bugzilla.mozilla.org/",
                    "https://testpilot.firefox.com/",
                    "https://accounts.firefox.com/"
                )
                assertThat("Should pass URLs to check", urls.sorted(),
                    equalTo(expected.sorted()))

                val visits = BooleanArray(urls.size, {
                    when (urls[it]) {
                        "https://mozilla.org/", "https://testpilot.firefox.com/" -> true
                        else -> false
                    }
                })
                return GeckoResult.fromValue(visits)
            }
        })

        // Since `getVisited` is called asynchronously after the page loads, we
        // can't use `waitForPageStop` here.
        sessionRule.session.loadUri(testUri)
        sessionRule.session.waitUntilCalled(GeckoSession.HistoryDelegate::class,
                                            "onVisited", "getVisited")

        // Inject a frame script to query the `:visited` style of a link, using
        // a special chrome-only method. Note that we can't use the current
        // browsers s
        val frameScriptDataUri = GeckoSession.createDataUri(String.format("""
            addMessageListener("HistoryDelegateTest:GetLinkColor", function onMessage(message) {
                if (content.document.documentURI != "%s") {
                    return;
                }
                let { selector } = message.data;
                let element = content.document.querySelector(selector);
                if (!element) {
                    sendAsyncMessage("HistoryDelegateTest:GetLinkColor", {
                        ok: false,
                        error: "No element for " + selector,
                    });
                    return;
                }
                let color = content.windowUtils.getVisitedDependentComputedStyle(element, "", "color");
                sendAsyncMessage("HistoryDelegateTest:GetLinkColor", { ok: true, color });
            });
        """, testUri).toByteArray(), null)

        // Note that we can't send the message directly to the current browser,
        // because `gBrowser` might not refer to the correct window. Instead,
        // we broadcast the message using the global message manager, and have
        // the frame script check the document URI.
        sessionRule.evaluateChromeJS(String.format("""
            Services.mm.loadFrameScript("%s", true);
            function getLinkColor(selector) {
                return new Promise((resolve, reject) => {
                    Services.mm.addMessageListener("HistoryDelegateTest:GetLinkColor", function onMessage(message) {
                        Services.mm.removeMessageListener("HistoryDelegateTest:GetLinkColor", onMessage);
                        if (message.data.ok) {
                            resolve(message.data.color);
                        } else {
                            reject(message.data.error);
                        }
                    });
                    Services.mm.broadcastAsyncMessage('HistoryDelegateTest:GetLinkColor', { selector });
                });
            }
        """, frameScriptDataUri))

        assertThat(
            "Mozilla should be visited",
            sessionRule.waitForChromeJS("getLinkColor('#mozilla')") as String,
            equalTo(VISITED_COLOR)
        )

        assertThat(
            "Test Pilot should be visited",
            sessionRule.waitForChromeJS("getLinkColor('#testpilot')") as String,
            equalTo(VISITED_COLOR)
        )

        assertThat(
            "Bugzilla should be unvisited",
            sessionRule.waitForChromeJS("getLinkColor('#bugzilla')") as String,
            equalTo(UNVISITED_COLOR)
        )
    }

    @ReuseSession(false)
    @Ignore //disable test on debug for frequent failures Bug 1544169
    @Test fun onHistoryStateChange() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.HistoryDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: GeckoSession.HistoryDelegate.HistoryList) {
                assertThat("History should have one entry", state.size,
                        equalTo(1))
                assertThat("URLs should match", state[state.currentIndex].uri,
                        endsWith(HELLO_HTML_PATH))
                assertThat("History index should be 0", state.currentIndex,
                        equalTo(0))
            }
        })

        sessionRule.session.loadTestPath(HELLO2_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.HistoryDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: GeckoSession.HistoryDelegate.HistoryList) {
                assertThat("History should have two entries", state.size,
                        equalTo(2))
                assertThat("URLs should match", state[state.currentIndex].uri,
                        endsWith(HELLO2_HTML_PATH))
                assertThat("History index should be 1", state.currentIndex,
                        equalTo(1))
            }
        })

        sessionRule.session.goBack()

        sessionRule.waitUntilCalled(object : Callbacks.HistoryDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: GeckoSession.HistoryDelegate.HistoryList) {
                assertThat("History should have two entries", state.size,
                        equalTo(2))
                assertThat("URLs should match", state[state.currentIndex].uri,
                        endsWith(HELLO_HTML_PATH))
                assertThat("History index should be 0", state.currentIndex,
                        equalTo(0))
            }
        })

        sessionRule.session.goForward()

        sessionRule.waitUntilCalled(object : Callbacks.HistoryDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: GeckoSession.HistoryDelegate.HistoryList) {
                assertThat("History should have two entries", state.size,
                        equalTo(2))
                assertThat("URLs should match", state[state.currentIndex].uri,
                        endsWith(HELLO2_HTML_PATH))
                assertThat("History index should be 1", state.currentIndex,
                        equalTo(1))
            }
        })

        sessionRule.session.gotoHistoryIndex(0)

        sessionRule.waitUntilCalled(object : Callbacks.HistoryDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: GeckoSession.HistoryDelegate.HistoryList) {
                assertThat("History should have two entries", state.size,
                        equalTo(2))
                assertThat("URLs should match", state[state.currentIndex].uri,
                        endsWith(HELLO_HTML_PATH))
                assertThat("History index should be 1", state.currentIndex,
                        equalTo(0))
            }
        })

        sessionRule.session.gotoHistoryIndex(1)

        sessionRule.waitUntilCalled(object : Callbacks.HistoryDelegate {
            @AssertCalled(count = 1)
            override fun onHistoryStateChange(session: GeckoSession, state: GeckoSession.HistoryDelegate.HistoryList) {
                assertThat("History should have two entries", state.size,
                        equalTo(2))
                assertThat("URLs should match", state[state.currentIndex].uri,
                        endsWith(HELLO2_HTML_PATH))
                assertThat("History index should be 1", state.currentIndex,
                        equalTo(1))
            }
        })
    }
}
