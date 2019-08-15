package org.mozilla.geckoview.test

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.WebNotification
import org.mozilla.geckoview.WebNotificationDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule

@RunWith(AndroidJUnit4::class)
@MediumTest
class WebNotificationTest : BaseSessionTest() {

    @Before fun setup() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val result = mainSession.waitForJS("Notification.requestPermission()")
        assertThat("Permission should be granted",
                result as String, equalTo("granted"))
    }

    @Test fun onShowNotification() {
        val runtime = sessionRule.runtime
        val notificationResult = GeckoResult<Void>()
        val register = {  delegate: WebNotificationDelegate -> runtime.webNotificationDelegate = delegate}
        val unregister = { _: WebNotificationDelegate -> runtime.webNotificationDelegate = null }

        sessionRule.addExternalDelegateDuringNextWait(WebNotificationDelegate::class, register,
            unregister, object : WebNotificationDelegate {
                @GeckoSessionTestRule.AssertCalled
                override fun onShowNotification(notification: WebNotification) {
                    assertThat("Title should match", notification.title, equalTo("The Title"))
                    assertThat("Body should match", notification.text, equalTo("The Text"))
                    assertThat("Tag should match", notification.tag, endsWith("Tag"))
                    assertThat("ImageUrl should match", notification.imageUrl, endsWith("icon.png"))
                    assertThat("Language should match", notification.lang, equalTo("en-US"))
                    assertThat("Direction should match", notification.textDirection, equalTo("ltr"))
                    assertThat("Require Interaction should match", notification.requireInteraction, equalTo(true))
                    notificationResult.complete(null)
                }
        })

        mainSession.evaluateJS("""
            new Notification('The Title', { body: 'The Text', cookie: 'Cookie',
                icon: 'icon.png', tag: 'Tag', dir: 'ltr', lang: 'en-US',
                requireInteraction: true });
            """.trimIndent())

        sessionRule.waitForResult(notificationResult)
    }

    @Test fun onCloseNotification() {
        val runtime = sessionRule.runtime
        val closeCalled = GeckoResult<Void>()
        val register = {  delegate: WebNotificationDelegate -> runtime.webNotificationDelegate = delegate}
        val unregister = { _: WebNotificationDelegate -> runtime.webNotificationDelegate = null }

        sessionRule.addExternalDelegateDuringNextWait(WebNotificationDelegate::class, register,
            unregister, object : WebNotificationDelegate {
                @GeckoSessionTestRule.AssertCalled
                override fun onCloseNotification(notification: WebNotification) {
                    closeCalled.complete(null)
                }
        })

        mainSession.evaluateJS("""
            const notification = new Notification('The Title', { body: 'The Text'});
            notification.close();
        """.trimIndent())

        sessionRule.waitForResult(closeCalled)
    }

    @Test fun clickNotification() {
        val runtime = sessionRule.runtime
        val notificationResult = GeckoResult<Void>()
        val register = {  delegate: WebNotificationDelegate -> runtime.webNotificationDelegate = delegate}
        val unregister = { _: WebNotificationDelegate -> runtime.webNotificationDelegate = null }
        var notificationShown: WebNotification? = null

        sessionRule.addExternalDelegateDuringNextWait(WebNotificationDelegate::class, register,
            unregister, object : WebNotificationDelegate {
                @GeckoSessionTestRule.AssertCalled
                override fun onShowNotification(notification: WebNotification) {
                    notificationShown = notification
                    notificationResult.complete(null)
                }
        })

        val promiseResult = mainSession.evaluatePromiseJS("""
            new Promise(resolve => {
                const notification = new Notification('The Title', { body: 'The Text' });
                notification.onclick = function() {
                    resolve(1);
                }
            });
        """.trimIndent())

        sessionRule.waitForResult(notificationResult)
        notificationShown!!.click()

        assertThat("Promise should have been resolved.", promiseResult.value as Double, equalTo(1.0))
    }

    @Test fun dismissNotification() {
        val runtime = sessionRule.runtime
        val notificationResult = GeckoResult<Void>()
        val register = {  delegate: WebNotificationDelegate -> runtime.webNotificationDelegate = delegate}
        val unregister = { _: WebNotificationDelegate -> runtime.webNotificationDelegate = null }
        var notificationShown: WebNotification? = null

        sessionRule.addExternalDelegateDuringNextWait(WebNotificationDelegate::class, register,
            unregister, object : WebNotificationDelegate {
                @GeckoSessionTestRule.AssertCalled
                override fun onShowNotification(notification: WebNotification) {
                    notificationShown = notification
                    notificationResult.complete(null)
                }
        })

        val promiseResult = mainSession.evaluatePromiseJS("""
            new Promise(resolve => {
                const notification = new Notification('The Title', { body: 'The Text'});
                notification.onclose = function() {
                    resolve(1);
                }
            });
        """.trimIndent())

        sessionRule.waitForResult(notificationResult)
        notificationShown!!.dismiss()

        assertThat("Promise should have been resolved", promiseResult.value as Double, equalTo(1.0))
    }
}
