/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.View.GONE
import android.view.View.VISIBLE
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.paging.LoadState
import androidx.paging.Pager
import androidx.paging.PagingConfig
import androidx.paging.PagingDataAdapter
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.support.ktx.kotlin.stripDefaultPort
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.mozilla.fenix.NavHostActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.nav

private const val MAX_ITEMS_PER_PAGE = 50

class SitePermissionsExceptionsFragment :
    Fragment(R.layout.fragment_site_permissions_exceptions), View.OnClickListener {
    private lateinit var emptyContainerMessage: View
    private lateinit var recyclerView: RecyclerView
    private lateinit var clearButton: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        (activity as NavHostActivity).getSupportActionBarAndInflateIfNecessary().show()
    }

    override fun onViewCreated(rootView: View, savedInstanceState: Bundle?) {
        super.onViewCreated(rootView, savedInstanceState)
        bindEmptyContainerMess(rootView)
        bindClearButton(rootView)
        bindRecyclerView(rootView)
    }

    private fun bindRecyclerView(rootView: View) {
        recyclerView = rootView.findViewById(R.id.exceptions)
        recyclerView.layoutManager = LinearLayoutManager(requireContext())

        val adapter = ExceptionsAdapter(this).apply {
            addLoadStateListener { loadState ->
                if (loadState.source.refresh is LoadState.NotLoading &&
                    loadState.append.endOfPaginationReached &&
                    itemCount < 1
                ) {
                    showEmptyListMessage()
                } else {
                    if (itemCount != 0) {
                        hideEmptyListMessage()
                    }
                }
            }
            recyclerView.adapter = this
        }

        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            val dataSourceFactory =
                requireContext().components.core.permissionStorage.getSitePermissionsPaged()

            val permissions = Pager(PagingConfig(MAX_ITEMS_PER_PAGE), null) {
                dataSourceFactory.asPagingSourceFactory().invoke()
            }.flow

            permissions.collect {
                adapter.submitData(it)
            }
        }
    }

    private fun hideEmptyListMessage() {
        emptyContainerMessage.visibility = GONE
        recyclerView.visibility = VISIBLE
        clearButton.visibility = VISIBLE
    }

    private fun showEmptyListMessage() {
        emptyContainerMessage.visibility = VISIBLE
        recyclerView.visibility = GONE
        clearButton.visibility = GONE
    }

    private fun bindEmptyContainerMess(rootView: View) {
        emptyContainerMessage = rootView.findViewById(R.id.empty_exception_container)
    }

    private fun bindClearButton(rootView: View) {
        clearButton = rootView.findViewById(R.id.delete_all_site_permissions_button)
        clearButton.setOnClickListener {
            AlertDialog.Builder(requireContext()).apply {
                setMessage(R.string.confirm_clear_permissions_on_all_sites)
                setTitle(R.string.clear_permissions)
                setPositiveButton(R.string.clear_permissions_positive) { dialog: DialogInterface, _ ->
                    deleteAllSitePermissions()
                    dialog.dismiss()
                }
                setNegativeButton(R.string.clear_permissions_negative) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
            }.show().withCenterAlignedButtons()
        }
    }

    private fun deleteAllSitePermissions() {
        viewLifecycleOwner.lifecycleScope.launch(Main) {
            requireContext().components.core.permissionStorage.deleteAllSitePermissions()

            showEmptyListMessage()
            // Reload the selected session.
            requireContext().components.useCases.sessionUseCases.reload()
        }
    }

    override fun onClick(view: View) {
        val sitePermissions = view.tag as SitePermissions
        val directions = SitePermissionsExceptionsFragmentDirections
            .actionSitePermissionsToExceptionsToSitePermissionsDetails(sitePermissions)
        nav(R.id.sitePermissionsExceptionsFragment, directions)
    }
}

class SitePermissionsViewHolder(
    val view: View,
    val iconView: ImageView,
    val siteTextView: TextView,
) :
    RecyclerView.ViewHolder(view)

/**
 * Adapter for the list of site permission exceptions.
 */
class ExceptionsAdapter(private val clickListener: View.OnClickListener) :
    PagingDataAdapter<SitePermissions, SitePermissionsViewHolder>(diffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SitePermissionsViewHolder {
        val context = parent.context
        val inflater = LayoutInflater.from(context)
        val view =
            inflater.inflate(R.layout.fragment_site_permissions_exceptions_item, parent, false)
        val iconView = view.findViewById<ImageView>(R.id.exception_icon)
        val siteTextView = view.findViewById<TextView>(R.id.exception_text)
        return SitePermissionsViewHolder(view, iconView, siteTextView)
    }

    override fun onBindViewHolder(holder: SitePermissionsViewHolder, position: Int) {
        val sitePermissions = requireNotNull(getItem(position))
        val context = holder.view.context
        context.components.core.icons.loadIntoView(holder.iconView, sitePermissions.origin)
        holder.siteTextView.text = sitePermissions.origin.stripDefaultPort()
        holder.view.tag = sitePermissions
        holder.view.setOnClickListener(clickListener)
    }

    companion object {

        private val diffCallback = object :
            DiffUtil.ItemCallback<SitePermissions>() {
            override fun areItemsTheSame(old: SitePermissions, new: SitePermissions) =
                old.origin == new.origin

            override fun areContentsTheSame(old: SitePermissions, new: SitePermissions) = old == new
        }
    }
}
