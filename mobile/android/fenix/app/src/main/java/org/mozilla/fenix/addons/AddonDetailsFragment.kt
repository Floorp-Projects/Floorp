/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.text.HtmlCompat
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_add_on_details.view.*
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.translate
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar
import java.text.DateFormat
import java.text.SimpleDateFormat
import java.util.Locale

/**
 * A fragment to show the details of an add-on.
 */
class AddonDetailsFragment : Fragment() {
    private val addon: Addon by lazy {
        AddonDetailsFragmentArgs.fromBundle(requireNotNull(arguments)).addon
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_add_on_details, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        bind(addon, view)
    }

    private fun bind(addon: Addon, view: View) {
        val title = addon.translatableName.translate()
        showToolbar(title)

        bindDetails(addon, view)
        bindAuthors(addon, view)
        bindVersion(addon, view)
        bindLastUpdated(addon, view)
        bindWebsite(addon, view)
        bindRating(addon, view)
    }

    private fun bindRating(addon: Addon, view: View) {
        addon.rating?.let {
            val ratingView = view.rating_view
            val userCountView = view.users_count

            val ratingContentDescription =
                getString(R.string.mozac_feature_addons_rating_content_description)
            ratingView.contentDescription = String.format(ratingContentDescription, it.average)
            ratingView.rating = it.average

            userCountView.text = getFormattedAmount(it.reviews)
        }
    }

    private fun bindWebsite(addon: Addon, view: View) {
        view.home_page_text.setOnClickListener {
            val intent =
                Intent(Intent.ACTION_VIEW).setData(Uri.parse(addon.siteUrl))
            startActivity(intent)
        }
    }

    private fun bindLastUpdated(addon: Addon, view: View) {
        view.last_updated_text.text = formatDate(addon.updatedAt)
    }

    private fun bindVersion(addon: Addon, view: View) {
        view.version_text.text = addon.version
    }

    private fun bindAuthors(addon: Addon, view: View) {
        view.author_text.text = addon.authors.joinToString { author ->
            author.name + " \n"
        }
    }

    private fun bindDetails(addon: Addon, view: View) {
        val detailsView = view.details
        val detailsText = addon.translatableDescription.translate()

        val parsedText = detailsText.replace("\n", "<br/>")
        val text = HtmlCompat.fromHtml(parsedText, HtmlCompat.FROM_HTML_MODE_COMPACT)

        detailsView.text = text
        detailsView.movementMethod = LinkMovementMethod.getInstance()
    }

    private fun formatDate(text: String): String {
        val formatter = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.getDefault())
        return DateFormat.getDateInstance().format(formatter.parse(text)!!)
    }
}
