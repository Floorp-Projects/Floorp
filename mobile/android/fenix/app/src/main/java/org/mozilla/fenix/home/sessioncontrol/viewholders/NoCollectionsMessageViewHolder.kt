/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import kotlinx.android.synthetic.main.no_collections_message.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.ViewHolder
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor

open class NoCollectionsMessageViewHolder(
    view: View,
    interactor: CollectionInteractor
) : ViewHolder(view) {

    init {
        view.add_tabs_to_collections_button.setOnClickListener {
            interactor.onAddTabsToCollectionTapped()
        }
    }
    companion object {
        const val LAYOUT_ID = R.layout.no_collections_message
    }
}
