/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.view.LayoutInflater
import android.view.View
import androidx.core.net.toUri
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.fragment_add_on_permissions.*
import mozilla.components.feature.addons.Addon
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class AddonPermissionsDetailsViewTest {

    private lateinit var view: View
    private lateinit var interactor: AddonPermissionsDetailsInteractor
    private lateinit var permissionsDetailsView: AddonPermissionsDetailsView
    private val addon = Addon(
        id = "",
        translatableName = mapOf(
            Addon.DEFAULT_LOCALE to "Some blank addon"
        )
    )
    private val learnMoreUrl =
        "https://support.mozilla.org/kb/permission-request-messages-firefox-extensions"

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(R.layout.fragment_add_on_permissions, null)
        interactor = mockk(relaxed = true)
        permissionsDetailsView = AddonPermissionsDetailsView(view, interactor)
    }

    @Test
    fun `clicking learn more opens learn more page in browser`() {
        permissionsDetailsView.bind(addon.copy(
            rating = null
        ))

        permissionsDetailsView.learn_more_label.performClick()

        verify { interactor.openWebsite(learnMoreUrl.toUri()) }
    }
}
