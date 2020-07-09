/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import androidx.core.content.getSystemService
import mozilla.components.support.utils.WebURLFinder

private const val MIME_TYPE_TEXT_PLAIN = "text/plain"
private const val MIME_TYPE_TEXT_HTML = "text/html"

/**
 * A clipboard utility class that allows copying and pasting links/text to & from the clipboard
 */
class ClipboardHandler(context: Context) {
    private val clipboard = context.getSystemService<ClipboardManager>()!!

    var text: String?
        get() {
            if (!clipboard.isPrimaryClipEmpty() &&
                (clipboard.isPrimaryClipPlainText() ||
                        clipboard.isPrimaryClipHtmlText())
            ) {
                return clipboard.firstPrimaryClipItem?.text.toString()
            }
            return null
        }
        set(value) {
            clipboard.setPrimaryClip(ClipData.newPlainText("Text", value))
        }

    val url: String?
        get() {
            return text?.let {
                val finder = WebURLFinder(it)
                finder.bestWebURL()
            }
        }

    private fun ClipboardManager.isPrimaryClipPlainText() =
        primaryClipDescription?.hasMimeType(MIME_TYPE_TEXT_PLAIN) ?: false

    private fun ClipboardManager.isPrimaryClipHtmlText() =
        primaryClipDescription?.hasMimeType(MIME_TYPE_TEXT_HTML) ?: false

    private fun ClipboardManager.isPrimaryClipEmpty() = primaryClip?.itemCount == 0

    private val ClipboardManager.firstPrimaryClipItem: ClipData.Item?
        get() = primaryClip?.getItemAt(0)
}
