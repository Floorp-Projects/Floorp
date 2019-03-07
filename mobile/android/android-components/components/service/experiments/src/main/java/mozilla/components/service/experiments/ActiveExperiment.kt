/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import androidx.annotation.VisibleForTesting

/**
 * An ActiveExperiment is an experiment that has a branch assigned.
 */
internal data class ActiveExperiment(
    val experiment: Experiment,
    val branch: String
) {
    fun save(context: Context) {
        getSharedPreferences(context)
            .edit()
            .putString(KEY_ID, experiment.id)
            .putString(KEY_BRANCH, branch)
            .apply()
    }

    companion object {
        fun load(
            context: Context,
            experimentList: List<Experiment>
        ): ActiveExperiment? {
            val prefs = getSharedPreferences(context)
            val id = prefs.getString(KEY_ID, null)
            val branch = prefs.getString(KEY_BRANCH, null)
            if (id.isNullOrEmpty() || branch.isNullOrEmpty()) {
                return null
            }

            val experiment = experimentList.find {
                it.id == id && it.branches.any { it.name == branch }
            } ?: return null

            return ActiveExperiment(experiment, branch)
        }

        fun clear(context: Context) {
            getSharedPreferences(context)
                .edit()
                .clear()
                .apply()
        }

        private fun getSharedPreferences(context: Context) =
            context.getSharedPreferences(PREF_NAME_ACTIVE_EXPERIMENT, Context.MODE_PRIVATE)

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal const val PREF_NAME_ACTIVE_EXPERIMENT = "mozilla.components.service.experiments.active_experiment"
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal const val KEY_ID = "id"
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal const val KEY_BRANCH = "branch"
    }
}
