/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

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
import androidx.core.view.children
import androidx.fragment.app.Fragment
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.SettingsHttpsOnlyBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings

/**
 * Lets the user customize HTTPS-only mode.
 */
class HttpsOnlyFragment : Fragment() {
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val binding = SettingsHttpsOnlyBinding.inflate(inflater)

        val summary = requireContext().getString(R.string.preferences_https_only_summary)
        val learnMore = requireContext().getString(R.string.preferences_http_only_learn_more)

        binding.httpsOnlySummary.run {
            text = combineTextWithLink(summary, learnMore).apply {
                setActionToUrlClick(this)
            }
            movementMethod = LinkMovementMethod.getInstance()
        }

        binding.httpsOnlySwitch.run {
            isChecked = context.settings().shouldUseHttpsOnly
            setHttpsModes(binding, isChecked)

            setOnCheckedChangeListener { _, isHttpsOnlyEnabled ->
                context.settings().shouldUseHttpsOnly = isHttpsOnlyEnabled
                setHttpsModes(binding, isHttpsOnlyEnabled)
                updateEngineHttpsOnlyMode()
            }
        }

        // Since the http-only modes are in a RadioGroup we only need one listener to know of all their changes.
        binding.httpsOnlyAllTabs.setOnCheckedChangeListener { _, _ ->
            updateEngineHttpsOnlyMode()
        }

        return binding.root
    }

    private fun setHttpsModes(binding: SettingsHttpsOnlyBinding, isHttpsOnlyEnabled: Boolean) {
        if (!isHttpsOnlyEnabled) {
            binding.httpsOnlyModes.apply {
                clearCheck()
                children.forEach { it.isEnabled = false }
            }
        } else {
            binding.httpsOnlyModes.children.forEach { it.isEnabled = true }
        }
    }

    private fun updateEngineHttpsOnlyMode() {
        requireContext().components.core.engine.settings.httpsOnlyMode =
            requireContext().settings().getHttpsOnlyMode()
    }

    private fun combineTextWithLink(
        text: String,
        linkTitle: String,
    ): SpannableStringBuilder {
        val rawTextWithLink = HtmlCompat.fromHtml(
            "$text <a href=\"\">$linkTitle</a>",
            HtmlCompat.FROM_HTML_MODE_COMPACT,
        )

        return SpannableStringBuilder(rawTextWithLink)
    }

    private fun setActionToUrlClick(
        spannableStringBuilder: SpannableStringBuilder,
    ) {
        val link = spannableStringBuilder.getSpans<URLSpan>()[0]
        val linkStart = spannableStringBuilder.getSpanStart(link)
        val linkEnd = spannableStringBuilder.getSpanEnd(link)
        val linkFlags = spannableStringBuilder.getSpanFlags(link)
        val linkClickListener: ClickableSpan = object : ClickableSpan() {
            override fun onClick(view: View) {
                view.setOnClickListener {
                    (activity as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(
                            SupportUtils.SumoTopic.HTTPS_ONLY_MODE,
                        ),
                        newTab = true,
                        from = BrowserDirection.FromHttpsOnlyMode,
                    )
                }
            }
        }
        spannableStringBuilder.setSpan(linkClickListener, linkStart, linkEnd, linkFlags)
        spannableStringBuilder.removeSpan(link)
    }
}
