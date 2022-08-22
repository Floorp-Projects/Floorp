/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.exceptions

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.ItemTouchHelper.SimpleCallback
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import org.mozilla.focus.GleanMetrics.TrackingProtectionExceptions
import org.mozilla.focus.R
import org.mozilla.focus.autocomplete.AutocompleteDomainFormatter
import org.mozilla.focus.databinding.FragmentExceptionsDomainsBinding
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.settings.BaseSettingsLikeFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.ViewUtils
import java.util.Collections
import kotlin.coroutines.CoroutineContext

private const val REMOVE_EXCEPTIONS_DISABLED_ALPHA = 0.5f
typealias DomainFormatter = (String) -> String

/**
 * Fragment showing settings UI listing all exception domains.
 */
open class ExceptionsListFragment : BaseSettingsLikeFragment(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main
    private var _binding: FragmentExceptionsDomainsBinding? = null
    protected val binding get() = _binding!!

    /**
     * ItemTouchHelper for reordering items in the domain list.
     */
    val itemTouchHelper: ItemTouchHelper = ItemTouchHelper(
        object : SimpleCallback(ItemTouchHelper.UP or ItemTouchHelper.DOWN, 0) {
            override fun onMove(
                recyclerView: RecyclerView,
                viewHolder: RecyclerView.ViewHolder,
                target: RecyclerView.ViewHolder,
            ): Boolean {
                val from = viewHolder.bindingAdapterPosition
                val to = target.bindingAdapterPosition

                (recyclerView.adapter as DomainListAdapter).move(from, to)

                return true
            }

            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {}

            override fun onSelectedChanged(viewHolder: RecyclerView.ViewHolder?, actionState: Int) {
                super.onSelectedChanged(viewHolder, actionState)

                if (viewHolder is DomainViewHolder) {
                    viewHolder.onSelected()
                }
            }

            override fun clearView(
                recyclerView: RecyclerView,
                viewHolder: RecyclerView.ViewHolder,
            ) {
                super.clearView(recyclerView, viewHolder)

                if (viewHolder is DomainViewHolder) {
                    viewHolder.onCleared()
                }
            }
        },
    )

    /**
     * In selection mode the user can select and remove items. In non-selection mode the list can
     * be reordered by the user.
     */
    open fun isSelectionMode() = false

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentExceptionsDomainsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.exceptionList.apply {
            layoutManager =
                LinearLayoutManager(activity, RecyclerView.VERTICAL, false)
            adapter = DomainListAdapter()
            setHasFixedSize(true)
        }

        binding.removeAllExceptions.isVisible = !isSelectionMode()

        if (!isSelectionMode()) {
            itemTouchHelper.attachToRecyclerView(binding.exceptionList)
        }

        binding.removeAllExceptions.setOnClickListener { removeButton ->
            removeButton.apply {
                isEnabled = false
                alpha = REMOVE_EXCEPTIONS_DISABLED_ALPHA
            }
            requireComponents.trackingProtectionUseCases.removeAllExceptions {
                val exceptionsListSize =
                    (binding.exceptionList.adapter as DomainListAdapter).itemCount
                TrackingProtectionExceptions.allowListCleared.record(
                    TrackingProtectionExceptions.AllowListClearedExtra(exceptionsListSize),
                )

                TelemetryWrapper.removeAllExceptionDomains(exceptionsListSize)

                requireComponents.appStore.dispatch(
                    AppAction.NavigateUp(
                        requireComponents.store.state.selectedTabId,
                    ),
                )
            }
        }
    }

    override fun onResume() {
        super.onResume()

        job = Job()

        showToolbar(getString(R.string.preference_exceptions))

        (binding.exceptionList.adapter as DomainListAdapter).refresh(requireActivity()) {
            if ((binding.exceptionList.adapter as DomainListAdapter).itemCount == 0) {
                requireComponents.appStore.dispatch(
                    AppAction.NavigateUp(requireComponents.store.state.selectedTabId),
                )
            }
            activity?.invalidateOptionsMenu()
        }
    }

    override fun onStop() {
        job.cancel()
        super.onStop()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
        menuInflater.inflate(R.menu.menu_exceptions_list, menu)
    }

    override fun onPrepareMenu(menu: Menu) {
        val removeItem = menu.findItem(R.id.remove)

        removeItem?.let {
            it.isVisible = isSelectionMode() || binding.exceptionList.adapter!!.itemCount > 0
            val isEnabled =
                !isSelectionMode() || (binding.exceptionList.adapter as DomainListAdapter).selection().isNotEmpty()
            ViewUtils.setMenuItemEnabled(it, isEnabled)
        }
    }

    override fun onMenuItemSelected(menuItem: MenuItem): Boolean = when (menuItem.itemId) {
        R.id.remove -> {
            requireComponents.appStore.dispatch(
                AppAction.OpenSettings(page = Screen.Settings.Page.PrivacyExceptionsRemove),
            )
            true
        }
        else -> false
    }

    /**
     * Adapter implementation for the list of exception domains.
     */
    inner class DomainListAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
        private var exceptions: List<TrackingProtectionException> = emptyList()
        private val selectedExceptions: MutableList<TrackingProtectionException> = mutableListOf()

        fun refresh(context: Context, body: (() -> Unit)? = null) {
            this@ExceptionsListFragment.launch(Dispatchers.Main) {
                context.components.trackingProtectionUseCases.fetchExceptions {
                    exceptions = it
                    notifyDataSetChanged()
                    body?.invoke()
                }
            }
        }

        override fun getItemViewType(position: Int) = DomainViewHolder.LAYOUT_ID

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder =
            when (viewType) {
                DomainViewHolder.LAYOUT_ID ->
                    DomainViewHolder(
                        LayoutInflater.from(parent.context).inflate(viewType, parent, false),
                    ) { AutocompleteDomainFormatter.format(it) }
                else -> throw IllegalArgumentException("Unknown view type: $viewType")
            }

        override fun getItemCount(): Int = exceptions.size

        override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
            if (holder is DomainViewHolder) {
                holder.bind(
                    exceptions[position],
                    isSelectionMode(),
                    selectedExceptions,
                    itemTouchHelper,
                    this@ExceptionsListFragment,
                )
            }
        }

        override fun onViewRecycled(holder: RecyclerView.ViewHolder) {
            if (holder is DomainViewHolder) {
                holder.checkBoxView.setOnCheckedChangeListener(null)
            }
        }

        fun selection(): List<TrackingProtectionException> = selectedExceptions

        fun move(from: Int, to: Int) {
            Collections.swap(exceptions, from, to)
            notifyItemMoved(from, to)

            // The underlying storage in GeckoView doesn't support ordering - and ordering is also
            // not necessary. We may just need to remove this feature from this list.
        }
    }

    /**
     * ViewHolder implementation for a domain item in the list.
     */
    private class DomainViewHolder(
        itemView: View,
        val domainFormatter: DomainFormatter? = null,
    ) : RecyclerView.ViewHolder(itemView) {
        val domainView: TextView = itemView.findViewById(R.id.domainView)
        val checkBoxView: CheckBox = itemView.findViewById(R.id.checkbox)
        val handleView: View = itemView.findViewById(R.id.handleView)

        companion object {
            val LAYOUT_ID = R.layout.item_custom_domain
        }

        fun bind(
            exception: TrackingProtectionException,
            isSelectionMode: Boolean,
            selectedExceptions: MutableList<TrackingProtectionException>,
            itemTouchHelper: ItemTouchHelper,
            fragment: ExceptionsListFragment,
        ) {
            domainView.text = domainFormatter?.invoke(exception.url) ?: exception.url

            checkBoxView.visibility = if (isSelectionMode) View.VISIBLE else View.GONE
            checkBoxView.isChecked = selectedExceptions.contains(exception)
            checkBoxView.setOnCheckedChangeListener { _: CompoundButton, isChecked: Boolean ->
                if (isChecked) {
                    selectedExceptions.add(exception)
                } else {
                    selectedExceptions.remove(exception)
                }

                fragment.activity?.invalidateOptionsMenu()
            }

            handleView.visibility = if (isSelectionMode) View.GONE else View.VISIBLE
            handleView.setOnTouchListener { _, event ->
                if (event.actionMasked == MotionEvent.ACTION_DOWN) {
                    itemTouchHelper.startDrag(this)
                }
                false
            }

            if (isSelectionMode) {
                itemView.setOnClickListener {
                    checkBoxView.isChecked = !checkBoxView.isChecked
                }
            }
        }

        fun onSelected() {
            itemView.setBackgroundColor(ContextCompat.getColor(itemView.context, R.color.disabled))
        }

        fun onCleared() {
            itemView.setBackgroundColor(0)
        }
    }
}
