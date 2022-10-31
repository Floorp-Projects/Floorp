/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.graphics.drawable.Animatable
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.view.isVisible
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.OFF_FOR_A_SITE
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.OFF_GLOBALLY
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.ON_TRACKERS_BLOCKED

/**
 * Internal widget to display the different icons of tracking protection, relies on the
 * [SiteTrackingProtection] state of each page.
 */
internal class TrackingProtectionIconView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : AppCompatImageView(context, attrs, defStyleAttr) {
    var siteTrackingProtection: SiteTrackingProtection? = null
        set(value) {
            if (value != field) {
                field = value
                updateIcon()
            }
        }

    @VisibleForTesting
    internal var trackingProtectionTint: Int? = null

    private var iconOnNoTrackersBlocked: Drawable =
        requireNotNull(AppCompatResources.getDrawable(context, DEFAULT_ICON_ON_NO_TRACKERS_BLOCKED))
    private var iconOnTrackersBlocked: Drawable =
        requireNotNull(AppCompatResources.getDrawable(context, DEFAULT_ICON_ON_TRACKERS_BLOCKED))
    private var disabledForSite: Drawable =
        requireNotNull(AppCompatResources.getDrawable(context, DEFAULT_ICON_OFF_FOR_A_SITE))

    fun setTint(tint: Int) {
        trackingProtectionTint = tint
    }

    fun setIcons(
        iconOnNoTrackersBlocked: Drawable,
        iconOnTrackersBlocked: Drawable,
        disabledForSite: Drawable,
    ) {
        this.iconOnNoTrackersBlocked = iconOnNoTrackersBlocked
        this.iconOnTrackersBlocked = iconOnTrackersBlocked
        this.disabledForSite = disabledForSite

        updateIcon()
    }

    @Synchronized
    private fun updateIcon() {
        val update = siteTrackingProtection?.toUpdate() ?: return

        isVisible = update.visible

        contentDescription = if (update.contentDescription != null) {
            context.getString(update.contentDescription)
        } else {
            null
        }

        setOrClearColorFilter(update.drawable)
        setImageDrawable(update.drawable)

        if (update.drawable is Animatable) {
            update.drawable.start()
        }
    }

    @VisibleForTesting
    internal fun setOrClearColorFilter(drawable: Drawable?) {
        if (drawable is Animatable) {
            clearColorFilter()
        } else {
            trackingProtectionTint?.let { setColorFilter(it) }
        }
    }

    companion object {
        val DEFAULT_ICON_ON_NO_TRACKERS_BLOCKED =
            R.drawable.mozac_ic_tracking_protection_on_no_trackers_blocked
        val DEFAULT_ICON_ON_TRACKERS_BLOCKED =
            R.drawable.mozac_ic_tracking_protection_on_trackers_blocked
        val DEFAULT_ICON_OFF_FOR_A_SITE =
            R.drawable.mozac_ic_tracking_protection_off_for_a_site
    }

    private fun SiteTrackingProtection.toUpdate(): Update = when (this) {
        ON_NO_TRACKERS_BLOCKED -> Update(
            iconOnNoTrackersBlocked,
            R.string.mozac_browser_toolbar_content_description_tracking_protection_on_no_trackers_blocked,
            true,
        )

        ON_TRACKERS_BLOCKED -> Update(
            iconOnTrackersBlocked,
            R.string.mozac_browser_toolbar_content_description_tracking_protection_on_trackers_blocked1,
            true,
        )

        OFF_FOR_A_SITE -> Update(
            disabledForSite,
            R.string.mozac_browser_toolbar_content_description_tracking_protection_off_for_a_site1,
            true,
        )

        OFF_GLOBALLY -> Update(
            null,
            null,
            false,
        )
    }
}

internal class Update(
    val drawable: Drawable?,
    @StringRes val contentDescription: Int?,
    val visible: Boolean,
)
