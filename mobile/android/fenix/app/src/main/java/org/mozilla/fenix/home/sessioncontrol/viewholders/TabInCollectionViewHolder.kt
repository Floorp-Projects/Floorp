/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.graphics.Outline
import android.view.View
import android.view.ViewOutlineProvider
import androidx.appcompat.content.res.AppCompatResources
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.list_element.divider_line
import kotlinx.android.synthetic.main.list_element.list_element_title
import kotlinx.android.synthetic.main.list_element.list_item_close_button
import kotlinx.android.synthetic.main.list_element.list_item_icon
import kotlinx.android.synthetic.main.list_element.list_item_url
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.util.dpToFloat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor
import mozilla.components.feature.tab.collections.Tab as ComponentTab

class TabInCollectionViewHolder(
    val view: View,
    val interactor: CollectionInteractor,
    override val containerView: View? = view
) : RecyclerView.ViewHolder(view), LayoutContainer {

    lateinit var collection: TabCollection
        private set
    lateinit var tab: ComponentTab
        private set
    var isLastTab = false

    init {
        list_item_icon.clipToOutline = true
        list_item_icon.outlineProvider = object : ViewOutlineProvider() {
            override fun getOutline(view: View, outline: Outline?) {
                outline?.setRoundRect(
                    0,
                    0,
                    view.width,
                    view.height,
                    TabViewHolder.favIconBorderRadiusInPx.dpToFloat(view.context.resources.displayMetrics)
                )
            }
        }

        view.setOnClickListener {
            interactor.onCollectionOpenTabClicked(tab)
        }

        list_item_close_button.increaseTapArea(buttonIncreaseDps)
        list_item_close_button.setOnClickListener {
            interactor.onCollectionRemoveTab(collection, tab)
        }
    }

    fun bindSession(collection: TabCollection, tab: ComponentTab, isLastTab: Boolean) {
        this.collection = collection
        this.tab = tab
        this.isLastTab = isLastTab
        updateTabUI()
    }

    private fun updateTabUI() {
        list_item_url.text = tab.url.toShortUrl(view.context.components.publicSuffixList)

        list_element_title.text = tab.title
        list_item_icon.context.components.core.icons.loadIntoView(list_item_icon, tab.url)

        // If I'm the last one...
        if (isLastTab) {
            view.background = AppCompatResources.getDrawable(view.context, R.drawable.rounded_bottom_corners)
            divider_line.visibility = View.GONE
        } else {
            view.setBackgroundColor(view.context.getColorFromAttr(R.attr.above))
            divider_line.visibility = View.VISIBLE
        }
    }

    companion object {
        const val buttonIncreaseDps = 12
        const val LAYOUT_ID = R.layout.list_element
    }
}
