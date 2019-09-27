/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.util

import android.util.AtomicFile
import org.json.JSONException
import java.io.FileOutputStream
import java.io.IOException

/**
 * Reads an [AtomicFile] and provides a deserialized version of its content.
 * @param block A function to be executed after the file is read and provides the content as
 * a [String]. It is expected that this function returns a deserialized version of the content
 * of the file.
 */
inline fun <T> AtomicFile.readAndDeserialize(block: (String) -> T): T? {
    return try {
        openRead().use {
            val text = it.bufferedReader().use { reader -> reader.readText() }
            block(text)
        }
    } catch (_: IOException) {
        null
    } catch (_: JSONException) {
        null
    }
}

/**
 * Writes an [AtomicFile] and indicates if the file was wrote.
 * @param block A function with provides the content of the file as a [String]
 * @return true if the file wrote otherwise false
 */
inline fun AtomicFile.writeString(block: () -> String): Boolean {
    var outputStream: FileOutputStream? = null
    return try {
        outputStream = startWrite()
        outputStream.write(block().toByteArray())
        finishWrite(outputStream)
        true
    } catch (_: IOException) {
        failWrite(outputStream)
        false
    } catch (_: JSONException) {
        failWrite(outputStream)
        false
    }
}
