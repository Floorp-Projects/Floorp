/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.HistoryDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled


import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Ignore
import org.mozilla.geckoview.test.util.UiThreadUtils

@RunWith(AndroidJUnit4::class)
@MediumTest
class HistoryDelegateTest : BaseSessionTest() {
    companion object {
        // Keep in sync with the styles in `LINKS_HTML_PATH`.
        const val UNVISITED_COLOR = "rgb(0, 0, 255)"
        const val VISITED_COLOR = "rgb(255, 0, 0)"
    }

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

        // Sometimes link changes are not applied immediately, wait for a little bit
        UiThreadUtils.waitForCondition({
            sessionRule.getLinkColor(testUri, "#mozilla") == VISITED_COLOR
        }, sessionRule.env.defaultTimeoutMillis)

        assertThat(
            "Mozilla should be visited",
            sessionRule.getLinkColor(testUri, "#mozilla"),
            equalTo(VISITED_COLOR)
        )

        assertThat(
            "Test Pilot should be visited",
            sessionRule.getLinkColor(testUri, "#testpilot"),
            equalTo(VISITED_COLOR)
        )

        assertThat(
            "Bugzilla should be unvisited",
            sessionRule.getLinkColor(testUri, "#bugzilla"),
            equalTo(UNVISITED_COLOR)
        )
    }

    @Ignore //disable test on debug for frequent failures Bug 1544169
    @Test fun onHistoryStateChange() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        sessionRule.waitUntilCalled(object : HistoryDelegate {
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

        sessionRule.waitUntilCalled(object : HistoryDelegate {
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

        sessionRule.waitUntilCalled(object : HistoryDelegate {
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

        sessionRule.waitUntilCalled(object : HistoryDelegate {
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

        sessionRule.waitUntilCalled(object : HistoryDelegate {
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

        sessionRule.waitUntilCalled(object : HistoryDelegate {
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

    @Test fun onHistoryStateChangeSavingState() {
        // TODO: Bug 1648158
        assumeThat(sessionRule.env.isFission, equalTo(false))

        // This is a smaller version of the above test, in the hopes to minimize race conditions
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        sessionRule.waitUntilCalled(object : HistoryDelegate {
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

        sessionRule.waitUntilCalled(object : HistoryDelegate {
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
