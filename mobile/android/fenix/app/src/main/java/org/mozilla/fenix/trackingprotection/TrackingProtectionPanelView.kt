/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.net.toUri
import androidx.core.view.AccessibilityDelegateCompat
import androidx.core.view.ViewCompat
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat
import androidx.core.view.isGone
import androidx.core.view.isVisible
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_tracking_protection_panel.*
import kotlinx.android.synthetic.main.component_tracking_protection_panel.details_blocking_header
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.TRACKING_CONTENT
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.REDIRECT_TRACKERS

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
@SuppressWarnings("TooManyFunctions")
class TrackingProtectionPanelView(
    override val containerView: ViewGroup,
    val interactor: TrackingProtectionPanelInteractor
) : LayoutContainer, View.OnClickListener {

    val view: ConstraintLayout = LayoutInflater.from(containerView.context)
        .inflate(R.layout.component_tracking_protection_panel, containerView, true)
        .findViewById(R.id.panel_wrapper)

    private var mode: TrackingProtectionState.Mode = TrackingProtectionState.Mode.Normal

    private var bucketedTrackers = TrackerBuckets()

    private var shouldFocusAccessibilityView: Boolean = true

    init {
        protection_settings.setOnClickListener {
            interactor.selectTrackingProtectionSettings()
        }
        details_back.setOnClickListener {
            interactor.onBackPressed()
        }
        setCategoryClickListeners()
    }

    fun update(state: TrackingProtectionState) {
        mode = state.mode
        bucketedTrackers.updateIfNeeded(state.listTrackers)

        when (val mode = state.mode) {
            is TrackingProtectionState.Mode.Normal -> setUIForNormalMode(state)
            is TrackingProtectionState.Mode.Details -> setUIForDetailsMode(
                mode.selectedCategory,
                mode.categoryBlocked
            )
        }

        setAccessibilityViewHierarchy(details_back, category_title)
    }

    private fun setUIForNormalMode(state: TrackingProtectionState) {
        details_mode.visibility = View.GONE
        normal_mode.visibility = View.VISIBLE
        protection_settings.isGone = state.tab is CustomTabSessionState

        not_blocking_header.isGone = bucketedTrackers.loadedIsEmpty()
        bindUrl(state.url)

        blocking_header.isGone = bucketedTrackers.blockedIsEmpty()
        updateCategoryVisibility()
        focusAccessibilityLastUsedCategory(state.lastAccessedCategory)
    }

    private fun setUIForDetailsMode(
        category: TrackingProtectionCategory,
        categoryBlocked: Boolean
    ) {
        normal_mode.visibility = View.GONE
        details_mode.visibility = View.VISIBLE
        category_title.setText(category.title)
        blocking_text_list.text = bucketedTrackers.get(category, categoryBlocked).joinToString("\n")
        category_description.setText(category.description)
        details_blocking_header.setText(
            if (categoryBlocked) {
                R.string.enhanced_tracking_protection_blocked
            } else {
                R.string.enhanced_tracking_protection_allowed
            }
        )

        details_back.requestFocus()
        details_back.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED)
    }

    /**
     * Will force accessibility focus to last entered details category.
     * Called when user returns from details_mode.
     * */
    private fun focusAccessibilityLastUsedCategory(categoryTitle: String) {
        if (categoryTitle.isNotEmpty()) {
            val viewToFocus = getLastUsedCategoryView(categoryTitle)
            if (viewToFocus != null && viewToFocus.isVisible && shouldFocusAccessibilityView) {
                viewToFocus.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED)
                shouldFocusAccessibilityView = false
            }
        }
    }

    /**
     * Checks whether the permission was allowed or blocked when they were last used based on
     * visibility, where "..._loaded" titles correspond to "Allowed" permissions and the other
     * corresponds to "Blocked" permissions for each category.
     */
    private fun getLastUsedCategoryView(categoryTitle: String) = when (categoryTitle) {
        CROSS_SITE_TRACKING_COOKIES.name -> {
            if (cross_site_tracking.isGone) cross_site_tracking_loaded else cross_site_tracking
        }
        SOCIAL_MEDIA_TRACKERS.name -> {
            if (social_media_trackers.isGone) social_media_trackers_loaded else social_media_trackers
        }
        FINGERPRINTERS.name -> {
            if (fingerprinters.isGone) fingerprinters_loaded else fingerprinters
        }
        TRACKING_CONTENT.name -> {
            if (tracking_content.isGone) tracking_content_loaded else tracking_content
        }
        CRYPTOMINERS.name -> {
            if (cryptominers.isGone) cryptominers_loaded else cryptominers
        }
        REDIRECT_TRACKERS.name -> {
            if (redirect_trackers.isGone) redirect_trackers_loaded else redirect_trackers
        }
        else -> null
    }

    private fun updateCategoryVisibility() {
        cross_site_tracking.isGone =
            bucketedTrackers.get(CROSS_SITE_TRACKING_COOKIES, true).isEmpty()
        social_media_trackers.isGone =
            bucketedTrackers.get(SOCIAL_MEDIA_TRACKERS, true).isEmpty()
        fingerprinters.isGone = bucketedTrackers.get(FINGERPRINTERS, true).isEmpty()
        tracking_content.isGone = bucketedTrackers.get(TRACKING_CONTENT, true).isEmpty()
        cryptominers.isGone = bucketedTrackers.get(CRYPTOMINERS, true).isEmpty()
        redirect_trackers.isGone = bucketedTrackers.get(REDIRECT_TRACKERS, true).isEmpty()

        cross_site_tracking_loaded.isGone =
            bucketedTrackers.get(CROSS_SITE_TRACKING_COOKIES, false).isEmpty()
        social_media_trackers_loaded.isGone =
            bucketedTrackers.get(SOCIAL_MEDIA_TRACKERS, false).isEmpty()
        fingerprinters_loaded.isGone = bucketedTrackers.get(FINGERPRINTERS, false).isEmpty()
        tracking_content_loaded.isGone = bucketedTrackers.get(TRACKING_CONTENT, false).isEmpty()
        cryptominers_loaded.isGone = bucketedTrackers.get(CRYPTOMINERS, false).isEmpty()
        redirect_trackers_loaded.isGone = bucketedTrackers.get(REDIRECT_TRACKERS, false).isEmpty()
    }

    private fun setCategoryClickListeners() {
        social_media_trackers.setOnClickListener(this)
        fingerprinters.setOnClickListener(this)
        cross_site_tracking.setOnClickListener(this)
        tracking_content.setOnClickListener(this)
        cryptominers.setOnClickListener(this)
        cross_site_tracking_loaded.setOnClickListener(this)
        social_media_trackers_loaded.setOnClickListener(this)
        fingerprinters_loaded.setOnClickListener(this)
        tracking_content_loaded.setOnClickListener(this)
        cryptominers_loaded.setOnClickListener(this)
        redirect_trackers_loaded.setOnClickListener(this)
    }

    override fun onClick(v: View) {
        val category = getCategory(v) ?: return
        v.context.metrics.track(Event.TrackingProtectionTrackerList)
        shouldFocusAccessibilityView = true
        interactor.openDetails(category, categoryBlocked = !isLoaded(v))
    }

    private fun bindUrl(url: String) {
        this.url.text = url.toUri().hostWithoutCommonPrefixes
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

    /**
     * Makes sure [view1] is followed by [view2] when navigating in accessibility mode.
     * */
    private fun setAccessibilityViewHierarchy(view1: View, view2: View) {
        ViewCompat.setAccessibilityDelegate(
            view2,
            object : AccessibilityDelegateCompat() {
                override fun onInitializeAccessibilityNodeInfo(
                    host: View?,
                    info: AccessibilityNodeInfoCompat
                ) {
                    info.setTraversalAfter(view1)
                    super.onInitializeAccessibilityNodeInfo(host, info)
                }
            }
        )
    }

    companion object {

        /**
         * Returns the [TrackingProtectionCategory] corresponding to the view ID.
         */
        private fun getCategory(v: View) = when (v.id) {
            R.id.social_media_trackers, R.id.social_media_trackers_loaded -> SOCIAL_MEDIA_TRACKERS
            R.id.fingerprinters, R.id.fingerprinters_loaded -> FINGERPRINTERS
            R.id.cross_site_tracking, R.id.cross_site_tracking_loaded -> CROSS_SITE_TRACKING_COOKIES
            R.id.tracking_content, R.id.tracking_content_loaded -> TRACKING_CONTENT
            R.id.cryptominers, R.id.cryptominers_loaded -> CRYPTOMINERS
            R.id.redirect_trackers, R.id.redirect_trackers_loaded -> REDIRECT_TRACKERS
            else -> null
        }

        /**
         * Returns true if the view corresponds to a "loaded" category
         */
        private fun isLoaded(v: View) = when (v.id) {
            R.id.social_media_trackers_loaded,
            R.id.cross_site_tracking_loaded,
            R.id.fingerprinters_loaded,
            R.id.tracking_content_loaded,
            R.id.cryptominers_loaded,
            R.id.redirect_trackers_loaded -> true

            R.id.social_media_trackers,
            R.id.fingerprinters,
            R.id.cross_site_tracking,
            R.id.tracking_content,
            R.id.cryptominers,
            R.id.redirect_trackers -> false
            else -> false
        }
    }
}
