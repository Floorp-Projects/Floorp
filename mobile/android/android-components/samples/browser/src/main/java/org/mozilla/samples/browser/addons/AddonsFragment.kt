/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.RatingBar
import android.widget.TextView
import android.widget.Toast
import androidx.annotation.StringRes
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.feature.addons.Addon
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.addons.AddonsFragment.CustomViewHolder.AddonViewHolder
import org.mozilla.samples.browser.addons.AddonsFragment.CustomViewHolder.SectionViewHolder
import org.mozilla.samples.browser.addons.PermissionsDialogFragment.PromptsStyling
import org.mozilla.samples.browser.ext.components

/**
 * Fragment use for managing add-ons.
 */
class AddonsFragment : Fragment(), View.OnClickListener {
    private lateinit var recyclerView: RecyclerView
    private val scope = CoroutineScope(Dispatchers.IO)

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_add_ons, container, false)
    }

    override fun onViewCreated(rootView: View, savedInstanceState: Bundle?) {
        super.onViewCreated(rootView, savedInstanceState)
        bindRecyclerView(rootView)
    }

    override fun onStart() {
        super.onStart()
        findPreviousDialogFragment()?.let { dialog ->
            dialog.onPositiveButtonClicked = onPositiveButtonClicked
            dialog.onNegativeButtonClicked = onNegativeButtonClicked
        }
    }

    private fun bindRecyclerView(rootView: View) {
        recyclerView = rootView.findViewById(R.id.add_ons_list)
        recyclerView.layoutManager = LinearLayoutManager(requireContext())
        scope.launch {
            val addons = requireContext().components.addonProvider.getAvailableAddons()

            scope.launch(Dispatchers.Main) {
                val adapter = AddonsAdapter(
                    this@AddonsFragment,
                    addons
                )
                recyclerView.adapter = adapter
            }
        }
    }

    /**
     * An adapter for displaying add-on items.
     */
    inner class AddonsAdapter(
        private val clickListener: View.OnClickListener,
        addons: List<Addon>
    ) : RecyclerView.Adapter<CustomViewHolder>() {
        private val items: List<Any>

        init {
            items = createListWithSections(addons)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CustomViewHolder {
            return if (viewType == VIEW_HOLDER_TYPE_ADDON) {
                createAddonViewHolder(parent)
            } else {
                createSectionViewHolder(parent)
            }
        }

        private fun createSectionViewHolder(parent: ViewGroup): CustomViewHolder {
            val context = parent.context
            val inflater = LayoutInflater.from(context)
            val view = inflater.inflate(R.layout.addons_section_item, parent, false)
            val titleView = view.findViewById<TextView>(R.id.title)

            return SectionViewHolder(view, titleView)
        }

        private fun createAddonViewHolder(parent: ViewGroup): AddonViewHolder {
            val context = parent.context
            val inflater = LayoutInflater.from(context)
            val view = inflater.inflate(R.layout.add_ons_item, parent, false)
            val iconView = view.findViewById<ImageView>(R.id.add_on_icon)
            val titleView = view.findViewById<TextView>(R.id.add_on_name)
            val summaryView = view.findViewById<TextView>(R.id.add_on_description)
            val ratingView = view.findViewById<RatingBar>(R.id.rating)
            val userCountView = view.findViewById<TextView>(R.id.users_count)
            val addButton = view.findViewById<ImageView>(R.id.add_button)
            return AddonViewHolder(
                view,
                iconView,
                titleView,
                summaryView,
                ratingView,
                userCountView,
                addButton
            )
        }

        override fun getItemCount() = items.size

        override fun getItemViewType(position: Int): Int {
            val isSection = items[position] !is Addon
            return if (isSection) VIEW_HOLDER_TYPE_SECTION else VIEW_HOLDER_TYPE_ADDON
        }

        override fun onBindViewHolder(holder: CustomViewHolder, position: Int) {
            val item = items[position]

            when (holder) {
                is SectionViewHolder -> bindSection(holder, item as Section)
                is AddonViewHolder -> bindAddon(holder, item as Addon)
            }
        }

        private fun bindSection(holder: SectionViewHolder, section: Section) {
            holder.titleView.setText(section.title)
        }

        private fun bindAddon(holder: AddonViewHolder, addon: Addon) {
            val context = holder.itemView.context
            addon.rating?.let {
                val userCount = context.getString(R.string.add_on_user_rating_count)
                val ratingContentDescription =
                    context.getString(R.string.add_on_rating_content_description)
                holder.ratingView.contentDescription =
                    String.format(ratingContentDescription, it.average)
                holder.ratingView.rating = it.average
                holder.userCountView.text = String.format(userCount, getFormattedAmount(it.reviews))
            }

            holder.titleView.text = addon.translatableName.translate()
            holder.summaryView.text = addon.translatableSummary.translate()
            holder.itemView.tag = addon
            holder.itemView.setOnClickListener(clickListener)
            holder.addButton.isVisible = !addon.installed
            val listener = if (!addon.installed) {
                clickListener
            } else {
                null
            }
            holder.addButton.setOnClickListener(listener)

            scope.launch {
                val iconBitmap = context.components.addonProvider.getAddonIconBitmap(addon)

                iconBitmap?.let {
                    MainScope().launch {
                        holder.iconView.setImageBitmap(it)
                    }
                }
            }
        }

        private fun createListWithSections(addons: List<Addon>): List<Any> {
            // We want to have the installed add-ons first in the list.
            val sortedAddons = addons.sortedBy { !it.installed }

            val itemsWithSections = ArrayList<Any>()
            val shouldAddInstalledSection = sortedAddons.first().installed
            var isRecommendedSectionAdded = false

            if (shouldAddInstalledSection) {
                itemsWithSections.add(Section(R.string.addon_settings_installed_section))
            }

            sortedAddons.forEach { addon ->
                if (!isRecommendedSectionAdded && !addon.installed) {
                    itemsWithSections.add(Section(R.string.addon_settings_recommended_section))
                    isRecommendedSectionAdded = true
                }

                itemsWithSections.add(addon)
            }
            return itemsWithSections
        }
    }

    /**
     * A base view holder.
     */
    sealed class CustomViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        /**
         * A view holder for displaying section items.
         */
        class SectionViewHolder(
            view: View,
            val titleView: TextView
        ) : CustomViewHolder(view)

        /**
         * A view holder for displaying add-on items.
         */
        class AddonViewHolder(
            view: View,
            val iconView: ImageView,
            val titleView: TextView,
            val summaryView: TextView,
            val ratingView: RatingBar,
            val userCountView: TextView,
            val addButton: ImageView
        ) : CustomViewHolder(view)
    }

    override fun onClick(view: View) {
        val context = view.context
        when (view.id) {
            R.id.add_button -> {
                val addon = (((view.parent) as View).tag as Addon)
                showPermissionDialog(addon)
            }
            R.id.add_on_item -> {
                val intent = Intent(context, InstalledAddonDetailsActivity::class.java)
                intent.putExtra("add_on", view.tag as Addon)
                context.startActivity(intent)
            }
            else -> {
            }
        }
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun findPreviousDialogFragment(): PermissionsDialogFragment? {
        return fragmentManager?.findFragmentByTag(PERMISSIONS_DIALOG_FRAGMENT_TAG) as? PermissionsDialogFragment
    }

    private fun showPermissionDialog(addon: Addon) {

        val dialog = PermissionsDialogFragment.newInstance(
            addonId = addon.id,
            title = addon.translatableName.translate(),
            permissions = addon.translatePermissions(),
            promptsStyling = PromptsStyling(
                gravity = Gravity.BOTTOM,
                shouldWidthMatchParent = true
            ),
            onPositiveButtonClicked = onPositiveButtonClicked,
            onNegativeButtonClicked = onNegativeButtonClicked
        )

        if (!isAlreadyADialogCreated() && fragmentManager != null) {
            dialog.show(requireFragmentManager(), PERMISSIONS_DIALOG_FRAGMENT_TAG)
        }
    }

    private val onPositiveButtonClicked: ((String) -> Unit) = { addonId ->
        Toast.makeText(this.requireContext(), "Installing add-on $addonId", Toast.LENGTH_SHORT)
            .show()
    }

    private val onNegativeButtonClicked = {
        Toast.makeText(this.requireContext(), "Cancel button clicked", Toast.LENGTH_SHORT)
            .show()
    }
    companion object {
        private const val PERMISSIONS_DIALOG_FRAGMENT_TAG = "ADDONS_PERMISSIONS_DIALOG_FRAGMENT"
        private const val VIEW_HOLDER_TYPE_SECTION = 0
        private const val VIEW_HOLDER_TYPE_ADDON = 1
    }

    private inner class Section(@StringRes val title: Int)
}
