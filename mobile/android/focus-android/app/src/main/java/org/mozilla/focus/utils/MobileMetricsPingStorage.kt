/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import android.util.AtomicFile
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException

class MobileMetricsPingStorage(
    private val context: Context,
    private val file: File = File("${context.cacheDir}/$STORAGE_FOLDER/$FILE_NAME")
) {
    private val atomicFile = AtomicFile(file)

    fun shouldStoreMetrics(): Boolean = !file.exists()
    fun clearStorage() { file.delete() }

    suspend fun save(json: JSONObject) {
        val stream = atomicFile.startWrite()
        try {
            stream.writer().use {
                it.append(json.toString())
            }

            atomicFile.finishWrite(stream)
        } catch (e: IOException) {
            atomicFile.failWrite(stream)
        }
    }

    suspend fun load(): JSONObject? {
        return try {
            JSONObject(String(atomicFile.readFully()))
        } catch (e: FileNotFoundException) {
            null
        } catch (e: JSONException) {
            null
        }
    }

    companion object {
        const val STORAGE_FOLDER = "mobile-metrics"
        const val FILE_NAME = "metrics.json"
    }
}
