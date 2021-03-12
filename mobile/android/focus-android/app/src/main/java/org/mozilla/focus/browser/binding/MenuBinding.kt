package org.mozilla.focus.browser.binding

import android.graphics.drawable.Drawable
import android.view.View
import android.widget.ImageButton
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.focus.ext.ifCustomTab
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.menu.browser.BrowserMenu
import java.lang.ref.WeakReference

class MenuBinding(
    private val fragment: BrowserFragment,
    store: BrowserStore,
    private val tabId: String?,
    private val menuView: ImageButton
) : AbstractBinding(store), View.OnClickListener {
    private var menuReference: WeakReference<BrowserMenu> = WeakReference<BrowserMenu>(null)

    init {
        menuView.setOnClickListener(this)
    }

    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
            .ifAnyChanged { tab -> arrayOf(tab.trackingProtection.blockedTrackers, tab.content.loading) }
            .collect { tab -> onStateChanged(tab) }
    }

    private fun onStateChanged(tab: SessionState) {
        val menu = menuReference.get() ?: return

        menu.updateTrackers(tab.trackingProtection.blockedTrackers.size)
        menu.updateLoading(tab.content.loading)
    }

    // LifecycleAwareFeature does not tell us about pause/resume yet. So we pass that to the
    // binding manually here.
    fun pause() {
        menuReference.get()?.let { menu ->
            menu.dismiss()
            menuReference.clear()
        }
    }

    fun updateIcon(drawable: Drawable) {
        menuView.setImageDrawable(drawable)
    }

    override fun onClick(view: View) {
        val menu = BrowserMenu(fragment.requireContext(), fragment, fragment.tab.ifCustomTab()?.config)
        menu.show(menuView)

        menuReference = WeakReference(menu)
    }

    fun showMenuButton() {
        menuView.visibility = View.VISIBLE
    }

    fun hideMenuButton() {
        menuView.visibility = View.GONE
    }
}
