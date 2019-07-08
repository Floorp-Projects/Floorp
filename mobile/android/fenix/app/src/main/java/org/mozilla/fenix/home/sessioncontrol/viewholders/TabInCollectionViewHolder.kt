/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.graphics.Outline
import android.view.View
import android.view.ViewOutlineProvider
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.tab_in_collection.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.ktx.android.util.dpToFloat
import org.jetbrains.anko.backgroundColor
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.urlToTrimmedHost
import org.mozilla.fenix.home.sessioncontrol.CollectionAction
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.TabCollection
import org.mozilla.fenix.home.sessioncontrol.onNext
import kotlin.coroutines.CoroutineContext
import mozilla.components.feature.tab.collections.Tab as ComponentTab

class TabInCollectionViewHolder(
    val view: View,
    val actionEmitter: Observer<SessionControlAction>,
    val job: Job,
    override val containerView: View? = view
) : RecyclerView.ViewHolder(view), LayoutContainer, CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    lateinit var collection: TabCollection
        private set
    lateinit var tab: ComponentTab
        private set
    var isLastTab = false

    init {
        collection_tab_icon.clipToOutline = true
        collection_tab_icon.outlineProvider = object : ViewOutlineProvider() {
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
            actionEmitter.onNext(CollectionAction.OpenTab(tab))
        }

        collection_tab_close_button.increaseTapArea(buttonIncreaseDps)
        collection_tab_close_button.setOnClickListener {
            actionEmitter.onNext(CollectionAction.RemoveTab(collection, tab))
        }
    }

    fun bindSession(collection: TabCollection, tab: ComponentTab, isLastTab: Boolean) {
        this.collection = collection
        this.tab = tab
        this.isLastTab = isLastTab
        updateTabUI()
    }

    private fun updateTabUI() {
        collection_tab_hostname.text = tab.url.urlToTrimmedHost(view.context)

        collection_tab_title.text = tab.title
        launch(Dispatchers.IO) {
            val bitmap = collection_tab_icon.context.components.core.icons
                .loadIcon(IconRequest(tab.url)).await().bitmap
            launch(Dispatchers.Main) {
                collection_tab_icon.setImageBitmap(bitmap)
            }
        }

        // If I'm the last one...
        if (isLastTab) {
            view.background = ContextCompat.getDrawable(view.context, R.drawable.rounded_bottom_corners)
            divider_line.visibility = View.GONE
        } else {
            view.backgroundColor = R.attr.above.getColorFromAttr(view.context)
            divider_line.visibility = View.VISIBLE
        }
    }

    companion object {
        const val buttonIncreaseDps = 12
        const val LAYOUT_ID = R.layout.tab_in_collection
    }
}
