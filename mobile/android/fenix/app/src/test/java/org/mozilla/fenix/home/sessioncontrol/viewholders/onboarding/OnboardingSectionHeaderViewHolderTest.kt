/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.OnboardingSectionHeaderBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingSectionHeaderViewHolderTest {

    private lateinit var binding: OnboardingSectionHeaderBinding

    @Before
    fun setup() {
        binding = OnboardingSectionHeaderBinding.inflate(LayoutInflater.from(testContext))
    }

    @Test
    fun `bind text`() {
        val holder = OnboardingSectionHeaderViewHolder(binding.root)
        holder.bind { "Hello world" }

        assertEquals(
            "Hello world",
            binding.sectionHeaderText.text
        )
    }
}
