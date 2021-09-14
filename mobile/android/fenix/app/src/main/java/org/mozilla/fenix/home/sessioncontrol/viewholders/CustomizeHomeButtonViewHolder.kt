/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.CustomizeHomeListItemBinding
import org.mozilla.fenix.home.sessioncontrol.CustomizeHomeIteractor

class CustomizeHomeButtonViewHolder(
    view: View,
    private val interactor: CustomizeHomeIteractor
) : RecyclerView.ViewHolder(view) {

    init {
        val binding = CustomizeHomeListItemBinding.bind(view)

        binding.customizeHome.setOnClickListener {
            interactor.openCustomizeHomePage()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.customize_home_list_item
    }
}
