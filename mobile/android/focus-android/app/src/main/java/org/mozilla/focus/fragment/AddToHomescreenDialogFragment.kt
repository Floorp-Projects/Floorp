/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.os.Bundle
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.ImageView
import androidx.appcompat.app.AlertDialog
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.isVisible
import androidx.fragment.app.DialogFragment
import androidx.preference.PreferenceManager
import mozilla.components.browser.icons.IconRequest
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.AddToHomeScreen
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.shortcut.HomeScreen
import org.mozilla.focus.shortcut.IconGenerator

/**
 * Fragment displaying a dialog where a user can change the title for a homescreen shortcut
 */
class AddToHomescreenDialogFragment : DialogFragment() {

    @Suppress("LongMethod")
    override fun onCreateDialog(bundle: Bundle?): AlertDialog {
        AddToHomeScreen.dialogDisplayed.record(NoExtras())
        val url = requireArguments().getString(URL)!!
        val title = requireArguments().getString(TITLE)
        val blockingEnabled = requireArguments().getBoolean(BLOCKING_ENABLED)
        val requestDesktop = requireArguments().getBoolean(REQUEST_DESKTOP)

        val builder = AlertDialog.Builder(requireActivity(), R.style.DialogStyle)
        builder.setCancelable(true)
        val inflater = requireActivity().layoutInflater
        val dialogView = inflater.inflate(R.layout.dialog_add_to_homescreen2, null)
        builder.setView(dialogView)

        val iconView = dialogView.findViewById<ImageView>(R.id.homescreen_icon)
        requireContext().components.icons.loadIntoView(
            iconView,
            IconRequest(url, isPrivate = true),
        )

        val blockIcon = dialogView.findViewById<ImageView>(R.id.homescreen_dialog_block_icon)
        blockIcon.setImageResource(R.drawable.mozac_ic_shield_slash_24)
        val warning =
            dialogView.findViewById<ConstraintLayout>(R.id.homescreen_dialog_warning_layout)
        warning.isVisible = !blockingEnabled

        val editableTitle = dialogView.findViewById<EditText>(R.id.edit_title)

        if (!TextUtils.isEmpty(title)) {
            editableTitle.setText(title)
        }

        setButtons(dialogView, editableTitle, url, blockingEnabled, requestDesktop, title)

        return builder.create()
    }

    @Suppress("LongParameterList")
    private fun setButtons(
        parentView: View,
        editableTitle: EditText,
        iconUrl: String,
        blockingEnabled: Boolean,
        requestDesktop: Boolean,
        initialTitle: String?,
    ) {
        val addToHomescreenDialogCancelButton =
            parentView.findViewById<Button>(R.id.addtohomescreen_dialog_cancel)
        val addToHomescreenDialogConfirmButton =
            parentView.findViewById<Button>(R.id.addtohomescreen_dialog_add)

        addToHomescreenDialogCancelButton.setOnClickListener {
            AddToHomeScreen.cancelButtonTapped.record(NoExtras())

            dismiss()
        }

        addToHomescreenDialogConfirmButton.setOnClickListener {
            HomeScreen.installShortCut(
                requireContext(),
                IconGenerator.generateLauncherIcon(requireContext(), iconUrl),
                iconUrl,
                editableTitle.text.toString().trim { it <= ' ' },
                blockingEnabled,
                requestDesktop,
            )

            val hasEditedTitle = initialTitle != editableTitle.text.toString().trim { it <= ' ' }
            AddToHomeScreen.addButtonTapped.record(
                AddToHomeScreen.AddButtonTappedExtra(
                    hasEditedTitle = hasEditedTitle,
                ),
            )

            PreferenceManager.getDefaultSharedPreferences(requireContext()).edit()
                .putBoolean(
                    requireContext().getString(R.string.has_added_to_home_screen),
                    true,
                ).apply()
            dismiss()
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        dialog?.window?.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE)
        return super.onCreateView(inflater, container, savedInstanceState)
    }

    companion object {
        const val FRAGMENT_TAG = "add-to-homescreen-prompt-dialog"
        private const val URL = "url"
        private const val TITLE = "title"
        private const val BLOCKING_ENABLED = "blocking_enabled"
        private const val REQUEST_DESKTOP = "request_desktop"

        fun newInstance(
            url: String,
            title: String,
            blockingEnabled: Boolean,
            requestDesktop: Boolean,
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
