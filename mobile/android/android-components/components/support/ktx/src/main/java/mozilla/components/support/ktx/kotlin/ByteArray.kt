/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import mozilla.components.support.base.log.logger.Logger
import java.security.MessageDigest

/**
 * Checks whether the given [test] byte sequence exists at the [offset] of this [ByteArray]
 */
fun ByteArray.containsAtOffset(offset: Int, test: ByteArray): Boolean {
    if (size - offset < test.size) {
        return false
    }

    for (i in 0 until test.size) {
        if (this[offset + i] != test[i]) {
            return false
        }
    }

    return true
}

fun ByteArray.toBitmap(opts: BitmapFactory.Options? = null): Bitmap? {
    return toBitmap(0, size, opts)
}

fun ByteArray.toBitmap(
    offset: Int,
    length: Int,
    opts: BitmapFactory.Options? = null,
): Bitmap? {
    if (length <= 0) {
        return null
    }

    return try {
        val bitmap = BitmapFactory.decodeByteArray(this, offset, length, opts)

        if (bitmap == null) {
            null
        } else if (bitmap.width <= 0 || bitmap.height <= 0) {
            Logger.warn("Decoded bitmap jas dimensions: ${bitmap.width} x ${bitmap.height}")
            null
        } else {
            bitmap
        }
    } catch (e: OutOfMemoryError) {
        Logger.warn("OutOfMemoryError while decoding byte array", e)
        null
    }
}

fun ByteArray.toSha256Digest(): ByteArray {
    return MessageDigest.getInstance("SHA-256").digest(this)
}

/**
 * @return A SHA-1 digest.
 */
fun ByteArray.toSha1Digest(): ByteArray {
    return MessageDigest.getInstance("SHA-1").digest(this)
}

/**
 * @return An unpadded byte array, according to PKCS#7.
 */
@Suppress("MagicNumber")
fun ByteArray.pkcs7unpad(): ByteArray {
    // Last byte tells us the padding length.
    val paddingLength = this.last()
    // Padding can't be more than 15 bytes.
    if (paddingLength in 0..16) {
        return this.copyOfRange(0, this.size - paddingLength)
    }
    return this
}

fun ByteArray.toHexString(): String {
    return toHexString(2 * this.size)
}

@Suppress("MagicNumber")
fun ByteArray.toHexString(hexLength: Int): String {
    val hs = StringBuilder(Math.max(2 * this.size, hexLength))
    var stmp: String

    for (n in 0 until hexLength - 2 * this.size) {
        hs.append("0")
    }

    for (n in this.indices) {
        stmp = Integer.toHexString(this[n].toInt() and 0XFF)

        if (stmp.length == 1) {
            hs.append("0")
        }
        hs.append(stmp)
    }

    return hs.toString()
}
