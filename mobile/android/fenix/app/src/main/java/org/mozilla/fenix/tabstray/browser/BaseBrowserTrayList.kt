/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.util.AttributeSet
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TrayItem
import org.mozilla.fenix.tabstray.ext.filterFromConfig
import org.mozilla.fenix.utils.view.LifecycleViewProvider

abstract class BaseBrowserTrayList @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : RecyclerView(context, attrs, defStyleAttr), TrayItem {

    /**
     * The browser tab types we would want to show.
     */
    enum class BrowserTabType { NORMAL, PRIVATE }

    /**
     * A configuration for classes that extend [BaseBrowserTrayList].
     */
    data class Configuration(val browserTabType: BrowserTabType)

    abstract val configuration: Configuration

    var interactor: TabsTrayInteractor? = null

    private val lifecycleProvider = LifecycleViewProvider(this)

    private val selectTabUseCase = SelectTabUseCaseWrapper(
        context.components.analytics.metrics,
        context.components.useCases.tabsUseCases.selectTab
    ) {
        interactor?.navigateToBrowser()
    }

    private val removeTabUseCase = RemoveTabUseCaseWrapper(
        context.components.analytics.metrics
    ) { sessionId ->
        interactor?.tabRemoved(sessionId)
    }

    private val tabsFeature by lazy {
        ViewBoundFeatureWrapper(
            feature = TabsFeature(
                adapter as TabsAdapter,
                context.components.core.store,
                selectTabUseCase,
                removeTabUseCase,
                { it.filterFromConfig(configuration) },
                { }
            ),
            owner = lifecycleProvider,
            view = this
        )
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        // This is weird, but I don't have a better solution right now: We need to keep a
        // lazy reference to the feature/adapter so that we do not re-create
        // it every time it's attached. This reference is our way to init.
        tabsFeature
    }
}
