/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.utils

import android.annotation.SuppressLint
import android.content.Context
import android.util.Base64
import androidx.annotation.DrawableRes
import androidx.annotation.RawRes
import java.io.BufferedReader
import java.io.IOException
import java.io.InputStream
import java.io.InputStreamReader
import java.nio.charset.StandardCharsets

private const val MINIMUM_DRAWABLE_SIZE_BYTES = 8

// Base64 encodes 3 bytes at a time, make sure we have a multiple of 3 here
// I don't know what a sensible chunk size is, let's just go with 300b.
private const val BYTE_ARRAY_READ_SIZE = 3 * 100

/**
 * HtmlLoader for loading localized content.
 */
object HtmlLoader {
    /**
     * Load a given (html or css) resource file into a String. The input can contain tokens that will
     * be replaced with localised strings.
     *
     * @param substitutionTable A table of substitions, e.g. %shortMessage% -> "Error loading page..."
     * Can be null, in which case no substitutions will be made.
     * @return The file content, with all substitutions having being made.
     */
    fun loadResourceFile(
        context: Context,
        @RawRes resourceID: Int,
        substitutionTable: Map<String, String>,
    ): String {
        try {
            BufferedReader(
                InputStreamReader(
                    context.resources.openRawResource(resourceID),
                    StandardCharsets.UTF_8,
                ),
            ).use { fileReader ->
                return readFromResource(fileReader, substitutionTable)
            }
        } catch (e: IOException) {
            throw IllegalStateException("Unable to load error page data", e)
        }
    }

    /**
     * Reads from a given resource and returns a String
     */
    private fun readFromResource(
        fileReader: BufferedReader,
        substitutionTable: Map<String, String>,
    ): String {
        val outputBuffer = StringBuilder()
        var line = ""
        while (fileReader.readLine()?.let { line = it } != null) {
            for ((key, value) in substitutionTable) {
                line = line.replace(key, value)
            }
            outputBuffer.append(line)
        }
        return outputBuffer.toString()
    }

    private val pngHeader = byteArrayOf(-119, 80, 78, 71, 13, 10, 26, 10)

    /**
     * Loads a given DRAWABLE resource file into a String.
     */
    // We are copying the approach BitmapFactory.decodeResource(Resources, int, Options)
    // uses - you are explicitly allowed to open Drawables, but the method has a @RawRes
    // annotation (despite officially supporting Drawables).
    @SuppressLint("ResourceType")
    fun loadPngAsDataURI(
        context: Context,
        @DrawableRes resourceID: Int,
    ): String {
        val builder = StringBuilder()
        builder.append("data:image/png;base64,")

        try {
            context.resources.openRawResource(resourceID).use { pngInputStream ->
                return readPngInputStream(pngInputStream, builder)
            }
        } catch (e: IOException) {
            throw IllegalStateException("Unable to load png data")
        }
    }

    /**
     * Reads a PNG input stream and returns a String
     */
    private fun readPngInputStream(
        pngInputStream: InputStream,
        builder: StringBuilder,
    ): String {
        val data = ByteArray(BYTE_ARRAY_READ_SIZE)
        var bytesRead: Int
        var headerVerified = false
        while (pngInputStream.read(data).also { bytesRead = it } > 0) {
            // Sanity check: lets make sure this is still a png (i.e. make sure the build system
            // or Android haven't broken / change the image format).
            if (!headerVerified) {
                check(bytesRead >= MINIMUM_DRAWABLE_SIZE_BYTES) { "Loaded drawable is improbably small" }
                for (i in pngHeader.indices) {
                    check(data[i] == pngHeader[i]) { "Invalid png detected" }
                }
                headerVerified = true
            }
            builder.append(Base64.encodeToString(data, 0, bytesRead, 0))
        }
        return builder.toString()
    }
}
