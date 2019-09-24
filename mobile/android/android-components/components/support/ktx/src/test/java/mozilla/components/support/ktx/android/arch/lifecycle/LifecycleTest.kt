/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.arch.lifecycle

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify

class LifecycleTest {

    @Test
    fun addObservers() {
        val observer1: LifecycleObserver = mock()
        val observer2: LifecycleObserver = mock()
        val lifecycle: Lifecycle = mock()

        lifecycle.addObservers(observer1, observer2)

        verify(lifecycle).addObserver(observer1)
        verify(lifecycle).addObserver(observer2)
    }
}
