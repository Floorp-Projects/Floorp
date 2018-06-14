/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.PopupWindow
import kotlinx.coroutines.experimental.launch
import mozilla.components.browser.domains.CustomDomains
import org.mozilla.focus.R
import org.mozilla.focus.telemetry.TelemetryWrapper

class AutocompleteQuickAddPopup(context: Context, url: String) : PopupWindow() {
    var onUrlAdded: ((Boolean) -> Unit)? = null

    init {
        val view = LayoutInflater.from(context).inflate(R.layout.autocomplete_quick_add_popup, null)
        contentView = view

        setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

        val button = view.findViewById<Button>(R.id.quick_add_autocomplete_button)

        var duplicateURL = false
        button.setOnClickListener {
            val job = launch {
                duplicateURL = CustomDomains.load(context).contains(url)

                if (duplicateURL) return@launch
                CustomDomains.add(context, url)

                TelemetryWrapper.saveAutocompleteDomainEvent(TelemetryWrapper.AutoCompleteEventSource.QUICK_ADD)
            }
            job.invokeOnCompletion { onUrlAdded?.invoke(!duplicateURL) }
        }

        isFocusable = true

        height = ViewGroup.LayoutParams.WRAP_CONTENT
        width = ViewGroup.LayoutParams.WRAP_CONTENT

        elevation = context.resources.getDimension(R.dimen.menu_elevation)
    }

    fun show(anchor: View) {
        super.showAsDropDown(anchor, X_POS_OFFSET, Y_POS_OFFSET)
    }

    companion object {
        private const val X_POS_OFFSET = -10
        private const val Y_POS_OFFSET = 6
    }
}
