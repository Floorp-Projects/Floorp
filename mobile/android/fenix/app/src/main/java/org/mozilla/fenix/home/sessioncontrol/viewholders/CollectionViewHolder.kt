/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.content.Context
import android.view.View
import androidx.core.graphics.BlendModeColorFilterCompat.createBlendModeColorFilterCompat
import androidx.core.graphics.BlendModeCompat.SRC_IN
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.CollectionHomeListRowBinding
import org.mozilla.fenix.utils.view.ViewHolder
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getIconColor
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.removeAndDisable
import org.mozilla.fenix.ext.removeTouchDelegate
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor
import org.mozilla.fenix.theme.ThemeManager

class CollectionViewHolder(
    view: View,
    val interactor: CollectionInteractor
) : ViewHolder(view) {

    private lateinit var collection: TabCollection
    private var expanded = false
    private var collectionMenu: CollectionItemMenu
    private var binding: CollectionHomeListRowBinding

    init {
        binding = CollectionHomeListRowBinding.bind(view)

        collectionMenu = CollectionItemMenu(
            view.context,
            { view.context.components.core.store.state.normalTabs.isNotEmpty() }
        ) {
            when (it) {
                is CollectionItemMenu.Item.DeleteCollection -> interactor.onDeleteCollectionTapped(collection)
                is CollectionItemMenu.Item.AddTab -> interactor.onCollectionAddTabTapped(collection)
                is CollectionItemMenu.Item.RenameCollection -> interactor.onRenameCollectionTapped(collection)
                is CollectionItemMenu.Item.OpenTabs -> interactor.onCollectionOpenTabsTapped(collection)
            }
        }

        binding.collectionOverflowButton.setOnClickListener {
            interactor.onCollectionMenuOpened()
            collectionMenu.menuBuilder
                .build(view.context)
                .show(anchor = it)
        }

        binding.collectionShareButton.setOnClickListener {
            interactor.onCollectionShareTabsClicked(collection)
        }

        view.clipToOutline = true
        view.setOnClickListener {
            interactor.onToggleCollectionExpanded(collection, !expanded)
        }
    }

    fun bindSession(collection: TabCollection, expanded: Boolean) {
        this.collection = collection
        this.expanded = expanded
        updateCollectionUI()
    }

    private fun updateCollectionUI() {
        binding.collectionTitle.text = collection.title

        itemView.isActivated = expanded
        if (expanded) {
            binding.collectionShareButton.apply {
                showAndEnable()
                increaseTapArea(buttonIncreaseDps)
            }
            binding.collectionOverflowButton.apply {
                showAndEnable()
                increaseTapArea(buttonIncreaseDps)
            }
        } else {

            binding.collectionShareButton.apply {
                removeAndDisable()
                removeTouchDelegate()
            }
            binding.collectionOverflowButton.apply {
                removeAndDisable()
                removeTouchDelegate()
            }
        }

        binding.collectionIcon.colorFilter = createBlendModeColorFilterCompat(
            collection.getIconColor(itemView.context),
            SRC_IN
        )
    }

    companion object {
        const val buttonIncreaseDps = 16
        const val LAYOUT_ID = R.layout.collection_home_list_row
        const val maxTitleLength = 20
    }
}

class CollectionItemMenu(
    private val context: Context,
    private val shouldShowAddTab: () -> Boolean,
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
                context.getString(R.string.collection_open_tabs)
            ) {
                onItemTapped.invoke(Item.OpenTabs)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.collection_rename)
            ) {
                onItemTapped.invoke(Item.RenameCollection)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.add_tab)
            ) {
                onItemTapped.invoke(Item.AddTab)
            }.apply { visible = shouldShowAddTab },

            SimpleBrowserMenuItem(
                context.getString(R.string.collection_delete),
                textColorResource = ThemeManager.resolveAttribute(R.attr.textWarning, context)
            ) {
                onItemTapped.invoke(Item.DeleteCollection)
            }
        )
    }
}
