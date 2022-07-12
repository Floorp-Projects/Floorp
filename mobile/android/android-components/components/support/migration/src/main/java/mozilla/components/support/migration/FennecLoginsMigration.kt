/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.database.DatabaseUtils
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteDatabase.OPEN_READONLY
import android.util.Base64
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.storage.Login
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.pkcs7unpad
import mozilla.components.support.ktx.kotlin.toHexString
import mozilla.components.support.ktx.kotlin.toSha1Digest
import java.lang.IllegalStateException
import javax.crypto.Cipher
import javax.crypto.Mac
import javax.crypto.SecretKeyFactory
import javax.crypto.spec.DESedeKeySpec
import javax.crypto.spec.IvParameterSpec
import javax.crypto.spec.SecretKeySpec

@Suppress("MagicNumber")
internal fun ByteArray.getEncodingLength(offset: Int): Int {
    // First byte indicates type.
    // Second byte is length of the octet string.
    // There are two definite forms of specifying length: short and long.
    // The short form consists of a single octet in which bit 8 is 0, and bits 1–7 encode the length
    // (which may be 0) as a number of octets.
    // The long form consist of 1 initial octet followed by 1 or more subsequent octets, containing the length.
    // In the initial octet, bit 8 is 1, and bits 1–7 (excluding the values 0 and 127) encode the number of octets
    // that follow.
    // +1 to skip the 'type' byte.
    val length = this[offset + 1].toInt() and 0xFF // bitmask 1111 1111
    val numOctets = if ((length and 0x80) != 0) { // bitmask 1000 0000
        length and 0x7F // bitmask 0111 1111
    } else {
        return length
    }

    // Example #1: we have an encoding that begins as [30 81 EB ...].
    // 30 means it's a sequence. 81 in binary is 10000001, so we're dealing with a long form and need to count
    // number of additional octets. We do that by 0x81 & 0x7F, or in binary 01111111 & 10000001 = 00000001,
    // meaning we have one additional length octet. That octet is EB, which in decimal is 235 - meaning, we have a
    // sequence whose content is 235 bytes long.

    // Example #2: we may have an octet string 1032 in length.
    // It will be encoded like this: [04 82 04 08 ...string octets....]
    // 04 is the type tag indicating octet string.
    // 82 in binary is 10000010, indicating that there are 2 more octets that encode length of the string.
    // So we take next two octets together: 04 08. In decimal that's 1032, which is the string length.
    return when (numOctets) {
        1 -> this[offset + 2].toInt() and 0xFF
        2 -> (this[offset + 2].toInt() and 0xFF shl 8) or (this[offset + 3].toInt() and 0xFF)
        3 -> (this[offset + 2].toInt() and 0xFF shl 16) or (this[offset + 3].toInt() and 0xFF shl 8) or
            (this[offset + 4].toInt() and 0xFF)
        else -> throw IllegalStateException("Unsupported number of octets: $numOctets")
    }
}

internal class LoginMigrationException(val failure: LoginsMigrationResult.Failure) : Exception(failure.toString())

internal sealed class LoginsMigrationResult {
    internal sealed class Success : LoginsMigrationResult() {
        internal object MasterPasswordIsSet : Success()

        internal data class ImportedLoginRecords(
            val totalRecordsDetected: Int,
            val failedToProcess: Int,
            val failedToImport: Int
        ) : Success()
    }

    internal sealed class Failure {
        internal data class FailedToCheckMasterPassword(val e: Exception) : Failure() {
            override fun toString(): String {
                return "Failed to check MP: $e"
            }
        }

        internal data class UnsupportedSignonsDbVersion(val version: Int) : Failure() {
            override fun toString(): String {
                return "Unsupported signons.sqlite DB version: $version"
            }
        }

        internal data class UnexpectedLoginsKeyMaterialAlg(val berDecoded: String) : Failure() {
            override fun toString(): String {
                return "Unexpected login key material encryption alg oid: $berDecoded"
            }
        }

        internal data class UnexpectedMetadataKeyMaterialAlg(val berDecoded: String) : Failure() {
            override fun toString(): String {
                return "Unexpected metadata key material encryption alg oid: $berDecoded"
            }
        }

        internal data class GetLoginsThrew(val e: Exception) : Failure() {
            override fun toString(): String {
                return "getLogins unexpectedly threw: $e"
            }
        }

