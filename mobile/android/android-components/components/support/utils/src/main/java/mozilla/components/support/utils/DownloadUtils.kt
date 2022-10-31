/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.net.Uri
import android.os.Environment
import android.webkit.MimeTypeMap
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.UnsupportedEncodingException
import java.util.regex.Pattern

object DownloadUtils {

    /**
     * This is the regular expression to match the content disposition type segment.
     *
     * A content disposition header can start either with inline or attachment followed by comma;
     *  For example: attachment; filename="filename.jpg" or inline; filename="filename.jpg"
     * (inline|attachment)\\s*; -> Match either inline or attachment, followed by zero o more
     * optional whitespaces characters followed by a comma.
     *
     */
    private const val contentDispositionType = "(inline|attachment)\\s*;"

    /**
     * This is the regular expression to match filename* parameter segment.
     *
     * A content disposition header could have an optional filename* parameter,
     * the difference between this parameter and the filename is that this uses
     * the encoding defined in RFC 5987.
     *
     * Some examples:
     *  filename*=utf-8''success.html
     *  filename*=iso-8859-1'en'file%27%20%27name.jpg
     *  filename*=utf-8'en'filename.jpg
     *
     * For matching this section we use:
     * \\s*filename\\s*=\\s*= -> Zero or more optional whitespaces characters
     * followed by filename followed by any zero or more whitespaces characters and the equal sign;
     *
     * (utf-8|iso-8859-1)-> Either utf-8 or iso-8859-1 encoding types.
     *
     * '[^']*'-> Zero or more characters that are inside of single quotes '' that are not single
     * quote.
     *
     * (\S*) -> Zero or more characters that are not whitespaces. In this group,
     * it's where we are going to have the filename.
     *
     */
    private const val contentDispositionFileNameAsterisk =
        "\\s*filename\\*\\s*=\\s*(utf-8|iso-8859-1)'[^']*'([^;\\s]*)"

    /**
     * Format as defined in RFC 2616 and RFC 5987
     * Both inline and attachment types are supported.
     * More details can be found
     * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Disposition
     *
     * The first segment is the [contentDispositionType], there you can find the documentation,
     * Next, it's the filename segment, where we have a filename="filename.ext"
     * For example, all of these could be possible in this section:
     * filename="filename.jpg"
     * filename="file\"name.jpg"
     * filename="file\\name.jpg"
     * filename="file\\\"name.jpg"
     * filename=filename.jpg
     *
     * For matching this section we use:
     * \\s*filename\\s*=\\s*= -> Zero or more whitespaces followed by filename followed
     *  by zero or more whitespaces and the equal sign.
     *
     * As we want to extract the the content of filename="THIS", we use:
     *
     * \\s* -> Zero or more whitespaces
     *
     *  (\"((?:\\\\.|[^|"\\\\])*)\" -> A quotation mark, optional : or \\ or any character,
     *  and any non quotation mark or \\\\ zero or more times.
     *
     *  For example: filename="file\\name.jpg", filename="file\"name.jpg" and filename="file\\\"name.jpg"
     *
     * We don't want to match after ; appears, For example filename="filename.jpg"; foo
     * we only want to match before the semicolon, so we use. |[^;]*)
     *
     * \\s* ->  Zero or more whitespaces.
     *
     *  For supporting cases, where we have both filename and filename*, we use:
     * "(?:;$contentDispositionFileNameAsterisk)?"
     *
     * Some examples:
     *
     * attachment; filename="_.jpg"; filename*=iso-8859-1'en'file%27%20%27name.jpg
     * attachment; filename="_.jpg"; filename*=iso-8859-1'en'file%27%20%27name.jpg
     */
    private val contentDispositionPattern = Pattern.compile(
        contentDispositionType +
            "\\s*filename\\s*=\\s*(\"((?:\\\\.|[^\"\\\\])*)\"|[^;]*)\\s*" +
            "(?:;$contentDispositionFileNameAsterisk)?",
        Pattern.CASE_INSENSITIVE,
    )

    /**
     * This is an alternative content disposition pattern where only filename* is available
     */
    private val fileNameAsteriskContentDispositionPattern = Pattern.compile(
        contentDispositionType +
            contentDispositionFileNameAsterisk,
        Pattern.CASE_INSENSITIVE,
    )

    /**
     * Keys for the capture groups inside contentDispositionPattern
     */
    private const val ENCODED_FILE_NAME_GROUP = 5
    private const val ENCODING_GROUP = 4
    private const val QUOTED_FILE_NAME_GROUP = 3
    private const val UNQUOTED_FILE_NAME = 2

    /**
     * Belongs to the [fileNameAsteriskContentDispositionPattern]
     */
    private const val ALTERNATIVE_FILE_NAME_GROUP = 3
    private const val ALTERNATIVE_ENCODING_GROUP = 2

    /**
     * Definition as per RFC 5987, section 3.2.1. (value-chars)
     */
    private val encodedSymbolPattern = Pattern.compile("%[0-9a-f]{2}|[0-9a-z!#$&+-.^_`|~]", Pattern.CASE_INSENSITIVE)

