/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent
import android.arch.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyPermanentlyInvalidatedException
import android.security.keystore.KeyProperties
import android.support.annotation.RequiresApi
import android.support.v4.hardware.fingerprint.FingerprintManagerCompat
import android.support.v4.os.CancellationSignal
import java.io.IOException
import java.security.KeyStore
import java.security.KeyStoreException
import java.security.NoSuchAlgorithmException
import java.security.NoSuchProviderException
import java.security.UnrecoverableKeyException
import java.security.InvalidKeyException
import java.security.InvalidAlgorithmParameterException
import java.security.cert.CertificateException
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.NoSuchPaddingException
import javax.crypto.SecretKey

@RequiresApi(Build.VERSION_CODES.M)
class BiometricAuthenticationHandler(
        private val context: Context) : FingerprintManagerCompat.AuthenticationCallback(), LifecycleObserver {

    private val fingerprintManager = FingerprintManagerCompat.from(context)

    private var cancellationSignal: CancellationSignal? = null
    private var selfCancelled = false
    private var keyStore: KeyStore? = null
    private var keyGenerator: KeyGenerator? = null
    private var cryptoObject: FingerprintManagerCompat.CryptoObject? = null

    var needsAuth = false; private set
    var biometricFragment: BiometricAuthenticationDialogFragment? = null; private set

    init {
        try {
            keyStore = KeyStore.getInstance("AndroidKeyStore")
        } catch (err: KeyStoreException) {
            throw RuntimeException("Failed to get instance of KeyStore", err)
        }

        try {
            keyGenerator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        } catch (err: Exception) {
            when (err) {
                is NoSuchAlgorithmException,
                is NoSuchProviderException ->
                    throw RuntimeException("Failed to get an instance of KeyGenerator", err)
                else -> throw err
            }
        }

        createKey(DEFAULT_KEY_NAME, false)

        val defaultCipher: Cipher = try {
            Cipher.getInstance(KeyProperties.KEY_ALGORITHM_AES + "/"
                    + KeyProperties.BLOCK_MODE_CBC + "/"
                    + KeyProperties.ENCRYPTION_PADDING_PKCS7)
        } catch (err: Exception) {
            when (err) {
                is NoSuchAlgorithmException,
                is NoSuchPaddingException ->
                    throw RuntimeException("Failed to get an instance of Cipher", err)
                else -> throw err
            }
        }

        if (initCipher(defaultCipher, DEFAULT_KEY_NAME)) {
            cryptoObject = FingerprintManagerCompat.CryptoObject(defaultCipher)
        }

        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
    }

    private fun initCipher(cipher: Cipher, keyName: String): Boolean {
        try {
            keyStore?.load(null)
            val key = keyStore?.getKey(keyName, null) as SecretKey
            cipher.init(Cipher.ENCRYPT_MODE, key)
            return true
        } catch (err: Exception) {
            when (err) {
                is KeyPermanentlyInvalidatedException -> return false
                is KeyStoreException,
                is CertificateException,
                is UnrecoverableKeyException,
                is IOException,
                is NoSuchAlgorithmException,
                is InvalidKeyException -> throw RuntimeException("Failed to init cipher", err)
                else -> throw err
            }
        }
    }

    private fun createKey(keyName: String, invalidatedByBiometricEnrollment: Boolean?) {
        try {
            keyStore?.load(null)

            val builder = KeyGenParameterSpec.Builder(keyName,
                    KeyProperties.PURPOSE_ENCRYPT).setBlockModes(KeyProperties.BLOCK_MODE_CBC)
                    .setUserAuthenticationRequired(true)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_PKCS7)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                builder.setInvalidatedByBiometricEnrollment(invalidatedByBiometricEnrollment!!)
            }

            keyGenerator?.init(builder.build())
            keyGenerator?.generateKey()

        } catch (err: Exception) {
            when (err) {
                is NoSuchAlgorithmException,
                is InvalidAlgorithmParameterException,
                is CertificateException,
                is IOException -> throw RuntimeException(err)
                else -> throw err
            }
        }
    }

    // Create the prompt and begin listening
    private fun startListening(cryptoObject: FingerprintManagerCompat.CryptoObject) {
        if (!Biometrics.hasFingerprintHardware(context)) return
        if (biometricFragment == null) {
            biometricFragment = BiometricAuthenticationDialogFragment()
        }

        cancellationSignal = CancellationSignal()
        selfCancelled = false
        fingerprintManager.authenticate(cryptoObject, 0, cancellationSignal, this, null)
    }

    fun stopListening() {
        cancellationSignal?.let {
            selfCancelled = true
            it.cancel()
            cancellationSignal = null
        }
    }

    fun startAuthentication() {
        val cryptoObject = cryptoObject
        if (needsAuth && cryptoObject != null) {
            startListening(cryptoObject)
        }
    }

    override fun onAuthenticationError(errMsgId: Int, errString: CharSequence?) {
        if (!selfCancelled) {
            biometricFragment?.displayError(errString.toString())
        }
    }

    override fun onAuthenticationSucceeded(result: FingerprintManagerCompat.AuthenticationResult?) {
        needsAuth = false
        biometricFragment?.onAuthenticated()
    }

    override fun onAuthenticationHelp(helpMsgId: Int, helpString: CharSequence?) {
        biometricFragment?.displayError(helpString.toString())
    }

    override fun onAuthenticationFailed() {
        biometricFragment?.onFailure()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun onPause() {
        needsAuth = true
        stopListening()
    }

    companion object {
        private const val DEFAULT_KEY_NAME = "default_key"
    }
}
