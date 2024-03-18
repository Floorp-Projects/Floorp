/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageButton
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import mozilla.components.support.ktx.android.content.getDrawableWithTint
import org.mozilla.fenix.R

/**
 * Updates views in [R.id.bottom_button_bar_layout] for collection creation.
 */
class CollectionCreationBottomBarView(
    private val interactor: CollectionCreationInteractor,
    private val layout: ViewGroup,
    private val iconButton: ImageButton,
    private val textView: TextView,
    private val saveButton: Button,
) {

    fun update(step: SaveCollectionStep, state: CollectionCreationState) {
        when (step) {
            SaveCollectionStep.SelectTabs -> updateForSelectTabs(state)
            SaveCollectionStep.SelectCollection -> updateForSelectCollection()
            else -> { /* noop */ }
        }
    }

    private fun updateForSelectTabs(state: CollectionCreationState) {
        layout.setOnClickListener(null)
        layout.isClickable = false

        iconButton.apply {
            val drawable = context.getDrawableWithTint(
                R.drawable.ic_close,
                ContextCompat.getColor(context, R.color.photonWhite),
            )
            setImageDrawable(drawable)
            contentDescription = context.getString(R.string.create_collection_close)
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
            setOnClickListener { interactor.close() }
        }

        textView.apply {
            text = if (state.selectedTabs.isEmpty()) {
                context.getString(R.string.create_collection_save_to_collection_empty)
            } else {
                context.getString(
                    if (state.selectedTabs.size == 1) {
                        R.string.create_collection_save_to_collection_tab_selected
                    } else {
                        R.string.create_collection_save_to_collection_tabs_selected
                    },
                    state.selectedTabs.size,
                )
            }
        }

        saveButton.apply {
            setOnClickListener {
                if (state.selectedTabCollection != null) {
                    interactor.selectCollection(
                        collection = state.selectedTabCollection,
                        tabs = state.selectedTabs.toList(),
                    )
                } else {
                    interactor.saveTabsToCollection(tabs = state.selectedTabs.toList())
                }
            }
            isVisible = state.selectedTabs.isNotEmpty()
        }
    }

    private fun updateForSelectCollection() {
        saveButton.visibility = View.GONE

        textView.text =
            textView.context.getString(R.string.create_collection_add_new_collection)

        iconButton.apply {
            val drawable = context.getDrawableWithTint(
                R.drawable.ic_new,
                ContextCompat.getColor(context, R.color.photonWhite),
            )
            setImageDrawable(drawable)
            contentDescription = null
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
            setOnClickListener { interactor.addNewCollection() }
        }
        layout.setOnClickListener { interactor.addNewCollection() }
    }
}
