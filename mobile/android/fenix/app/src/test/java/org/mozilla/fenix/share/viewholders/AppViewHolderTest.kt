/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.viewholders

import android.view.LayoutInflater
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.AppShareListItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.share.ShareToAppsInteractor
import org.mozilla.fenix.share.listadapters.AppShareOption

@RunWith(FenixRobolectricTestRunner::class)
class AppViewHolderTest {

    private lateinit var binding: AppShareListItemBinding
    private lateinit var viewHolder: AppViewHolder
    private lateinit var interactor: ShareToAppsInteractor

    @Before
    fun setup() {
        interactor = mockk(relaxUnitFun = true)

        binding = AppShareListItemBinding.inflate(LayoutInflater.from(testContext))
        viewHolder = AppViewHolder(binding.root, interactor)
    }

    @Test
    fun `bind app share option`() {
        val app = AppShareOption(
            name = "Pocket",
            icon = getDrawable(testContext, R.drawable.ic_pocket)!!,
            packageName = "com.mozilla.pocket",
            activityName = "MainActivity"
        )
        viewHolder.bind(app)

        assertEquals("Pocket", binding.appName.text)
        assertEquals(app.icon, binding.appIcon.drawable)
    }

    @Test
    fun `trigger interactor if application is bound`() {
        val app = AppShareOption(
            name = "Pocket",
            icon = getDrawable(testContext, R.drawable.ic_pocket)!!,
            packageName = "com.mozilla.pocket",
            activityName = "MainActivity"
        )

        viewHolder.itemView.performClick()
        verify { interactor wasNot Called }

        viewHolder.bind(app)
        viewHolder.itemView.performClick()
        verify { interactor.onShareToApp(app) }
    }
}
