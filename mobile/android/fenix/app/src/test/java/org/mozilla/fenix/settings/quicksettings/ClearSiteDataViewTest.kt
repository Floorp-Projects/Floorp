/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.View
import android.widget.FrameLayout
import androidx.navigation.NavController
import io.mockk.MockKAnnotations
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verifyOrder
import kotlinx.coroutines.test.TestScope
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.QuicksettingsClearSiteDataBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ClearSiteDataViewTest {
    private lateinit var view: ClearSiteDataView
    private lateinit var binding: QuicksettingsClearSiteDataBinding
    private lateinit var interactor: ClearSiteDataViewInteractor
    private lateinit var navController: NavController

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = mockk(relaxed = true)
        navController = mockk(relaxed = true)
        view = spyk(
            ClearSiteDataView(
                testContext,
                TestScope(),
                FrameLayout(testContext),
                View(testContext),
                interactor,
                navController,
            ),
        )
        binding = view.binding
    }

    @Test
    fun `clear site`() {
        val state = WebsiteInfoState(
            websiteUrl = "https://developers.mozilla.org",
            websiteTitle = "Mozilla",
            websiteSecurityUiValues = WebsiteSecurityUiValues.SECURE,
            certificateName = "Certificate",
        )

        view.update(state)

        binding.clearSiteData.callOnClick()

        verifyOrder {
            view.askToClear()
            navController.popBackStack()
        }
    }
}
