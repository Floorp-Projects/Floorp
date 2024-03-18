/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.privacy.studies

import android.os.Bundle
import android.text.SpannableStringBuilder
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.URLSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.core.text.HtmlCompat
import androidx.core.text.getSpans
import androidx.lifecycle.ViewModelProvider
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentStudiesBinding
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.settings.BaseSettingsLikeFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.utils.SupportUtils
import kotlin.system.exitProcess

class StudiesFragment : BaseSettingsLikeFragment() {
    private var _binding: FragmentStudiesBinding? = null
    private val binding get() = _binding!!
    private lateinit var viewModel: StudiesViewModel

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentStudiesBinding.inflate(inflater, container, false)
        viewModel = ViewModelProvider(
            this,
        ).get(StudiesViewModel::class.java)
        setLearnMore()
        setStudiesSwitch()
        setRemoveStudyListener()
        setObservers()
        return binding.root
    }

    override fun onStart() {
        super.onStart()
        showToolbar(getString(R.string.preference_studies))
    }

    private fun setLearnMore() {
        val sumoUrl =
            SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.STUDIES)
        val description = getString(R.string.preference_studies_summary)
        val learnMore = getString(R.string.studies_learn_more)
        val rawText = "$description <a href=\"$sumoUrl\">$learnMore</a>"
        val text = HtmlCompat.fromHtml(rawText, HtmlCompat.FROM_HTML_MODE_COMPACT)

        val spannableStringBuilder = SpannableStringBuilder(text)
        val links = spannableStringBuilder.getSpans<URLSpan>()
        for (link in links) {
            addActionToLinks(spannableStringBuilder, link)
        }
        binding.studiesDescription.text = spannableStringBuilder
        binding.studiesDescription.movementMethod = LinkMovementMethod.getInstance()
    }

    private fun addActionToLinks(spannableStringBuilder: SpannableStringBuilder, link: URLSpan) {
        val start = spannableStringBuilder.getSpanStart(link)
        val end = spannableStringBuilder.getSpanEnd(link)
        val flags = spannableStringBuilder.getSpanFlags(link)
        val clickable: ClickableSpan = object : ClickableSpan() {
            override fun onClick(view: View) {
                view.setOnClickListener {
                    openLearnMore.invoke()
                }
            }
        }
        spannableStringBuilder.setSpan(clickable, start, end, flags)
        spannableStringBuilder.removeSpan(link)
    }

    private val openLearnMore = {
        val tabId = requireContext().components.tabsUseCases.addTab(
            url = SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.STUDIES),
            source = SessionState.Source.Internal.Menu,
            selectTab = true,
            private = true,
        )
        requireContext().components.appStore.dispatch(AppAction.OpenTab(tabId))
    }

    private fun setStudiesTitleByState(switchState: Boolean) {
        binding.studiesTitle.text = if (switchState) {
            getString(R.string.preference_state_on)
        } else {
            getString(R.string.preference_state_off)
        }
    }

    private fun setStudiesSwitch() {
        binding.studiesSwitch.setOnClickListener {
            val builder = AlertDialog.Builder(requireContext())
                .setPositiveButton(
                    R.string.action_ok,
                ) { dialog, _ ->
                    viewModel.setStudiesState(binding.studiesSwitch.isChecked)
                    dialog.dismiss()
                    quitTheApp()
                }
                .setNegativeButton(
                    R.string.action_cancel,
                ) { dialog, _ ->
                    binding.studiesSwitch.isChecked = !binding.studiesSwitch.isChecked
                    setStudiesTitleByState(binding.studiesSwitch.isChecked)
                    dialog.dismiss()
                }
                .setTitle(R.string.preference_studies)
                .setMessage(R.string.studies_restart_app)
                .setCancelable(false)
            val alertDialog: AlertDialog = builder.create()
            alertDialog.show()
        }
    }

    private fun quitTheApp() {
        exitProcess(0)
    }

    private fun setRemoveStudyListener() {
        binding.studiesList.studiesAdapter.removeStudyListener = { study ->
            viewModel.removeStudy(study)
        }
    }

    private fun setObservers() {
        viewModel.exposedStudies.observe(
            viewLifecycleOwner,
        ) { studies ->
            binding.studiesList.studiesAdapter.submitList(studies)
        }

        viewModel.studiesState.observe(
            viewLifecycleOwner,
        ) { state ->
            binding.studiesSwitch.isChecked = state
            setStudiesTitleByState(state)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
