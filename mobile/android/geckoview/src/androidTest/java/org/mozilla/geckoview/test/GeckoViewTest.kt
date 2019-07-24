package org.mozilla.geckoview.test

import android.support.test.filters.LargeTest
import android.support.test.rule.ActivityTestRule
import android.support.test.runner.AndroidJUnit4
import android.support.v4.view.ViewCompat
import android.view.View

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
    val activityRule = ActivityTestRule<GeckoViewTestActivity>(GeckoViewTestActivity::class.java)
    var sessionRule = GeckoSessionTestRule()

    val view get() = activityRule.activity.view

    @get:Rule
    val rules = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        // Attach the default session from the session rule to the GeckoView
        view.setSession(sessionRule.session)
    }

    @After
    fun cleanup() {
        view.releaseSession()
    }

    @Test
    fun setSessionOnClosed() {
        view.session!!.close()
        view.setSession(GeckoSession())
    }

    @Test(expected = IllegalStateException::class)
    fun setSessionOnOpenThrows() {
        assertThat("Session is open", view.session!!.isOpen, equalTo(true))
        view.setSession(GeckoSession())
    }

    @Test(expected = java.lang.IllegalStateException::class)
    fun displayAlreadyAcquired() {
        assertThat("View should be attached",
                ViewCompat.isAttachedToWindow(view), equalTo(true))
        view.session!!.acquireDisplay()
    }

    @Test
    fun relaseOnDetach() {
        // The GeckoDisplay should be released when the View is detached from the window...
        view.onDetachedFromWindow()
        view.session!!.releaseDisplay(view.session!!.acquireDisplay())
    }
}
