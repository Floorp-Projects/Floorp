/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import android.view.View
import kotlinx.android.synthetic.main.onboarding_header.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingHeaderViewHolderTest {

    private lateinit var view: View

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext)
            .inflate(OnboardingHeaderViewHolder.LAYOUT_ID, null)
    }

    @Test
    fun `bind header text`() {
        OnboardingHeaderViewHolder(view)

        assertEquals("Welcome to Firefox Preview!", view.header_text.text)
    }
}
