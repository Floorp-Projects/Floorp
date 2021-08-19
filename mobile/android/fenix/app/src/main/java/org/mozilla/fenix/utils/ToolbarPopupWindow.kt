/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import android.widget.PopupWindow
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import java.lang.ref.WeakReference
import mozilla.components.browser.state.selector.findCustomTab
import org.mozilla.fenix.databinding.BrowserToolbarPopupWindowBinding

object ToolbarPopupWindow {
    fun show(
        view: WeakReference<View>,
        customTabId: String? = null,
        handlePasteAndGo: (String) -> Unit,
        handlePaste: (String) -> Unit,
        copyVisible: Boolean = true
    ) {
        val context = view.get()?.context ?: return
        val clipboard = context.components.clipboardHandler
        if (!copyVisible && clipboard.text.isNullOrEmpty()) return

        val isCustomTabSession = customTabId != null

        val binding = BrowserToolbarPopupWindowBinding.inflate(LayoutInflater.from(context))
        val popupWindow = PopupWindow(
            binding.root,
            LinearLayout.LayoutParams.WRAP_CONTENT,
            context.resources.getDimensionPixelSize(R.dimen.context_menu_height),
            true
        )
        popupWindow.elevation =
            context.resources.getDimension(R.dimen.mozac_browser_menu_elevation)

        // This is a workaround for SDK<23 to allow popup dismissal on outside or back button press
        // See: https://github.com/mozilla-mobile/fenix/issues/10027
        popupWindow.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

        binding.copy.isVisible = copyVisible

        binding.paste.isVisible = !clipboard.text.isNullOrEmpty() && !isCustomTabSession
        binding.pasteAndGo.isVisible =
            !clipboard.text.isNullOrEmpty() && !isCustomTabSession

        binding.copy.setOnClickListener {
            popupWindow.dismiss()
            clipboard.text = getUrlForClipboard(
                it.context.components.core.store,
                customTabId
            )

            view.get()?.let {
                FenixSnackbar.make(
                    view = it,
                    duration = Snackbar.LENGTH_SHORT,
                    isDisplayedWithBrowserToolbar = true
                )
                    .setText(context.getString(R.string.browser_toolbar_url_copied_to_clipboard_snackbar))
                    .show()
            }
            context.components.analytics.metrics.track(Event.CopyUrlUsed)
        }

        binding.paste.setOnClickListener {
            popupWindow.dismiss()
            handlePaste(clipboard.text!!)
        }

        binding.pasteAndGo.setOnClickListener {
            popupWindow.dismiss()
            handlePasteAndGo(clipboard.text!!)
        }

        view.get()?.let {
            popupWindow.showAsDropDown(
                it,
                context.resources.getDimensionPixelSize(R.dimen.context_menu_x_offset),
                0,
                Gravity.START
            )
        }
    }

    @VisibleForTesting
    internal fun getUrlForClipboard(
        store: BrowserStore,
        customTabId: String? = null
    ): String? {
        return if (customTabId != null) {
            val customTab = store.state.findCustomTab(customTabId)
            customTab?.content?.url
        } else {
            val selectedTab = store.state.selectedTab
            selectedTab?.readerState?.activeUrl ?: selectedTab?.content?.url
        }
    }
}
