/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.os.Parcel
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import android.util.Base64
import org.hamcrest.MatcherAssert.assertThat
import org.hamcrest.Matchers.*
import org.json.JSONObject
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.RejectedPromiseException
import org.mozilla.geckoview.test.util.Callbacks
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.SecureRandom
import java.security.interfaces.ECPublicKey
import java.security.spec.ECGenParameterSpec

@RunWith(AndroidJUnit4::class)
@MediumTest
class WebPushTest : BaseSessionTest() {
    companion object {
        val PUSH_ENDPOINT: String = "https://test.endpoint"
        val APP_SERVER_KEY_PAIR: KeyPair = generateKeyPair()
        val AUTH_SECRET: ByteArray = generateAuthSecret()
        val BROWSER_KEY_PAIR: KeyPair = generateKeyPair()

        private fun generateKeyPair(): KeyPair {
            try {
                val spec = ECGenParameterSpec("secp256r1")
                val generator = KeyPairGenerator.getInstance("EC")
                generator.initialize(spec)
                return generator.generateKeyPair()
            } catch (e: Exception) {
                throw RuntimeException(e)
            }
        }

        private fun generateAuthSecret(): ByteArray {
            val bytes = ByteArray(16)
            SecureRandom().nextBytes(bytes)

            return bytes
        }
    }

    var delegate: TestPushDelegate? = null

    @Before
    fun setup() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webnotifications.requireuserinteraction" to false))
        // Grant "desktop notification" permission
        mainSession.delegateUntilTestEnd(object : Callbacks.PermissionDelegate {
            override fun onContentPermissionRequest(session: GeckoSession, perm: GeckoSession.PermissionDelegate.ContentPermission):
                    GeckoResult<Int>? {
                assertThat("Should grant DESKTOP_NOTIFICATIONS permission", perm.permission, equalTo(GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION))
                return GeckoResult.fromValue(GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW)
            }
        })

        delegate = TestPushDelegate()

        sessionRule.addExternalDelegateUntilTestEnd(WebPushDelegate::class,
                { d -> sessionRule.runtime.webPushController.setDelegate(d) },
                { sessionRule.runtime.webPushController.setDelegate(null) }, delegate!!)


