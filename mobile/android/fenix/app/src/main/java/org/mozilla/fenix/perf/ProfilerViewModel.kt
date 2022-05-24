/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.ext.components

/**
 * [ViewModel] to keep track of the profiler state
 */
class ProfilerViewModel(application: Application) : AndroidViewModel(application) {
    val isProfilerActive: MutableLiveData<Boolean> = MutableLiveData(
        application.components.core.engine.profiler?.isProfilerActive() ?: false
    )

    /**
     * @return profiler status
     */
    fun getProfilerState(): LiveData<Boolean> {
        // We check here since this can be polled from anywhere in Fenix.
        getApplication<FenixApplication>().components.core.engine.profiler?.let {
            check(it.isProfilerActive() == isProfilerActive.value) {
                "The Profiler state from Gecko is out of sync with the LiveData profiler state."
            }
        }
        return isProfilerActive
    }

    /**
     * @param isActive whether the profiler is active or not
     */
    fun setProfilerState(isActive: Boolean) {
        isProfilerActive.value = isActive
    }
}
