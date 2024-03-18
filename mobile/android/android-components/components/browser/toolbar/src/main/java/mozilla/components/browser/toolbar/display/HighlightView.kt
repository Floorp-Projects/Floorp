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
import mozilla.components.concept.toolbar.Toolbar.Highlight
import mozilla.components.concept.toolbar.Toolbar.Highlight.NONE
import mozilla.components.concept.toolbar.Toolbar.Highlight.PERMISSIONS_CHANGED

/**
 * Internal widget to display a dot notification.
 */
internal class HighlightView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : AppCompatImageView(context, attrs, defStyleAttr) {

    init {
        visibility = GONE
    }

    var state: Highlight = NONE
        set(value) {
            if (value != field) {
                field = value
                updateIcon()
            }
        }

    @VisibleForTesting
    internal var highlightTint: Int? = null

    private var highlightIcon: Drawable =
        requireNotNull(AppCompatResources.getDrawable(context, DEFAULT_ICON))

    fun setTint(tint: Int) {
        highlightTint = tint
        setColorFilter(tint)
    }

    fun setIcon(icons: Drawable) {
        this.highlightIcon = icons

        updateIcon()
    }

    @Synchronized
    @VisibleForTesting
    internal fun updateIcon() {
        val update = state.toUpdate()

        isVisible = update.visible

        contentDescription = if (update.contentDescription != null) {
            context.getString(update.contentDescription)
        } else {
            null
        }

        highlightTint?.let { setColorFilter(it) }
        setImageDrawable(update.drawable)
    }

    companion object {
        val DEFAULT_ICON = R.drawable.mozac_dot_notification
    }

    private fun Highlight.toUpdate(): Update = when (this) {
        PERMISSIONS_CHANGED -> Update(
            highlightIcon,
            R.string.mozac_browser_toolbar_content_description_autoplay_blocked,
            true,
        )

        NONE -> Update(
            null,
            null,
            false,
        )
    }
}
