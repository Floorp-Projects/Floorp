/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.util.AttributeSet
import androidx.recyclerview.widget.ConcatAdapter
import mozilla.components.browser.tabstray.TabViewHolder
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.tabstray.ext.browserAdapter
import org.mozilla.fenix.tabstray.ext.inactiveTabsAdapter
import org.mozilla.fenix.tabstray.ext.tabGroupAdapter
import org.mozilla.fenix.tabstray.ext.titleHeaderAdapter

class NormalBrowserTrayList @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AbstractBrowserTrayList(context, attrs, defStyleAttr) {

    private val concatAdapter by lazy { adapter as ConcatAdapter }

    private val inactiveTabsBinding by lazy {
        InactiveTabsBinding(tabsTrayStore, concatAdapter.inactiveTabsAdapter)
    }

    private val normalTabsBinding by lazy {
        NormalTabsBinding(tabsTrayStore, context.components.core.store, concatAdapter.browserAdapter)
    }

    private val titleHeaderBinding by lazy {
        OtherHeaderBinding(tabsTrayStore) { concatAdapter.titleHeaderAdapter.handleListChanges(it) }
    }

    private val tabGroupBinding by lazy {
        TabGroupBinding(tabsTrayStore) { concatAdapter.tabGroupAdapter.submitList(it) }
    }

    private val inactiveTabsInteractor by lazy {
        DefaultInactiveTabsInteractor(
            InactiveTabsController(
                tabsTrayStore,
                context.components.appStore,
                concatAdapter.inactiveTabsAdapter,
                context.settings()
            )
        )
    }

    private val touchHelper by lazy {
        TabsTouchHelper(
            interactionDelegate = concatAdapter.browserAdapter.interactor,
            onViewHolderTouched = {
                it is TabViewHolder && swipeToDelete.isSwipeable
            },
            onViewHolderDraw = { context.components.settings.gridTabView.not() },
            featureNameHolder = concatAdapter.browserAdapter
        )
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        concatAdapter.inactiveTabsAdapter.inactiveTabsInteractor = inactiveTabsInteractor

        inactiveTabsBinding.start()
        normalTabsBinding.start()
        titleHeaderBinding.start()
        tabGroupBinding.start()

        touchHelper.attachToRecyclerView(this)
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        inactiveTabsBinding.stop()
        normalTabsBinding.stop()
        titleHeaderBinding.stop()
        tabGroupBinding.stop()

        touchHelper.attachToRecyclerView(null)
    }
}
