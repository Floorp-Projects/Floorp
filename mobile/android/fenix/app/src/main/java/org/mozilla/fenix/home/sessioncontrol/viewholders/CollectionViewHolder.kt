/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.content.Context
import android.graphics.PorterDuff.Mode.SRC_IN
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.collection_home_list_row.*
import kotlinx.android.synthetic.main.collection_home_list_row.view.*
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.components.description
import org.mozilla.fenix.ext.getIconColor
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.home.sessioncontrol.CollectionAction
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.TabCollection
import org.mozilla.fenix.home.sessioncontrol.onNext

class CollectionViewHolder(
    val view: View,
    val actionEmitter: Observer<SessionControlAction>,
    override val containerView: View? = view
) :
    RecyclerView.ViewHolder(view), LayoutContainer {

    private lateinit var collection: TabCollection
    private var expanded = false
    private var sessionHasOpenTabs = false
    private var collectionMenu: CollectionItemMenu

    init {
        collectionMenu = CollectionItemMenu(view.context, sessionHasOpenTabs) {
            when (it) {
                is CollectionItemMenu.Item.DeleteCollection -> actionEmitter.onNext(CollectionAction.Delete(collection))
                is CollectionItemMenu.Item.AddTab -> actionEmitter.onNext(CollectionAction.AddTab(collection))
                is CollectionItemMenu.Item.RenameCollection -> actionEmitter.onNext(CollectionAction.Rename(collection))
                is CollectionItemMenu.Item.OpenTabs -> actionEmitter.onNext(CollectionAction.OpenTabs(collection))
            }
        }

        collection_overflow_button.run {
            increaseTapArea(buttonIncreaseDps)
            setOnClickListener {
                collectionMenu.menuBuilder
                    .build(view.context)
                    .show(anchor = it)
            }
        }

        collection_share_button.run {
            increaseTapArea(buttonIncreaseDps)
            setOnClickListener {
                actionEmitter.onNext(CollectionAction.ShareTabs(collection))
            }
        }

        view.clipToOutline = true
        view.setOnClickListener {
            handleExpansion(expanded)
        }
    }

    fun bindSession(collection: TabCollection, expanded: Boolean, sessionHasOpenTabs: Boolean) {
        this.collection = collection
        this.expanded = expanded
        this.sessionHasOpenTabs = sessionHasOpenTabs
        collectionMenu.sessionHasOpenTabs = sessionHasOpenTabs
        updateCollectionUI()
    }

    private fun updateCollectionUI() {
        view.collection_title.text = collection.title
        view.collection_description.text = collection.description(view.context)
        val layoutParams = view.layoutParams as ViewGroup.MarginLayoutParams

        view.isActivated = expanded
        if (expanded) {
            layoutParams.bottomMargin = 0
            collection_title.setPadding(0, 0, 0, EXPANDED_PADDING)
            view.collection_description.visibility = View.GONE
        } else {
            layoutParams.bottomMargin = COLLAPSED_MARGIN
            view.collection_description.visibility = View.VISIBLE
        }

        view.collection_icon.setColorFilter(
            collection.getIconColor(view.context),
            SRC_IN
        )
    }

    private fun handleExpansion(isExpanded: Boolean) {
        if (isExpanded) {
            actionEmitter.onNext(CollectionAction.Collapse(collection))
        } else {
            actionEmitter.onNext(CollectionAction.Expand(collection))
        }
    }

    companion object {
        const val buttonIncreaseDps = 16
        const val EXPANDED_PADDING = 60
        const val COLLAPSED_MARGIN = 12
        const val LAYOUT_ID = R.layout.collection_home_list_row
        const val maxTitleLength = 20
    }
}

class CollectionItemMenu(
    private val context: Context,
    var sessionHasOpenTabs: Boolean,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
        object DeleteCollection : Item()
        object AddTab : Item()
        object RenameCollection : Item()
        object OpenTabs : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(
                context.getString(R.string.collection_delete),
                textColorResource = ThemeManager.resolveAttribute(R.attr.destructive, context)
            ) {
                onItemTapped.invoke(Item.DeleteCollection)
            },
            SimpleBrowserMenuItem(
                context.getString(R.string.add_tab)
            ) {
                onItemTapped.invoke(Item.AddTab)
            }.apply { visible = { sessionHasOpenTabs } },
            SimpleBrowserMenuItem(
                context.getString(R.string.collection_rename)
            ) {
                onItemTapped.invoke(Item.RenameCollection)
            },
            SimpleBrowserMenuItem(
                context.getString(R.string.collection_open_tabs)
            ) {
                onItemTapped.invoke(Item.OpenTabs)
            }
        )
    }
}
