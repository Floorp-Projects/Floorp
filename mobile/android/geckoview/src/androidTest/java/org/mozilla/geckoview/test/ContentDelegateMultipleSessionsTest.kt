/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.IgnoreCrash

import androidx.annotation.AnyThread
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*


@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentDelegateMultipleSessionsTest : BaseSessionTest() {
    val contentProcNameRegex = ".*:tab\\d+$".toRegex()

    @AnyThread
    fun killAllContentProcesses() {
        val contentProcessPids = sessionRule.getAllSessionPids()
        for (pid in contentProcessPids) {
            sessionRule.killContentProcess(pid)
        }
    }

    fun resetContentProcesses() {
        val isMainSessionAlreadyOpen = mainSession.isOpen()
        killAllContentProcesses()

        if (isMainSessionAlreadyOpen) {
            mainSession.waitUntilCalled(object : ContentDelegate {
                @AssertCalled(count = 1)
                override fun onKill(session: GeckoSession) {
                }
            })
        }

        mainSession.open()
    }

    fun getE10sProcessCount(): Int {
        val extensionProcessPref = "extensions.webextensions.remote"
        val isExtensionProcessEnabled = (sessionRule.getPrefs(extensionProcessPref)[0] as Boolean)
        val e10sProcessCountPref = "dom.ipc.processCount"
        var numContentProcesses = (sessionRule.getPrefs(e10sProcessCountPref)[0] as Int)

        if (isExtensionProcessEnabled && numContentProcesses > 1) {
            // Extension process counts against the content process budget
            --numContentProcesses 
        }

        return numContentProcesses
    }

    // This function ensures that a second GeckoSession that shares the same
    // content process as mainSession is returned to the test:
    //
    // First, we assume that we're starting with a known initial state with respect
    // to sessions and content processes:
    // * mainSession is the only session, it is open, and its content process is the only
    //   content process (but note that the content process assigned to mainSession is
    //   *not* guaranteed to be ":tab0").
    // * With multi-e10s configured to run N content processes, we create and open
    //   an additional N content processes. With the default e10s process allocation
    //   scheme, this means that the first N-1 new sessions we create each get their
    //   own content process. The Nth new session is assigned to the same content
    //   process as mainSession, which is the session we want to return to the test.
    fun getSecondGeckoSession(): GeckoSession {
        val numContentProcesses = getE10sProcessCount()

        // If we change the content process allocation scheme, this function will need to be
        // fixed to ensure that we still have two test sessions in at least one content
        // process (with one of those sessions being mainSession).
        val additionalSessions = Array(numContentProcesses) { _ -> sessionRule.createOpenSession() }

        // The second session that shares a process with mainSession should be at
        // the end of the array.
        return additionalSessions.last()
    }

    @Before
    fun setup() {
        resetContentProcesses()
    }

    @IgnoreCrash
    @Test fun crashContentMultipleSessions() {
        // TODO: Bug 1673952
        assumeThat(sessionRule.env.isFission, equalTo(false))

        val newSession = getSecondGeckoSession()

        // We can inadvertently catch the `onCrash` call for the cached session if we don't specify
        // individual sessions here. Therefore, assert 'onCrash' is called for the two sessions
        // individually...
        val mainSessionCrash = GeckoResult<Void>()
        val newSessionCrash = GeckoResult<Void>()

        // ...but we use GeckoResult.allOf for waiting on the aggregated results
        val allCrashesFound = GeckoResult.allOf(mainSessionCrash, newSessionCrash)

        sessionRule.delegateUntilTestEnd(object : ContentDelegate {
            fun reportCrash(session: GeckoSession) {
                if (session == mainSession) {
                    mainSessionCrash.complete(null)
                } else if (session == newSession) {
                    newSessionCrash.complete(null)
                }
            }
            // Slower devices may not catch crashes in a timely manner, so we check to see
            // if either `onKill` or `onCrash` is called
            override fun onCrash(session: GeckoSession) {
                reportCrash(session)
            }
            override fun onKill(session: GeckoSession) {
                reportCrash(session)
            }
        })

        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitForPageStop()

        mainSession.loadUri(CONTENT_CRASH_URL)

        sessionRule.waitForResult(allCrashesFound)
    }

    @IgnoreCrash
    @Test fun killContentMultipleSessions() {
        val newSession = getSecondGeckoSession()

        val mainSessionKilled = GeckoResult<Void>()
        val newSessionKilled = GeckoResult<Void>()

        val allKillEventsReceived = GeckoResult.allOf(mainSessionKilled, newSessionKilled)

        sessionRule.delegateUntilTestEnd(object : ContentDelegate {
            override fun onKill(session: GeckoSession) {
                if (session == mainSession) {
                    mainSessionKilled.complete(null)
                } else if (session == newSession) {
                    newSessionKilled.complete(null)
                }
            }
        })

        killAllContentProcesses()

        sessionRule.waitForResult(allKillEventsReceived)
    }
}
