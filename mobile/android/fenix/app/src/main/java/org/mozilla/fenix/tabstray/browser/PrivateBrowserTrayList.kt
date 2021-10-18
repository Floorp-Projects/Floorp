/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.util.AttributeSet
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PACKAGE_PRIVATE
import mozilla.components.feature.tabs.tabstray.TabsFeature
import org.mozilla.fenix.ext.components

class PrivateBrowserTrayList @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AbstractBrowserTrayList(context, attrs, defStyleAttr) {

    override val tabsFeature by lazy {
        // NB: The use cases here are duplicated because there isn't a nicer
        // way to share them without a better dependency injection solution.
        TabsFeature(
            adapter as BrowserTabsAdapter,
            context.components.core.store,
        ) { it.content.private }
    }
    private val touchHelper by lazy {
        TabsTouchHelper(
            interactionDelegate = (adapter as BrowserTabsAdapter).delegate,
            onViewHolderTouched = { swipeToDelete.isSwipeable },
            onViewHolderDraw = { context.components.settings.gridTabView.not() },
            featureNameHolder = (adapter as BrowserTabsAdapter)
        )
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        tabsFeature.start()
        swipeToDelete.start()

        adapter?.onAttachedToRecyclerView(this)

        touchHelper.attachToRecyclerView(this)
    }

    @VisibleForTesting(otherwise = PACKAGE_PRIVATE)
    public override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        tabsFeature.stop()
        swipeToDelete.stop()

        // Notify the adapter that it is released from the view preemptively.
        adapter?.onDetachedFromRecyclerView(this)

        touchHelper.attachToRecyclerView(null)
    }
}
