/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Build
import android.os.PersistableBundle
import android.view.textclassifier.TextClassifier
import androidx.annotation.VisibleForTesting
import androidx.core.content.getSystemService
import mozilla.components.support.ktx.kotlin.MAX_URI_LENGTH
import mozilla.components.support.utils.SafeUrl
import mozilla.components.support.utils.WebURLFinder
import org.mozilla.fenix.perf.Performance.logger

private const val MIME_TYPE_TEXT_PLAIN = "text/plain"
private const val MIME_TYPE_TEXT_HTML = "text/html"
private const val MIME_TYPE_TEXT_URL = "text/x-moz-url"

/**
 * A clipboard utility class that allows copying and pasting links/text to & from the clipboard
 */
class ClipboardHandler(val context: Context) {
    private val clipboard = context.getSystemService<ClipboardManager>()!!

    /**
     * Provides access to the current content of the clipboard, be aware this is a sensitive
     * API as from Android 12 and above, accessing it will trigger a notification letting the user
     * know the app has accessed the clipboard, make sure when you call this API that users are
     * completely aware that we are accessing the clipboard.
     * See for more details https://github.com/mozilla-mobile/fenix/issues/22271.
     */
    var text: String?
        get() {
            if (clipboard.isPrimaryClipEmpty()) {
                return null
            }
            if (containsText()) {
                return firstSafePrimaryClipItemText
            }
            return null
        }
        set(value) {
            val clipData = ClipData.newPlainText("Text", value)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                clipData.apply {
                    description.extras = PersistableBundle().apply {
                        putBoolean("android.content.extra.IS_SENSITIVE", false)
                    }
                }
            }
            clipboard.setPrimaryClip(clipData)
        }

    /**
     * Provides access to the sensitive content of the clipboard, be aware this is a sensitive
     * API as from Android 12 and above, accessing it will trigger a notification letting the user
     * know the app has accessed the clipboard, make sure when you call this API that users are
     * completely aware that we are accessing the clipboard.
     * See for more details https://github.com/mozilla-mobile/fenix/issues/22271.
     *
     */
    var sensitiveText: String?
        get() {
            return text
        }
        set(value) {
            val clipData = ClipData.newPlainText("Text", value)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                clipData.apply {
                    description.extras = PersistableBundle().apply {
                        putBoolean("android.content.extra.IS_SENSITIVE", true)
                    }
                }
            }
            clipboard.setPrimaryClip(clipData)
        }

    /**
     * Returns a possible URL from the actual content of the clipboard, be aware this is a sensitive
     * API as from Android 12 and above, accessing it will trigger a notification letting the user
     * know the app has accessed the clipboard, make sure when you call this API that users are
     * completely aware that we are accessing the clipboard.
     * See for more details https://github.com/mozilla-mobile/fenix/issues/22271.
     */
    fun extractURL(): String? {
        return text?.let {
            if (it.length > MAX_URI_LENGTH) {
                return null
            }

            val finder = WebURLFinder(it)
            finder.bestWebURL()
        }
    }

    /**
     * Returns whether or not the clipboard data contains text.
     * We cannot rely on `isPrimaryClipEmpty()` since it triggers a clipboard access system notification.
     */
    fun containsText(): Boolean {
        return clipboard.isPrimaryClipHtmlText() ||
            clipboard.isPrimaryClipPlainText() ||
            clipboard.isPrimaryClipUrlText()
    }

    @Suppress("MagicNumber")
    internal fun containsURL(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val description = clipboard.primaryClipDescription
            // An IllegalStateException is thrown if the url is too long.
            val score =
                try {
                    description?.getConfidenceScore(TextClassifier.TYPE_URL) ?: 0F
                } catch (e: IllegalStateException) {
                    0F
                }
            score >= 0.7F
        } else {
            !extractURL().isNullOrEmpty()
        }
    }

    private fun ClipboardManager.isPrimaryClipPlainText() =
        primaryClipDescription?.hasMimeType(MIME_TYPE_TEXT_PLAIN) ?: false

    private fun ClipboardManager.isPrimaryClipHtmlText() =
        primaryClipDescription?.hasMimeType(MIME_TYPE_TEXT_HTML) ?: false

    private fun ClipboardManager.isPrimaryClipUrlText() =
        primaryClipDescription?.hasMimeType(MIME_TYPE_TEXT_URL) ?: false

    /**
     * Returns whether or not the clipboard has any clip data.
     * Reads the clip data, be aware this is a sensitive API as from Android 12 and above,
     * accessing it will trigger a notification letting the user know the app has accessed the clipboard,
     * make sure when you call this API that users are completely aware that we are accessing the clipboard.
     * See https://github.com/mozilla-mobile/fenix/issues/22271 for more details.
     */
    private fun ClipboardManager.isPrimaryClipEmpty() = primaryClip?.itemCount == 0

    /**
     * Returns a [ClipData.Item] from the Android clipboard.
     * @return a string representation of the first item on the clipboard, if
     * the clipboard currently has an item or null if it does not.
     *
     * Note: this can throw a [android.os.DeadSystemException] if the clipboard content is too large,
     * or various exceptions for certain vendors, due to modifications made to the Android clipboard code.
     */
    @Suppress("TooGenericExceptionCaught")
    private val ClipboardManager.firstPrimaryClipItem: ClipData.Item?
        get() = try {
            primaryClip?.getItemAt(0)
        } catch (exception: Exception) {
            logger.error("Fetching clipboard content failed with: $exception")
            null
        }

    @VisibleForTesting
    internal val firstSafePrimaryClipItemText: String?
        get() = SafeUrl.stripUnsafeUrlSchemes(context, clipboard.firstPrimaryClipItem?.text)
}