        mainSession.loadTestPath(PUSH_HTML_PATH)
        mainSession.waitForPageStop()
    }

    @After
    fun tearDown() {
        sessionRule.runtime.webPushController.setDelegate(null)
        delegate = null
    }

    private fun verifySubscription(subscription: JSONObject) {
        assertThat("Push endpoint should match", subscription.getString("endpoint"), equalTo(PUSH_ENDPOINT))

        val keys = subscription.getJSONObject("keys")
        val authSecret = Base64.decode(keys.getString("auth"), Base64.URL_SAFE)
        val encryptionKey = WebPushUtils.keyFromString(keys.getString("p256dh"))

        assertThat("Auth secret should match", authSecret, equalTo(AUTH_SECRET))
        assertThat("Encryption key should match", encryptionKey, equalTo(BROWSER_KEY_PAIR.public))
    }

    @Test
    fun subscribe() {
        // PushManager.subscribe()
        val appServerKey = WebPushUtils.keyToString(APP_SERVER_KEY_PAIR.public as ECPublicKey)
        var pushSubscription = mainSession.evaluatePromiseJS("window.doSubscribe(\"$appServerKey\")").value as JSONObject
        assertThat("Should have a stored subscription", delegate!!.storedSubscription, notNullValue())
        verifySubscription(pushSubscription)

        // PushManager.getSubscription()
        pushSubscription = mainSession.evaluatePromiseJS("window.doGetSubscription()").value as JSONObject
        verifySubscription(pushSubscription)
    }

    @Test
    fun subscribeNoAppServerKey() {
        // PushManager.subscribe()
        var pushSubscription = mainSession.evaluatePromiseJS("window.doSubscribe()").value as JSONObject
        assertThat("Should have a stored subscription", delegate!!.storedSubscription, notNullValue())
        verifySubscription(pushSubscription)

        // PushManager.getSubscription()
        pushSubscription = mainSession.evaluatePromiseJS("window.doGetSubscription()").value as JSONObject
        verifySubscription(pushSubscription)
    }

    @Test(expected = RejectedPromiseException::class)
    fun subscribeNullDelegate() {
        sessionRule.runtime.webPushController.setDelegate(null)
        mainSession.evaluatePromiseJS("window.doSubscribe()").value as JSONObject
    }

    @Test(expected = RejectedPromiseException::class)
    fun getSubscriptionNullDelegate() {
        sessionRule.runtime.webPushController.setDelegate(null)
        mainSession.evaluatePromiseJS("window.doGetSubscription()").value as JSONObject
    }

    @Test
    fun unsubscribe() {
        subscribe()

        // PushManager.unsubscribe()
        val unsubResult = mainSession.evaluatePromiseJS("window.doUnsubscribe()").value as JSONObject
        assertThat("Unsubscribe result should be non-null", unsubResult, notNullValue())
        assertThat("Should not have a stored subscription", delegate!!.storedSubscription, nullValue())
    }

    @Test
    fun pushEvent() {
        subscribe()

        val p = mainSession.evaluatePromiseJS("window.doWaitForPushEvent()")

        val testPayload = "The Payload";
        sessionRule.runtime.webPushController.onPushEvent(delegate!!.storedSubscription!!.scope, testPayload.toByteArray(Charsets.UTF_8))

        assertThat("Push data should match", p.value as String, equalTo(testPayload))
    }

    @Test
    fun pushEventWithoutData() {
        subscribe()

        val p = mainSession.evaluatePromiseJS("window.doWaitForPushEvent()")

        sessionRule.runtime.webPushController.onPushEvent(delegate!!.storedSubscription!!.scope, null)

        assertThat("Push data should be empty", p.value as String, equalTo(""))
    }

    private fun sendNotification() {
        val notificationResult = GeckoResult<Void>()
        val runtime = sessionRule.runtime
        val register = {  delegate: WebNotificationDelegate -> runtime.webNotificationDelegate = delegate}
        val unregister = { _: WebNotificationDelegate -> runtime.webNotificationDelegate = null }

        val expectedTitle = "The title"
        val expectedBody = "The body"

        sessionRule.addExternalDelegateDuringNextWait(WebNotificationDelegate::class, register,
                unregister, object : WebNotificationDelegate {
            @GeckoSessionTestRule.AssertCalled
            override fun onShowNotification(notification: WebNotification) {
                assertThat("Title should match", notification.title, equalTo(expectedTitle))
                assertThat("Body should match", notification.text, equalTo(expectedBody))
                assertThat("Source should match", notification.source, endsWith("sw.js"))
                notificationResult.complete(null)
            }
        })

        val testPayload = JSONObject()
        testPayload.put("title", expectedTitle)
        testPayload.put("body", expectedBody)

        sessionRule.runtime.webPushController.onPushEvent(delegate!!.storedSubscription!!.scope, testPayload.toString().toByteArray(Charsets.UTF_8))
        sessionRule.waitForResult(notificationResult)
    }

    @Test
    fun pushEventWithNotification() {
        subscribe()
        sendNotification()
    }

    @Test
    fun subscriptionChanged() {
        subscribe()

        val p = mainSession.evaluatePromiseJS("window.doWaitForSubscriptionChange()")

        sessionRule.runtime.webPushController.onSubscriptionChanged(delegate!!.storedSubscription!!.scope)

        assertThat("Result should not be null", p.value, notNullValue())
    }

    @Test(expected = IllegalArgumentException::class)
    fun invalidDuplicateKeys() {
        WebPushSubscription("https://scope", PUSH_ENDPOINT,
                WebPushUtils.keyToBytes(APP_SERVER_KEY_PAIR.public as ECPublicKey),
                WebPushUtils.keyToBytes(APP_SERVER_KEY_PAIR.public as ECPublicKey)!!, AUTH_SECRET)
    }

    @Test
    fun parceling() {
        val testScope = "https://test.scope";
        val sub = WebPushSubscription(testScope, PUSH_ENDPOINT,
                WebPushUtils.keyToBytes(APP_SERVER_KEY_PAIR.public as ECPublicKey),
                WebPushUtils.keyToBytes(BROWSER_KEY_PAIR.public as ECPublicKey)!!, AUTH_SECRET)

        val parcel = Parcel.obtain()
        sub.writeToParcel(parcel, 0)
        parcel.setDataPosition(0)

        val sub2 = WebPushSubscription.CREATOR.createFromParcel(parcel)
        assertThat("Scope should match", sub.scope, equalTo(sub2.scope))
        assertThat("Endpoint should match", sub.endpoint, equalTo(sub2.endpoint))
        assertThat("App server key should match", sub.appServerKey, equalTo(sub2.appServerKey))
        assertThat("Encryption key should match", sub.browserPublicKey, equalTo(sub2.browserPublicKey))
        assertThat("Auth secret should match", sub.authSecret, equalTo(sub2.authSecret))
    }

    class TestPushDelegate : WebPushDelegate {
        var storedSubscription: WebPushSubscription? = null

        override fun onGetSubscription(scope: String): GeckoResult<WebPushSubscription>? {
            return GeckoResult.fromValue(storedSubscription)
        }

        override fun onUnsubscribe(scope: String): GeckoResult<Void>? {
            storedSubscription = null
            return GeckoResult.fromValue(null)
        }

        override fun onSubscribe(scope: String, appServerKey: ByteArray?): GeckoResult<WebPushSubscription>? {
            appServerKey?.let { assertThat("Application server key should match", it, equalTo(WebPushUtils.keyToBytes(APP_SERVER_KEY_PAIR.public as ECPublicKey))) }
            storedSubscription = WebPushSubscription(scope, PUSH_ENDPOINT, appServerKey, WebPushUtils.keyToBytes(BROWSER_KEY_PAIR.public as ECPublicKey)!!, AUTH_SECRET)
            return GeckoResult.fromValue(storedSubscription)
        }
    }
}
