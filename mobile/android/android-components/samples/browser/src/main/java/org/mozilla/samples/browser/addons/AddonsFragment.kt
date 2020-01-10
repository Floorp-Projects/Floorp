/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.os.Bundle
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
import kotlinx.android.synthetic.main.fragment_add_ons.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.PermissionsDialogFragment
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.addons.AddonsFragment.CustomViewHolder.AddonViewHolder
import org.mozilla.samples.browser.addons.AddonsFragment.CustomViewHolder.SectionViewHolder
import org.mozilla.samples.browser.addons.AddonsFragment.CustomViewHolder.UnsupportedSectionViewHolder
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
        }
    }

    private fun bindRecyclerView(rootView: View) {
        recyclerView = rootView.findViewById(R.id.add_ons_list)
        recyclerView.layoutManager = LinearLayoutManager(requireContext())
        scope.launch {
            try {
                val addons = requireContext().components.addonManager.getAddons()

                scope.launch(Dispatchers.Main) {
                    val adapter = AddonsAdapter(
                        this@AddonsFragment,
                        addons
                    )
                    recyclerView.adapter = adapter
                }
            } catch (e: AddonManagerException) {
                scope.launch(Dispatchers.Main) {
                    Toast.makeText(activity, "Failed to query Add-ons!", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    /**
     * An adapter for displaying add-on items.
     */
    @Suppress("TooManyFunctions", "LargeClass")
    inner class AddonsAdapter(
        private val clickListener: View.OnClickListener,
        addons: List<Addon>
    ) : RecyclerView.Adapter<CustomViewHolder>() {
        private val items: List<Any>

        private val unsupportedAddons = ArrayList<Addon>()

        init {
            items = createListWithSections(addons)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CustomViewHolder {
            return when (viewType) {
                VIEW_HOLDER_TYPE_ADDON -> createAddonViewHolder(parent)
                VIEW_HOLDER_TYPE_SECTION -> createSectionViewHolder(parent)
                VIEW_HOLDER_TYPE_NOT_YET_SUPPORTED_SECTION -> createUnsupportedSectionViewHolder(parent)
                else -> throw IllegalArgumentException("Unrecognized viewType")
            }
        }

        private fun createSectionViewHolder(parent: ViewGroup): CustomViewHolder {
            val context = parent.context
            val inflater = LayoutInflater.from(context)
            val view = inflater.inflate(R.layout.addons_section_item, parent, false)
            val titleView = view.findViewById<TextView>(R.id.title)

            return SectionViewHolder(view, titleView)
        }

        private fun createUnsupportedSectionViewHolder(parent: ViewGroup): CustomViewHolder {
            val context = parent.context
            val inflater = LayoutInflater.from(context)
            val view = inflater.inflate(R.layout.addons_section_unsupported_section_item, parent, false)
            val titleView = view.findViewById<TextView>(R.id.title)

            return UnsupportedSectionViewHolder(view, titleView)
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
            return when (items[position]) {
                is Addon -> VIEW_HOLDER_TYPE_ADDON
                is Section -> VIEW_HOLDER_TYPE_SECTION
                is NotYetSupportedSection -> VIEW_HOLDER_TYPE_NOT_YET_SUPPORTED_SECTION
                else -> throw IllegalArgumentException("items[position] has unrecognized type")
            }
        }

        override fun onBindViewHolder(holder: CustomViewHolder, position: Int) {
            val item = items[position]

            when (holder) {
                is SectionViewHolder -> bindSection(holder, item as Section)
                is AddonViewHolder -> bindAddon(holder, item as Addon)
                is UnsupportedSectionViewHolder -> bindNotYetSupportedSection(holder, item as NotYetSupportedSection)
            }
        }

        private fun bindSection(holder: SectionViewHolder, section: Section) {
            holder.titleView.setText(section.title)
        }

        private fun bindNotYetSupportedSection(holder: UnsupportedSectionViewHolder, section: NotYetSupportedSection) {
            holder.titleView.setText(section.title)
            holder.itemView.setOnClickListener {
                val intent = Intent(context, NotYetSupportedAddonActivity::class.java)
                intent.putExtra("add_ons", unsupportedAddons)
                context!!.startActivity(intent)
            }
        }

        private fun bindAddon(holder: AddonViewHolder, addon: Addon) {
            val context = holder.itemView.context
            addon.rating?.let {
                val userCount = context.getString(R.string.mozac_feature_addons_user_rating_count)
                val ratingContentDescription =
                    context.getString(R.string.mozac_feature_addons_rating_content_description)
                holder.ratingView.contentDescription =
                    String.format(ratingContentDescription, it.average)
                holder.ratingView.rating = it.average
                holder.userCountView.text = String.format(userCount, getFormattedAmount(it.reviews))
            }

            holder.titleView.text =
                if (addon.translatableName.isNotEmpty()) {
                    addon.translatableName.translate()
                } else {
                    addon.id
                }

            if (addon.translatableSummary.isNotEmpty()) {
                holder.summaryView.text = addon.translatableSummary.translate()
            } else {
                holder.summaryView.visibility = View.GONE
            }

            holder.itemView.tag = addon
            holder.itemView.setOnClickListener(clickListener)
            holder.addButton.isVisible = !addon.isInstalled()
            val listener = if (!addon.isInstalled()) {
                clickListener
            } else {
                null
            }
            holder.addButton.setOnClickListener(listener)

            scope.launch {
                val iconBitmap = context.components.addonCollectionProvider.getAddonIconBitmap(addon)

                iconBitmap?.let {
                    MainScope().launch {
                        holder.iconView.setImageBitmap(it)
                    }
                }
            }
        }

        private fun createListWithSections(addons: List<Addon>): List<Any> {
            val itemsWithSections = ArrayList<Any>()
            val installedAddons = ArrayList<Addon>()
            val recommendedAddons = ArrayList<Addon>()

            addons.forEach { addon ->
                if (addon.isInstalled()) {
                    if (!addon.isSupported()) {
                        unsupportedAddons.add(addon)
                    } else {
                        installedAddons.add(addon)
                    }
                } else {
                    recommendedAddons.add(addon)
                }
            }

            // Add installed section and addons if available
            if (installedAddons.isNotEmpty()) {
                itemsWithSections.add(Section(R.string.mozac_feature_addons_installed_section))

                itemsWithSections.addAll(installedAddons)
            }

            // Add recommended section and addons if available
            if (recommendedAddons.isNotEmpty()) {
                itemsWithSections.add(Section(R.string.mozac_feature_addons_recommended_section))

                itemsWithSections.addAll(recommendedAddons)
            }

            // Add unsupported section
            if (unsupportedAddons.isNotEmpty()) {
                itemsWithSections.add(NotYetSupportedSection(R.string.mozac_feature_addons_unsupported_section))
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
         * A view holder for displaying Not yet supported section items.
         */
        class UnsupportedSectionViewHolder(
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
                val addon = view.tag as Addon
                if (addon.isInstalled()) {
                    val intent = Intent(context, InstalledAddonDetailsActivity::class.java)
                    intent.putExtra("add_on", addon)
                    context.startActivity(intent)
                } else {
                    val intent = Intent(context, AddonDetailsActivity::class.java)
                    intent.putExtra("add_on", addon)
                    this.startActivity(intent)
                }
            }
            else -> {}
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
            addon = addon,
            onPositiveButtonClicked = onPositiveButtonClicked
        )

        if (!isAlreadyADialogCreated() && fragmentManager != null) {
            dialog.show(requireFragmentManager(), PERMISSIONS_DIALOG_FRAGMENT_TAG)
        }
    }

    private val onPositiveButtonClicked: ((Addon) -> Unit) = { addon ->
        addonProgressOverlay.visibility = View.VISIBLE
        requireContext().components.addonManager.installAddon(
            addon,
            onSuccess = {
                Toast.makeText(
                    requireContext(),
                    "Successfully installed: ${it.translatableName.translate()}",
                    Toast.LENGTH_SHORT
                ).show()

                this@AddonsFragment.view?.let { view ->
                    bindRecyclerView(view)
                }

                addonProgressOverlay.visibility = View.GONE
            },
            onError = { _, _ ->
                Toast.makeText(
                    requireContext(),
                    "Failed to install: ${addon.translatableName.translate()}",
                    Toast.LENGTH_SHORT
                ).show()

                addonProgressOverlay.visibility = View.GONE
            }
        )
    }

    companion object {
        private const val PERMISSIONS_DIALOG_FRAGMENT_TAG = "ADDONS_PERMISSIONS_DIALOG_FRAGMENT"
        private const val VIEW_HOLDER_TYPE_SECTION = 0
        private const val VIEW_HOLDER_TYPE_NOT_YET_SUPPORTED_SECTION = 1
        private const val VIEW_HOLDER_TYPE_ADDON = 2
    }

    private inner class Section(@StringRes val title: Int)

    private inner class NotYetSupportedSection(@StringRes val title: Int)
}
