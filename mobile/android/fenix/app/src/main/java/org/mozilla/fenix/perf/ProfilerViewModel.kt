/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

/**
 * [ViewModel] to keep track of the profiler state
 */
class ProfilerViewModel : ViewModel() {
    var isProfilerActive: MutableLiveData<Boolean> = MutableLiveData()

    /**
     * @return profiler status
     */
    fun getProfilerState(): LiveData<Boolean> {
        return isProfilerActive
    }

    /**
     * @param isActive whether the profiler is active or not
     */
    fun setProfilerState(isActive: Boolean) {
        isProfilerActive.value = isActive
    }
}
