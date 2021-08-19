/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shortcut

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.widget.addTextChangedListener
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.selectedTab
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentCreateShortcutBinding
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.requireComponents

class CreateShortcutFragment : DialogFragment() {
    private var _binding: FragmentCreateShortcutBinding? = null
    private val binding get() = _binding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.CreateShortcutDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentCreateShortcutBinding.inflate(inflater, container, false)

        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val tab = requireComponents.core.store.state.selectedTab

        if (tab == null) {
            dismiss()
        } else {
            requireComponents.core.icons.loadIntoView(binding.faviconImage, tab.content.url)

            binding.cancelButton.setOnClickListener { dismiss() }
            binding.addButton.setOnClickListener {
                val text = binding.shortcutText.text.toString().trim()
                requireActivity().lifecycleScope.launch {
                    requireComponents.useCases.webAppUseCases.addToHomescreen(text)
                }
                dismiss()
            }

            binding.shortcutText.addTextChangedListener {
                updateAddButtonEnabledState()
            }

            binding.shortcutText.setText(tab.content.title)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()

        _binding = null
    }

    private fun updateAddButtonEnabledState() {
        val text = binding.shortcutText.text
        binding.addButton.isEnabled = text.isNotBlank()
        binding.addButton.alpha = if (text.isNotBlank()) ENABLED_ALPHA else DISABLED_ALPHA
    }

    companion object {
        private const val ENABLED_ALPHA = 1.0f
        private const val DISABLED_ALPHA = 0.4f
    }
}
