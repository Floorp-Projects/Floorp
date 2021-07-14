/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.util.AttributeSet
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.tabs.tabstray.TabsFeature
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * The base class for a tabs tray list that wants to display browser tabs.
 */
abstract class AbstractBrowserTrayList @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : RecyclerView(context, attrs, defStyleAttr) {

    lateinit var interactor: TabsTrayInteractor
    lateinit var tabsTrayStore: TabsTrayStore

    /**
     * A [TabsFeature] is required for each browser list to ensure one always exists for displaying
     * tabs.
     */
    abstract val tabsFeature: TabsFeature

    // NB: The use cases here are duplicated because there isn't a nicer
    // way to share them without a better dependency injection solution.
    protected val selectTabUseCase = SelectTabUseCaseWrapper(
        context.components.analytics.metrics,
        context.components.useCases.tabsUseCases.selectTab
    ) {
        interactor.onBrowserTabSelected()
    }

    protected val removeTabUseCase = RemoveTabUseCaseWrapper(
        context.components.analytics.metrics
    ) { sessionId ->
        interactor.onDeleteTab(sessionId)
    }

    protected val swipeToDelete by lazy {
        SwipeToDeleteBinding(tabsTrayStore)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        swipeToDelete.start()

        adapter?.onAttachedToRecyclerView(this)
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        swipeToDelete.stop()

        // Notify the adapter that it is released from the view preemptively.
        adapter?.onDetachedFromRecyclerView(this)
    }
}
