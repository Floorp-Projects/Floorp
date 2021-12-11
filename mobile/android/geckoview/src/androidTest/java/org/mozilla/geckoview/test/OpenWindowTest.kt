package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.equalTo
import org.hamcrest.Matchers.not
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.gecko.util.ThreadUtils
import org.mozilla.geckoview.*
import org.mozilla.geckoview.GeckoRuntime.ServiceWorkerDelegate
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.GeckoSession.PermissionDelegate
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.util.UiThreadUtils

@RunWith(AndroidJUnit4::class)
@MediumTest
class OpenWindowTest : BaseSessionTest() {

    @Before
    fun setup() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webnotifications.requireuserinteraction" to false))

        // Grant "desktop notification" permission
        mainSession.delegateUntilTestEnd(object : PermissionDelegate {
            override fun onContentPermissionRequest(session: GeckoSession, perm: PermissionDelegate.ContentPermission): GeckoResult<Int>? {
                assertThat("Should grant DESKTOP_NOTIFICATIONS permission", perm.permission, equalTo(PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION))
                return GeckoResult.fromValue(PermissionDelegate.ContentPermission.VALUE_ALLOW);
            }
        })
    }

    private fun openPageClickNotification() {
        mainSession.loadTestPath(OPEN_WINDOW_PATH)
        sessionRule.waitForPageStop()
        val result = mainSession.waitForJS("Notification.requestPermission()")
        assertThat("Permission should be granted",
                result as String, equalTo("granted"))

        val notificationResult = GeckoResult<Void>()
        var notificationShown: WebNotification? = null

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                notificationShown = notification
                notificationResult.complete(null)
            }
        })
        mainSession.evaluateJS("showNotification()");
        sessionRule.waitForResult(notificationResult)
        notificationShown!!.click()
    }

    @Test
    @NullDelegate(ServiceWorkerDelegate::class)
    fun openWindowNullDelegate() {
        sessionRule.delegateUntilTestEnd(object : ContentDelegate, NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, url: String?) {
                // we should not open the target url
                assertThat("URL should notmatch", url, not(createTestUrl(OPEN_WINDOW_TARGET_PATH)))
            }
        })
        openPageClickNotification()
        UiThreadUtils.loopUntilIdle(sessionRule.env.defaultTimeoutMillis)
    }

    @Test
    fun openWindowNullResult() {
        sessionRule.delegateUntilTestEnd(object : ContentDelegate, NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, url: String?) {
                // we should not open the target url
                assertThat("URL should notmatch", url, not(createTestUrl(OPEN_WINDOW_TARGET_PATH)))
            }
        })
        openPageClickNotification()
        sessionRule.waitUntilCalled(object : ServiceWorkerDelegate {
            @AssertCalled(count = 1)
            override fun onOpenWindow(url: String): GeckoResult<GeckoSession> {
                ThreadUtils.assertOnUiThread()
                return GeckoResult.fromValue(null)
            }
        })
    }

    @Test
    fun openWindowSameSession() {
        sessionRule.delegateUntilTestEnd(object : ServiceWorkerDelegate {
            @AssertCalled(count = 1)
            override fun onOpenWindow(url: String): GeckoResult<GeckoSession> {
                ThreadUtils.assertOnUiThread()
                return GeckoResult.fromValue(mainSession)
            }
        })
        openPageClickNotification()
        sessionRule.waitUntilCalled(object : ContentDelegate, NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLocationChange(session: GeckoSession, url: String?) {
                assertThat("Should be on the main session", session, equalTo(mainSession))
                assertThat("URL should match", url, equalTo(createTestUrl(OPEN_WINDOW_TARGET_PATH)))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Should be on the main session", session, equalTo(mainSession))
                assertThat("Title should be correct", title, equalTo("Open Window test target"))
            }
        })
    }

    @Test
    fun openWindowNewSession() {
        var targetSession: GeckoSession? = null
        sessionRule.delegateUntilTestEnd(object : ServiceWorkerDelegate {
            @AssertCalled(count = 1)
            override fun onOpenWindow(url: String): GeckoResult<GeckoSession> {
                ThreadUtils.assertOnUiThread()
                targetSession = sessionRule.createOpenSession()
                return GeckoResult.fromValue(targetSession)
            }
        })
        openPageClickNotification()
        sessionRule.waitUntilCalled(object : ContentDelegate, NavigationDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onLocationChange(session: GeckoSession, url: String?) {
                assertThat("Should be on the target session", session, equalTo(targetSession))
                assertThat("URL should match", url, equalTo(createTestUrl(OPEN_WINDOW_TARGET_PATH)))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Should be on the target session", session, equalTo(targetSession))
                assertThat("Title should be correct", title, equalTo("Open Window test target"))
            }
        })
    }
}
