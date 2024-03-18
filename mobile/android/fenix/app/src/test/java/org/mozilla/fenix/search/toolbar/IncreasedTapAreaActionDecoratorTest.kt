/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.view.View
import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.justRun
import io.mockk.mockkStatic
import io.mockk.verify
import mozilla.components.concept.toolbar.Toolbar
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.increaseTapArea

class IncreasedTapAreaActionDecoratorTest {

    @MockK lateinit var action: Toolbar.Action

    @MockK lateinit var view: View

    private lateinit var increasedTapAreaActionDecorator: IncreasedTapAreaActionDecorator

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        increasedTapAreaActionDecorator = IncreasedTapAreaActionDecorator(action)
        justRun { action.bind(view) }
        mockkStatic(View::increaseTapArea)
        justRun { view.increaseTapArea(IncreasedTapAreaActionDecorator.TAP_INCREASE_DPS) }
    }

    @Test
    fun `increased tap area of view on bind`() {
        increasedTapAreaActionDecorator.bind(view)

        verify { view.increaseTapArea(IncreasedTapAreaActionDecorator.TAP_INCREASE_DPS) }
    }

    @Test
    fun `should invoke provided action bind`() {
        increasedTapAreaActionDecorator.bind(view)

        verify { action.bind(view) }
    }
}
