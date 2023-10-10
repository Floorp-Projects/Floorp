/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.net.Uri
import android.text.SpannableStringBuilder
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.URLSpan
import android.view.View
import androidx.core.net.toUri
import androidx.core.text.HtmlCompat
import androidx.core.text.getSpans
import androidx.core.view.isVisible
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.translateDescription
import mozilla.components.feature.addons.ui.updatedAtDate
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentAddOnDetailsBinding
import java.text.DateFormat
import java.text.NumberFormat
import java.util.Locale

interface AddonDetailsInteractor {

    /**
     * Open the given URL in the browser.
     */
    fun openWebsite(url: Uri)

    /**
     * Display the updater dialog.
     */
    fun showUpdaterDialog(addon: Addon)
}

/**
 * Shows the details of an add-on.
 */
class AddonDetailsBindingDelegate(
    private val binding: FragmentAddOnDetailsBinding,
    private val interactor: AddonDetailsInteractor,
) {

    private val dateFormatter = DateFormat.getDateInstance()
    private val numberFormatter = NumberFormat.getNumberInstance(Locale.getDefault())

    fun bind(addon: Addon) {
        bindDetails(addon)
        bindAuthor(addon)
        bindVersion(addon)
        bindLastUpdated(addon)
        bindWebsite(addon)
        bindRating(addon)
    }

    private fun bindRating(addon: Addon) {
        addon.rating?.let { rating ->
            val resources = binding.root.resources
            val ratingContentDescription = resources.getString(R.string.mozac_feature_addons_rating_content_description)
            binding.ratingView.contentDescription = String.format(ratingContentDescription, rating.average)
            binding.ratingView.rating = rating.average

            binding.reviewCount.text = numberFormatter.format(rating.reviews)

            if (addon.ratingUrl.isNotBlank()) {
                binding.reviewCount.setTextColor(binding.root.context.getColorFromAttr(R.attr.textAccent))
                binding.reviewCount.setOnClickListener {
                    interactor.openWebsite(addon.ratingUrl.toUri())
                }
            }
        }
    }

    private fun bindWebsite(addon: Addon) {
        if (addon.siteUrl.isBlank()) {
            binding.homePageLabel.isVisible = false
            binding.homePageDivider.isVisible = false
            return
        }

        binding.homePageLabel.setOnClickListener {
            interactor.openWebsite(addon.siteUrl.toUri())
        }
    }

    private fun bindLastUpdated(addon: Addon) {
        if (addon.updatedAt.isBlank()) {
            binding.lastUpdatedLabel.isVisible = false
            binding.lastUpdatedText.isVisible = false
            binding.lastUpdatedDivider.isVisible = false
            return
        }

        binding.lastUpdatedText.text = dateFormatter.format(addon.updatedAtDate)
    }

    private fun bindVersion(addon: Addon) {
        var version = addon.installedState?.version
        if (version.isNullOrEmpty()) {
            version = addon.version
        }
        binding.versionText.text = version

        if (addon.isInstalled()) {
            binding.versionText.setOnLongClickListener {
                interactor.showUpdaterDialog(addon)
                true
            }
        } else {
            binding.versionText.setOnLongClickListener(null)
        }
    }

    private fun bindAuthor(addon: Addon) {
        val author = addon.author
        if (author == null || author.name.isBlank()) {
            binding.authorLabel.isVisible = false
            binding.authorText.isVisible = false
            binding.authorDivider.isVisible = false
            return
        }

        binding.authorText.text = author.name

        if (author.url.isNotBlank()) {
            binding.authorText.setTextColor(binding.root.context.getColorFromAttr(R.attr.textAccent))
            binding.authorText.setOnClickListener {
                interactor.openWebsite(author.url.toUri())
            }
        }
    }

    private fun bindDetails(addon: Addon) {
        val detailsText = addon.translateDescription(binding.root.context)

        val parsedText = detailsText.replace("\n", "<br/>")
        val text = HtmlCompat.fromHtml(parsedText, HtmlCompat.FROM_HTML_MODE_COMPACT)

        val spannableStringBuilder = SpannableStringBuilder(text)
        val links = spannableStringBuilder.getSpans<URLSpan>()
        for (link in links) {
            addActionToLinks(spannableStringBuilder, link)
        }
        binding.details.text = spannableStringBuilder
        binding.details.movementMethod = LinkMovementMethod.getInstance()
    }

    private fun addActionToLinks(
        spannableStringBuilder: SpannableStringBuilder,
        link: URLSpan,
    ) {
        val start = spannableStringBuilder.getSpanStart(link)
        val end = spannableStringBuilder.getSpanEnd(link)
        val flags = spannableStringBuilder.getSpanFlags(link)
        val clickable: ClickableSpan = object : ClickableSpan() {
            override fun onClick(view: View) {
                view.setOnClickListener {
                    interactor.openWebsite(link.url.toUri())
                }
            }
        }
        spannableStringBuilder.setSpan(clickable, start, end, flags)
        spannableStringBuilder.removeSpan(link)
    }
}
