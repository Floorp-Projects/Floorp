/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import androidx.annotation.RequiresApi

import org.mozilla.focus.utils.UrlUtils

import mozilla.components.support.utils.SafeIntent

/**
 * Activity for receiving and processing an ACTION_PROCESS_TEXT intent.
 */
class TextActionActivity : Activity() {
    @RequiresApi(api = Build.VERSION_CODES.M)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = SafeIntent(intent)

        val searchTextCharSequence = intent.getCharSequenceExtra(Intent.EXTRA_PROCESS_TEXT)
        val searchText = searchTextCharSequence?.toString() ?: ""

        val searchUrl = UrlUtils.createSearchUrl(this, searchText)

        val searchIntent = Intent(this, IntentReceiverActivity::class.java)
        searchIntent.action = Intent.ACTION_VIEW
        searchIntent.putExtra(EXTRA_TEXT_SELECTION, true)
        searchIntent.data = Uri.parse(searchUrl)
        searchIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP

        startActivity(searchIntent)

        finish()
    }

    companion object {
        const val EXTRA_TEXT_SELECTION = "text_selection"
    }
}
