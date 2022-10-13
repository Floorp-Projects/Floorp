/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.text.method.LinkMovementMethod
import android.view.View
import android.widget.RatingBar
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.text.HtmlCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.showInformationDialog
import mozilla.components.feature.addons.ui.translateDescription
import mozilla.components.feature.addons.ui.translateName
import mozilla.components.feature.addons.update.DefaultAddonUpdater
import org.mozilla.samples.browser.R
import java.text.DateFormat
import java.text.SimpleDateFormat
import java.util.Locale

/**
 * An activity to show the details of an add-on.
 */
class AddonDetailsActivity : AppCompatActivity() {

    private val updateAttemptStorage: DefaultAddonUpdater.UpdateAttemptStorage by lazy {
        DefaultAddonUpdater.UpdateAttemptStorage(this)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_add_on_details)
        val addon = requireNotNull(intent.getParcelableExtra<Addon>("add_on"))
        bind(addon)
    }

    private fun bind(addon: Addon) {
        title = addon.translateName(this)

        bindDetails(addon)

        bindAuthors(addon)

        bindVersion(addon)

        bindLastUpdated(addon)

        bindWebsite(addon)

        bindRating(addon)
    }

    private fun bindRating(addon: Addon) {
        addon.rating?.let {
            val ratingView = findViewById<RatingBar>(R.id.rating_view)
            val userCountView = findViewById<TextView>(R.id.users_count)

            val ratingContentDescription = getString(R.string.mozac_feature_addons_rating_content_description)
            ratingView.contentDescription = String.format(ratingContentDescription, it.average)
            ratingView.rating = it.average

            userCountView.text = getFormattedAmount(it.reviews)
        }
    }

    private fun bindWebsite(addon: Addon) {
        findViewById<View>(R.id.home_page_text).setOnClickListener {
            val intent =
                Intent(Intent.ACTION_VIEW).setData(Uri.parse(addon.siteUrl))
            startActivity(intent)
        }
    }

    private fun bindLastUpdated(addon: Addon) {
        val lastUpdatedView = findViewById<TextView>(R.id.last_updated_text)
        lastUpdatedView.text = formatDate(addon.updatedAt)
    }

    private fun bindVersion(addon: Addon) {
        val versionView = findViewById<TextView>(R.id.version_text)
        versionView.text = addon.installedState?.version?.ifEmpty { addon.version } ?: addon.version

        if (addon.isInstalled()) {
            versionView.setOnLongClickListener {
                showUpdaterDialog(addon)
                true
            }
        }
    }

    private fun showUpdaterDialog(addon: Addon) {
        val context = this@AddonDetailsActivity
        val scope = CoroutineScope(Dispatchers.IO)
        scope.launch {
            val updateAttempt = updateAttemptStorage.findUpdateAttemptBy(addon.id)
            updateAttempt?.let {
                withContext(Dispatchers.Main) {
                    it.showInformationDialog(context)
                }
            }
        }
    }

    private fun bindAuthors(addon: Addon) {
        val authorsView = findViewById<TextView>(R.id.author_text)

        val authorText = addon.authors.joinToString { author ->
            author.name + " \n"
        }

        authorsView.text = authorText
    }

    private fun bindDetails(addon: Addon) {
        val detailsView = findViewById<TextView>(R.id.details)
        val detailsText = addon.translateDescription(this)

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
