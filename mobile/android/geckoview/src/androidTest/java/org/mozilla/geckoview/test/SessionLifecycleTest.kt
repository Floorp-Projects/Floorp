/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.GeckoView
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ClosedSessionAtStart
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.test.util.UiThreadUtils

import android.os.Bundle
import android.os.Debug
import android.os.Parcelable
import android.os.SystemClock
import android.support.test.InstrumentationRegistry
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import android.util.Log
import android.util.SparseArray

import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
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
        //disable readFromParcel_throwOnAlreadyOpen for frequent failures Bug 1532186
        assumeThat(sessionRule.env.isDebugBuild, equalTo(true))
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
        // disable test on opt for frequently failing Bug 1519591
        assumeThat(sessionRule.env.isDebugBuild, equalTo(true))
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
            override fun onLocationChange(session: GeckoSession, url: String?) {
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

    @WithDevToolsAPI
    @Test fun readFromParcel_focusedInput() {
        // When an input is focused, make sure SessionTextInput is still active after transferring.
        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("$('#input').focus()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS))
            }
        })

        val newSession = sessionRule.createClosedSession()
        mainSession.toParcel { parcel ->
            newSession.readFromParcel(parcel)
        }

        // We generate an extra focus event during transfer.
        newSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS))
            }
        })

        newSession.evaluateJS("$('#input').blur()")
        newSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                // We generate an extra focus event during transfer.
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_BLUR))
            }
        })
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
    @Test fun restoreRuntimeSettings_noSession() {
        val extrasSetting = Bundle(2)
        extrasSetting.putInt("test1", 10)
        extrasSetting.putBoolean("test2", true)

        val settings = GeckoRuntimeSettings.Builder()
                       .javaScriptEnabled(false)
                       .extras(extrasSetting)
                       .build()

        settings.toParcel { parcel ->
            val newSettings = GeckoRuntimeSettings.Builder().build()
            newSettings.readFromParcel(parcel)

            assertThat("Parceled settings must match",
                       newSettings.javaScriptEnabled,
                       equalTo(settings.javaScriptEnabled))
            assertThat("Parceled settings must match",
                       newSettings.extras.getInt("test1"),
                       equalTo(settings.extras.getInt("test1")))
            assertThat("Parceled settings must match",
                       newSettings.extras.getBoolean("test2"),
                       equalTo(settings.extras.getBoolean("test2")))
        }
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
        assertThat("View session is closed", view.session!!.isOpen, equalTo(false))
    }

    @Test fun restoreInstanceState_sameOpenSession() {
        // We should keep the session open when restoring the same open session.
        val view = testRestoreInstanceState(mainSession, mainSession)
        assertThat("View session is unchanged", view.session, equalTo(mainSession))
        assertThat("View session is open", view.session!!.isOpen, equalTo(true))
        view.session!!.reload()
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
