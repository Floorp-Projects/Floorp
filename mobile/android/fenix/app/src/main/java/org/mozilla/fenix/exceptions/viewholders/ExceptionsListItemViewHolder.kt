/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.exception_item.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsAction
import org.mozilla.fenix.exceptions.ExceptionsItem
import org.mozilla.fenix.ext.components
import kotlin.coroutines.CoroutineContext

class ExceptionsListItemViewHolder(
    view: View,
    private val actionEmitter: Observer<ExceptionsAction>,
    val job: Job
) : RecyclerView.ViewHolder(view), CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    private val favicon = view.favicon_image
    private val url = view.domainView
    private val deleteButton = view.delete_exception

    private var item: ExceptionsItem? = null

    init {
        deleteButton.setOnClickListener {
            item?.let {
                actionEmitter.onNext(ExceptionsAction.Delete.One(it))
            }
        }
    }

    fun bind(item: ExceptionsItem) {
        this.item = item
        url.text = item.url
        updateFavIcon(item.url)
    }

    private fun updateFavIcon(url: String) {
        launch(Dispatchers.IO) {
            val bitmap = favicon.context.components.core.icons
                .loadIcon(IconRequest(url)).await().bitmap
            launch(Dispatchers.Main) {
                favicon.setImageBitmap(bitmap)
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.exception_item
    }
}