        internal data class RustImportThrew(val e: Exception) : Failure() {
            override fun toString(): String {
                return "Rust import unexpectedly threw: $e"
            }
        }
    }
}

@VisibleForTesting
internal data class FennecLoginRecords(
    // Records that we were able to process.
    val records: List<Login>,
    // Total number of records that we saw in the database.
    val totalRecordsDetected: Int
)

@Suppress("LargeClass")
internal object FennecLoginsMigration {
    const val DEFAULT_MASTER_PASSWORD = ""
    private const val EXPECTED_DB_VERSION = 6

    private val logger = Logger("FennecLoginsMigration")
    private val keyForId: MutableMap<String, ByteArray> = mutableMapOf()

    @Suppress("TooGenericExceptionCaught", "ReturnCount", "LongParameterList", "ComplexMethod")
    internal suspend fun migrate(
        crashReporter: CrashReporting,
        signonsDbPath: String,
        key4DbPath: String,
        loginsStorage: SyncableLoginsStorage,
        masterPassword: String = DEFAULT_MASTER_PASSWORD
    ): Result<LoginsMigrationResult> {
        val noMasterPassword = try {
            isMasterPasswordValid(masterPassword, key4DbPath)
        } catch (e: Exception) {
            logger.error("Failed to check master password", e)
            return Result.Failure(LoginMigrationException(LoginsMigrationResult.Failure.FailedToCheckMasterPassword(e)))
        }

        if (!noMasterPassword) {
            return Result.Success(LoginsMigrationResult.Success.MasterPasswordIsSet)
        }

        val fennecRecords = try {
            getLogins(crashReporter, masterPassword, signonsDbPath = signonsDbPath, key4DbPath = key4DbPath)
        } catch (e: Exception) {
            return Result.Failure(LoginMigrationException(LoginsMigrationResult.Failure.GetLoginsThrew(e)))
        }

        loginsStorage.importLoginsAsync(fennecRecords.records)

        return Result.Success(
            LoginsMigrationResult.Success.ImportedLoginRecords(
                totalRecordsDetected = fennecRecords.totalRecordsDetected,
                failedToProcess = (fennecRecords.totalRecordsDetected - fennecRecords.records.size),
                failedToImport = 0,
            )
        )
    }

    /**
     * Checks if provided [masterPassword] is valid (can be used to decrypt records).
     *
     * @param masterPassword A master password to check.
     * @param key4DbPath Path to key4 db.
     * @return True if [masterPassword] is correct, false otherwise.
     */
    internal fun isMasterPasswordValid(masterPassword: String, key4DbPath: String): Boolean {
        val db = SQLiteDatabase.openDatabase(key4DbPath, null, OPEN_READONLY)
        // item1 is global salt, item2 is DER-encoded blob containing: entry salt and ciphertext.
        // Master password is combined with global salt and entry salt, and from this combination we derive key and iv
        // necessary for the TDES cipher. Once we have key and iv, we can decrypt item2.
        // item2, once decrypted using a correct master password, will contain a "password check" string.
        return db.rawQuery("select item1, item2 from metadata where id = 'password'", null).use {
            it.moveToNext()

            val globalSalt = it.getBlob(it.getColumnIndexOrThrow("item1"))
            val item2 = it.getBlob(it.getColumnIndexOrThrow("item2"))

            val passwordCheckPlaintext = derDecodeKey4Entry(item2)
                .decrypt(masterPassword, globalSalt)
                .pkcs7unpad()
            val realPasswordCheck = "password-check".toByteArray()

            passwordCheckPlaintext.contentEquals(realPasswordCheck)
        }
    }

