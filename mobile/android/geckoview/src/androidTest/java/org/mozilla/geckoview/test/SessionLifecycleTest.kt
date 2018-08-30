/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.GeckoView
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ClosedSessionAtStart
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.test.util.UiThreadUtils

import android.os.Debug
import android.os.Parcelable
import android.os.SystemClock
import android.support.test.InstrumentationRegistry
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import android.util.Log
import android.util.SparseArray

import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.io.IOException
import java.lang.ref.ReferenceQueue
import java.lang.ref.WeakReference

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
class SessionLifecycleTest : BaseSessionTest() {
    companion object {
        val LOGTAG = "SessionLifecycleTest"
    }

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

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
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
            assertThat("Session is still open", session.isOpen, equalTo(true))
            session.close()

            val newSession = sessionRule.createClosedSession()
            newSession.readFromParcel(parcel)
            assertThat("New session should not be open",
                       newSession.isOpen, equalTo(false))
        }

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()
    }

    @Test fun readFromParcel_closedSessionAfterReadParcel() {
        val session = sessionRule.createOpenSession()

        session.toParcel { parcel ->
            assertThat("Session is still open", session.isOpen, equalTo(true))
            val newSession = sessionRule.createClosedSession()
            newSession.readFromParcel(parcel)
            assertThat("New session should be open",
                    newSession.isOpen, equalTo(true))
            assertThat("Old session should be closed",
                    session.isOpen, equalTo(false))
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

    private fun testRestoreInstanceState(fromSession: GeckoSession?,
                                         ontoSession: GeckoSession?) =
            GeckoView(InstrumentationRegistry.getTargetContext()).apply {
                id = 0
                if (fromSession != null) {
                    setSession(fromSession, sessionRule.runtime)
                }

                val state = SparseArray<Parcelable>()
                saveHierarchyState(state)

                if (ontoSession !== fromSession) {
                    releaseSession()
                    if (ontoSession != null) {
                        setSession(ontoSession, sessionRule.runtime)
                    }
                }
                restoreHierarchyState(state)
            }

    @ClosedSessionAtStart
    @Test fun restoreInstanceState_noSessionOntoNoSession() {
        val view = testRestoreInstanceState(null, null)
        assertThat("View session is restored", view.session, nullValue())
    }

    @ClosedSessionAtStart
    @Test fun restoreInstanceState_closedSessionOntoNoSession() {
        val view = testRestoreInstanceState(mainSession, null)
        assertThat("View session is restored", view.session, equalTo(mainSession))
        assertThat("View session is closed", view.session?.isOpen, equalTo(false))
    }

    @Test fun restoreInstanceState_openSessionOntoNoSession() {
        val view = testRestoreInstanceState(mainSession, null)
        assertThat("View session is restored", view.session, equalTo(mainSession))
        assertThat("View session is open", view.session?.isOpen, equalTo(true))
        view.session?.reload()
        sessionRule.waitForPageStop()
    }

    @ClosedSessionAtStart
    @Test fun restoreInstanceState_noSessionOntoClosedSession() {
        val view = testRestoreInstanceState(null, sessionRule.createClosedSession())
        assertThat("View session is not restored", view.session, notNullValue())
        assertThat("View session is closed", view.session?.isOpen, equalTo(false))
    }

    @ClosedSessionAtStart
    @Test fun restoreInstanceState_closedSessionOntoClosedSession() {
        val view = testRestoreInstanceState(mainSession, sessionRule.createClosedSession())
        assertThat("View session is restored", view.session, equalTo(mainSession))
        assertThat("View session is closed", view.session?.isOpen, equalTo(false))
    }

    @Test fun restoreInstanceState_openSessionOntoClosedSession() {
        val view = testRestoreInstanceState(mainSession, sessionRule.createClosedSession())
        assertThat("View session is restored", view.session, equalTo(mainSession))
        assertThat("View session is open", view.session?.isOpen, equalTo(true))
        view.session?.reload()
        sessionRule.waitForPageStop()
    }

    @ClosedSessionAtStart
    @Test fun restoreInstanceState_noSessionOntoOpenSession() {
        val view = testRestoreInstanceState(null, sessionRule.createOpenSession())
        assertThat("View session is not restored", view.session, notNullValue())
        assertThat("View session is open", view.session?.isOpen, equalTo(true))
        view.session?.reload()
        sessionRule.waitForPageStop()
    }

    @ClosedSessionAtStart
    @Test fun restoreInstanceState_closedSessionOntoOpenSession() {
        val view = testRestoreInstanceState(mainSession, sessionRule.createOpenSession())
        assertThat("View session is not restored", view.session, not(equalTo(mainSession)))
        assertThat("View session is open", view.session?.isOpen, equalTo(true))
        view.session?.reload()
        sessionRule.waitForPageStop()
    }

    @Test fun restoreInstanceState_openSessionOntoOpenSession() {
        val view = testRestoreInstanceState(mainSession, sessionRule.createOpenSession())
        assertThat("View session is restored", view.session, equalTo(mainSession))
        assertThat("View session is open", view.session?.isOpen, equalTo(true))
        view.session?.reload()
        sessionRule.waitForPageStop()
    }

    @ClosedSessionAtStart
    @Test fun restoreInstanceState_sameClosedSession() {
        val view = testRestoreInstanceState(mainSession, mainSession)
        assertThat("View session is unchanged", view.session, equalTo(mainSession))
        assertThat("View session is closed", view.session.isOpen, equalTo(false))
    }

    @Test fun restoreInstanceState_sameOpenSession() {
        // We should keep the session open when restoring the same open session.
        val view = testRestoreInstanceState(mainSession, mainSession)
        assertThat("View session is unchanged", view.session, equalTo(mainSession))
        assertThat("View session is open", view.session.isOpen, equalTo(true))
        view.session.reload()
        sessionRule.waitForPageStop()
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

    @Test fun collectClosed() {
        // We can't use a normal scoped function like `run` because
        // those are inlined, which leaves a local reference.
        fun createSession(): QueuedWeakReference<GeckoSession> {
            return QueuedWeakReference<GeckoSession>(GeckoSession())
        }

        waitUntilCollected(createSession())
    }

    @Test fun collectAfterClose() {
        fun createSession(): QueuedWeakReference<GeckoSession> {
            val s = GeckoSession()
            s.open(sessionRule.runtime)
            s.close()
            return QueuedWeakReference<GeckoSession>(s)
        }

        waitUntilCollected(createSession())
    }

    @Test fun collectOpen() {
        fun createSession(): QueuedWeakReference<GeckoSession> {
            val s = GeckoSession()
            s.open(sessionRule.runtime)
            return QueuedWeakReference<GeckoSession>(s)
        }

        waitUntilCollected(createSession())
    }

    private fun dumpHprof() {
        try {
            val dest = File(InstrumentationRegistry.getTargetContext()
                    .filesDir.parent, "dump.hprof").absolutePath
            Debug.dumpHprofData(dest)
            Log.d(LOGTAG, "Dumped hprof to $dest")
        } catch (e: IOException) {
            Log.e(LOGTAG, "Failed to dump hprof", e)
        }

    }

    private fun waitUntilCollected(ref: QueuedWeakReference<*>) {
        val start = SystemClock.uptimeMillis()
        while (ref.queue.poll() == null) {
            val elapsed = SystemClock.uptimeMillis() - start
            if (elapsed > sessionRule.timeoutMillis) {
                dumpHprof()
                throw UiThreadUtils.TimeoutException("Timed out after " + elapsed + "ms")
            }

            try {
                UiThreadUtils.loopUntilIdle(100)
            } catch (e: UiThreadUtils.TimeoutException) {
            }
            Runtime.getRuntime().gc()
        }
    }

    class QueuedWeakReference<T> @JvmOverloads constructor(obj: T, var queue: ReferenceQueue<T> =
            ReferenceQueue()) : WeakReference<T>(obj, queue)
}
