/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.net.Uri
import android.webkit.MimeTypeMap
import java.io.ByteArrayOutputStream
import java.io.UnsupportedEncodingException
import java.util.regex.Pattern

object DownloadUtils {

    /**
     * Format as defined in RFC 2616 and RFC 5987
     * Both inline and attachment types are supported.
     */
    private val contentDispositionPattern = Pattern.compile("(inline|attachment)\\s*;" +
            "\\s*filename\\s*=\\s*(\"((?:\\\\.|[^\"\\\\])*)\"|[^;]*)\\s*" +
            "(?:;\\s*filename\\*\\s*=\\s*(utf-8|iso-8859-1)'[^']*'(\\S*))?",
            Pattern.CASE_INSENSITIVE)

    /**
     * Keys for the capture groups inside contentDispositionPattern
     */
    private const val ENCODED_FILE_NAME_GROUP = 5
    private const val ENCODING_GROUP = 4
    private const val QUOTED_FILE_NAME_GROUP = 3
    private const val UNQUOTED_FILE_NAME = 2

    /**
     * Definition as per RFC 5987, section 3.2.1. (value-chars)
     */
    private val encodedSymbolPattern = Pattern.compile("%[0-9a-f]{2}|[0-9a-z!#$&+-.^_`|~]", Pattern.CASE_INSENSITIVE)

    /**
     * Guess the name of the file that should be downloaded.
     *
     * This method is largely identical to [android.webkit.URLUtil.guessFileName]
     * which unfortunately does not implement RFC 5987.
     */
    @JvmStatic
    fun guessFileName(contentDisposition: String?, url: String?, mimeType: String?): String {
        val filename = extractFileNameFromUrl(contentDisposition, url)

        // Split filename between base and extension
        // Add an extension if filename does not have one
        return if (filename.contains('.')) {
            changeExtension(filename, mimeType)
        } else {
            filename + createExtension(mimeType)
        }
    }

    private fun extractFileNameFromUrl(contentDisposition: String?, url: String?): String {
        var filename: String? = null

        // Extract file name from content disposition header field
        if (contentDisposition != null) {
            filename = parseContentDisposition(contentDisposition)?.substringAfterLast('/')
        }

        // If all the other http-related approaches failed, use the plain uri
        if (filename == null) {
            // If there is a query string strip it, same as desktop browsers
            val decodedUrl: String? = Uri.decode(url)?.substringBefore('?')
            if (decodedUrl?.endsWith('/') == false) {
                filename = decodedUrl.substringAfterLast('/')
            }
        }

        // Finally, if couldn't get filename from URI, get a generic filename
        if (filename == null) {
            filename = "downloadfile"
        }

        return filename
    }

    private fun parseContentDisposition(contentDisposition: String): String? {
        try {
            val m = contentDispositionPattern.matcher(contentDisposition)

            if (m.find()) {
                // If escaped string is found, decode it using the given encoding.
                val encodedFileName = m.group(ENCODED_FILE_NAME_GROUP)
                val encoding = m.group(ENCODING_GROUP)

                if (encodedFileName != null && encoding != null) {
                    return decodeHeaderField(encodedFileName, encoding)
                }

                // Return quoted string if available and replace escaped characters.
                val quotedFileName = m.group(QUOTED_FILE_NAME_GROUP)

                return quotedFileName?.replace("\\\\(.)".toRegex(), "$1")
                    ?: m.group(UNQUOTED_FILE_NAME)

                // Otherwise try to extract the unquoted file name
            }
        } catch (ex: IllegalStateException) {
            // This function is defined as returning null when it can't parse the header
        } catch (ex: UnsupportedEncodingException) {
            // Do nothing
        }

        return null
    }

    @Throws(UnsupportedEncodingException::class)
    private fun decodeHeaderField(field: String, encoding: String): String {
        val m = encodedSymbolPattern.matcher(field)
        val stream = ByteArrayOutputStream()

        while (m.find()) {
            val symbol = m.group()

            if (symbol.startsWith("%")) {
                stream.write(symbol.substring(1).toInt(radix = 16))
            } else {
                stream.write(symbol[0].toInt())
            }
        }

        return stream.toString(encoding)
    }

    /**
     * Compare the filename extension with the mime type and change it if necessary.
     */
    private fun changeExtension(filename: String, mimeType: String?): String {
        var extension: String? = null
        val dotIndex = filename.lastIndexOf('.')

        if (mimeType != null) {
            val mimeTypeMap = MimeTypeMap.getSingleton()
            // Compare the last segment of the extension against the mime type.
            // If there's a mismatch, discard the entire extension.
            val typeFromExt = mimeTypeMap.getMimeTypeFromExtension(filename.substringAfterLast('.'))
            if (typeFromExt?.equals(mimeType, ignoreCase = true) != false) {
                extension = mimeTypeMap.getExtensionFromMimeType(mimeType)?.let { ".$it" }
                // Check if the extension needs to be changed
                if (extension != null && filename.endsWith(extension, ignoreCase = true)) {
                    return filename
                }
            }
        }

        return if (extension != null) {
            filename.substring(0, dotIndex) + extension
        } else {
            filename
        }
    }

    /**
     * Guess the extension for a file using the mime type.
     */
    private fun createExtension(mimeType: String?): String {
        var extension: String? = null

        if (mimeType != null) {
            extension = MimeTypeMap.getSingleton().getExtensionFromMimeType(mimeType)?.let { ".$it" }
        }
        if (extension == null) {
            extension = if (mimeType?.startsWith("text/", ignoreCase = true) == true) {
                if (mimeType.equals("text/html", ignoreCase = true)) {
                    ".html"
                } else {
                    ".txt"
                }
            } else {
                // If there's no mime type assume binary data
                ".bin"
            }
        }

        return extension
    }
}