    @Suppress("LongMethod")
    internal fun getLogins(
        crashReporter: CrashReporting,
        masterPassword: String,
        signonsDbPath: String,
        key4DbPath: String
    ): FennecLoginRecords {
        val db = SQLiteDatabase.openDatabase(signonsDbPath, null, OPEN_READONLY)
        val records: MutableList<Login> = mutableListOf()

        // db.version runs a 'pragma user_version;' query, so cache the result in case we need to throw.
        val dbVersion = db.version
        if (dbVersion != EXPECTED_DB_VERSION) {
            throw LoginMigrationException(LoginsMigrationResult.Failure.UnsupportedSignonsDbVersion(dbVersion))
        }

        val totalRecordCount = DatabaseUtils.longForQuery(
            db, "select count(id) from moz_logins", null
        ).toInt()

        val selectQuery = """
            select
                hostname,
                httpRealm,
                formSubmitURL,
                usernameField,
                passwordField,
                encryptedUsername,
                encryptedPassword,
                guid,
                timeCreated,
                timesUsed,
                timeLastUsed,
                timePasswordChanged
            from
                moz_logins
        """.trimIndent()

        val exceptionCollector: MutableMap<String, Exception> = mutableMapOf()

        db.rawQuery(selectQuery, null).use {
            while (it.moveToNext()) {
                collectExceptionsIntoASet(exceptionCollector) {
                    val encryptedUsername = it.getString(it.getColumnIndexOrThrow("encryptedUsername"))
                    val encryptedPassword = it.getString(it.getColumnIndexOrThrow("encryptedPassword"))

                    val derUsername = Base64.decode(encryptedUsername, Base64.DEFAULT)
                    val plaintextUsername = derDecodeLoginEntry(derUsername)
                        .decrypt(masterPassword, key4DbPath)
                        .pkcs7unpad()

                    val derPassword = Base64.decode(encryptedPassword, Base64.DEFAULT)
                    val plaintextPassword = derDecodeLoginEntry(derPassword)
                        .decrypt(masterPassword, key4DbPath)
                        .pkcs7unpad()

                    val encodedUsername = String(plaintextUsername, Charsets.UTF_8)
                    val encodedPassword = String(plaintextPassword, Charsets.UTF_8)

                    val hostname = it.getString(it.getColumnIndexOrThrow("hostname"))
                    val httpRealm = it.getString(it.getColumnIndexOrThrow("httpRealm"))
                    val formSubmitUrl = it.getString(it.getColumnIndexOrThrow("formSubmitURL"))
                    val usernameField = it.getString(it.getColumnIndexOrThrow("usernameField"))
                    val passwordField = it.getString(it.getColumnIndexOrThrow("passwordField"))
                    val guid = it.getString(it.getColumnIndexOrThrow("guid"))
                    val timeCreated = it.getLong(it.getColumnIndexOrThrow("timeCreated"))
                    val timesUsed = it.getInt(it.getColumnIndexOrThrow("timesUsed"))
                    val timeLastUsed = it.getLong(it.getColumnIndexOrThrow("timeLastUsed"))
                    val timePasswordChanged = it.getLong(it.getColumnIndexOrThrow("timePasswordChanged"))

                    val fullRecord = Login(
                        guid = guid,
                        origin = hostname,
                        username = encodedUsername,
                        password = encodedPassword,
                        httpRealm = httpRealm,
                        formActionOrigin = formSubmitUrl,
                        timesUsed = timesUsed.toLong(),
                        timeCreated = timeCreated,
                        timeLastUsed = timeLastUsed,
                        timePasswordChanged = timePasswordChanged,
                        usernameField = usernameField,
                        passwordField = passwordField
                    )
                    records.add(fullRecord)
                }
            }
        }

        // Report exceptions encountered to Sentry. Exceptions are collected into a set, to prevent spamming Sentry
        // if we encounter the same exception again and again in a while loop above.
        exceptionCollector.forEach {
            crashReporter.submitCaughtException(it.value)
        }

        return FennecLoginRecords(records = records, totalRecordsDetected = totalRecordCount)
    }

    private fun SignonsEntry.decrypt(masterPassword: String, key4DbPath: String): ByteArray {
        // Make sure this entry is encrypted with an algorithm we understand.
        this.oid.matchesBER(BER_ENCODED_DES_EDE3_CBC) { berHex ->
            throw LoginMigrationException(LoginsMigrationResult.Failure.UnexpectedLoginsKeyMaterialAlg(berHex))
        }

        // To decrypt a signons.sqlite entry (username or password), we first need to obtain an encryption key.
        // This key is stored in an encrypted form in the key4.db file; we can use our masterPassword to decrypt it.
        val key = getKeyProtectedWithMasterPassword(this.keyId, masterPassword, key4DbPath)
            ?: throw IllegalStateException()

        // Once we have the key, we can go ahead and decrypt this entry.
        return decryptTDES(key, this.iv, this.cipherText)
    }

