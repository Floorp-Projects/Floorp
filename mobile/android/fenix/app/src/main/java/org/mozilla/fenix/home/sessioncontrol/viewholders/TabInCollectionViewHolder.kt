/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.graphics.Outline
import android.view.View
import android.view.ViewOutlineProvider
import androidx.appcompat.content.res.AppCompatResources
import kotlinx.android.synthetic.main.list_element.*
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.util.dpToFloat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.ViewHolder
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor
import mozilla.components.feature.tab.collections.Tab as ComponentTab

class TabInCollectionViewHolder(
    view: View,
    val interactor: CollectionInteractor,
    private val differentLastItem: Boolean = false
) : ViewHolder(view) {

    lateinit var collection: TabCollection
        private set
    lateinit var tab: ComponentTab
        private set
    var isLastItem = false

    init {
        list_item_favicon.clipToOutline = true
        list_item_favicon.outlineProvider = object : ViewOutlineProvider() {
            override fun getOutline(view: View, outline: Outline?) {
                outline?.setRoundRect(
                    0,
                    0,
                    view.width,
                    view.height,
                    FAV_ICON_BORDER_RADIUS_IN_DP.dpToFloat(view.context.resources.displayMetrics)
                )
            }
        }

        itemView.setOnClickListener {
            interactor.onCollectionOpenTabClicked(tab)
        }

        list_item_action_button.increaseTapArea(buttonIncreaseDps)
        list_item_action_button.setOnClickListener {
            interactor.onCollectionRemoveTab(collection, tab)
        }
    }

    fun bindSession(collection: TabCollection, tab: ComponentTab, isLastTab: Boolean) {
        this.collection = collection
        this.tab = tab
        this.isLastItem = isLastTab
        updateTabUI()
    }

    private fun updateTabUI() {
        val context = itemView.context
        list_item_url.text = tab.url.toShortUrl(context.components.publicSuffixList)

        list_element_title.text = tab.title
        list_item_favicon.context.components.core.icons.loadIntoView(list_item_favicon, tab.url)

        // If last item and we want to change UI for it
        if (isLastItem && differentLastItem) {
            itemView.background = AppCompatResources.getDrawable(context, R.drawable.rounded_bottom_corners)
        } else {
            itemView.setBackgroundColor(context.getColorFromAttr(R.attr.above))
        }
    }

    companion object {
        const val buttonIncreaseDps = 12
        const val LAYOUT_ID = R.layout.list_element
        const val FAV_ICON_BORDER_RADIUS_IN_DP = 4
    }
}
