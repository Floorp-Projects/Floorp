/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.text.method.LinkMovementMethod
import android.view.View
import androidx.core.text.HtmlCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_add_on_details.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.showInformationDialog
import mozilla.components.feature.addons.ui.translatedName
import mozilla.components.feature.addons.ui.translatedDescription
import mozilla.components.feature.addons.update.DefaultAddonUpdater.UpdateAttemptStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar
import java.text.DateFormat
import java.text.SimpleDateFormat
import java.util.Locale

/**
 * A fragment to show the details of an add-on.
 */
class AddonDetailsFragment : Fragment(R.layout.fragment_add_on_details) {
    private val updateAttemptStorage: UpdateAttemptStorage by lazy {
        UpdateAttemptStorage(requireContext())
    }

    private val args by navArgs<AddonDetailsFragmentArgs>()

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        bind(args.addon, view)
    }

    private fun bind(addon: Addon, view: View) {
        val title = addon.translatedName
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
        view.home_page_label.setOnClickListener {
            val intent =
                Intent(Intent.ACTION_VIEW).setData(Uri.parse(addon.siteUrl))
            startActivity(intent)
        }
    }

    private fun bindLastUpdated(addon: Addon, view: View) {
        view.last_updated_text.text = formatDate(addon.updatedAt)
    }

    private fun bindVersion(addon: Addon, view: View) {
        view.version_text.text =
            addon.installedState?.version?.ifEmpty { addon.version } ?: addon.version
        if (addon.isInstalled()) {
            view.version_text.setOnLongClickListener {
                showUpdaterDialog(addon)
                true
            }
        }
    }

    private fun showUpdaterDialog(addon: Addon) {
        lifecycleScope.launch(IO) {
            val updateAttempt = updateAttemptStorage.findUpdateAttemptBy(addon.id)
            updateAttempt?.let {
                withContext(Main) {
                    it.showInformationDialog(requireContext())
                }
            }
        }
    }

    private fun bindAuthors(addon: Addon, view: View) {
        view.author_text.text = addon.authors.joinToString { author ->
            author.name + " \n"
        }
    }

    private fun bindDetails(addon: Addon, view: View) {
        val detailsView = view.details
        val detailsText = addon.translatedDescription

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
