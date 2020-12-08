/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.view.isVisible
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar.PermissionHighlights
import mozilla.components.concept.toolbar.Toolbar.PermissionHighlights.AUTOPLAY_BLOCKED
import mozilla.components.concept.toolbar.Toolbar.PermissionHighlights.NONE

/**
 * Internal widget to display the different icons of site permission.
 */
internal class PermissionHighlightsIconView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AppCompatImageView(context, attrs, defStyleAttr) {

    init {
        visibility = GONE
    }

    var permissionHighlights: PermissionHighlights = NONE
        set(value) {
            if (value != field) {
                field = value
                updateIcon()
            }
        }

    @VisibleForTesting
    internal var permissionTint: Int? = null

    private var iconAutoplayBlocked: Drawable =
        requireNotNull(AppCompatResources.getDrawable(context, DEFAULT_ICON_AUTOPLAY_BLOCKED))

    fun setTint(tint: Int) {
        permissionTint = tint
        setColorFilter(tint)
    }

    fun setIcons(icons: DisplayToolbar.Icons.PermissionHighlights) {
        this.iconAutoplayBlocked = icons.autoPlayBlocked

        updateIcon()
    }

    @Synchronized
    @VisibleForTesting
    internal fun updateIcon() {
        val update = permissionHighlights.toUpdate()

        isVisible = update.visible

        contentDescription = if (update.contentDescription != null) {
            context.getString(update.contentDescription)
        } else {
            null
        }

        permissionTint?.let { setColorFilter(it) }
        setImageDrawable(update.drawable)
    }

    companion object {
        val DEFAULT_ICON_AUTOPLAY_BLOCKED =
            R.drawable.mozac_ic_autoplay_blocked
    }

    private fun PermissionHighlights.toUpdate(): Update = when (this) {
        AUTOPLAY_BLOCKED -> Update(
            iconAutoplayBlocked,
            R.string.mozac_browser_toolbar_content_description_autoplay_blocked,
            true)

        NONE -> Update(
            null,
            null,
            false
        )
    }
}
