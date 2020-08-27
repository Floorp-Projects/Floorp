package org.mozilla.fenix.helpers

import androidx.test.espresso.IdlingResource
import androidx.test.espresso.IdlingResource.ResourceCallback

class RecyclerViewIdlingResource constructor(private val recycler: androidx.recyclerview.widget.RecyclerView, val minItemCount: Int = 0) :
    IdlingResource {

    private var callback: ResourceCallback? = null

    override fun isIdleNow(): Boolean {
        if (recycler.adapter != null && recycler.adapter!!.itemCount > minItemCount) {
            if (callback != null) {
                callback!!.onTransitionToIdle()
            }
            return true
        }
        return false
    }

    override fun registerIdleTransitionCallback(callback: ResourceCallback) {
        this.callback = callback
    }

    override fun getName(): String {
        return RecyclerViewIdlingResource::class.java.name + ":" + recycler.id
    }
}
