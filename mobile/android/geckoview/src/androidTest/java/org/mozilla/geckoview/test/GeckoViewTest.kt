package org.mozilla.geckoview.test

import androidx.test.filters.LargeTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.core.view.ViewCompat
import androidx.test.ext.junit.rules.ActivityScenarioRule

import org.hamcrest.Matchers.equalTo
import org.junit.*
import org.junit.Assume.assumeThat

import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autofill

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.util.UiThreadUtils
import java.io.File

@RunWith(AndroidJUnit4::class)
@LargeTest
class GeckoViewTest : BaseSessionTest() {
    val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)

    @get:Rule
    override val rules = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        activityRule.scenario.onActivity {
            // Attach the default session from the session rule to the GeckoView
            it.view.setSession(sessionRule.session)
        }
    }

    @After
    fun cleanup() {
        activityRule.scenario.onActivity {
            it.view.releaseSession()
        }
    }

    @Test
    fun setSessionOnClosed() {
        activityRule.scenario.onActivity {
            it.view.session!!.close()
            it.view.setSession(GeckoSession())
        }
    }

    @Test
    fun setSessionOnOpenDoesNotThrow() {
        activityRule.scenario.onActivity {
            assertThat("Session is open", it.view.session!!.isOpen, equalTo(true))
            val newSession = GeckoSession()
            it.view.setSession(newSession)
            assertThat("The new session should be correctly set.",
                it.view.session, equalTo(newSession))
        }
    }

    @Test(expected = java.lang.IllegalStateException::class)
    fun displayAlreadyAcquired() {
        activityRule.scenario.onActivity {
            assertThat("View should be attached",
                    ViewCompat.isAttachedToWindow(it.view), equalTo(true))
            it.view.session!!.acquireDisplay()
        }
    }

    @Test
    fun relaseOnDetach() {
        activityRule.scenario.onActivity {
            // The GeckoDisplay should be released when the View is detached from the window...
            it.view.onDetachedFromWindow()
            it.view.session!!.releaseDisplay(it.view.session!!.acquireDisplay())
        }
    }

    private fun waitUntilContentProcessPriority(high: List<GeckoSession>, low: List<GeckoSession>) {
        val highPids = high.map { sessionRule.getSessionPid(it) }.toSet()
        val lowPids = low.map { sessionRule.getSessionPid(it) }.toSet()

        UiThreadUtils.waitForCondition({
            getContentProcessesPriority(highPids).count { it > 100 } == 0
                    && getContentProcessesPriority(lowPids).count { it < 300 } == 0
        }, env.defaultTimeoutMillis)
    }

    fun getContentProcessesPriority(pids: Collection<Int>) : List<Int> {
        return pids.map { pid ->
            File("/proc/$pid/oom_score").readText(Charsets.UTF_8).trim().toInt()
        }
    }

    fun setupPriorityTest(): GeckoSession {
        // This makes the test a little bit faster
        sessionRule.setPrefsUntilTestEnd(mapOf(
            "dom.ipc.processPriorityManager.backgroundGracePeriodMS" to 0,
            "dom.ipc.processPriorityManager.backgroundPerceivableGracePeriodMS" to 0,
        ))

        val otherSession = sessionRule.createOpenSession()
        // The process manager sets newly created processes to FOREGROUND priority until they
        // are de-prioritized, so we need to activate and deactivate the session to trigger
        // a setPriority call.
        otherSession.setActive(true)
        otherSession.setActive(false)

        // Need a dummy page to be able to get the PID from the session
        otherSession.loadUri("https://example.com")
        otherSession.waitForPageStop()

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        waitUntilContentProcessPriority(
            high = listOf(mainSession), low = listOf(otherSession)
        )

        return otherSession
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    fun setTabActiveKeepsTabAtHighPriority() {
        // Bug 1768102 - Doesn't seem to work on Fission
        assumeThat(env.isFission || env.isIsolatedProcess, equalTo(false))
        activityRule.scenario.onActivity {
            val otherSession = setupPriorityTest()

            // A tab with priority hint does not get de-prioritized even when
            // the surface is destroyed
            mainSession.setPriorityHint(GeckoSession.PRIORITY_HIGH)

            // This will destroy mainSession's surface and create a surface for otherSession
            it.view.setSession(otherSession)

            waitUntilContentProcessPriority(high = listOf(mainSession, otherSession), low = listOf())

            // Destroying otherSession's surface should leave mainSession as the sole high priority
            // tab
            it.view.releaseSession()

            waitUntilContentProcessPriority(high = listOf(mainSession), low = listOf())

            // Cleanup
            mainSession.setPriorityHint(GeckoSession.PRIORITY_DEFAULT)
        }
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    fun extensionCurrentTabRaisesPriority() {
        // Bug 1767346
        assumeThat(false, equalTo(true))

        val otherSession = setupPriorityTest()

        // Setting priorityHint to PRIORITY_HIGH raises priority
        mainSession.setPriorityHint(GeckoSession.PRIORITY_HIGH)

        waitUntilContentProcessPriority(
            high = listOf(mainSession, otherSession), low = listOf()
        )

        // Setting the priorityHint to default should lower priority
        mainSession.setPriorityHint(GeckoSession.PRIORITY_DEFAULT)

        waitUntilContentProcessPriority(
            high = listOf(mainSession), low = listOf(otherSession)
        )
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    fun processPriorityTest() {
        // Doesn't seem to work on Fission
        assumeThat(env.isFission || env.isIsolatedProcess, equalTo(false))
        activityRule.scenario.onActivity {
            val otherSession = setupPriorityTest()

            // After setting otherSession to the view, otherSession should be high priority
            // and mainSession should be de-prioritized
            it.view.setSession(otherSession)

            waitUntilContentProcessPriority(
                high = listOf(otherSession), low = listOf(mainSession))

            // After releasing otherSession, both sessions should be low priority
            it.view.releaseSession()

            waitUntilContentProcessPriority(
                high = listOf(), low = listOf(mainSession, otherSession))

            // Test that re-setting mainSession in the view raises the priority again
            it.view.setSession(mainSession)
            waitUntilContentProcessPriority(
                high = listOf(mainSession), low = listOf(otherSession))

            // Setting the session to active should also raise priority
            otherSession.setActive(true)
            waitUntilContentProcessPriority(
                high = listOf(mainSession, otherSession), low = listOf())
        }
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    fun setPriorityHint() {
        // Bug 1767346
        assumeThat(false, equalTo(true))

        val otherSession = setupPriorityTest()

        // Setting priorityHint to PRIORITY_HIGH raises priority
        otherSession.setPriorityHint(GeckoSession.PRIORITY_HIGH)

        waitUntilContentProcessPriority(
            high = listOf(mainSession, otherSession), low = listOf()
        )

        // Setting priorityHint to PRIORITY_DEFAULT should lower priority
        otherSession.setPriorityHint(GeckoSession.PRIORITY_DEFAULT)

        waitUntilContentProcessPriority(
            high = listOf(mainSession), low = listOf(otherSession)
        )
    }
}
