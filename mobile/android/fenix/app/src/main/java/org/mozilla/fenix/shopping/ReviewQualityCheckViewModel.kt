/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update

/**
 * A ViewModel to communicate between ReviewQualityCheckFragment bottom sheet and the parent
 * fragment.
 */
class ReviewQualityCheckViewModel : ViewModel() {

    private val _isBottomSheetVisible = MutableStateFlow(false)
    val isBottomSheetVisible: StateFlow<Boolean> = _isBottomSheetVisible

    /**
     * Invoked when bottom sheet dialog is created.
     */
    fun onDialogCreated() {
        _isBottomSheetVisible.update { true }
    }

    /**
     * Invoked when bottom sheet dialog is dismissed.
     */
    fun onDialogDismissed() {
        _isBottomSheetVisible.update { false }
    }
}
