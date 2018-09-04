/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import org.junit.Assert
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.nio.charset.StandardCharsets
import java.security.Key
import java.security.KeyStore
import java.security.InvalidKeyException
import java.security.SecureRandom
import java.security.Security
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey

private val DEFAULTPASS = "testit!".toCharArray()

/* mock keystore wrapper to deal with intricacies of how Java/Anroid key management work */
internal class MockStoreWrapper : KeyStoreWrapper() {
    override fun loadKeyStore(): KeyStore {
        val ks = KeyStore.getInstance("JCEKS")
        ks.load(null)
        return ks
    }

    override fun getKeyFor(label: String): Key? =
            getKeyStore().getKey(label, DEFAULTPASS)
    override fun makeKeyFor(label: String): SecretKey {
        val gen = KeyGenerator.getInstance("AES")
        gen.init(256)
        val key = gen.generateKey()
        getKeyStore().setKeyEntry(label, key, DEFAULTPASS, null)

        return key
    }
}

@RunWith(RobolectricTestRunner::class)
class KeystoreTest {
    private var wrapper = MockStoreWrapper()
    private var rng = SecureRandom()

    private fun createKeystore(label: String): Keystore {
        val ks = Keystore(label)
        ks.wrapper = wrapper
        return ks
    }

    @Before
    fun setUp() {
        Security.setProperty("crypto.policy", "unlimited")
    }

    @Test
    fun testWorkingWithLabel() {
        val keystore = createKeystore("test-labels")

        Assert.assertFalse(keystore.available())
        keystore.generateKey()
        Assert.assertTrue(keystore.available())
        keystore.deleteKey()
        Assert.assertFalse(keystore.available())
    }

    @Test
    fun testCreateEncryptCipher() {
        val keystore = createKeystore("test-encrypt-ciphers")

        Assert.assertFalse(keystore.available())
        var caught = false
        var cipher: Cipher? = null
        try {
            cipher = keystore.createEncryptCipher()
        } catch (ex: InvalidKeyException) {
            caught = true
        } finally {
            Assert.assertTrue("unexpected success", caught)
            Assert.assertNull(cipher)
        }

        keystore.generateKey()
        Assert.assertTrue(keystore.available())
        cipher = keystore.createEncryptCipher()
        Assert.assertEquals(CIPHER_SPEC, cipher.algorithm)
        Assert.assertNotNull(cipher.iv)
    }

    @Test
    fun testCreateDecryptCipher() {
        val keystore = createKeystore("test-decrypt-ciphers")
        val iv = ByteArray(12)
        rng.nextBytes(iv)

        Assert.assertFalse(keystore.available())
        var caught = false
        var cipher: Cipher? = null
        try {
            cipher = keystore.createDecryptCipher(iv)
        } catch (ex: InvalidKeyException) {
            caught = true
        } finally {
            Assert.assertTrue("unexpected success", caught)
            Assert.assertNull(cipher)
        }

        keystore.generateKey()
        Assert.assertTrue(keystore.available())
        cipher = keystore.createDecryptCipher(iv)
        Assert.assertEquals(CIPHER_SPEC, cipher.algorithm)
        Assert.assertArrayEquals(iv, cipher.iv)
    }

    @Ignore("troubleshooting test-env crypto errors")
    @Test
    fun testCryptoRoundTrip() {
        val keystore = createKeystore("test-roundtrip")
        keystore.generateKey()

        var input = "classic plaintext 'hello, world'".toByteArray(StandardCharsets.UTF_8)
        var encrypted = keystore.encryptBytes(input)
        Assert.assertNotNull(encrypted)
        var output = keystore.decryptBytes(encrypted)
        Assert.assertArrayEquals(input, output)
    }
}
