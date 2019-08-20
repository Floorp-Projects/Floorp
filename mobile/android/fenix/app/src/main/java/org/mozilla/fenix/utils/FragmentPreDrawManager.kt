/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import androidx.core.view.doOnPreDraw
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Helper class that allows executing code immediately before [Fragment]s View being drawn.
 */
class FragmentPreDrawManager(
    private val fragment: Fragment
) {
    init {
        fragment.postponeEnterTransition()
    }

    fun execute(code: () -> Unit) {
        fragment.view?.doOnPreDraw {
            fragment.viewLifecycleOwner.lifecycleScope.launch {
                delay(ANIM_SCROLL_DELAY)
                code()
                fragment.startPostponedEnterTransition()
            }
        }
    }

    companion object {
        private const val ANIM_SCROLL_DELAY = 100L
    }
}
