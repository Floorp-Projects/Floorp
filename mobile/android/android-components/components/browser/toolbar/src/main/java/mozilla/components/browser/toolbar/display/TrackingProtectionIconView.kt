/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.graphics.drawable.Animatable
import android.graphics.drawable.Drawable
import android.graphics.drawable.StateListDrawable
import android.util.AttributeSet
import android.view.View
import androidx.annotation.StringRes
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.view.isVisible
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.OFF_GLOBALLY
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.ON_TRACKERS_BLOCKED
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.OFF_FOR_A_SITE

/**
 * Internal widget to display the different icons of tracking protection, relies on the
 * [SiteTrackingProtection] state of each page.
 */
internal class TrackingProtectionIconView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AppCompatImageView(context, attrs, defStyleAttr) {

    var siteTrackingProtection: SiteTrackingProtection = OFF_GLOBALLY
        set(value) {
            if (value != field) {
                field = value
                refreshDrawableState()
                updateIcon()
            }

            field = value
        }

    fun setIcons(
        iconOnNoTrackersBlocked: Drawable,
        iconOnTrackersBlocked: Drawable,
        disabledForSite: Drawable
    ) {
        val stateList = StateListDrawable()

        stateList.addState(intArrayOf(R.attr.stateOnNoTrackersBlocked), iconOnNoTrackersBlocked)
        stateList.addState(intArrayOf(R.attr.stateOnTrackersBlocked), iconOnTrackersBlocked)
        stateList.addState(intArrayOf(R.attr.stateOffForASite), disabledForSite)

        setImageDrawable(stateList)
        refreshDrawableState()
        updateIcon()
    }

    @Suppress("SENSELESS_COMPARISON")
    override fun onCreateDrawableState(extraSpace: Int): IntArray {
        // We need this here because in some situations, this function is getting called from
        // the super() constructor on the View class, way before this class properties get
        // initialized causing a null pointer exception.
        // See for more details: https://github.com/mozilla-mobile/android-components/issues/4058
        if (siteTrackingProtection == null) return super.onCreateDrawableState(extraSpace)

        val drawableState = when (siteTrackingProtection) {
            ON_NO_TRACKERS_BLOCKED -> R.attr.stateOnNoTrackersBlocked
            ON_TRACKERS_BLOCKED -> R.attr.stateOnTrackersBlocked
            OFF_FOR_A_SITE -> R.attr.stateOffForASite
            OFF_GLOBALLY -> {
                // the icon will be hidden, no state needed.
                return super.onCreateDrawableState(extraSpace)
            }
        }

        val drawableStates = super.onCreateDrawableState(extraSpace + 1)

        View.mergeDrawableStates(drawableStates, intArrayOf(drawableState))
        return drawableStates
    }

    private fun updateIcon() {
        val descriptionId = when (siteTrackingProtection) {
            ON_NO_TRACKERS_BLOCKED -> {
                isVisible = true
                R.string.mozac_browser_toolbar_content_description_tracking_protection_on_no_trackers_blocked
            }
            ON_TRACKERS_BLOCKED -> {
                isVisible = true
                R.string.mozac_browser_toolbar_content_description_tracking_protection_on_trackers_blocked
            }
            OFF_FOR_A_SITE -> {
                isVisible = true
                R.string.mozac_browser_toolbar_content_description_tracking_protection_off_for_a_site
            }

            OFF_GLOBALLY -> {
                isVisible = false
                null
            }
        }

        setContentDescription(descriptionId)

        val currentDrawable = drawable?.current
        if (currentDrawable is Animatable) {
            currentDrawable.start()
        }
    }

    private fun setContentDescription(@StringRes id: Int?) {
        contentDescription = if (id != null) context.getString(id) else null
    }

    companion object {
        val DEFAULT_ICON_ON_NO_TRACKERS_BLOCKED =
            R.drawable.mozac_ic_tracking_protection_on_no_trackers_blocked
        val DEFAULT_ICON_ON_TRACKERS_BLOCKED =
            R.drawable.mozac_ic_tracking_protection_on_trackers_blocked
        val DEFAULT_ICON_OFF_FOR_A_SITE =
            R.drawable.mozac_ic_tracking_protection_on_trackers_blocked
    }
}
