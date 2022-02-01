/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.appcompat.content.res.AppCompatResources
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.ui.widgets.WidgetSiteItemView
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor
import org.mozilla.fenix.utils.view.ViewHolder
import mozilla.components.feature.tab.collections.Tab as ComponentTab

class TabInCollectionViewHolder(
    private val view: WidgetSiteItemView,
    val interactor: CollectionInteractor,
    private val icons: BrowserIcons = view.context.components.core.icons,
    private val publicSuffixList: PublicSuffixList = view.context.components.publicSuffixList
) : ViewHolder(view) {

    lateinit var collection: TabCollection
        private set
    lateinit var tab: ComponentTab
        private set
    var isLastItem = false
        private set

    init {
        if (SDK_INT >= Build.VERSION_CODES.M) {
            view.foreground = AppCompatResources.getDrawable(
                view.context,
                view.context.theme.resolveAttribute(R.attr.selectableItemBackground)
            )
        }

        // This needs to match the elevation of the CollectionViewHolder for the shadow
        view.elevation = view.resources.getDimension(R.dimen.home_item_elevation)

        view.setOnClickListener {
            interactor.onCollectionOpenTabClicked(tab)
        }

        view.setSecondaryButton(
            icon = R.drawable.ic_close,
            contentDescription = R.string.remove_tab_from_collection
        ) {
            interactor.onCollectionRemoveTab(collection, tab, wasSwiped = false)
        }
    }

    fun bindSession(collection: TabCollection, tab: ComponentTab, isLastTab: Boolean) {
        this.collection = collection
        this.tab = tab
        this.isLastItem = isLastTab
        updateTabUI()
    }

    private fun updateTabUI() {
        view.setText(
            label = tab.title,
            caption = tab.url.toShortUrl(publicSuffixList)
        )

        icons.loadIntoView(view.iconView, tab.url)

        // If last item and we want to change UI for it
        val context = view.context
        if (isLastItem) {
            view.background = AppCompatResources.getDrawable(context, R.drawable.rounded_bottom_corners)
        } else {
            view.setBackgroundColor(context.getColorFromAttr(R.attr.above))
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.site_list_item
    }
}
