/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.privacy.studies

import android.content.Context
import android.content.DialogInterface
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.appcompat.app.AlertDialog
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.textview.MaterialTextView
import org.mozilla.focus.R

const val SECTION_LAYOUT_ID = R.layout.studies_section_item
const val ACTIVE_STUDY_LAYOUT_ID = R.layout.active_study_item
sealed class StudiesViewHolder(var viewGroup: ViewGroup) : RecyclerView.ViewHolder(viewGroup) {

    class ActiveStudiesViewHolder(rootView: ConstraintLayout) :
        StudiesViewHolder(viewGroup = rootView) {
        private val titleView = rootView.findViewById<MaterialTextView>(R.id.title)
        private val summaryView = rootView.findViewById<MaterialTextView>(R.id.summary)
        private val removeButton = rootView.findViewById<MaterialTextView>(R.id.remove)

        fun bindStudy(
            activeStudy: StudiesListItem.ActiveStudy,
            removeStudyListener: (StudiesListItem.ActiveStudy) -> Unit,
        ) {
            titleView.text = activeStudy.value.userFacingName
            summaryView.text = activeStudy.value.userFacingDescription
            removeButton.setOnClickListener { view ->
                showRemoveDialog(view.context, removeStudyListener, activeStudy)
            }
        }

        private fun showRemoveDialog(
            context: Context,
            studyListener: (StudiesListItem.ActiveStudy) -> Unit,
            activeStudy: StudiesListItem.ActiveStudy,
        ): AlertDialog {
            val builder = AlertDialog.Builder(context)
                .setPositiveButton(
                    R.string.action_ok,
                ) { dialog, _ ->
                    studyListener.invoke(activeStudy)
                    dialog.dismiss()
                }
                .setNegativeButton(
                    R.string.action_cancel,
                ) { dialog: DialogInterface, _ ->
                    dialog.dismiss()
                }
                .setTitle(R.string.preference_studies)
                .setMessage(R.string.studies_restart_app)
                .setCancelable(false)
            val alertDialog: AlertDialog = builder.create()
            alertDialog.show()
            return alertDialog
        }
    }

    class SectionViewHolder(rootView: LinearLayout) : StudiesViewHolder(viewGroup = rootView) {
        private val titleView = rootView.findViewById<MaterialTextView>(R.id.title)
        private val dividerView = rootView.findViewById<View>(R.id.divider)

        fun bindSection(section: StudiesListItem.Section) {
            titleView.text = titleView.context.resources.getString(section.titleId)
            dividerView.isVisible = section.visibleDivider
        }
    }
}