    private fun Key4Entry.decrypt(masterPassword: String, globalSalt: ByteArray): ByteArray {
        // Make sure this entry is encrypted with an algorithm we understand.
        this.oid.matchesBER(BER_ENCODED_PKCS_12_PBEWITHSHA1ANDTRIPLEDESCBC) { berHex ->
            throw LoginMigrationException(LoginsMigrationResult.Failure.UnexpectedMetadataKeyMaterialAlg(berHex))
        }

        // To decrypt a key4.db entry, we need a key and an iv.
        // These are derived from the master password, global and entry salts.
        val derivedKeyMaterial = deriveKeyIvPKCS12PBE(masterPassword, globalSalt, entrySalt)
        val keyMaterial = derivedKeyMaterial[0]
        val ivMaterial = derivedKeyMaterial[1]

        // Once we have key and iv, we can decrypt this entry.
        return decryptTDES(keyMaterial, ivMaterial, cipherText)
    }

    private fun ByteArray.matchesBER(berHex: String, otherwise: (hexValue: String) -> Unit): Boolean {
        val hex = this.toHexString()
        return if (hex != berHex) {
            otherwise(hex)
            false
        } else {
            true
        }
    }

    /**
     * Gets a key from [key4DbPath] identified by [keyId] and protected by [masterPassword].
     *
     * This is a memoizing function.
     */
    private fun getKeyProtectedWithMasterPassword(
        keyId: ByteArray,
        masterPassword: String,
        key4DbPath: String
    ): ByteArray? {
        val cacheKey = keyId.toHexString() + masterPassword + key4DbPath

        keyForId[cacheKey]?.let { return it }

        val globalSalt = getGlobalSalt(key4DbPath)
        val db = SQLiteDatabase.openDatabase(key4DbPath, null, OPEN_READONLY)
        // a102 is keyid.
        // a11 is a binary blob. It's ASN.1 decoded, and tells us algorithm oid, entry salt, iteration count, and
        // ciphertext for the key itself. See derDecodeKey4Entry for details.
        val a11 = db.rawQuery("select a11, a102 from nssPrivate", null).use {
            while (it.moveToNext()) {
                val maybeRightKeyId = it.getBlob(it.getColumnIndexOrThrow("a102"))
                if (maybeRightKeyId.contentEquals(keyId)) {
                    return@use it.getBlob(it.getColumnIndexOrThrow("a11"))
                }
            }
            return null
        }

        // Now that we have a11, decode it and decrypt the key stored in it.
        val decryptedKeyMaterial = derDecodeKey4Entry(a11).decrypt(masterPassword, globalSalt)
        // Unpad the key, store it in the cache for future reference.
        return (decryptedKeyMaterial.copyOfRange(0, DES_KEY_SIZE)).also { keyForId[cacheKey] = it }
    }

    /**
     * Obtains a global salt from the [key4DbPath]. Each record in it will also have a separate "entry salt".
     */
    private fun getGlobalSalt(key4DbPath: String): ByteArray {
        val db = SQLiteDatabase.openDatabase(key4DbPath, null, OPEN_READONLY)
        return db.rawQuery("select item1 from metadata where id = 'password'", null).use {
            it.moveToNext()
            it.getBlob(it.getColumnIndexOrThrow("item1"))
        }
    }

    /**
     * Decrypts [ciphertext] using TDES.
     */
    private fun decryptTDES(key: ByteArray, iv: ByteArray, ciphertext: ByteArray): ByteArray {
        val keyFactory = SecretKeyFactory.getInstance(DES_EDE)
        val desKeySpec = DESedeKeySpec(key)

        val secretKey = keyFactory.generateSecret(desKeySpec)
        val cipher = Cipher.getInstance(DES_EDE_CBC_NOPADDING)

        val encIV = IvParameterSpec(iv)

        cipher.init(Cipher.DECRYPT_MODE, secretKey, encIV)

        return cipher.doFinal(ciphertext)
    }

