/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_SEND
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.app_share_list_item.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.AdapterWithJob
import kotlin.coroutines.CoroutineContext

class AppShareRecyclerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : RecyclerView(context, attrs, defStyleAttr) {

    init {
        layoutManager = GridLayoutManager(context, 2, GridLayoutManager.HORIZONTAL, false)
    }
}

class AppShareAdapter(
    private val context: Context,
    val actionEmitter: Observer<ShareAction>,
    private val intentType: String = "text/plain"
) : AdapterWithJob<AppShareItemViewHolder>(), CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + adapterJob
    private var size: Int = 0
    private val shareItems: MutableList<ShareItem> = mutableListOf()

    init {
        val testIntent = Intent(ACTION_SEND).apply {
            type = intentType
            flags = FLAG_ACTIVITY_NEW_TASK
        }

        launch {
            val activities = context.packageManager.queryIntentActivities(testIntent, 0)

            val items = activities.map { resolveInfo ->
                ShareItem(
                    resolveInfo.loadLabel(context.packageManager).toString(),
                    resolveInfo.loadIcon(context.packageManager),
                    resolveInfo.activityInfo.packageName,
                    resolveInfo.activityInfo.name
                )
            }

            size = activities.size
            shareItems.addAll(items)

            // Notify adapter on the UI thread when the dataset is populated.
            withContext(Dispatchers.Main) {
                notifyDataSetChanged()
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AppShareItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(AppShareItemViewHolder.LAYOUT_ID, parent, false)
        return AppShareItemViewHolder(view, actionEmitter)
    }

    override fun getItemCount(): Int = size

    override fun onBindViewHolder(holder: AppShareItemViewHolder, position: Int) {
        holder.bind(shareItems[position])
    }
}

class AppShareItemViewHolder(
    itemView: View,
    actionEmitter: Observer<ShareAction>
) : RecyclerView.ViewHolder(itemView) {

    private var shareItem: ShareItem? = null

    init {
        itemView.setOnClickListener {
            shareItem?.let {
                actionEmitter.onNext(ShareAction.ShareAppClicked(it))
            }
        }
    }

    internal fun bind(item: ShareItem) {
        shareItem = item
        itemView.app_name.text = item.name
        itemView.app_icon.setImageDrawable(item.icon)
    }

    companion object {
        const val LAYOUT_ID = R.layout.app_share_list_item
    }
}

data class ShareItem(val name: String, val icon: Drawable, val packageName: String, val activityName: String)
