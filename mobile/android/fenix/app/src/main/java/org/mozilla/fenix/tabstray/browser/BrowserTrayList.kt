/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.util.AttributeSet
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.tabs.tabstray.TabsFeature
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.ext.filterFromConfig

class BrowserTrayList @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : RecyclerView(context, attrs, defStyleAttr) {

    /**
     * The browser tab types we would want to show.
     */
    enum class BrowserTabType { NORMAL, PRIVATE }

    lateinit var browserTabType: BrowserTabType
    lateinit var interactor: TabsTrayInteractor
    lateinit var tabsTrayStore: TabsTrayStore

    private val tabsFeature by lazy {
        // NB: The use cases here are duplicated because there isn't a nicer
        // way to share them without a better dependency injection solution.
        val selectTabUseCase = SelectTabUseCaseWrapper(
            context.components.analytics.metrics,
            context.components.useCases.tabsUseCases.selectTab
        ) {
            interactor.onBrowserTabSelected()
        }

        val removeTabUseCase = RemoveTabUseCaseWrapper(
            context.components.analytics.metrics
        ) { sessionId ->
            interactor.onDeleteTab(sessionId)
        }

        TabsFeature(
            adapter as TabsAdapter,
            context.components.core.store,
            selectTabUseCase,
            removeTabUseCase,
            { it.filterFromConfig(browserTabType) },
            { }
        )
    }

    private val swipeToDelete by lazy {
        SwipeToDeleteBinding(tabsTrayStore)
    }

    private val touchHelper by lazy {
        TabsTouchHelper(
            observable = adapter as TabsAdapter,
            onViewHolderTouched = { swipeToDelete.isSwipeable },
            onViewHolderDraw = { context.components.settings.gridTabView.not() }
        )
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        tabsFeature.start()
        swipeToDelete.start()

        adapter?.onAttachedToRecyclerView(this)

        touchHelper.attachToRecyclerView(this)
    }

    @VisibleForTesting
    public override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        tabsFeature.stop()
        swipeToDelete.stop()

        // Notify the adapter that it is released from the view preemptively.
        adapter?.onDetachedFromRecyclerView(this)

        touchHelper.attachToRecyclerView(null)
    }
}
