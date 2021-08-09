/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.studies

import android.view.View
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton

/**
 * A base view holder for Studies.
 */
sealed class CustomViewHolder(view: View) : RecyclerView.ViewHolder(view) {
    /**
     * A view holder for displaying section items.
     */
    class SectionViewHolder(
        view: View,
        val titleView: TextView,
        val divider: View
    ) : CustomViewHolder(view)

    /**
     * A view holder for displaying study items.
     */
    class StudyViewHolder(
        view: View,
        val titleView: TextView,
        val summaryView: TextView,
        val deleteButton: MaterialButton,
    ) : CustomViewHolder(view)
}
