/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.util.TypedValue
import android.view.View
import android.view.ViewGroup
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.toolbar.Toolbar
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.theme.ThemeManager
import java.lang.ref.WeakReference

/**
 * A [Toolbar.Action] implementation that shows a [TabCounter].
 */
class TabCounterToolbarButton(
    private val sessionManager: SessionManager,
    private val isPrivate: Boolean,
    private val onItemTapped: (TabCounterMenuItem) -> Unit = {},
    private val showTabs: () -> Unit
) : Toolbar.Action {
    private var reference: WeakReference<TabCounter> = WeakReference<TabCounter>(null)

    override fun createView(parent: ViewGroup): View {
        sessionManager.register(sessionManagerObserver, view = parent)

        val view = TabCounter(parent.context).apply {
            reference = WeakReference(this)
            setOnClickListener {
                showTabs.invoke()
            }

            setOnLongClickListener {
                getTabContextMenu(it.context).show(it)
                true
            }

            addOnAttachStateChangeListener(object : View.OnAttachStateChangeListener {
                override fun onViewAttachedToWindow(v: View?) {
                    setCount(sessionManager.sessionsOfType(private = isPrivate).count())
                }

                override fun onViewDetachedFromWindow(v: View?) { /* no-op */ }
            })
        }

        // Set selectableItemBackgroundBorderless
        val outValue = TypedValue()
        parent.context.theme.resolveAttribute(
            android.R.attr.selectableItemBackgroundBorderless,
            outValue,
            true
        )
        view.setBackgroundResource(outValue.resourceId)
        return view
    }

    override fun bind(view: View) = Unit

    private fun updateCount() {
        val count = sessionManager.sessionsOfType(private = isPrivate).count()

        reference.get()?.let {
            it.setCountWithAnimation(count)
        }
    }

    private fun getTabContextMenu(context: Context): BrowserMenu {
        val primaryTextColor = ThemeManager.resolveAttribute(R.attr.primaryText, context)
        val menuItems = listOf(
            BrowserMenuImageText(
                label = context.getString(R.string.close_tab),
                imageResource = R.drawable.ic_close,
                iconTintColorResource = primaryTextColor,
                textColorResource = primaryTextColor
            ) {
                onItemTapped(TabCounterMenuItem.CloseTab)
            },
            BrowserMenuDivider(),
            BrowserMenuImageText(
                label = context.getString(R.string.browser_menu_new_tab),
                imageResource = R.drawable.ic_new,
                iconTintColorResource = primaryTextColor,
                textColorResource = primaryTextColor
            ) {
                onItemTapped(TabCounterMenuItem.NewTab(false))
            },
            BrowserMenuImageText(
                label = context.getString(R.string.home_screen_shortcut_open_new_private_tab_2),
                imageResource = R.drawable.ic_private_browsing,
                iconTintColorResource = primaryTextColor,
                textColorResource = primaryTextColor
            ) {
                onItemTapped(TabCounterMenuItem.NewTab(true))
            }
        )
        return BrowserMenuBuilder(menuItems).build(context)
    }

    private val sessionManagerObserver = object : SessionManager.Observer {
        override fun onSessionAdded(session: Session) {
            updateCount()
        }

        override fun onSessionRemoved(session: Session) {
            updateCount()
        }

        override fun onSessionsRestored() {
            updateCount()
        }

        override fun onAllSessionsRemoved() {
            updateCount()
        }
    }
}
