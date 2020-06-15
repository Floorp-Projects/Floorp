/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.net.Uri
import android.text.method.LinkMovementMethod
import android.view.View
import androidx.core.net.toUri
import androidx.core.text.HtmlCompat
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.fragment_add_on_details.*
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.translatedDescription
import org.mozilla.fenix.R
import java.text.DateFormat
import java.text.NumberFormat
import java.text.SimpleDateFormat
import java.util.Locale

interface AddonDetailsInteractor {

    /**
     * Open the given addon siteUrl in the browser.
     */
    fun openWebsite(addonSiteUrl: Uri)

    /**
     * Display the updater dialog.
     */
    fun showUpdaterDialog(addon: Addon)
}

/**
 * Shows the details of an add-on.
 */
class AddonDetailsView(
    override val containerView: View,
    private val interactor: AddonDetailsInteractor
) : LayoutContainer {

    private val dateParser = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.getDefault())
    private val dateFormatter = DateFormat.getDateInstance()
    private val numberFormatter = NumberFormat.getNumberInstance(Locale.getDefault())

    fun bind(addon: Addon) {
        bindDetails(addon)
        bindAuthors(addon)
        bindVersion(addon)
        bindLastUpdated(addon)
        bindWebsite(addon)
        bindRating(addon)
    }

    private fun bindRating(addon: Addon) {
        addon.rating?.let { rating ->
            val resources = containerView.resources
            val ratingContentDescription =
                resources.getString(R.string.mozac_feature_addons_rating_content_description)
            rating_view.contentDescription = String.format(ratingContentDescription, rating.average)
            rating_view.rating = rating.average

            users_count.text = numberFormatter.format(rating.reviews)
        }
    }

    private fun bindWebsite(addon: Addon) {
        home_page_label.setOnClickListener {
            interactor.openWebsite(addon.siteUrl.toUri())
        }
    }

    private fun bindLastUpdated(addon: Addon) {
        last_updated_text.text = formatDate(addon.updatedAt)
    }

    private fun bindVersion(addon: Addon) {
        var version = addon.installedState?.version
        if (version.isNullOrEmpty()) {
            version = addon.version
        }
        version_text.text = version

        if (addon.isInstalled()) {
            version_text.setOnLongClickListener {
                interactor.showUpdaterDialog(addon)
                true
            }
        } else {
            version_text.setOnLongClickListener(null)
        }
    }

    private fun bindAuthors(addon: Addon) {
        author_text.text = addon.authors.joinToString { author -> author.name }.trim()
    }

    private fun bindDetails(addon: Addon) {
        val detailsText = addon.translatedDescription

        val parsedText = detailsText.replace("\n", "<br/>")
        val text = HtmlCompat.fromHtml(parsedText, HtmlCompat.FROM_HTML_MODE_COMPACT)

        details.text = text
        details.movementMethod = LinkMovementMethod.getInstance()
    }

    private fun formatDate(text: String): String {
        return dateFormatter.format(dateParser.parse(text)!!)
    }
}
