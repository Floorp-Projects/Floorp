/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.feature.accounts.push.VerificationDelegate.Companion.MAX_REQUEST_IN_INTERVAL
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class VerificationDelegateTest {

    @Before
    fun setup() {
        preference(testContext).edit().remove(PREF_LAST_VERIFIED).apply()
    }

    @Test
    fun `init uses current timestamp`() {
        val timestamp = System.currentTimeMillis()
        val verifier = VerificationDelegate(testContext)
        assertEquals(0, verifier.innerCount)
        assertTrue(timestamp <= verifier.innerTimestamp && timestamp + 1000 > verifier.innerTimestamp)
    }

    @Test
    fun `init uses cached timestamp`() {
        lastVerifiedPref = Pair(1000, 50)

        val verifier = VerificationDelegate(testContext)
        assertEquals(50, verifier.innerCount)
        assertEquals(1000, verifier.innerTimestamp)
    }

    @Test
    fun `after interval the counter resets`() {
        lastVerifiedPref = Pair(System.currentTimeMillis() - VERIFY_NOW_INTERVAL, 50)

        val verifier = VerificationDelegate(testContext)

        assertEquals(50, verifier.innerCount)
        assertEquals(50, lastVerifiedPref.second)

        val result = verifier.allowedToRenew()

        assertEquals(0, verifier.innerCount)

        assertEquals(0, lastVerifiedPref.second)

        assertTrue(result)
    }

    @Test
    fun `false if requesting above rate limit`() {
        val timestamp = System.currentTimeMillis()
        lastVerifiedPref = Pair(timestamp, 501)

        val verifier = VerificationDelegate(testContext)

        assertEquals(MAX_REQUEST_IN_INTERVAL + 1, verifier.innerCount)
        assertEquals(MAX_REQUEST_IN_INTERVAL + 1, lastVerifiedPref.second)

        val result = verifier.allowedToRenew()

        assertFalse(result)
        assertEquals(timestamp, verifier.innerTimestamp)
    }

    @Test
    fun `reset when above rate limit and interval`() {
        lastVerifiedPref = Pair(System.currentTimeMillis() - VERIFY_NOW_INTERVAL, 501)

        val verifier = VerificationDelegate(testContext)

        assertEquals(501, verifier.innerCount)
        assertEquals(501, lastVerifiedPref.second)

        val result = verifier.allowedToRenew()

        assertEquals(0, verifier.innerCount)

        assertEquals(0, lastVerifiedPref.second)

        assertTrue(result)
    }

    @Test
    fun `increment updates inner values and cache`() {
        val verifier = VerificationDelegate(testContext)

        assertEquals(0, verifier.innerCount)
        assertEquals(0, lastVerifiedPref.second)

        verifier.increment()

        assertEquals(1, verifier.innerCount)
        assertEquals(1, lastVerifiedPref.second)
    }

    @Test
    fun `rate-limiting disabled short circuits check`() {
        val timestamp = System.currentTimeMillis()
        lastVerifiedPref = Pair(timestamp, 501)

        val verifier = VerificationDelegate(testContext, true)

        val result = verifier.allowedToRenew()

        assertTrue(result)
    }

    companion object {
        var lastVerifiedPref: Pair<Long, Int>
            get() {
                val stringResult = requireNotNull(
                    preference(testContext).getString(
                        PREF_LAST_VERIFIED,
                        "{\"timestamp\": ${System.currentTimeMillis()}, \"totalCount\": 0}",
                    ),
                )
                val json = JSONObject(stringResult)
                return Pair(json.getLong("timestamp"), json.getInt("totalCount"))
            }
            set(value) {
                preference(testContext).edit()
                    .putString(PREF_LAST_VERIFIED, "{\"timestamp\": ${value.first}, \"totalCount\": ${value.second}}")
                    .apply()
            }

        private const val VERIFY_NOW_INTERVAL = 25 * 60 * 60 * 1000L // 25 hours in milliseconds
    }
}
