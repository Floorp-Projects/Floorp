/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.content.Context
import android.os.Bundle
import android.text.InputType
import android.view.View
import android.view.WindowManager
import androidx.annotation.StringRes
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_saved_login_site_info.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar

/**
 * Displays saved login information for a single website.
 */
class SavedLoginSiteInfoFragment : Fragment(R.layout.fragment_saved_login_site_info) {

    private val args by navArgs<SavedLoginSiteInfoFragmentArgs>()

    override fun onPause() {
        // If we pause this fragment, we want to pop users back to reauth
        if (findNavController().currentDestination?.id != R.id.savedLoginsFragment) {
            activity?.window?.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
            findNavController().popBackStack(R.id.loginsFragment, false)
        }
        super.onPause()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        siteInfoText.text = args.savedLoginItem.url
        copySiteItem.setOnClickListener(
            CopyButtonListener(args.savedLoginItem.url, R.string.logins_site_copied)
        )

        usernameInfoText.text = args.savedLoginItem.userName
        copyUsernameItem.setOnClickListener(
            CopyButtonListener(args.savedLoginItem.userName, R.string.logins_username_copied)
        )

        passwordInfoText.inputType =
            InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
        passwordInfoText.text = args.savedLoginItem.password
        revealPasswordItem.setOnClickListener {
            togglePasswordReveal(it.context)
        }
        copyPasswordItem.setOnClickListener(
            CopyButtonListener(args.savedLoginItem.password, R.string.logins_password_copied)
        )
    }

    private fun togglePasswordReveal(context: Context) {
        if (passwordInfoText.inputType == InputType.TYPE_TEXT_VARIATION_PASSWORD or InputType.TYPE_CLASS_TEXT) {
            context.components.analytics.metrics.track(Event.ViewLoginPassword)
            passwordInfoText.inputType = InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            revealPasswordItem.setImageDrawable(getDrawable(context, R.drawable.mozac_ic_password_hide))
            revealPasswordItem.contentDescription = context.getString(R.string.saved_login_hide_password)
        } else {
            passwordInfoText.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
            revealPasswordItem.setImageDrawable(getDrawable(context, R.drawable.mozac_ic_password_reveal))
            revealPasswordItem.contentDescription = context.getString(R.string.saved_login_reveal_password)
        }
        // For the new type to take effect you need to reset the text
        passwordInfoText.text = args.savedLoginItem.password
    }

    override fun onResume() {
        super.onResume()
        activity?.window?.setFlags(
            WindowManager.LayoutParams.FLAG_SECURE,
            WindowManager.LayoutParams.FLAG_SECURE
        )
        showToolbar(args.savedLoginItem.url)
    }

    /**
     * Click listener for a textview's copy button.
     * @param value Value to be copied
     * @param snackbarText Text to display in snackbar after copying.
     */
    private inner class CopyButtonListener(
        private val value: String?,
        @StringRes private val snackbarText: Int
    ) : View.OnClickListener {
        override fun onClick(view: View) {
            val clipboard = view.context.components.clipboardHandler
            clipboard.text = value
            showCopiedSnackbar(view.context.getString(snackbarText))
            view.context.components.analytics.metrics.track(Event.CopyLogin)
        }

        private fun showCopiedSnackbar(copiedItem: String) {
            view?.let {
                FenixSnackbar.make(it, Snackbar.LENGTH_SHORT).setText(copiedItem).show()
            }
        }
    }
}
