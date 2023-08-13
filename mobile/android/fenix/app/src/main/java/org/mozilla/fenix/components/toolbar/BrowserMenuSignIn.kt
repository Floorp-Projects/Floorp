/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.View
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.item.BrowserMenuImageText
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

/**
 * A menu item for displaying account information. The item computes the label on every bind call,
 * to provide each menu with the latest account manager state.
 *
 * @property textColorResource ID of color resource to tint the text.
 * @param imageResource ID of a drawable resource to be shown as icon.
 * @param listener Callback to be invoked when this menu item is clicked.
 */
class BrowserMenuSignIn(
    @ColorRes private val textColorResource: Int,
    @DrawableRes imageResource: Int = R.drawable.ic_signed_out,
    listener: () -> Unit = {},
) : BrowserMenuImageText(
    String(),
    imageResource,
    textColorResource = textColorResource,
    listener = listener,
) {
    override fun bind(menu: BrowserMenu, view: View) {
        super.bind(menu, view)
        val textView = view.findViewById<TextView>(R.id.text)
        textView.text = getLabel(textView.context)
        textView.setTextColor(ContextCompat.getColor(textView.context, textColorResource))
    }

    /**
     * Return the proper label for the sign in button.
     *
     * There are 3 states that the account state could be in:
     * 1) If the user is signed in and the account information is known and a display name is set
     * display the account display name else display the account email.
     * 2) The user is not signed in.
     * 3) Display an account info placeholder string if the user is signed in, but the account state
     * is unknown or being checked. Could by improved by using:
     * https://github.com/mozilla/application-services/issues/5328
     */
    @VisibleForTesting
    internal fun getLabel(context: Context): String = with(context) {
        val isSignedIn = components.settings.signedInFxaAccount
        val email = components.backgroundServices.syncStore.state.account?.email
        val displayName = components.backgroundServices.syncStore.state.account?.displayName

        if (isSignedIn) {
            displayName ?: email ?: resources.getString(R.string.browser_menu_account_settings)
        } else {
            resources.getString(R.string.sync_menu_sync_and_save_data)
        }
    }
}
