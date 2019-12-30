/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.topsites

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.top_site_item.view.*
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor

class TopSiteItemViewHolder(
    private val view: View,
    private val interactor: TopSiteInteractor
) : RecyclerView.ViewHolder(view) {
    private lateinit var topSite: TopSite

    init {
        view.top_site_item.setOnClickListener {
            interactor.onSelectTopSite(topSite.url)
        }
    }

    fun bind(topSite: TopSite) {
        this.topSite = topSite
        view.top_site_title.text = topSite.title
        view.context.components.core.icons.loadIntoView(view.favicon_image, topSite.url)
    }

    companion object {
        const val LAYOUT_ID = R.layout.top_site_item
    }
}
