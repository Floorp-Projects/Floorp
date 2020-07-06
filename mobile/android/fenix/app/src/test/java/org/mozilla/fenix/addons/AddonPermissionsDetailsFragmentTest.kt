/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Intent.ACTION_VIEW
import androidx.core.net.toUri
import kotlinx.android.synthetic.main.fragment_add_on_permissions.*
import mozilla.components.feature.addons.Addon
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.createAddedTestFragmentInNavHostActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Shadows.shadowOf

@RunWith(FenixRobolectricTestRunner::class)
class AddonPermissionsDetailsFragmentTest {

    private val addon = Addon(
        id = "",
        translatableName = mapOf(
            Addon.DEFAULT_LOCALE to "Some blank addon"
        )
    )

    @Test
    fun `trigger view intent when learn more is clicked`() {
        val fragment = createAddedTestFragmentInNavHostActivity {
            AddonPermissionsDetailsFragment().apply {
                arguments = AddonPermissionsDetailsFragmentArgs(addon).toBundle()
            }
        }

        assertEquals("Some blank addon", fragment.activity?.title)

        fragment.learn_more_label.performClick()

        val intent = shadowOf(fragment.activity).peekNextStartedActivity()
        assertEquals(ACTION_VIEW, intent.action)
        assertEquals(
            "https://support.mozilla.org/kb/permission-request-messages-firefox-extensions".toUri(),
            intent.data
        )
    }
}
