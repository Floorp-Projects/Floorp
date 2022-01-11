package org.mozilla.geckoview.test

import androidx.test.filters.LargeTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.core.view.ViewCompat
import androidx.test.ext.junit.rules.ActivityScenarioRule

import org.hamcrest.MatcherAssert.assertThat
import org.hamcrest.Matchers.equalTo

import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule

@RunWith(AndroidJUnit4::class)
@LargeTest
class GeckoViewTest {
    val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)
    var sessionRule = GeckoSessionTestRule()

    @get:Rule
    val rules = RuleChain.outerRule(activityRule).around(sessionRule)

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

    @Test(expected = IllegalStateException::class)
    fun setSessionOnOpenThrows() {
        activityRule.scenario.onActivity {
            assertThat("Session is open", it.view.session!!.isOpen, equalTo(true))
            it.view.setSession(GeckoSession())
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
}
