package org.mozilla.fenix.utils

import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.Job

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * [RecyclerView.Adapter] with a [Job] for coroutines.
 * The adapterJob is setup when the adapter is initialized to a RecyclerView and canceled when detached.
 */
abstract class AdapterWithJob<VH : RecyclerView.ViewHolder> : RecyclerView.Adapter<VH>() {
    protected var adapterJob: Job = Job()

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        adapterJob.cancel()
    }
}

/**
 * [ListAdapter] with a [Job] for coroutines.
 * The adapterJob is setup when the adapter is initialized to a RecyclerView and canceled when detached.
 */
abstract class ListAdapterWithJob<T, VH : RecyclerView.ViewHolder>(
    diffCallback: DiffUtil.ItemCallback<T>
) : ListAdapter<T, VH>(diffCallback) {
    protected var adapterJob: Job = Job()

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        adapterJob.cancel()
    }
}
