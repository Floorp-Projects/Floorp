/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.push

import android.app.Notification
import androidx.core.app.NotificationCompat
import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import org.junit.Test
import org.mozilla.fenix.R

class LeanplumNotificationCustomizerTest {

    private val customizer = LeanplumNotificationCustomizer()

    @Test
    fun `customize adds icon`() {
        val builder = mockk<NotificationCompat.Builder>(relaxed = true)
        customizer.customize(builder, mockk())

        verify { builder.setSmallIcon(R.drawable.ic_status_logo) }
    }

    @Test
    fun `customize for BigPictureStyle does nothing`() {
        val builder = mockk<Notification.Builder>()
        customizer.customize(builder, mockk(), mockk())

        verify { builder wasNot Called }
    }
}
