/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.content.res.ColorStateList
import android.util.TypedValue
import android.view.View
import android.widget.FrameLayout
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.ContextCompat
import androidx.core.view.setPadding
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.toolbar.R
import mozilla.components.browser.toolbar.facts.emitOpenMenuFact

@Suppress("ViewConstructor") // This view is only instantiated in code
internal class MenuButton(
    context: Context,
    private val parent: View
) : FrameLayout(context) {

    @VisibleForTesting internal var menu: BrowserMenu? = null

    private val menuIcon = AppCompatImageView(context).apply {
        setPadding(resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_menu_padding))
        setImageResource(mozilla.components.ui.icons.R.drawable.mozac_ic_menu)
        contentDescription = context.getString(R.string.mozac_browser_toolbar_menu_button)
    }

    init {
        val outValue = TypedValue()
        context.theme.resolveAttribute(
            android.R.attr.selectableItemBackgroundBorderless,
            outValue,
            true)
        setBackgroundResource(outValue.resourceId)

        visibility = View.GONE
        isClickable = true
        isFocusable = true

        setOnClickListener {
            menu = menuBuilder?.build(context)
            val endAlwaysVisible = menuBuilder?.endOfMenuAlwaysVisible ?: false
            menu?.show(
                anchor = this,
                orientation = BrowserMenu.determineMenuOrientation(parent),
                endOfMenuAlwaysVisible = endAlwaysVisible
            ) { menu = null }

            emitOpenMenuFact(menuBuilder?.extras)
        }

        addView(menuIcon)
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
    fun invalidateMenu() {
        menu?.invalidate()
        val highlightColorResource: Int? = menuBuilder?.items?.let { items ->
            items.forEach { item ->
                if (item is BrowserMenuHighlightableItem && item.isHighlighted()) {
                    return@let item.highlight.colorResource
                }
            }
            null
        }

        // If a highlighted item is found, show the indicator.
        if (highlightColorResource != null) {
            val color = ContextCompat.getColor(context, highlightColorResource)
            menuIcon.setBackgroundResource(R.drawable.mozac_menu_indicator)
            menuIcon.backgroundTintList = ColorStateList.valueOf(color)
        } else {
            menuIcon.setBackgroundResource(0)
        }
    }

    fun dismissMenu() {
        menu?.dismiss()
    }

    fun setColorFilter(@ColorInt color: Int) {
        menuIcon.setColorFilter(color)
    }
}
