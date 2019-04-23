package org.mozilla.fenix.collections

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.collection_tab_list_row.view.*
import kotlinx.android.synthetic.main.tab_list_row.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import kotlin.coroutines.CoroutineContext

class CollectionCreationTabListAdapter(
    val actionEmitter: Observer<CollectionCreationAction>
) : RecyclerView.Adapter<TabViewHolder>() {


    private var data: List<Tab> = listOf()
    private lateinit var job: Job

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(TabViewHolder.LAYOUT_ID, parent, false)

        return TabViewHolder(view, actionEmitter, job)
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        holder.bind(data[position])
    }

    override fun getItemCount(): Int = data.size

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        job = Job()
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        job.cancel()
    }

    fun updateData(tabs: List<Tab>) {
        data = tabs
        notifyDataSetChanged()
    }
}

class TabViewHolder(
    val view: View,
    actionEmitter: Observer<CollectionCreationAction>,
    val job: Job
) :
    RecyclerView.ViewHolder(view), CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    var tab: Tab? = null

    init { }

    fun bind(tab: Tab) {
        this.tab = tab

        view.hostname.text = tab.hostname
        view.tab_title.text = tab.title
        launch(Dispatchers.IO) {
            val bitmap = view.favicon_image.context.components.utils.icons
                .loadIcon(IconRequest(tab.url)).await().bitmap
            launch(Dispatchers.Main) {
                view.favicon_image.setImageBitmap(bitmap)
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.collection_tab_list_row
    }
}