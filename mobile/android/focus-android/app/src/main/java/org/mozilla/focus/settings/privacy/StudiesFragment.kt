/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.privacy

import android.os.Bundle
import android.text.SpannableStringBuilder
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.URLSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.text.HtmlCompat
import androidx.core.text.getSpans
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentStudiesBinding
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.settings.BaseSettingsLikeFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.utils.SupportUtils

class StudiesFragment : BaseSettingsLikeFragment() {
    private lateinit var binding: FragmentStudiesBinding

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        binding = FragmentStudiesBinding.inflate(inflater, container, false)
        setLearnMore()
        setStudiesState(requireContext().settings.isExperimentationEnabled)
        setStudiesSwitch()
        return binding.root
    }

    override fun onStart() {
        super.onStart()
        updateTitle(R.string.preference_studies)
    }

    private fun setLearnMore() {
        val sumoUrl =
            SupportUtils.getSumoURLForTopic(requireContext(), SupportUtils.SumoTopic.STUDIES)
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
            url = SupportUtils.getSumoURLForTopic(requireContext(), SupportUtils.SumoTopic.STUDIES),
            source = SessionState.Source.Internal.Menu,
            selectTab = true,
            private = true
        )
        requireContext().components.appStore.dispatch(AppAction.OpenTab(tabId))
    }

    private fun setStudiesState(switchState: Boolean) {
        binding.studiesTitle.text = if (switchState) {
            getString(R.string.preference_state_on)
        } else {
            getString(R.string.preference_state_off)
        }
    }

    private fun setStudiesSwitch() {
        binding.studiesSwitch.isChecked = requireContext().settings.isExperimentationEnabled
        binding.studiesSwitch.setOnCheckedChangeListener { _, isChecked ->
            setStudiesState(isChecked)
            requireContext().settings.isExperimentationEnabled = isChecked
        }
    }
}
