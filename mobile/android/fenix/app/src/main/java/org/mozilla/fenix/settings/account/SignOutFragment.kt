/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import com.google.android.material.bottomsheet.BottomSheetDialog
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import androidx.appcompat.app.AppCompatDialogFragment
import kotlinx.coroutines.launch
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentSignOutBinding
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached

class SignOutFragment : AppCompatDialogFragment() {
    private lateinit var accountManager: FxaAccountManager

    private var _binding: FragmentSignOutBinding? = null
    private val binding get() = _binding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.BottomSheet)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        BottomSheetDialog(requireContext(), this.theme).apply {
            setOnShowListener {
                val bottomSheet =
                    findViewById<View>(com.google.android.material.R.id.design_bottom_sheet) as FrameLayout
                val behavior = BottomSheetBehavior.from(bottomSheet)
                behavior.state = BottomSheetBehavior.STATE_EXPANDED
            }
        }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        accountManager = requireComponents.backgroundServices.accountManager
        _binding = FragmentSignOutBinding.inflate(inflater, container, false)

        binding.signOutMessage.text = String.format(
            binding.root.context.getString(
                R.string.sign_out_confirmation_message_2
            ),
            binding.root.context.getString(R.string.app_name)
        )
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.signOutDisconnect.setOnClickListener {
            lifecycleScope.launch {
                requireComponents
                    .backgroundServices.accountAbnormalities.userRequestedLogout()
                accountManager.logout()
            }.invokeOnCompletion {
                runIfFragmentIsAttached {
                    if (this.isVisible) {
                        dismiss()
                    }
                    findNavController().popBackStack()
                }
            }
        }

        binding.signOutCancel.setOnClickListener {
            dismiss()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
