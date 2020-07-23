/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.tabstray.TabsTray

/**
 * A customizable tabs tray for browsers.
 */
@Deprecated("Use a RecyclerView directly instead; styling can be passed to the TabsAdapter. " +
    "This class will be removed in a future release.")
class BrowserTabsTray @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
    val tabsAdapter: TabsAdapter = TabsAdapter(),
    layout: LayoutManager = GridLayoutManager(context, 2),
    itemDecoration: DividerItemDecoration? = null
) : RecyclerView(context, attrs, defStyleAttr),
    TabsTray by tabsAdapter {

    init {
        val attr = context.obtainStyledAttributes(attrs, R.styleable.BrowserTabsTray, defStyleAttr, 0)
        val tabsTrayStyling = TabsTrayStyling(
            attr.getColor(R.styleable.BrowserTabsTray_tabsTrayItemBackgroundColor, DEFAULT_ITEM_BACKGROUND_COLOR),
            attr.getColor(R.styleable.BrowserTabsTray_tabsTraySelectedItemBackgroundColor,
                DEFAULT_ITEM_BACKGROUND_SELECTED_COLOR),
            attr.getColor(R.styleable.BrowserTabsTray_tabsTrayItemTextColor, DEFAULT_ITEM_TEXT_COLOR),
            attr.getColor(R.styleable.BrowserTabsTray_tabsTraySelectedItemTextColor, DEFAULT_ITEM_TEXT_SELECTED_COLOR),
            attr.getColor(R.styleable.BrowserTabsTray_tabsTrayItemUrlTextColor, DEFAULT_ITEM_TEXT_COLOR),
            attr.getColor(R.styleable.BrowserTabsTray_tabsTraySelectedItemUrlTextColor,
                DEFAULT_ITEM_TEXT_SELECTED_COLOR)
        )
        attr.recycle()

        itemDecoration?.let { addItemDecoration(it) }

        adapter = tabsAdapter.apply {
            styling = tabsTrayStyling
        }

        layoutManager = layout
    }

    /**
     * Convenience method to cast this object to an Android View object.
     */
    override fun asView(): View {
        return this
    }
}
