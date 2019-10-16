/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.net.toUri
import androidx.core.view.isGone
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_tracking_protection_panel.*
import kotlinx.android.synthetic.main.switch_with_description.view.*
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.TRACKING_CONTENT

/**
 * Interface for the TrackingProtectionPanelViewInteractor. This interface is implemented by objects that want
 * to respond to user interaction on the TrackingProtectionPanelView
 */
interface TrackingProtectionPanelViewInteractor {
    /**
     * Called whenever the settings option is tapped
     */
    fun selectTrackingProtectionSettings()

    /**
     * Called whenever the tracking protection toggle for this site is toggled
     * @param isEnabled new status of session tracking protection
     */
    fun trackingProtectionToggled(isEnabled: Boolean)

    /**
     * Called whenever back is pressed
     */
    fun onBackPressed()

    /**
     * Called whenever an active tracking protection category is tapped
     * @param category The Tracking Protection Category to view details about
     * @param categoryBlocked The trackers from this category were blocked
     */
    fun openDetails(category: TrackingProtectionCategory, categoryBlocked: Boolean)
}

/**
 * View that contains and configures the Tracking Protection Panel
 */
class TrackingProtectionPanelView(
    override val containerView: ViewGroup,
    val interactor: TrackingProtectionPanelInteractor
) : LayoutContainer, View.OnClickListener {

    val view: ConstraintLayout = LayoutInflater.from(containerView.context)
        .inflate(R.layout.component_tracking_protection_panel, containerView, true)
        .findViewById(R.id.panel_wrapper)

    private var mode: TrackingProtectionState.Mode = TrackingProtectionState.Mode.Normal

    private var bucketedTrackers = TrackerBuckets()

    fun update(state: TrackingProtectionState) {
        if (state.mode != mode) {
            mode = state.mode
        }

        bucketedTrackers.updateIfNeeded(state.listTrackers)

        when (val mode = state.mode) {
            is TrackingProtectionState.Mode.Normal -> setUIForNormalMode(state)
            is TrackingProtectionState.Mode.Details -> setUIForDetailsMode(
                mode.selectedCategory,
                mode.categoryBlocked
            )
        }
    }

    private fun setUIForNormalMode(state: TrackingProtectionState) {
        details_mode.visibility = View.GONE
        normal_mode.visibility = View.VISIBLE
        protection_settings.isGone = state.session?.customTabConfig != null

        not_blocking_header.isGone = bucketedTrackers.loadedIsEmpty()
        bindUrl(state.url)
        bindTrackingProtectionInfo(state.isTrackingProtectionEnabled)
        protection_settings.setOnClickListener {
            interactor.selectTrackingProtectionSettings()
        }

        blocking_header.isGone = bucketedTrackers.blockedIsEmpty()
        updateCategoryVisibility()
        setCategoryClickListeners()
    }

    private fun updateCategoryVisibility() {
        cross_site_tracking.isGone =
            bucketedTrackers.get(CROSS_SITE_TRACKING_COOKIES, true).isEmpty()
        social_media_trackers.isGone = bucketedTrackers.get(SOCIAL_MEDIA_TRACKERS, true).isEmpty()
        fingerprinters.isGone = bucketedTrackers.get(FINGERPRINTERS, true).isEmpty()
        tracking_content.isGone = bucketedTrackers.get(TRACKING_CONTENT, true).isEmpty()
        cryptominers.isGone = bucketedTrackers.get(CRYPTOMINERS, true).isEmpty()

        social_media_trackers_loaded.isGone =
            bucketedTrackers.get(SOCIAL_MEDIA_TRACKERS, false).isEmpty()
        fingerprinters_loaded.isGone = bucketedTrackers.get(FINGERPRINTERS, false).isEmpty()
        tracking_content_loaded.isGone = bucketedTrackers.get(TRACKING_CONTENT, false).isEmpty()
        cryptominers_loaded.isGone = bucketedTrackers.get(CRYPTOMINERS, false).isEmpty()
    }

    private fun setCategoryClickListeners() {
        social_media_trackers.setOnClickListener(this)
        fingerprinters.setOnClickListener(this)
        cross_site_tracking.setOnClickListener(this)
        tracking_content.setOnClickListener(this)
        cryptominers.setOnClickListener(this)
        social_media_trackers_loaded.setOnClickListener(this)
        fingerprinters_loaded.setOnClickListener(this)
        tracking_content_loaded.setOnClickListener(this)
        cryptominers_loaded.setOnClickListener(this)
    }

    override fun onClick(v: View) {
        val category = getCategory(v) ?: return
        v.context.metrics.track(Event.TrackingProtectionTrackerList)
        interactor.openDetails(category, categoryBlocked = !isLoaded(v))
    }

    private fun setUIForDetailsMode(
        category: TrackingProtectionCategory,
        categoryBlocked: Boolean
    ) {
        val context = view.context

        normal_mode.visibility = View.GONE
        details_mode.visibility = View.VISIBLE
        category_title.text = context.getString(category.title)
        blocking_text_list.text = bucketedTrackers.get(category, categoryBlocked).joinToString("\n")
        category_description.text = context.getString(category.description)
        details_blocking_header.text = context.getString(
            if (categoryBlocked) {
                R.string.enhanced_tracking_protection_blocked
            } else {
                R.string.enhanced_tracking_protection_allowed
            }
        )
        details_back.setOnClickListener {
            interactor.onBackPressed()
        }
    }

    private fun bindUrl(url: String) {
        this.url.text = url.toUri().hostWithoutCommonPrefixes
    }

    private fun bindTrackingProtectionInfo(isTrackingProtectionOn: Boolean) {
        trackingProtectionSwitch.switchItemDescription.text =
            view.context.getString(if (isTrackingProtectionOn) R.string.etp_panel_on else R.string.etp_panel_off)
        trackingProtectionSwitch.switch_widget.isChecked = isTrackingProtectionOn

        trackingProtectionSwitch.switch_widget.setOnCheckedChangeListener { _, isChecked ->
            interactor.trackingProtectionToggled(isChecked)
        }
    }

    fun onBackPressed(): Boolean {
        return when (mode) {
            is TrackingProtectionState.Mode.Details -> {
                mode = TrackingProtectionState.Mode.Normal
                interactor.onBackPressed()
                true
            }
            else -> false
        }
    }

    companion object {

        /**
         * Returns the [TrackingProtectionCategory] corresponding to the view ID.
         */
        private fun getCategory(v: View) = when (v.id) {
            R.id.social_media_trackers, R.id.social_media_trackers_loaded -> SOCIAL_MEDIA_TRACKERS
            R.id.fingerprinters, R.id.fingerprinters_loaded -> FINGERPRINTERS
            R.id.cross_site_tracking -> CROSS_SITE_TRACKING_COOKIES
            R.id.tracking_content, R.id.tracking_content_loaded -> TRACKING_CONTENT
            R.id.cryptominers, R.id.cryptominers_loaded -> CRYPTOMINERS
            else -> null
        }

        /**
         * Returns true if the view corresponds to a "loaded" category
         */
        private fun isLoaded(v: View) = when (v.id) {
            R.id.social_media_trackers_loaded,
            R.id.fingerprinters_loaded,
            R.id.tracking_content_loaded,
            R.id.cryptominers_loaded -> true

            R.id.social_media_trackers,
            R.id.fingerprinters,
            R.id.cross_site_tracking,
            R.id.tracking_content,
            R.id.cryptominers -> false
            else -> false
        }
    }
}
