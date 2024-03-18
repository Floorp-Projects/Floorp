/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.toolbar

import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.tabs.R
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.ui.tabcounter.TabCounter
import mozilla.components.ui.tabcounter.TabCounterMenu
import java.lang.ref.WeakReference

/**
 * A [Toolbar.Action] implementation that shows a [TabCounter].
 */
open class TabCounterToolbarButton(
    private val lifecycleOwner: LifecycleOwner,
    private val countBasedOnSelectedTabType: Boolean = true,
    private val showTabs: () -> Unit,
    private val store: BrowserStore,
    private val menu: TabCounterMenu? = null,
    private val showMaskInPrivateMode: Boolean = false,
) : Toolbar.Action {

    private var reference = WeakReference<TabCounter>(null)

    override fun createView(parent: ViewGroup): View {
        store.flowScoped(lifecycleOwner) { flow ->
            flow.map { state -> getTabCount(state) }
                .distinctUntilChanged()
                .collect {
                        tabs ->
                    updateCount(tabs)
                }
        }

        val tabCounter = TabCounter(parent.context).apply {
            reference = WeakReference(this)
            setOnClickListener {
                showTabs.invoke()
            }

            menu?.let { menu ->
                setOnLongClickListener {
                    menu.menuController.show(anchor = it)
                    true
                }
            }

            addOnAttachStateChangeListener(
                object : View.OnAttachStateChangeListener {
                    override fun onViewAttachedToWindow(v: View) {
                        setCount(getTabCount(store.state))
                    }

                    override fun onViewDetachedFromWindow(v: View) { /* no-op */ }
                },
            )

            contentDescription = parent.context.getString(R.string.mozac_feature_tabs_toolbar_tabs_button)

            toggleCounterMask(showMaskInPrivateMode && isPrivate(store))
        }

        // Set selectableItemBackgroundBorderless
        tabCounter.setBackgroundResource(
            parent.context.theme.resolveAttribute(
                android.R.attr.selectableItemBackgroundBorderless,
            ),
        )

        return tabCounter
    }

    override fun bind(view: View) = Unit

    private fun getTabCount(state: BrowserState): Int {
        return if (countBasedOnSelectedTabType) {
            state.getNormalOrPrivateTabs(isPrivate(store)).size
        } else {
            state.tabs.size
        }
    }

    /**
     * Update the tab counter button on the toolbar.
     *
     * @property count the updated tab count
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun updateCount(count: Int) {
        reference.get()?.setCountWithAnimation(count)
    }

    /**
     * Check if the selected tab is private.
     *
     * @property store the [BrowserStore] associated with this instance
     */
    fun isPrivate(store: BrowserStore): Boolean {
        return store.state.selectedTab?.content?.private ?: false
    }
}
