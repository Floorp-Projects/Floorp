/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ClosedSessionAtStart
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4

import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class SessionLifecycleTest : BaseSessionTest() {

    @Test fun open_interleaved() {
        val session1 = sessionRule.createOpenSession()
        val session2 = sessionRule.createOpenSession()
        session1.close()
        val session3 = sessionRule.createOpenSession()
        session2.close()
        session3.close()

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()
    }

    @Test fun open_repeated() {
        for (i in 1..5) {
            sessionRule.session.close()
            sessionRule.session.open()
        }
        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()
    }

    @Test fun open_allowCallsWhileClosed() {
        sessionRule.session.close()

        sessionRule.session.loadUri(HELLO_HTML_PATH)
        sessionRule.session.reload()

        sessionRule.session.open()
        sessionRule.session.waitForPageStops(2)
    }

    @Test(expected = IllegalStateException::class)
    fun open_throwOnAlreadyOpen() {
        // Throw exception if retrying to open again; otherwise we would leak the old open window.
        sessionRule.session.open()
    }

    @Test(expected = IllegalStateException::class)
    fun setChromeURI_throwOnOpenSession() {
        sessionRule.session.settings.setString(GeckoSessionSettings.CHROME_URI, "chrome://invalid/path/to.xul")
    }

    @Test(expected = IllegalStateException::class)
    fun setScreenID_throwOnOpenSession() {
        sessionRule.session.settings.setInt(GeckoSessionSettings.SCREEN_ID, 42)
    }

    @Test(expected = IllegalStateException::class)
    fun setUsePrivateMode_throwOnOpenSession() {
        sessionRule.session.settings.setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, true)
    }

    @Test(expected = IllegalStateException::class)
    fun setUseMultiprocess_throwOnOpenSession() {
        sessionRule.session.settings.setBoolean(
                GeckoSessionSettings.USE_MULTIPROCESS,
                !sessionRule.session.settings.getBoolean(GeckoSessionSettings.USE_MULTIPROCESS))
    }

    @Test fun readFromParcel() {
        val session = sessionRule.createOpenSession()

        session.toParcel { parcel ->
            val newSession = sessionRule.createClosedSession()
            newSession.readFromParcel(parcel)

            assertThat("New session has same settings",
                       newSession.settings, equalTo(session.settings))
            assertThat("New session is open", newSession.isOpen, equalTo(true))

            newSession.close()
            assertThat("New session can be closed", newSession.isOpen, equalTo(false))
        }

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()
    }

    @Test(expected = IllegalStateException::class)
    fun readFromParcel_throwOnAlreadyOpen() {
        // Throw exception if retrying to open again; otherwise we would leak the old open window.
        sessionRule.session.toParcel { parcel ->
            sessionRule.createOpenSession().readFromParcel(parcel)
        }
    }

    @Test fun readFromParcel_canLoadPageAfterRead() {
        val newSession = sessionRule.createClosedSession()

        sessionRule.session.toParcel { parcel ->
            newSession.readFromParcel(parcel)
        }

        newSession.reload()
        newSession.waitForPageStop()
    }

    @Test fun readFromParcel_closedSession() {
        val session = sessionRule.createClosedSession()

        session.toParcel { parcel ->
            val newSession = sessionRule.createClosedSession()
            newSession.readFromParcel(parcel)
            assertThat("New session should not be open",
                       newSession.isOpen, equalTo(false))
        }

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()
    }

    @Test fun readFromParcel_closedSessionAfterParceling() {
        val session = sessionRule.createOpenSession()

        session.toParcel { parcel ->
            session.close()

            val newSession = sessionRule.createClosedSession()
            newSession.readFromParcel(parcel)
            assertThat("New session should not be open",
                       newSession.isOpen, equalTo(false))
        }

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()
    }

    @Test fun readFromParcel_closeOpenAndLoad() {
        val newSession = sessionRule.createClosedSession()

        sessionRule.session.toParcel { parcel ->
            newSession.readFromParcel(parcel)
        }

        newSession.close()
        newSession.open()

        newSession.reload()
        newSession.waitForPageStop()
    }

    @Test fun readFromParcel_allowCallsBeforeUnparceling() {
        val newSession = sessionRule.createClosedSession()

        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.reload()

        sessionRule.session.toParcel { parcel ->
            newSession.readFromParcel(parcel)
        }
        newSession.waitForPageStops(2)
    }

    @Test fun readFromParcel_chained() {
        val session1 = sessionRule.createClosedSession()
        val session2 = sessionRule.createClosedSession()
        val session3 = sessionRule.createClosedSession()

        sessionRule.session.toParcel { parcel ->
            session1.readFromParcel(parcel)
        }
        session1.toParcel { parcel ->
            session2.readFromParcel(parcel)
        }
        session2.toParcel { parcel ->
            session3.readFromParcel(parcel)
        }

        session3.reload()
        session3.waitForPageStop()
    }

    @NullDelegate(GeckoSession.NavigationDelegate::class)
    @ClosedSessionAtStart
    @Test fun readFromParcel_moduleUpdated() {
        val session = sessionRule.createOpenSession()

        // Disable navigation notifications on the old, open session.
        assertThat("Old session navigation delegate should be null",
                   session.navigationDelegate, nullValue())

        // Enable navigation notifications on the new, closed session.
        var onLocationCount = 0
        sessionRule.session.navigationDelegate = object : Callbacks.NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, url: String) {
                onLocationCount++
            }
        }

        // Transferring the old session to the new session should
        // automatically re-enable navigation notifications.
        session.toParcel { parcel ->
            sessionRule.session.readFromParcel(parcel)
        }

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        assertThat("New session should receive navigation notifications",
                   onLocationCount, equalTo(1))
    }

    @Test fun createFromParcel() {
        val session = sessionRule.createOpenSession()

        session.toParcel { parcel ->
            val newSession = sessionRule.wrapSession(
                    GeckoSession.CREATOR.createFromParcel(parcel))

            assertThat("New session has same settings",
                       newSession.settings, equalTo(session.settings))
            assertThat("New session is open", newSession.isOpen, equalTo(true))

            newSession.close()
            assertThat("New session can be closed", newSession.isOpen, equalTo(false))
        }

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()
    }
}
