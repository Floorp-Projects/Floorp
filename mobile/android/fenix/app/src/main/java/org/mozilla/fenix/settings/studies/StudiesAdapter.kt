/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.studies

import android.annotation.SuppressLint
import android.content.Context
import android.content.DialogInterface
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.core.view.isVisible
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import com.google.android.material.button.MaterialButton
import mozilla.components.service.nimbus.messaging.MESSAGING_FEATURE_ID
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.studies.CustomViewHolder.SectionViewHolder
import org.mozilla.fenix.settings.studies.CustomViewHolder.StudyViewHolder

private const val VIEW_HOLDER_TYPE_SECTION = 0
private const val VIEW_HOLDER_TYPE_STUDY = 1

/**
 * An adapter for displaying studies items. This will display information related to the state of
 * a study such as active. In addition, it will perform actions such as removing a study.
 *
 * @param studiesDelegate Delegate that will provides method for handling
 * the studies actions items.
 * @param studies The list of studies.
 *  * @property studiesDelegate Delegate that will provides method for handling
 * the studies actions items.
 * @param shouldSubmitOnInit The sole purpose of this property is to prevent the submitList function
 * to run on init, it should only be used from tests.
 */
@Suppress("LargeClass")
class StudiesAdapter(
    private val studiesDelegate: StudiesAdapterDelegate,
    studies: List<EnrolledExperiment>,
    private val shouldSubmitOnInit: Boolean = true,
) : ListAdapter<Any, CustomViewHolder>(DifferCallback) {
    /**
     * Represents all the studies that will be distributed in multiple headers like
     * active, and completed, this helps to have the data source of the items,
     * displayed in the UI.
     */
    @VisibleForTesting
    internal var studiesMap: MutableMap<String, EnrolledExperiment> =
        studies.associateBy({ it.slug }, { it }).toMutableMap()

    init {
        if (shouldSubmitOnInit) {
            submitList(createListWithSections(studies))
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CustomViewHolder {
        return when (viewType) {
            VIEW_HOLDER_TYPE_STUDY -> createStudiesViewHolder(parent)
            VIEW_HOLDER_TYPE_SECTION -> createSectionViewHolder(parent)
            else -> throw IllegalArgumentException("Unrecognized viewType")
        }
    }

    private fun createSectionViewHolder(parent: ViewGroup): CustomViewHolder {
        val context = parent.context
        val inflater = LayoutInflater.from(context)
        val view = inflater.inflate(R.layout.studies_section_item, parent, false)
        val titleView = view.findViewById<TextView>(R.id.title)
        val divider = view.findViewById<View>(R.id.divider)
        return SectionViewHolder(view, titleView, divider)
    }

    private fun createStudiesViewHolder(parent: ViewGroup): StudyViewHolder {
        val context = parent.context
        val view = LayoutInflater.from(context).inflate(R.layout.study_item, parent, false)
        val titleView = view.findViewById<TextView>(R.id.studyTitle)
        val summaryView = view.findViewById<TextView>(R.id.study_description)
        val removeButton = view.findViewById<MaterialButton>(R.id.remove_button)
        return StudyViewHolder(
            view,
            titleView,
            summaryView,
            removeButton,
        )
    }

    override fun getItemViewType(position: Int): Int {
        return when (getItem(position)) {
            is EnrolledExperiment -> VIEW_HOLDER_TYPE_STUDY
            is Section -> VIEW_HOLDER_TYPE_SECTION
            else -> throw IllegalArgumentException("items[position] has unrecognized type")
        }
    }

    override fun onBindViewHolder(holder: CustomViewHolder, position: Int) {
        val item = getItem(position)

        when (holder) {
            is SectionViewHolder -> bindSection(holder, item as Section)
            is StudyViewHolder -> bindStudy(holder, item as EnrolledExperiment)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun bindSection(holder: SectionViewHolder, section: Section) {
        holder.titleView.setText(section.title)
        holder.divider.isVisible = section.visibleDivider
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun bindStudy(holder: StudyViewHolder, study: EnrolledExperiment) {
        holder.titleView.text = study.userFacingName
        holder.summaryView.text = study.userFacingDescription

        holder.deleteButton.setOnClickListener {
            showDeleteDialog(holder.titleView.context, study)
        }
    }

    @VisibleForTesting
    internal fun showDeleteDialog(context: Context, study: EnrolledExperiment): AlertDialog {
        val builder = AlertDialog.Builder(context)
            .setPositiveButton(
                R.string.studies_restart_dialog_ok,
            ) { dialog, _ ->
                studiesDelegate.onRemoveButtonClicked(study)
                dialog.dismiss()
            }
            .setNegativeButton(
                R.string.studies_restart_dialog_cancel,
            ) { dialog: DialogInterface, _ ->
                dialog.dismiss()
            }
            .setTitle(R.string.preference_experiments_2)
            .setMessage(R.string.studies_restart_app)
            .setCancelable(false)
        val alertDialog: AlertDialog = builder.create().withCenterAlignedButtons()
        alertDialog.show()
        return alertDialog
    }

    internal fun createListWithSections(studies: List<EnrolledExperiment>): List<Any> {
        val itemsWithSections = ArrayList<Any>()
        val activeStudies = ArrayList<EnrolledExperiment>()

        activeStudies.addAll(filterStudies(studies))

        if (activeStudies.isNotEmpty()) {
            itemsWithSections.add(Section(R.string.studies_active, true))
            itemsWithSections.addAll(activeStudies)
        }

        return itemsWithSections
    }

    /**
     * Filter out studies which only affect the messaging feature.
     */
    private fun filterStudies(studies: List<EnrolledExperiment>) =
        studies.filterNot {
            it.featureIds.size == 1 && it.featureIds.contains(MESSAGING_FEATURE_ID)
        }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal data class Section(@StringRes val title: Int, val visibleDivider: Boolean = true)

    /**
     * Removes the portion of the list that contains the provided [study].
     *
     * @param study The study to be removed.
     */
    fun removeStudy(study: EnrolledExperiment) {
        studiesMap.remove(study.slug)
        submitList(createListWithSections(studiesMap.values.toList()))
    }

    internal object DifferCallback : DiffUtil.ItemCallback<Any>() {
        override fun areItemsTheSame(oldItem: Any, newItem: Any): Boolean {
            return when {
                oldItem is EnrolledExperiment && newItem is EnrolledExperiment -> oldItem.slug == newItem.slug
                oldItem is Section && newItem is Section -> oldItem.title == newItem.title
                else -> false
            }
        }

        @SuppressLint("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: Any, newItem: Any): Boolean {
            return oldItem == newItem
        }
    }
}
