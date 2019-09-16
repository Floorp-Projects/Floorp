package mozilla.components.lib.crash

import java.util.PriorityQueue
import kotlin.collections.ArrayList

internal class BreadcrumbPriorityQueue(
    private val maxSize: Int
) : PriorityQueue<Breadcrumb>() {
    @Synchronized
    override fun add(element: Breadcrumb?): Boolean {
        val result = super.add(element)
        if (this.size > maxSize) {
            this.poll()
        }
        return result
    }

    @Synchronized
    fun toSortedArrayList(): ArrayList<Breadcrumb> {
        val breadcrumbsArrayList: ArrayList<Breadcrumb> = arrayListOf()
        if (isNotEmpty()) {
            breadcrumbsArrayList.addAll(this)

            /* Sort by timestamp before reporting */
            breadcrumbsArrayList.sortBy { it.date }
        }
        return breadcrumbsArrayList
    }
}
