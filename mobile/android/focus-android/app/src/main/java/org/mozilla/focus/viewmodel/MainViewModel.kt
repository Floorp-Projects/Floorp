/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.viewmodel

import android.arch.lifecycle.LiveData
import android.arch.lifecycle.MutableLiveData
import android.arch.lifecycle.ViewModel

class MainViewModel : ViewModel() {
    private val experimentsLiveData = MutableLiveData<Boolean>()

    init {
        experimentsLiveData.value = false
    }

    fun showExperiments() {
        experimentsLiveData.value = true
    }

    fun getExperimentsLiveData(): LiveData<Boolean> {
        return experimentsLiveData
    }
}
