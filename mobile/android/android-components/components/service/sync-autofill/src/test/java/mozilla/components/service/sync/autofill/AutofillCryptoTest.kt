/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.KeyGenerationReason
import mozilla.components.concept.storage.ManagedKey
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class AutofillCryptoTest {

    private lateinit var securePrefs: SecureAbove22Preferences

    @Before
    fun setup() {
        // forceInsecure is set in the tests because a keystore wouldn't be configured in the test environment.
        securePrefs = SecureAbove22Preferences(testContext, "autofill", forceInsecure = true)
    }

    @Test
    fun `get key - new`() = runTest {
        val storage = mock<AutofillCreditCardsAddressesStorage>()
        val crypto = AutofillCrypto(testContext, securePrefs, storage)
        val key = crypto.getOrGenerateKey()
        assertEquals(KeyGenerationReason.New, key.wasGenerated)

        // key was persisted, subsequent fetches return it.
        val key2 = crypto.getOrGenerateKey()
        assertNull(key2.wasGenerated)

        assertEquals(key.key, key2.key)
        verifyNoInteractions(storage)
    }

    @Test
    fun `get key - lost`() = runTest {
        val storage = mock<AutofillCreditCardsAddressesStorage>()
        val crypto = AutofillCrypto(testContext, securePrefs, storage)
        val key = crypto.getOrGenerateKey()
        assertEquals(KeyGenerationReason.New, key.wasGenerated)

        // now, let's loose the key. It'll be regenerated
        securePrefs.clear()
        val key2 = crypto.getOrGenerateKey()
        assertEquals(KeyGenerationReason.RecoveryNeeded.Lost, key2.wasGenerated)

        assertNotEquals(key.key, key2.key)
        verify(storage).scrubEncryptedData()
    }

    @Test
    fun `get key - corrupted`() = runTest {
        val storage = mock<AutofillCreditCardsAddressesStorage>()
        val crypto = AutofillCrypto(testContext, securePrefs, storage)
        val key = crypto.getOrGenerateKey()
        assertEquals(KeyGenerationReason.New, key.wasGenerated)

        // now, let's corrupt the key. It'll be regenerated
        securePrefs.putString(AutofillCrypto.AUTOFILL_KEY, "garbage")

        val key2 = crypto.getOrGenerateKey()
        assertEquals(KeyGenerationReason.RecoveryNeeded.Corrupt, key2.wasGenerated)

        assertNotEquals(key.key, key2.key)
        verify(storage).scrubEncryptedData()
    }

    @Test
    fun `get key - corrupted subtly`() = runTest {
        val storage = mock<AutofillCreditCardsAddressesStorage>()
        val crypto = AutofillCrypto(testContext, securePrefs, storage)
        val key = crypto.getOrGenerateKey()
        assertEquals(KeyGenerationReason.New, key.wasGenerated)

        // now, let's corrupt the key. It'll be regenerated
        // this key is shaped correctly, but of course it won't be the same as what we got back in the first call to key()
        securePrefs.putString(AutofillCrypto.AUTOFILL_KEY, "{\"kty\":\"oct\",\"k\":\"GhsmEtujZN_qMEgw1ZHhcJhdAFR9EkUgb94qANel-P4\"}")

        val key2 = crypto.getOrGenerateKey()
        assertEquals(KeyGenerationReason.RecoveryNeeded.Corrupt, key2.wasGenerated)

        assertNotEquals(key.key, key2.key)
        verify(storage).scrubEncryptedData()
    }

    @Test
    fun `encrypt and decrypt card - normal`() = runTest {
        val crypto = AutofillCrypto(testContext, securePrefs, mock())
        val key = crypto.getOrGenerateKey()
        val plaintext1 = CreditCardNumber.Plaintext("4111111111111111")
        val plaintext2 = CreditCardNumber.Plaintext("4111111111111111")

        val encrypted1 = crypto.encrypt(key, plaintext1)!!
        val encrypted2 = crypto.encrypt(key, plaintext2)!!

        // We use a non-deterministic encryption scheme.
        assertNotEquals(encrypted1, encrypted2)

        assertEquals("4111111111111111", crypto.decrypt(key, encrypted1)!!.number)
        assertEquals("4111111111111111", crypto.decrypt(key, encrypted2)!!.number)
    }

    @Test
    fun `encrypt and decrypt card - bad keys`() = runTest {
        val crypto = AutofillCrypto(testContext, securePrefs, mock())
        val plaintext = CreditCardNumber.Plaintext("4111111111111111")

        val badKey = ManagedKey(key = "garbage", wasGenerated = null)
        assertNull(crypto.encrypt(badKey, plaintext))

        // This isn't a valid key.
        val corruptKey = ManagedKey(key = "{\"kty\":\"oct\",\"k\":\"GhsmEtujZN_qMEgw1ZHhcJhdAFR9EkU\"}", wasGenerated = null)
        assertNull(crypto.encrypt(corruptKey, plaintext))

        val goodKey = crypto.getOrGenerateKey()
        val encrypted = crypto.encrypt(goodKey, plaintext)!!

        assertNull(crypto.decrypt(badKey, encrypted))
        assertNull(crypto.decrypt(corruptKey, encrypted))
    }
}
