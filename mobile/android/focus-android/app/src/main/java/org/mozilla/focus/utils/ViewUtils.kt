/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.app.Activity
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.view.MenuItem
import android.view.View
import android.view.inputmethod.InputMethodManager
import androidx.annotation.StringRes
import com.google.android.material.snackbar.Snackbar
import org.mozilla.focus.ext.tryAsActivity
import java.lang.ref.WeakReference

object ViewUtils {

    private const val MENU_ITEM_ALPHA_ENABLED = 255
    private const val MENU_ITEM_ALPHA_DISABLED = 130

    /**
     * Runnable to show the keyboard for a specific view.
     */
    @Suppress("ReturnCount")
    private class ShowKeyboard(view: View?) : Runnable {
        companion object {
            private const val INTERVAL_MS = 100
            private const val MAX_TRIES = 10
        }

        private val viewReference: WeakReference<View?> = WeakReference(view)
        private val handler: Handler = Handler(Looper.getMainLooper())
        private var tries: Int = MAX_TRIES

        override fun run() {
            val myView = viewReference.get() ?: return
            val activity: Activity = myView.context?.tryAsActivity() ?: return
            val imm = activity.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager ?: return

            when {
                tries <= 0 -> return
                !myView.isFocusable -> return
                !myView.isFocusableInTouchMode -> return
                !myView.requestFocus() -> {
                    post()
                    return
                }
                !imm.isActive(myView) -> {
                    post()
                    return
                }
                !imm.showSoftInput(myView, InputMethodManager.SHOW_IMPLICIT) -> {
                    post()
                }
            }
        }

        fun post() {
            tries--
            handler.postDelayed(this, INTERVAL_MS.toLong())
        }
    }

    fun showKeyboard(view: View?) {
        val showKeyboard = ShowKeyboard(view)
        showKeyboard.post()
    }

    fun hideKeyboard(view: View?) {
        val imm = view?.context?.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager

        imm.hideSoftInputFromWindow(view.windowToken, 0)
    }

    /**
     * Create a custom FocusSnackbar.
     */
    fun showBrandedSnackbar(view: View?, @StringRes resId: Int, delayMillis: Int) {
        val context = view!!.context
        val snackbar = FocusSnackbar.make(view, Snackbar.LENGTH_LONG)
        snackbar.setText(context.getString(resId))

        view.postDelayed({ snackbar.show() }, delayMillis.toLong())
    }

    /**
     * Enable or disable a [MenuItem]
     * If the menu item is disabled it can not be clicked and the menu icon is semi-transparent
     *
     * @param menuItem the menu item to enable/disable
     * @param enabled true if the menu item should be enabled
     */
    fun setMenuItemEnabled(menuItem: MenuItem, enabled: Boolean) {
        menuItem.isEnabled = enabled
        val icon = menuItem.icon
        if (icon != null) {
            icon.mutate().alpha = if (enabled) MENU_ITEM_ALPHA_ENABLED else MENU_ITEM_ALPHA_DISABLED
        }
    }
}
