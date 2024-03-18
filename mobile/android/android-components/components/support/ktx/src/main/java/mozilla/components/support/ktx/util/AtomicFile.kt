/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.util

import android.util.AtomicFile
import android.util.JsonReader
import android.util.JsonWriter
import org.json.JSONException
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStreamWriter

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
    return stream { writer ->
        writer.write(block())
    }
}

/**
 * Opens the [AtomicFile] for writing and provides a [JsonWriter] to [block] for writing JSON
 * directly to the file.
 *
 * At the end of [block] the writer will be flushed and the file closed.
 */
inline fun AtomicFile.streamJSON(block: JsonWriter.() -> Unit): Boolean {
    return stream { writer ->
        val jsonWriter = JsonWriter(writer)
        block(jsonWriter)
        jsonWriter.flush()
    }
}

/**
 * Opens the [AtomicFile] for reading and provides a [JsonReader] to [block] for reading JSON from
 * the file.
 */
inline fun <R> AtomicFile.readJSON(block: JsonReader.() -> R): R? {
    var reader: InputStream? = null

    return try {
        reader = openRead()

        val jsonReader = JsonReader(reader.bufferedReader())
        block(jsonReader)
    } catch (e: IOException) {
        null
    } finally {
        reader?.close()
    }
}

/**
 * Opens the [AtomicFile] for writing and provides an [OutputStreamWriter] to [block] for writing
 * directly to the file.
 *
 * At the end of [block] the writer will be flushed and the file closed.
 */
inline fun AtomicFile.stream(block: (OutputStreamWriter) -> Unit): Boolean {
    var outputStream: FileOutputStream? = null
    return try {
        outputStream = startWrite()

        outputStream.buffered().writer().apply {
            block(this)
            flush()
        }

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
