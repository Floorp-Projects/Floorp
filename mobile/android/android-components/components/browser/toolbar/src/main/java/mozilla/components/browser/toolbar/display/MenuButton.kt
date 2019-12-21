/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.content.res.ColorStateList
import android.util.AttributeSet
import android.view.View
import android.widget.FrameLayout
import android.widget.ImageView
import androidx.annotation.ColorInt
import androidx.annotation.DrawableRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.HighlightableMenuItem
import mozilla.components.browser.toolbar.R
import mozilla.components.browser.toolbar.facts.emitOpenMenuFact
import mozilla.components.support.ktx.android.content.res.resolveAttribute

internal class MenuButton @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr) {

    @VisibleForTesting internal var menu: BrowserMenu? = null

    private val menuIcon =
        createImageView(R.drawable.mozac_ic_menu, context.getString(R.string.mozac_browser_toolbar_menu_button))
    private val highlightView =
        createImageView(R.drawable.mozac_menu_indicator)
    private val notificationIconView =
        createImageView(R.drawable.mozac_menu_notification)

    init {
        setBackgroundResource(context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless))

        visibility = View.GONE
        isClickable = true
        isFocusable = true

        setOnClickListener {
            if (menu == null) {
                menu = menuBuilder?.build(context)
                val endAlwaysVisible = menuBuilder?.endOfMenuAlwaysVisible ?: false
                menu?.show(
                    anchor = this,
                    orientation = BrowserMenu.determineMenuOrientation(parent as View?),
                    endOfMenuAlwaysVisible = endAlwaysVisible
                ) { menu = null }

                emitOpenMenuFact(menuBuilder?.extras)
            } else {
                menu?.dismiss()
            }
        }

        addView(highlightView)
        addView(menuIcon)
        addView(notificationIconView)
    }

    var menuBuilder: BrowserMenuBuilder? = null
        set(value) {
            field = value
            menu?.dismiss()
            if (value != null) {
                visibility = View.VISIBLE
            } else {
                visibility = View.GONE
                menu = null
            }
        }

    /**
     * Declare that the menu items should be updated if needed.
     */
    @Suppress("Deprecation")
    fun invalidateMenu() {
        menu?.invalidate()

        val highlight = menuBuilder?.items.orEmpty()
            .asSequence()
            .filter { it.visible() }
            .mapNotNull { it as? HighlightableMenuItem }
            .filter { it.isHighlighted() }
            .map { it.highlight }
            .maxBy {
                // Select the highlight with the highest priority
                when (it) {
                    is BrowserMenuHighlight.HighPriority -> 2
                    is BrowserMenuHighlight.LowPriority -> 1
                    is BrowserMenuHighlight.ClassicHighlight -> 0
                }
            }

        // If a highlighted item is found, show the indicator
        when (highlight) {
            is BrowserMenuHighlight.HighPriority -> {
                highlightView.imageTintList = ColorStateList.valueOf(highlight.backgroundTint)
                highlightView.visibility = View.VISIBLE
                notificationIconView.visibility = View.GONE
            }
            is BrowserMenuHighlight.LowPriority -> {
                notificationIconView.setColorFilter(highlight.notificationTint)
                highlightView.visibility = View.GONE
                notificationIconView.visibility = View.VISIBLE
            }
            is BrowserMenuHighlight.ClassicHighlight -> {
                val color = ContextCompat.getColor(context, highlight.colorResource)
                highlightView.imageTintList = ColorStateList.valueOf(color)
                highlightView.visibility = View.VISIBLE
                notificationIconView.visibility = View.GONE
            }
            null -> {
                highlightView.visibility = View.GONE
                notificationIconView.visibility = View.GONE
            }
        }
    }

    fun dismissMenu() {
        menu?.dismiss()
    }

    fun setColorFilter(@ColorInt color: Int) {
        menuIcon.setColorFilter(color)
    }

    private fun createImageView(@DrawableRes image: Int, description: String? = null) =
        AppCompatImageView(context).apply {
            setImageResource(image)
            scaleType = ImageView.ScaleType.CENTER
            if (description != null) {
                contentDescription = description
            } else {
                importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
                visibility = View.GONE
            }
        }
}
