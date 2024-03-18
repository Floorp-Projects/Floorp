/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webpush

import org.junit.Test

class WebPushSubscriptionTest {

    @Test
    fun `constructor`() {
        val scope = "https://mozilla.org"
        val endpoint = "https://pushendpoint.mozilla.org/send/message/here"
        val appServerKey = byteArrayOf(10, 2, 15, 11)
        val publicKey = byteArrayOf(11, 10, 2, 15)
        val authSecret = byteArrayOf(15, 11, 10, 2)
        val sub = WebPushSubscription(
            scope,
            endpoint,
            appServerKey,
            publicKey,
            authSecret,
        )

        assert(scope == sub.scope)
        assert(endpoint == sub.endpoint)
        assert(appServerKey.contentEquals(sub.appServerKey!!))
        assert(publicKey.contentEquals(sub.publicKey))
        assert(authSecret.contentEquals(sub.authSecret))
    }

    @Test
    fun `WebPushSubscription equals`() {
        val sub1 = WebPushSubscription(
            "https://mozilla.org",
            "https://pushendpoint.mozilla.org/send/message/here",
            byteArrayOf(10, 2, 15, 11),
            byteArrayOf(11, 10, 2, 15),
            byteArrayOf(15, 11, 10, 2),
        )
        val sub2 = WebPushSubscription(
            "https://mozilla.org",
            "https://pushendpoint.mozilla.org/send/message/here",
            byteArrayOf(10, 2, 15, 11),
            byteArrayOf(11, 10, 2, 15),
            byteArrayOf(15, 11, 10, 2),
        )

        assert(sub1 == sub2)
    }

    @Test
    fun `WebPushSubscription equals with optional`() {
        val sub1 = WebPushSubscription(
            "https://mozilla.org",
            "https://pushendpoint.mozilla.org/send/message/here",
            byteArrayOf(10, 2, 15, 11),
            byteArrayOf(11, 10, 2, 15),
            byteArrayOf(15, 11, 10, 2),
        )

        val sub2 = WebPushSubscription(
            "https://mozilla.org",
            "https://pushendpoint.mozilla.org/send/message/here",
            null,
            byteArrayOf(11, 10, 2, 15),
            byteArrayOf(15, 11, 10, 2),
        )

        assert(sub1 != sub2)

        val sub3 = WebPushSubscription(
            "https://mozilla.org",
            "https://pushendpoint.mozilla.org/send/message/here",
            byteArrayOf(10, 2, 15),
            byteArrayOf(11, 10, 2, 15),
            byteArrayOf(15, 11, 10, 2),
        )

        val notSub = "notSub"

        assert(sub1 != sub2)
        assert(sub2 != sub3)
        assert(sub1 != sub3)
        assert(sub3 != sub1)
        assert(sub3 != sub2)
        assert(sub1 != notSub as Any)
    }

    @Test
    fun `hashCode is generated consistently from the class data`() {
        val sub1 = WebPushSubscription(
            "https://mozilla.org",
            "https://pushendpoint.mozilla.org/send/message/here",
            byteArrayOf(10, 2, 15, 11),
            byteArrayOf(11, 10, 2, 15),
            byteArrayOf(15, 11, 10, 2),
        )

        val sub2 = WebPushSubscription(
            "https://mozilla.org",
            "https://pushendpoint.mozilla.org/send/message/here",
            null,
            byteArrayOf(11, 10, 2, 15),
            byteArrayOf(15, 11, 10, 2),
        )

        assert(sub1.hashCode() == sub1.hashCode())
        assert(sub1.hashCode() != sub2.hashCode())
    }
}
