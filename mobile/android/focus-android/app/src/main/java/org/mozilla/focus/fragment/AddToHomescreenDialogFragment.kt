/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.os.Bundle
import android.preference.PreferenceManager
import android.support.v4.app.DialogFragment
import android.support.v7.app.AlertDialog
import android.text.TextUtils
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import org.mozilla.focus.R
import org.mozilla.focus.shortcut.HomeScreen
import org.mozilla.focus.shortcut.IconGenerator
import org.mozilla.focus.telemetry.TelemetryWrapper

/**
 * Fragment displaying a dialog where a user can change the title for a homescreen shortcut
 */
class AddToHomescreenDialogFragment : DialogFragment() {

    @Suppress("LongMethod")
    override fun onCreateDialog(bundle: Bundle?): AlertDialog {
        val url = arguments!!.getString(URL)
        val title = arguments!!.getString(TITLE)
        val blockingEnabled = arguments!!.getBoolean(BLOCKING_ENABLED)
        val requestDesktop = arguments!!.getBoolean(REQUEST_DESKTOP)

        val builder = AlertDialog.Builder(activity!!, R.style.DialogStyle)
        builder.setCancelable(true)
        builder.setTitle(requireActivity().getString(R.string.menu_add_to_home_screen))

        val inflater = requireActivity().layoutInflater
        val dialogView = inflater.inflate(R.layout.add_to_homescreen, null)
        builder.setView(dialogView)

        // For the dialog we display the Pre Oreo version of the icon because the Oreo+
        // adaptive launcher icon does not have a mask applied until we create the shortcut
        val iconBitmap = IconGenerator.generateLauncherIconPreOreo(
            context!!,
            IconGenerator.getRepresentativeCharacter(url)
        )
        val iconView = dialogView.findViewById<ImageView>(R.id.homescreen_icon)
        iconView.setImageBitmap(iconBitmap)

        val blockIcon = dialogView.findViewById<ImageView>(R.id.homescreen_dialog_block_icon)
        blockIcon.setImageResource(R.drawable.ic_tracking_protection_disabled)

        val addToHomescreenDialogCancelButton =
            dialogView.findViewById<Button>(R.id.addtohomescreen_dialog_cancel)
        val addToHomescreenDialogConfirmButton =
            dialogView.findViewById<Button>(R.id.addtohomescreen_dialog_add)

        val warning = dialogView.findViewById<LinearLayout>(R.id.homescreen_dialog_warning_layout)
        warning.visibility = if (blockingEnabled) View.GONE else View.VISIBLE

        val editableTitle = dialogView.findViewById<EditText>(R.id.edit_title)

        if (!TextUtils.isEmpty(title)) {
            editableTitle.setText(title)
            editableTitle.setSelection(title!!.length)
        }

        addToHomescreenDialogCancelButton.setOnClickListener {
            TelemetryWrapper.cancelAddToHomescreenShortcutEvent()
            dismiss()
        }

        addToHomescreenDialogConfirmButton.setOnClickListener {
            HomeScreen.installShortCut(
                context,
                IconGenerator.generateLauncherIcon(context!!, url),
                url,
                editableTitle.text.toString().trim { it <= ' ' },
                blockingEnabled,
                requestDesktop
            )
            TelemetryWrapper.addToHomescreenShortcutEvent()
            PreferenceManager.getDefaultSharedPreferences(context).edit()
                .putBoolean(
                    context!!.getString(R.string.has_added_to_home_screen),
                    true
                ).apply()
            dismiss()
        }
        return builder.create()
    }

    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)
        val dialog = dialog
        if (dialog != null) {
            val window = dialog.window
            window?.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE)
        }
    }

    companion object {
        val FRAGMENT_TAG = "add-to-homescreen-prompt-dialog"
        private val URL = "url"
        private val TITLE = "title"
        private val BLOCKING_ENABLED = "blocking_enabled"
        private val REQUEST_DESKTOP = "request_desktop"

        fun newInstance(
            url: String,
            title: String,
            blockingEnabled: Boolean,
            requestDesktop: Boolean
        ): AddToHomescreenDialogFragment {
            val frag = AddToHomescreenDialogFragment()
            val args = Bundle()
            args.putString(URL, url)
            args.putString(TITLE, title)
            args.putBoolean(BLOCKING_ENABLED, blockingEnabled)
            args.putBoolean(REQUEST_DESKTOP, requestDesktop)
            frag.arguments = args
            return frag
        }
    }
}
