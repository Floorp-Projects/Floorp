/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.privacy.studies

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.focus.Components
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import kotlin.system.exitProcess

class StudiesViewModel(application: Application) : AndroidViewModel(application) {
    private var localStudies: MutableList<StudiesListItem> = mutableListOf()
    val exposedStudies: MutableLiveData<List<StudiesListItem>> = MutableLiveData()
    val studiesState: MutableLiveData<Boolean> = MutableLiveData()
    private val appContext = application

    init {
        getExperiments(application.components)
        setStudiesState(application.settings.isExperimentationEnabled)
    }

    private fun getExperiments(components: Components) {
        viewModelScope.launch(Dispatchers.IO) {
            localStudies.add(StudiesListItem.Section(R.string.studies_active, true))

            components.experiments.getActiveExperiments().map { activeExperiment ->
                localStudies.add(StudiesListItem.ActiveStudy(activeExperiment))
            }
            localStudies.add(StudiesListItem.Section(R.string.studies_completed, true))
            exposedStudies.postValue(localStudies)
        }
    }

    fun setStudiesState(state: Boolean) {
        appContext.settings.isExperimentationEnabled = state
        appContext.components.experiments.globalUserParticipation = state
        studiesState.postValue(state)
    }

    fun removeStudy(study: StudiesListItem.ActiveStudy) {
        viewModelScope.launch(Dispatchers.IO) {
            localStudies.remove(study)
            exposedStudies.postValue(localStudies)
            appContext.components.experiments.register(
                object : NimbusInterface.Observer {
                    override fun onUpdatesApplied(updated: List<EnrolledExperiment>) {
                        // Wait until the experiment is unrolled from nimbus to restart the app
                        exitProcess(0)
                    }
                },
            )
            appContext.components.experiments.optOut(study.value.slug)
            appContext.components.experiments.applyPendingExperiments()
        }
    }
}
