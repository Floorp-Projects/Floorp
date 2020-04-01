/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import androidx.core.view.OneShotPreDrawListener
import androidx.fragment.app.Fragment
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import org.junit.Test

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
class FragmentPreDrawManagerTest {
    private fun doNothing() { /*noop*/ }

    @Test
    fun `execute doOnPreDraw`() {
        runBlockingTest {
            val fragmentMock = mockk<Fragment>(relaxed = true)
            val fragmentPreDrawManager = FragmentPreDrawManager(fragmentMock)
            val listener = OneShotPreDrawListener.add(fragmentMock.view!!) { mockk() }

            verify { fragmentMock.postponeEnterTransition() }
            fragmentPreDrawManager.execute { doNothing() }
            verify { fragmentMock.view?.viewTreeObserver?.addOnPreDrawListener(listener) }
        }
    }
}
