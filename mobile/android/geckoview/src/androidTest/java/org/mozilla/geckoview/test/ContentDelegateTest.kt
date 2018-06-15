/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoResponse
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.IgnoreCrash
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentDelegateTest : BaseSessionTest() {

    @Test fun titleChange() {
        sessionRule.session.loadTestPath(TITLE_CHANGE_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 2)
            override fun onTitleChange(session: GeckoSession, title: String) {
                assertThat("Title should match", title,
                           equalTo(forEachCall("Title1", "Title2")))
            }
        })
    }

    @Test fun download() {
        sessionRule.session.loadTestPath(DOWNLOAD_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.NavigationDelegate, Callbacks.ContentDelegate {

            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int,
                                       response: GeckoResponse<Boolean>) {
                response.respond(false)
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String, response: GeckoResponse<GeckoSession>) {
            }

            @AssertCalled(count = 1)
            override fun onExternalResponse(session: GeckoSession, response: GeckoSession.WebResponseInfo) {
                assertThat("Uri should start with data:", response.uri, startsWith("data:"))
                assertThat("Content type should match", response.contentType, equalTo("text/plain"))
                assertThat("Content length should be non-zero", response.contentLength, greaterThan(0L))
                assertThat("Filename should match", response.filename, equalTo("download.txt"))
            }
        })
    }

    @IgnoreCrash
    @ReuseSession(false)
    @Test fun crashContent() {
        // This test doesn't make sense without multiprocess
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        // Cannot test x86 debug builds due to Gecko's "ah_crap_handler"
        // that waits for debugger to attach during a SIGSEGV.
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.cpuArch == "x86",
                   equalTo(false))

        mainSession.loadUri(CONTENT_CRASH_URL)
        mainSession.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onCrash(session: GeckoSession) {
                assertThat("Session should be closed after a crash",
                           session.isOpen, equalTo(false))
            }
        });

        // Recover immediately
        mainSession.open()
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitUntilCalled(object: Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @IgnoreCrash
    @ReuseSession(false)
    @Test fun crashContentMultipleSessions() {
        // This test doesn't make sense without multiprocess
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        // Cannot test x86 debug builds due to Gecko's "ah_crap_handler"
        // that waits for debugger to attach during a SIGSEGV.
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.cpuArch == "x86",
                   equalTo(false))

        // XXX we need to make sure all sessions in a given content process receive onCrash().
        // If we add multiple content processes, this test will need to be fixed to ensure the
        // test sessions go into the same one.
        val newSession = sessionRule.createOpenSession()
        mainSession.loadUri(CONTENT_CRASH_URL)

        // We can inadvertently catch the `onCrash` call for the cached session if we don't specify
        // individual sessions here. Therefore, assert 'onCrash' is called for the two sessions
        // individually.
        val remainingSessions = mutableListOf(newSession, mainSession)
        while (remainingSessions.isNotEmpty()) {
            sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
                @AssertCalled(count = 1)
                override fun onCrash(session: GeckoSession) {
                    remainingSessions.remove(session)
                }
            })
        }
    }

    @WithDevToolsAPI
    @WithDisplay(width = 400, height = 400)
    @Test fun saveAndRestoreState() {
        val startUri = createTestUrl(SAVE_STATE_PATH)
        mainSession.loadUri(startUri)
        sessionRule.waitForPageStop()

        mainSession.evaluateJS("$('#name').value = 'the name'; window.scrollBy(0, 100);")

        val state = sessionRule.waitForResult(mainSession.saveState())
        assertThat("State should not be null", state, notNullValue())

        mainSession.loadUri("about:blank")
        sessionRule.waitForPageStop()

        mainSession.restoreState(state)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URI should match", url, equalTo(startUri))
            }
        })

        assertThat("'name' field should match",
                mainSession.evaluateJS("$('#name').value").toString(),
                equalTo("the name"))

        assertThat("Scroll position should match",
                mainSession.evaluateJS("window.scrollY") as Double,
                closeTo(100.0, .5))
    }
}
