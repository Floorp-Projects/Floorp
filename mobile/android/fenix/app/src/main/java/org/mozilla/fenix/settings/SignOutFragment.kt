/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.android.synthetic.main.fragment_sign_out.view.*
import kotlinx.coroutines.launch
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents

class SignOutFragment : BottomSheetDialogFragment() {
    private lateinit var accountManager: FxaAccountManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(DialogFragment.STYLE_NO_TITLE, R.style.FirefoxAccountsDialogStyle)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)

        dialog.setOnShowListener {
            val bottomSheet = dialog.findViewById<View>(
                com.google.android.material.R.id.design_bottom_sheet
            ) as? FrameLayout
            val behavior = BottomSheetBehavior.from(bottomSheet)
            behavior.state = BottomSheetBehavior.STATE_EXPANDED
        }

        return dialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        accountManager = requireComponents.backgroundServices.accountManager
        return inflater.inflate(R.layout.fragment_sign_out, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        view.signOutDisconnect.setOnClickListener {
            lifecycleScope.launch {
                accountManager.logoutAsync().await()
            }.invokeOnCompletion {
                if (!findNavController().popBackStack(R.id.settingsFragment, false)) {
                    dismiss()
                }
            }
        }

        view.signOutCancel.setOnClickListener {
            dismiss()
        }
    }
}
