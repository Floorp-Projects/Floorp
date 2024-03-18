/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import androidx.annotation.VisibleForTesting
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.text.HtmlCompat
import androidx.core.view.AccessibilityDelegateCompat
import androidx.core.view.ViewCompat
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat
import androidx.core.view.isGone
import androidx.core.view.isVisible
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentTrackingProtectionPanelBinding
import org.mozilla.fenix.ext.addUnderline
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.REDIRECT_TRACKERS
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
     * Called whenever back is pressed
     */
    fun onBackPressed()

    /**
     * Called whenever back button is pressed in Detail mode.
     */
    fun onExitDetailMode()

    /**
     * Called whenever an active tracking protection category is tapped
     * @param category The Tracking Protection Category to view details about
     * @param categoryBlocked The trackers from this category were blocked
     */
    fun openDetails(category: TrackingProtectionCategory, categoryBlocked: Boolean)

    /**
     * Called when the Learn more link for SmartBlock is clicked.
     */
    fun onLearnMoreClicked()
}

/**
 * View that contains and configures the Tracking Protection Panel
 */
@SuppressWarnings("TooManyFunctions")
class TrackingProtectionPanelView(
    val containerView: ViewGroup,
    val interactor: TrackingProtectionPanelInteractor,
) : View.OnClickListener {

    @VisibleForTesting
    internal val binding = ComponentTrackingProtectionPanelBinding.inflate(
        LayoutInflater.from(containerView.context),
        containerView,
        true,
    )

    val view: ConstraintLayout = binding.panelWrapper

    private var mode: ProtectionsState.Mode = ProtectionsState.Mode.Normal

    private var bucketedTrackers = TrackerBuckets()

    private var shouldFocusAccessibilityView: Boolean = true

    init {
        binding.protectionSettings.setOnClickListener {
            interactor.selectTrackingProtectionSettings()
        }

        binding.detailsBack.setOnClickListener {
            interactor.onExitDetailMode()
        }

        binding.navigateBack.setOnClickListener {
            interactor.onBackPressed()
        }

        setCategoryClickListeners()
    }

    /**
     * Updates the display mode of the Protection view.
     */
    fun update(state: ProtectionsState) {
        mode = state.mode
        bucketedTrackers.updateIfNeeded(state.listTrackers)

        when (val mode = state.mode) {
            is ProtectionsState.Mode.Normal -> setUIForNormalMode(state)
            is ProtectionsState.Mode.Details -> setUIForDetailsMode(
                mode.selectedCategory,
                mode.categoryBlocked,
            )
        }

        setAccessibilityViewHierarchy(binding.detailsBack, binding.categoryTitle)
    }

    private fun setUIForNormalMode(state: ProtectionsState) {
        binding.detailsMode.visibility = View.GONE
        binding.normalMode.visibility = View.VISIBLE

        binding.protectionSettings.isGone = state.tab is CustomTabSessionState
        binding.notBlockingHeader.isGone = bucketedTrackers.loadedIsEmpty()
        binding.blockingHeader.isGone = bucketedTrackers.blockedIsEmpty()

        if (containerView.context.settings().enabledTotalCookieProtection) {
            binding.crossSiteTracking.text = containerView.context.getString(R.string.etp_cookies_title_2)
            binding.crossSiteTrackingLoaded.text = containerView.context.getString(R.string.etp_cookies_title_2)
        }

        updateCategoryVisibility()
        focusAccessibilityLastUsedCategory(state.lastAccessedCategory)
    }

    private fun setUIForDetailsMode(
        category: TrackingProtectionCategory,
        categoryBlocked: Boolean,
    ) {
        val containASmartBlockItem = bucketedTrackers.get(category, categoryBlocked).any { it.unBlockedBySmartBlock }
        binding.normalMode.visibility = View.GONE
        binding.detailsMode.visibility = View.VISIBLE

        if (category == CROSS_SITE_TRACKING_COOKIES &&
            containerView.context.settings().enabledTotalCookieProtection
        ) {
            binding.categoryTitle.setText(R.string.etp_cookies_title_2)
            binding.categoryDescription.setText(R.string.etp_cookies_description_2)
        } else {
            binding.categoryTitle.setText(category.title)
            binding.categoryDescription.setText(category.description)
        }

        binding.smartblockDescription.isVisible = containASmartBlockItem
        binding.smartblockLearnMore.isVisible = containASmartBlockItem

        val trackersList = bucketedTrackers.get(category, categoryBlocked).joinToString("<br/>") {
            createTrackerItem(it, containASmartBlockItem)
        }

        binding.blockingTextList.text = HtmlCompat.fromHtml(trackersList, HtmlCompat.FROM_HTML_MODE_COMPACT)

        // show description for SmartBlock tracking content in details
        if (containASmartBlockItem) {
            with(binding.smartblockLearnMore) {
                movementMethod = LinkMovementMethod.getInstance()
                addUnderline()
                setOnClickListener { interactor.onLearnMoreClicked() }
            }
        }

        binding.detailsBlockingHeader.setText(
            if (categoryBlocked) {
                R.string.enhanced_tracking_protection_blocked
            } else {
                R.string.enhanced_tracking_protection_allowed
            },
        )

        binding.detailsBack.requestFocus()
        binding.detailsBack.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED)
    }

    private fun createTrackerItem(tracker: TrackerLog, isUnblockedSection: Boolean): String {
        val space = if (isUnblockedSection) "&nbsp;&nbsp;" else ""
        return if (tracker.unBlockedBySmartBlock) {
            "<b>*${tracker.url.tryGetHostFromUrl()}</b>"
        } else {
            "$space${tracker.url.tryGetHostFromUrl()}"
        }
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
            if (binding.crossSiteTracking.isGone) binding.crossSiteTrackingLoaded else binding.crossSiteTracking
        }
        SOCIAL_MEDIA_TRACKERS.name -> {
            if (binding.socialMediaTrackers.isGone) binding.socialMediaTrackersLoaded else binding.socialMediaTrackers
        }
        FINGERPRINTERS.name -> {
            if (binding.fingerprinters.isGone) binding.fingerprintersLoaded else binding.fingerprinters
        }
        TRACKING_CONTENT.name -> {
            if (binding.trackingContent.isGone) binding.trackingContentLoaded else binding.trackingContent
        }
        CRYPTOMINERS.name -> {
            if (binding.cryptominers.isGone) binding.cryptominersLoaded else binding.cryptominers
        }
        REDIRECT_TRACKERS.name -> {
            if (binding.redirectTrackers.isGone) binding.redirectTrackersLoaded else binding.redirectTrackers
        }
        else -> null
    }

    private fun updateCategoryVisibility() {
        binding.crossSiteTracking.isGone =
            bucketedTrackers.get(CROSS_SITE_TRACKING_COOKIES, true).isEmpty()
        binding.socialMediaTrackers.isGone =
            bucketedTrackers.get(SOCIAL_MEDIA_TRACKERS, true).isEmpty()
        binding.fingerprinters.isGone = bucketedTrackers.get(FINGERPRINTERS, true).isEmpty()
        binding.trackingContent.isGone = bucketedTrackers.get(TRACKING_CONTENT, true).isEmpty()
        binding.cryptominers.isGone = bucketedTrackers.get(CRYPTOMINERS, true).isEmpty()
        binding.redirectTrackers.isGone = bucketedTrackers.get(REDIRECT_TRACKERS, true).isEmpty()

        binding.crossSiteTrackingLoaded.isGone =
            bucketedTrackers.get(CROSS_SITE_TRACKING_COOKIES, false).isEmpty()
        binding.socialMediaTrackersLoaded.isGone =
            bucketedTrackers.get(SOCIAL_MEDIA_TRACKERS, false).isEmpty()
        binding.fingerprintersLoaded.isGone = bucketedTrackers.get(FINGERPRINTERS, false).isEmpty()
        binding.trackingContentLoaded.isGone = bucketedTrackers.get(TRACKING_CONTENT, false).isEmpty()
        binding.cryptominersLoaded.isGone = bucketedTrackers.get(CRYPTOMINERS, false).isEmpty()
        binding.redirectTrackersLoaded.isGone = bucketedTrackers.get(REDIRECT_TRACKERS, false).isEmpty()
    }

    private fun setCategoryClickListeners() {
        binding.socialMediaTrackers.setOnClickListener(this)
        binding.fingerprinters.setOnClickListener(this)
        binding.crossSiteTracking.setOnClickListener(this)
        binding.trackingContent.setOnClickListener(this)
        binding.cryptominers.setOnClickListener(this)

        binding.crossSiteTrackingLoaded.setOnClickListener(this)
        binding.socialMediaTrackersLoaded.setOnClickListener(this)
        binding.fingerprintersLoaded.setOnClickListener(this)
        binding.trackingContentLoaded.setOnClickListener(this)
        binding.cryptominersLoaded.setOnClickListener(this)
        binding.redirectTrackersLoaded.setOnClickListener(this)
    }

    override fun onClick(v: View) {
        val category = getCategory(v) ?: return
        TrackingProtection.etpTrackerList.record(NoExtras())
        shouldFocusAccessibilityView = true
        interactor.openDetails(category, categoryBlocked = !isLoaded(v))
    }

    fun onBackPressed(): Boolean {
        return when (mode) {
            is ProtectionsState.Mode.Details -> {
                mode = ProtectionsState.Mode.Normal
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
                    host: View,
                    info: AccessibilityNodeInfoCompat,
                ) {
                    info.setTraversalAfter(view1)
                    super.onInitializeAccessibilityNodeInfo(host, info)
                }
            },
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
            R.id.redirect_trackers_loaded,
            -> true

            R.id.social_media_trackers,
            R.id.fingerprinters,
            R.id.cross_site_tracking,
            R.id.tracking_content,
            R.id.cryptominers,
            R.id.redirect_trackers,
            -> false
            else -> false
        }
    }
}
