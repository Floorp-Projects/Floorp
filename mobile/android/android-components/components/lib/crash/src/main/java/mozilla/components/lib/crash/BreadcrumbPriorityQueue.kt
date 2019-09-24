/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import java.util.PriorityQueue

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