    /**
     * Derives IV and Key material from the provided salts and [masterPassword].
     * @return List of [key, iv].
     */
    private fun deriveKeyIvPKCS12PBE(
        masterPassword: String,
        globalSalt: ByteArray,
        entrySalt: ByteArray
    ): List<ByteArray> {
        // Derive 'key' and 'iv' material from a combination of Master Password, global salt and entry salt.
        // For reference, see https://github.com/nss-dev/nss/blob/471fcb11a6c7b8a7c8e49e609c72e8bc2a297397/lib/softoken/lowpbe.c#L560
        // and https://github.com/louisabraham/ffpass/blob/master/ffpass/__init__.py#L110
        val hp = (globalSalt + masterPassword.toByteArray()).toSha1Digest()
        val pes = entrySalt + ByteArray(PES_SIZE - entrySalt.size)
        val chp = (hp + entrySalt).toSha1Digest()
        val keySpec = SecretKeySpec(chp, HMAC_SHA1)
        val hasher = Mac.getInstance(HMAC_SHA1)
        hasher.init(keySpec)
        val k1 = hasher.doFinal(pes + entrySalt)
        hasher.reset()
        val tk = hasher.doFinal(pes)
        hasher.reset()
        val k2 = hasher.doFinal(tk + entrySalt)
        val k = k1 + k2

        val iv = k.copyOfRange(k.size - DES_IV_SIZE, k.size)
        val key = k.copyOfRange(0, DES_KEY_SIZE)

        return listOf(key, iv)
    }

    /**
     * @param byteArray ASN.1 DER-encoded metadata entry from key4.db.
     * @return List of [OID, entry salt, ciphertext].
     */
    private fun derDecodeKey4Entry(byteArray: ByteArray): Key4Entry {
        // Sample ASN.1 DER encoded record:
        // SEQUENCE (2 elem)
        //      SEQUENCE (2 elem)
        //          OBJECT IDENTIFIER 1.2.840.113549.1.12.5.1.3 pkcs-12-PBEWithSha1AndTripleDESCBC # oid
        //          SEQUENCE (2 elem)
        //              OCTET STRING (20 byte) B225D5E839B4CCED1F7C85ED7793D77BBC4E02B3 # entry salt
        //              INTEGER 1 # iteration count
        //      OCTET STRING (16 byte) 39E791333C515A996EC6B1BAB11B576A # ciphertext, of variable length

        // NB: 'expectedLength' is asserted for every field before ciphertext.
        // Values are expected to be of known lengths, which is why offsets are simply hard-coded.
        val oid = byteArray.derValue(DER_TYPE_OID, offset = 4, expectedLength = 11)
        val entrySalt = byteArray.derValue(DER_TYPE_OCTET_STRING, offset = 19, expectedLength = 20)
        val cipherText = byteArray.derValue(DER_TYPE_OCTET_STRING, offset = 44)

        return Key4Entry(oid = oid.data, entrySalt = entrySalt.data, cipherText = cipherText.data)
    }

    /**
     * @return List of [keyId, OID, iv, ciphertext].
     */
    private fun derDecodeLoginEntry(byteArray: ByteArray): SignonsEntry {
        // Sample ASN.1 DER encoded record:
        // SEQUENCE (3 elem)
        //      OCTET STRING (16 byte) F8000000000000000000000000000001 # keyId
        //      SEQUENCE (2 elem)
        //          OBJECT IDENTIFIER 1.2.840.113549.3.7 des-EDE3-CBC (RSADSI encryptionAlgorithm) # oid
        //          OCTET STRING (8 byte) 6DC95F1B0484514A # iv
        //      OCTET STRING (8 byte) D726CD05D3A917FF # ciphertext, of variable length

        // Note that unlike in [derDecodeKey4Entry], here we're dealing with data of arbitrary length.
        // To deal with that, we need to keep track of a running offset, which is then used to obtain data values.
        // Expected length of data values are asserted, since this is a parser of a very narrow purpose.
        // Cipher text is last, so its length can be anything - however, its varying length will affect offset value
        // of the first sequence tag (since it needs to specify length, which will include every field, including
        // cipher text).
        val seqLength = byteArray.getEncodingLength(0)
        var offset = 1 + getEncodingLengthOffset(seqLength)
        val keyId = byteArray.derValue(DER_TYPE_OCTET_STRING, offset = offset, expectedLength = 16)
        // +2 for the inner sequence offset (1 for its type octet, one for its length)
        offset += keyId.totalLength + 2
        val oid = byteArray.derValue(DER_TYPE_OID, offset = offset, expectedLength = 8)
        offset += oid.totalLength
        val iv = byteArray.derValue(DER_TYPE_OCTET_STRING, offset = offset, expectedLength = 8)
        offset += iv.totalLength
        val cipherText = byteArray.derValue(DER_TYPE_OCTET_STRING, offset = offset)

        return SignonsEntry(keyId = keyId.data, oid = oid.data, iv = iv.data, cipherText = cipherText.data)
    }

