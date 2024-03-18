/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.support.test.mock

/**
 * A default implementation of [StickyItemsAdapter] to be used in tests.
 */
class FakeStickyItemsAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder>(), StickyItemsAdapter {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder =
        mock()

    override fun getItemCount(): Int = 42

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {}

    override fun isStickyItem(position: Int): Boolean = false
}
