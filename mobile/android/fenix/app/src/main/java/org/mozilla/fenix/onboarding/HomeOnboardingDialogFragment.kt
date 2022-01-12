/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentOnboardingHomeDialogBinding
import org.mozilla.fenix.ext.settings

/**
 * Dialog displayed once when one or multiples of these sections are shown in the home screen
 * recentTabs,recentBookmarks,historyMetadata or pocketArticles.
 */
class HomeOnboardingDialogFragment : DialogFragment() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.HomeOnboardingDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_onboarding_home_dialog, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val binding = FragmentOnboardingHomeDialogBinding.bind(view)

        val appName = requireContext().getString(R.string.app_name)
        binding.welcomeTitle.text =
            requireContext().getString(R.string.onboarding_home_screen_title_3, appName)
        binding.homeTitle.text = requireContext().getString(
            R.string.onboarding_home_screen_section_home_title_3,
            appName
        )

        binding.finishButton.setOnClickListener {
            context?.settings()?.let { settings ->
                settings.hasShownHomeOnboardingDialog = true
            }
            dismiss()
        }
    }
}
