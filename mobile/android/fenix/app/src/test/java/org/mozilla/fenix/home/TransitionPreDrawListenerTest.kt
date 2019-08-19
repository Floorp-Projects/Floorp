/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.ViewTreeObserver
import androidx.fragment.app.Fragment
import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test

class TransitionPreDrawListenerTest {

    private lateinit var fragment: Fragment
    private lateinit var viewTreeObserver: ViewTreeObserver

    @Before
    fun setup() {
        fragment = mockk(relaxed = true)
        viewTreeObserver = mockk(relaxed = true)
    }

    @Test
    fun `adds observer when constructed`() {
        val listener = TransitionPreDrawListener(fragment, viewTreeObserver) {}
        verify { fragment.viewLifecycleOwner.lifecycle.addObserver(listener) }
    }

    @Test
    fun `adds listener on create and removes on destroy`() {
        val listener = TransitionPreDrawListener(fragment, viewTreeObserver) {}

        listener.onCreateView()
        verify { viewTreeObserver.addOnPreDrawListener(listener) }

        listener.onDestroyView()
        verify { viewTreeObserver.removeOnPreDrawListener(listener) }
    }
}