    /**
     * Keep aligned with desktop generic content types:
     * https://searchfox.org/mozilla-central/source/browser/components/downloads/DownloadsCommon.jsm#208
     */
    private val GENERIC_CONTENT_TYPES = arrayOf(
        "application/octet-stream",
        "binary/octet-stream",
        "application/unknown",
    )

    /**
     * Maximum number of characters for the title length.
     *
     * Android OS is Linux-based and therefore would have the limitations of the linux filesystem
     * it uses under the hood. To the best of our knowledge, Android only supports EXT3, EXT4,
     * exFAT, and EROFS filesystems. From these three, the maximum file name length is 255.
     *
     * @see <a href="https://en.wikipedia.org/wiki/Comparison_of_file_systems#Limits"/>
     */
    private const val MAX_FILE_NAME_LENGTH = 255

    /**
     * The HTTP response code for a successful request.
     */
    const val RESPONSE_CODE_SUCCESS = 200

    /**
     * Guess the name of the file that should be downloaded.
     *
     * This method is largely identical to [android.webkit.URLUtil.guessFileName]
     * which unfortunately does not implement RFC 5987.
     */

    @Suppress("Deprecation")
    @JvmStatic
    fun guessFileName(
        contentDisposition: String?,
        destinationDirectory: String?,
        url: String?,
        mimeType: String?,
    ): String {
        // Split fileName between base and extension
        // Add an extension if filename does not have one
        val extractedFileName = extractFileNameFromUrl(contentDisposition, url)
        val sanitizedMimeType = sanitizeMimeType(mimeType)

        val fileName = if (extractedFileName.contains('.')) {
            if (GENERIC_CONTENT_TYPES.contains(mimeType)) {
                extractedFileName
            } else {
                changeExtension(extractedFileName, sanitizedMimeType)
            }
        } else {
            extractedFileName + createExtension(sanitizedMimeType)
        }

        return destinationDirectory?.let {
            uniqueFileName(Environment.getExternalStoragePublicDirectory(destinationDirectory), fileName)
        } ?: fileName
    }

    // Some site add extra information after the mimetype, for example 'application/pdf; qs=0.001'
    // we just want to extract the mimeType and ignore the rest.
    fun sanitizeMimeType(mimeType: String?): String? {
        return (
            if (mimeType != null) {
                if (mimeType.contains(";")) {
                    mimeType.substringBefore(";")
                } else {
                    mimeType
                }
            } else {
                null
            }
            )?.trim()
    }

    /**
     * Checks if the file exists so as not to overwrite one already in the destination directory
     */
    fun uniqueFileName(directory: File, fileName: String): String {
        var fileExtension = ".${fileName.substringAfterLast(".")}"

        // Check if an extension was found or not
        if (fileExtension == ".$fileName") { fileExtension = "" }

        val baseFileName = fileName.replace(fileExtension, "")

        var potentialFileName = File(directory, fileName)
        var copyVersionNumber = 1

        while (potentialFileName.exists()) {
            potentialFileName = File(directory, "$baseFileName($copyVersionNumber)$fileExtension")
            copyVersionNumber += 1
        }

        return potentialFileName.name
    }

    /**
     * Create a Content Disposition formatted string with the receiver used as the filename and
     * file extension set as PDF.
     *
     * This is primarily useful for connecting the "Save to PDF" feature response to downloads.
     */
    fun makePdfContentDisposition(filename: String): String {
        return filename
            .take(MAX_FILE_NAME_LENGTH)
            .run {
                "attachment; filename=$this.pdf;"
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
        return try {
            parseContentDispositionWithFileName(contentDisposition)
                ?: parseContentDispositionWithFileNameAsterisk(contentDisposition)
        } catch (ex: IllegalStateException) {
            // This function is defined as returning null when it can't parse the header
            null
        } catch (ex: UnsupportedEncodingException) {
            // Do nothing
            null
        }
    }

    private fun parseContentDispositionWithFileName(contentDisposition: String): String? {
        val m = contentDispositionPattern.matcher(contentDisposition)
        return if (m.find()) {
            val encodedFileName = m.group(ENCODED_FILE_NAME_GROUP)
            val encoding = m.group(ENCODING_GROUP)
            if (encodedFileName != null && encoding != null) {
                decodeHeaderField(encodedFileName, encoding)
            } else {
                // Return quoted string if available and replace escaped characters.
                val quotedFileName = m.group(QUOTED_FILE_NAME_GROUP)
                quotedFileName?.replace("\\\\(.)".toRegex(), "$1")
                    ?: m.group(UNQUOTED_FILE_NAME)
            }
        } else {
            null
        }
    }

    private fun parseContentDispositionWithFileNameAsterisk(contentDisposition: String): String? {
        val alternative = fileNameAsteriskContentDispositionPattern.matcher(contentDisposition)

        return if (alternative.find()) {
            val encoding = alternative.group(ALTERNATIVE_ENCODING_GROUP) ?: return null
            val fileName = alternative.group(ALTERNATIVE_FILE_NAME_GROUP) ?: return null
            decodeHeaderField(fileName, encoding)
        } else {
            null
        }
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
                stream.write(symbol[0].code)
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
                // checking startsWith to ignoring encoding value such as "text/html; charset=utf-8"
                if (mimeType.startsWith("text/html", ignoreCase = true)) {
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
