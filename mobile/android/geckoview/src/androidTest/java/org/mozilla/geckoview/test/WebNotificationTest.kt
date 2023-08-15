package org.mozilla.geckoview.test

import android.os.Parcel
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PermissionDelegate
import org.mozilla.geckoview.WebNotification
import org.mozilla.geckoview.WebNotificationDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule

const val VERY_LONG_IMAGE_URL = "https://example.com/this/is/a/very/long/address/that/is/meant/to/be/longer/than/is/one/hundred/and/fifth/characters/long/for/testing/imageurl/length.ico"

@RunWith(AndroidJUnit4::class)
@MediumTest
class WebNotificationTest : BaseSessionTest() {

    @Before fun setup() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webnotifications.requireuserinteraction" to false))

        // Grant "desktop notification" permission
        mainSession.delegateUntilTestEnd(object : PermissionDelegate {
            override fun onContentPermissionRequest(session: GeckoSession, perm: PermissionDelegate.ContentPermission):
                GeckoResult<Int>? {
                assertThat("Should grant DESKTOP_NOTIFICATIONS permission", perm.permission, equalTo(PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION))
                return GeckoResult.fromValue(PermissionDelegate.ContentPermission.VALUE_ALLOW)
            }
        })

        val result = mainSession.waitForJS("Notification.requestPermission()")
        assertThat(
            "Permission should be granted",
            result as String,
            equalTo("granted"),
        )
    }

    @Test fun onSilentNotification() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webnotifications.silent.enabled" to true))
        val notificationResult = GeckoResult<Void>()

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                assertThat("Title should match", notification.title, equalTo("The Title"))
                assertThat("Silent should match", notification.silent, equalTo(true))
                assertThat("Vibrate should match", notification.vibrate, equalTo(intArrayOf()))
                assertThat("Source should match", notification.source, equalTo(createTestUrl(HELLO_HTML_PATH)))
                notificationResult.complete(null)
            }
        })

        mainSession.evaluateJS(
            """
            new Notification('The Title', { body: 'The Text', silent: true });
            """.trimIndent(),
        )

        sessionRule.waitForResult(notificationResult)
    }

    fun assertNotificationData(notification: WebNotification, requireInteraction: Boolean) {
        assertThat("Title should match", notification.title, equalTo("The Title"))
        assertThat("Body should match", notification.text, equalTo("The Text"))
        assertThat("Tag should match", notification.tag, endsWith("Tag"))
        assertThat("ImageUrl should match", notification.imageUrl, endsWith("icon.png"))
        assertThat("Language should match", notification.lang, equalTo("en-US"))
        assertThat("Direction should match", notification.textDirection, equalTo("ltr"))
        assertThat(
            "Require Interaction should match",
            notification.requireInteraction,
            equalTo(requireInteraction),
        )
        assertThat("Vibrate should match", notification.vibrate, equalTo(intArrayOf(1, 2, 3, 4)))
        assertThat("Silent should match", notification.silent, equalTo(false))
        assertThat("Source should match", notification.source, equalTo(createTestUrl(HELLO_HTML_PATH)))
    }

    @GeckoSessionTestRule.Setting.List(
        GeckoSessionTestRule.Setting(
            key = GeckoSessionTestRule.Setting.Key.USE_PRIVATE_MODE,
            value = "true",
        ),
    )
    @Ignore // Bug 1843046 - Disabled because private notifications are temporarily disabled.
    @Test
    fun onShowNotification() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webnotifications.vibrate.enabled" to true))
        val notificationResult = GeckoResult<Void>()
        val requireInteraction =
            sessionRule.getPrefs("dom.webnotifications.requireinteraction.enabled")[0] as Boolean

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                assertNotificationData(notification, requireInteraction)
                assertThat("privateBrowsing should match", notification.privateBrowsing, equalTo(true))
                notificationResult.complete(null)
            }
        })

        mainSession.evaluateJS(
            """
            new Notification('The Title', { body: 'The Text', cookie: 'Cookie',
                icon: 'icon.png', tag: 'Tag', dir: 'ltr', lang: 'en-US',
                requireInteraction: true, vibrate: [1,2,3,4] });
            """.trimIndent(),
        )

        sessionRule.waitForResult(notificationResult)
    }

    @Test fun onCloseNotification() {
        val closeCalled = GeckoResult<Void>()

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onCloseNotification(notification: WebNotification) {
                closeCalled.complete(null)
            }
        })

        mainSession.evaluateJS(
            """
            const notification = new Notification('The Title', { body: 'The Text'});
            notification.close();
            """.trimIndent(),
        )

        sessionRule.waitForResult(closeCalled)
    }

    @Test fun clickNotificationParceled() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webnotifications.vibrate.enabled" to true))
        val notificationResult = GeckoResult<WebNotification>()
        val requireInteraction =
            sessionRule.getPrefs("dom.webnotifications.requireinteraction.enabled")[0] as Boolean

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                notificationResult.complete(notification)
            }
        })

        val promiseResult = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                const notification = new Notification('The Title', {
                   body: 'The Text',
                   cookie: 'Cookie',
                   icon: 'icon.png',
                   tag: 'Tag',
                   dir: 'ltr',
                   lang: 'en-US',
                   requireInteraction: true,
                   vibrate: [1,2,3,4]
                });
                notification.onclick = function() {
                    resolve(1);
                }
            });
            """.trimIndent(),
        )

        val notification = sessionRule.waitForResult(notificationResult)
        assertNotificationData(notification, requireInteraction)
        assertThat("privateBrowsing should match", notification.privateBrowsing, equalTo(false))

        // Test that we can click from a deserialized notification
        val parcel = Parcel.obtain()
        notification.writeToParcel(parcel, 0)
        parcel.setDataPosition(0)

        val deserialized = WebNotification.CREATOR.createFromParcel(parcel)
        assertNotificationData(deserialized, requireInteraction)
        assertThat("privateBrowsing should match", deserialized.privateBrowsing, equalTo(false))

        deserialized!!.click()
        assertThat("Promise should have been resolved.", promiseResult.value as Double, equalTo(1.0))
    }

    @GeckoSessionTestRule.Setting.List(
        GeckoSessionTestRule.Setting(
            key = GeckoSessionTestRule.Setting.Key.USE_PRIVATE_MODE,
            value = "true",
        ),
    )
    @Ignore // Bug 1843046 - Disabled because private notifications are temporarily disabled.
    @Test
    fun clickPrivateNotificationParceled() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webnotifications.vibrate.enabled" to true))
        val notificationResult = GeckoResult<WebNotification>()
        val requireInteraction =
            sessionRule.getPrefs("dom.webnotifications.requireinteraction.enabled")[0] as Boolean

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                notificationResult.complete(notification)
            }
        })

        val promiseResult = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                const notification = new Notification('The Title', {
                   body: 'The Text',
                   cookie: 'Cookie',
                   icon: 'icon.png',
                   tag: 'Tag',
                   dir: 'ltr',
                   lang: 'en-US',
                   requireInteraction: true,
                   vibrate: [1,2,3,4]
                });
                notification.onclick = function() {
                    resolve(1);
                }
            });
            """.trimIndent(),
        )

        val notification = sessionRule.waitForResult(notificationResult)
        assertNotificationData(notification, requireInteraction)
        assertThat("privateBrowsing should match", notification.privateBrowsing, equalTo(true))

        // Test that we can click from a deserialized notification
        val parcel = Parcel.obtain()
        notification.writeToParcel(parcel, 0)
        parcel.setDataPosition(0)

        val deserialized = WebNotification.CREATOR.createFromParcel(parcel)
        assertNotificationData(deserialized, requireInteraction)
        assertThat("privateBrowsing should match", deserialized.privateBrowsing, equalTo(true))

        deserialized!!.click()
        assertThat("Promise should have been resolved.", promiseResult.value as Double, equalTo(1.0))
    }

    @Test fun clickNotification() {
        val notificationResult = GeckoResult<Void>()
        var notificationShown: WebNotification? = null

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                notificationShown = notification
                notificationResult.complete(null)
            }
        })

        val promiseResult = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                const notification = new Notification('The Title', { body: 'The Text' });
                notification.onclick = function() {
                    resolve(1);
                }
            });
            """.trimIndent(),
        )

        sessionRule.waitForResult(notificationResult)
        notificationShown!!.click()

        assertThat("Promise should have been resolved.", promiseResult.value as Double, equalTo(1.0))
    }

    @Test fun dismissNotification() {
        val notificationResult = GeckoResult<Void>()
        var notificationShown: WebNotification? = null

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                notificationShown = notification
                notificationResult.complete(null)
            }
        })

        val promiseResult = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                const notification = new Notification('The Title', { body: 'The Text'});
                notification.onclose = function() {
                    resolve(1);
                }
            });
            """.trimIndent(),
        )

        sessionRule.waitForResult(notificationResult)
        notificationShown!!.dismiss()

        assertThat("Promise should have been resolved", promiseResult.value as Double, equalTo(1.0))
    }

    @Test fun writeToParcel() {
        val notificationResult = GeckoResult<WebNotification>()

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                notificationResult.complete(notification)
            }
        })

        val promiseResult = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                const notification = new Notification('The Title', { body: 'The Text' });
                notification.onclose = function() {
                    resolve(1);
                }
            });
            """.trimIndent(),
        )

        val notification = sessionRule.waitForResult(notificationResult)
        notification.dismiss()

        // Ensure we always have a non-null URL from js.
        assertNotNull(notification.imageUrl)

        // Test that we can serialize a notification
        val parcel = Parcel.obtain()
        notification.writeToParcel(parcel, /* ignored */ -1)

        assertThat("Promise should have been resolved.", promiseResult.value as Double, equalTo(1.0))
    }

    @Test fun writeToParcelLongImageUrl() {
        val notificationResult = GeckoResult<WebNotification>()

        sessionRule.delegateDuringNextWait(object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                notificationResult.complete(notification)
            }
        })

        val promiseResult = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                const notification = new Notification('The Title',
                    {
                        body: 'The Text',
                        icon: '$VERY_LONG_IMAGE_URL'
                    });
                notification.onclose = function() {
                    resolve(1);
                }
            });
            """.trimIndent(),
        )

        val notification = sessionRule.waitForResult(notificationResult)
        notification.dismiss()

        // Ensure we have an imageUrl longer than our max to start with.
        assertNotNull(notification.imageUrl)
        assertTrue(notification.imageUrl!!.length > 150)

        // Test that we can serialize a notification with an imageUrl.length >= 150
        val parcel = Parcel.obtain()
        notification.writeToParcel(parcel, /* ignored */ -1)
        parcel.setDataPosition(0)

        val serializedNotification = WebNotification.CREATOR.createFromParcel(parcel)
        assertTrue(serializedNotification.imageUrl!!.isBlank())

        assertThat("Promise should have been resolved.", promiseResult.value as Double, equalTo(1.0))
    }
}
