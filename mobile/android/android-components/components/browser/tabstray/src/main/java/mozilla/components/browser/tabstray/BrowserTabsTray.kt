/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.content.Context
import android.support.v7.widget.GridLayoutManager
import android.support.v7.widget.RecyclerView
import android.util.AttributeSet
import android.view.View
import mozilla.components.concept.tabstray.TabsTray

/**
 * A customizable tabs tray for browsers.
 */
class BrowserTabsTray @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
    private val tabsAdapter: TabsAdapter = TabsAdapter()
) : RecyclerView(context, attrs, defStyleAttr),
    TabsTray by tabsAdapter {

    init {
        tabsAdapter.tabsTray = this

        layoutManager = GridLayoutManager(context, 2)
        adapter = tabsAdapter
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        // Multiple tabs that we are currently displaying may be subscribed to a Session to update
        // automatically. Unsubscribe all of them now so that we do not reference them and keep them
        // in memory.
        tabsAdapter.unsubscribeHolders()
    }

    /**
     * Convenience method to cast this object to an Android View object.
     */
    override fun asView(): View {
        return this
    }
}
