/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class ProfilerViewModel : ViewModel() {
    var isProfilerActive : MutableLiveData<Boolean> = MutableLiveData()

    fun getProfilerState(): LiveData<Boolean> {
        return isProfilerActive
    }

    fun setProfilerState(isActive: Boolean){
        isProfilerActive.value = isActive
    }
}