/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ClosedSessionAtStart
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.UiThreadUtils

import android.os.Bundle
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4

import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import java.lang.ref.ReferenceQueue
import java.lang.ref.WeakReference

@RunWith(AndroidJUnit4::class)
@MediumTest
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

    @WithDisplay(width = 100, height = 100)
    @Test fun asyncScriptsSuspendedWhileInactive() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        // Deactivate the GeckoSession and confirm that rAF/setTimeout/etc callbacks do not run
        mainSession.setActive(false)
        mainSession.evaluateJS(
            """function fail() {
                 document.documentElement.style.backgroundColor = 'green';
               }
               requestAnimationFrame(fail);
               setTimeout(fail, 1);
               fetch("missing.html").catch(fail);""")
        mainSession.waitForJS("new Promise(resolve => { resolve() })")
        val isNotGreen = mainSession.evaluateJS("document.documentElement.style.backgroundColor !== 'green'") as Boolean
        assertThat("requestAnimationFrame has not run yet", isNotGreen, equalTo(true))

        // Reactivate the GeckoSession and confirm that rAF/setTimeout/etc callbacks now run
        mainSession.setActive(true)
        mainSession.waitForJS("new Promise(resolve => requestAnimationFrame(() => { resolve(); }))");
        var isGreen = mainSession.evaluateJS("document.documentElement.style.backgroundColor === 'green'") as Boolean
        assertThat("requestAnimationFrame has run", isGreen, equalTo(true))
    }

    private fun waitUntilCollected(ref: QueuedWeakReference<*>) {
        UiThreadUtils.waitForCondition({
            Runtime.getRuntime().gc()
            ref.queue.poll() != null
        }, sessionRule.timeoutMillis)
    }

    class QueuedWeakReference<T> @JvmOverloads constructor(obj: T, var queue: ReferenceQueue<T> =
            ReferenceQueue()) : WeakReference<T>(obj, queue)
}