    /**
     * A very simplistic DER value lookup.
     * Asserts that [type] of element at [offset] is correct, and its length matches [expectedLength] (if provided).
     */
    @Suppress("MagicNumber")
    private fun ByteArray.derValue(type: Byte, offset: Int, expectedLength: Int? = null): BerElem {
        // First byte at offset is type. Make sure it matches expected type.
        check(this[offset] == type)
        val lengthByte = this[offset + 1].toInt()
        if (lengthByte == 0x80) {
            throw NotImplementedError("indefinite length form not supported")
        }
        val length = this.getEncodingLength(offset)
        expectedLength?.let { check(length == expectedLength) { "$length != $expectedLength" } }
        // +2 to skip the 'type' and 'length' bytes.
        val lengthOffset = getEncodingLengthOffset(length)
        return BerElem(
            totalLength = 1 + lengthOffset + length, // type byte, length byte(s), length itself
            data = this.copyOfRange(offset + 1 + lengthOffset, offset + 1 + lengthOffset + length)
        )
    }

    /**
     * @return How many bytes it'd take to encode length parameter describing value of length [length].
     */
    @Suppress("MagicNumber")
    private fun getEncodingLengthOffset(length: Int): Int {
        return when (length) {
            in 0..127 -> 1
            in 128..255 -> 2
            in 256..65535 -> 3
            in 65536..16777215 -> 4
            // We don't need to deal with larger values.
            else -> throw IllegalStateException("Octet length not supported: $length")
        }
    }

    /**
     * Runs [block], and inserts exception that it may possibly throw into a set of exceptions, the [collector].
     */
    @Suppress("TooGenericExceptionCaught")
    @VisibleForTesting
    internal fun collectExceptionsIntoASet(collector: MutableMap<String, Exception>, block: () -> Unit) {
        try {
            block()
        } catch (e: Exception) {
            collector[e.uniqueId()] = e
        }
    }
}

// OID 1.2.840.113549.1.12.5.1.3, used for encrypting key material in key4.db
private const val BER_ENCODED_PKCS_12_PBEWITHSHA1ANDTRIPLEDESCBC = "2a864886f70d010c050103"

// OID 1.2.840.113549.3.7, used for encrypting username/password values in signons.sqlite
private const val BER_ENCODED_DES_EDE3_CBC = "2a864886f70d0307"

private const val HMAC_SHA1 = "hmacSHA1"
private const val DES_EDE = "DESede"
private const val DES_EDE_CBC_NOPADDING = "DESede/CBC/NoPadding"
private const val DES_KEY_SIZE = 24
private const val DES_IV_SIZE = 8

private const val PES_SIZE = 20

private const val DER_TYPE_OID: Byte = 0x06
private const val DER_TYPE_OCTET_STRING: Byte = 0x04

/**
 * DER-encoded entry in key4.db. Represents a managed key.
 * @param oid An algorithm used to encrypt [cipherText].
 * @param entrySalt A salt used during encryption.
 * @param cipherText Encrypted value.
 */
private data class Key4Entry(
    val oid: ByteArray,
    val entrySalt: ByteArray,
    val cipherText: ByteArray
) {
    override fun equals(other: Any?): Boolean {
        throw NotImplementedError()
    }

    override fun hashCode(): Int {
        throw NotImplementedError()
    }
}

/**
 * DER-encoded entry in signons.sqlite. Could be either encryptedUsername or encryptedPassword.
 * @param keyId A reference to a managed key in key4.db (see [Key4Entry]).
 * @param oid An algorithm used to encrypt [cipherText].
 * @param iv An IV value used during encryption.
 * @param cipherText Encrypted value.
 */
private data class SignonsEntry(
    val keyId: ByteArray,
    val oid: ByteArray,
    val iv: ByteArray,
    val cipherText: ByteArray
) {
    override fun equals(other: Any?): Boolean {
        throw NotImplementedError()
    }

    override fun hashCode(): Int {
        throw NotImplementedError()
    }
}

/**
 * A representation of some BER-encoded value, which is [data], with a total length of [totalLength].
 * Total length takes into account type and length bytes, as well as total size of [data].
 */
private data class BerElem(val totalLength: Int, val data: ByteArray) {
    override fun equals(other: Any?): Boolean {
        throw NotImplementedError()
    }

    override fun hashCode(): Int {
        throw NotImplementedError()
    }
}
