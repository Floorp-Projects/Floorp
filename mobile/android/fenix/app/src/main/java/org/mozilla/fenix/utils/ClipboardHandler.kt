/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Build
import android.view.textclassifier.TextClassifier
import androidx.annotation.VisibleForTesting
import androidx.core.content.getSystemService
import mozilla.components.support.utils.SafeUrl
import mozilla.components.support.utils.WebURLFinder

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
            if (clipboard.isPrimaryClipPlainText() ||
                clipboard.isPrimaryClipHtmlText() ||
                clipboard.isPrimaryClipUrlText()
            ) {
                return firstSafePrimaryClipItemText
            }
            return null
        }
        set(value) {
            clipboard.setPrimaryClip(ClipData.newPlainText("Text", value))
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
            val finder = WebURLFinder(it)
            finder.bestWebURL()
        }
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

    private fun ClipboardManager.isPrimaryClipEmpty() = primaryClip?.itemCount == 0

    private val ClipboardManager.firstPrimaryClipItem: ClipData.Item?
        get() = primaryClip?.getItemAt(0)

    @VisibleForTesting
    internal val firstSafePrimaryClipItemText: String?
        get() = SafeUrl.stripUnsafeUrlSchemes(context, clipboard.firstPrimaryClipItem?.text)
}
