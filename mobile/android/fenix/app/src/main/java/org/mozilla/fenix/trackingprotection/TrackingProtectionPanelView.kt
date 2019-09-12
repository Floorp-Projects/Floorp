/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.net.toUri
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_tracking_protection_panel.*
import kotlinx.android.synthetic.main.fragment_quick_settings_dialog_sheet.url
import kotlinx.android.synthetic.main.switch_with_description.view.*
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.SOCIAL
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.AD
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.ANALYTICS
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getHostFromUrl
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
) : LayoutContainer {
    val view: ConstraintLayout = LayoutInflater.from(containerView.context)
        .inflate(R.layout.component_tracking_protection_panel, containerView, true)
        .findViewById(R.id.panel_wrapper)

    private val context get() = view.context

    var mode: TrackingProtectionState.Mode = TrackingProtectionState.Mode.Normal
        private set

    var trackers: List<Tracker> = listOf()
        private set

    var bucketedTrackers: HashMap<TrackingProtectionCategory, List<String>> = HashMap()

    var loadedTrackers: List<Tracker> = listOf()
        private set

    var bucketedLoadedTrackers: HashMap<TrackingProtectionCategory, List<String>> = HashMap()

    fun update(state: TrackingProtectionState) {
        if (state.mode != mode) {
            mode = state.mode
        }

        if (state.listTrackers != trackers) {
            trackers = state.listTrackers
            bucketedTrackers = getHashMapOfTrackersForCategory(state.listTrackers)
        }

        if (state.listTrackersLoaded != loadedTrackers) {
            loadedTrackers = state.listTrackersLoaded
            bucketedLoadedTrackers = getHashMapOfTrackersForCategory(state.listTrackersLoaded)
        }

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
        protection_settings.visibility =
            if (state.session?.customTabConfig != null) View.GONE else View.VISIBLE

        not_blocking_header.visibility =
            if (bucketedLoadedTrackers.size == 0) View.GONE else View.VISIBLE
        bindUrl(state.url)
        bindTrackingProtectionInfo(state.isTrackingProtectionEnabled)
        protection_settings.setOnClickListener {
            interactor.selectTrackingProtectionSettings()
        }

        blocking_header.visibility =
            if (bucketedTrackers.size == 0) View.GONE else View.VISIBLE
        updateCategoryVisibility()
        setCategoryClickListeners()
    }

    @Suppress("ComplexMethod")
    private fun updateCategoryVisibility() {
        cross_site_tracking.visibility = bucketedTrackers.getVisibility(CROSS_SITE_TRACKING_COOKIES)
        social_media_trackers.visibility = bucketedTrackers.getVisibility(SOCIAL_MEDIA_TRACKERS)
        fingerprinters.visibility = bucketedTrackers.getVisibility(FINGERPRINTERS)
        tracking_content.visibility = bucketedTrackers.getVisibility(TRACKING_CONTENT)
        cryptominers.visibility = bucketedTrackers.getVisibility(CRYPTOMINERS)

        cross_site_tracking_loaded.visibility =
            bucketedLoadedTrackers.getVisibility(CROSS_SITE_TRACKING_COOKIES)
        social_media_trackers_loaded.visibility =
            bucketedLoadedTrackers.getVisibility(SOCIAL_MEDIA_TRACKERS)
        fingerprinters_loaded.visibility = bucketedLoadedTrackers.getVisibility(FINGERPRINTERS)
        tracking_content_loaded.visibility = bucketedLoadedTrackers.getVisibility(TRACKING_CONTENT)
        cryptominers_loaded.visibility = bucketedLoadedTrackers.getVisibility(CRYPTOMINERS)
    }

    private fun HashMap<TrackingProtectionCategory, List<String>>.getVisibility(
        category: TrackingProtectionCategory
    ): Int = if (this[category]?.isNotEmpty() == true) View.VISIBLE else View.GONE

    private fun setCategoryClickListeners() {
        social_media_trackers.setOnClickListener {
            interactor.openDetails(SOCIAL_MEDIA_TRACKERS, categoryBlocked = true)
        }
        fingerprinters.setOnClickListener {
            interactor.openDetails(FINGERPRINTERS, categoryBlocked = true)
        }
        cross_site_tracking.setOnClickListener {
            interactor.openDetails(CROSS_SITE_TRACKING_COOKIES, categoryBlocked = true)
        }
        tracking_content.setOnClickListener {
            interactor.openDetails(TRACKING_CONTENT, categoryBlocked = true)
        }
        cryptominers.setOnClickListener {
            interactor.openDetails(CRYPTOMINERS, categoryBlocked = true)
        }
        social_media_trackers_loaded.setOnClickListener {
            interactor.openDetails(SOCIAL_MEDIA_TRACKERS, categoryBlocked = false)
        }
        fingerprinters_loaded.setOnClickListener {
            interactor.openDetails(FINGERPRINTERS, categoryBlocked = false)
        }
        cross_site_tracking_loaded.setOnClickListener {
            interactor.openDetails(CROSS_SITE_TRACKING_COOKIES, categoryBlocked = false)
        }
        tracking_content_loaded.setOnClickListener {
            interactor.openDetails(TRACKING_CONTENT, categoryBlocked = false)
        }
        cryptominers_loaded.setOnClickListener {
            interactor.openDetails(CRYPTOMINERS, categoryBlocked = false)
        }
    }

    private fun setUIForDetailsMode(
        category: TrackingProtectionCategory,
        categoryBlocked: Boolean
    ) {
        normal_mode.visibility = View.GONE
        details_mode.visibility = View.VISIBLE
        category_title.text = context.getString(category.title)
        val stringList = bucketedTrackers[category]?.joinToString("\n")
        blocking_text_list.text = stringList
        category_description.text = context.getString(category.description)
        details_blocking_header.text =
            context.getString(
                if (categoryBlocked) R.string.enhanced_tracking_protection_blocked else
                    R.string.enhanced_tracking_protection_allowed
            )
        details_back.setOnClickListener {
            interactor.onBackPressed()
        }
    }

    private fun getHashMapOfTrackersForCategory(
        list: List<Tracker>
    ): HashMap<TrackingProtectionCategory, List<String>> {
        val hashMap = HashMap<TrackingProtectionCategory, List<String>>()
        items@ for (item in list) {
            when {
                item.trackingCategories.contains(CRYPTOMINING) -> {
                    hashMap[CRYPTOMINERS] =
                        (hashMap[CRYPTOMINERS]
                            ?: listOf()).plus(item.url.getHostFromUrl() ?: item.url)
                    continue@items
                }
                item.trackingCategories.contains(FINGERPRINTING) -> {
                    hashMap[FINGERPRINTERS] =
                        (hashMap[FINGERPRINTERS]
                            ?: listOf()).plus(item.url.getHostFromUrl() ?: item.url)
                    continue@items
                }
                item.trackingCategories.contains(SOCIAL) -> {
                    hashMap[SOCIAL_MEDIA_TRACKERS] =
                        (hashMap[SOCIAL_MEDIA_TRACKERS] ?: listOf()).plus(
                            item.url.getHostFromUrl() ?: item.url
                        )
                    continue@items
                }
                item.trackingCategories.contains(AD) ||
                        item.trackingCategories.contains(SOCIAL) ||
                        item.trackingCategories.contains(ANALYTICS) -> {
                    hashMap[TRACKING_CONTENT] =
                        (hashMap[TRACKING_CONTENT] ?: listOf()).plus(
                            item.url.getHostFromUrl() ?: item.url
                        )
                    continue@items
                }
            }
        }
        return hashMap
    }

    private fun bindUrl(url: String) {
        this.url.text = url.toUri().hostWithoutCommonPrefixes
    }

    private fun bindTrackingProtectionInfo(isTrackingProtectionOn: Boolean) {
        tracking_protection.switchItemDescription.text =
            context.getString(if (isTrackingProtectionOn) R.string.etp_panel_on else R.string.etp_panel_off)
        tracking_protection.switch_widget.isChecked = isTrackingProtectionOn

        tracking_protection.switch_widget.setOnCheckedChangeListener { _, isChecked ->
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
}
