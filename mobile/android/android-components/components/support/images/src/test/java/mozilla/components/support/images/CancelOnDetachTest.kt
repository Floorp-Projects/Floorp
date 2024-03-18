/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images

import android.view.View
import kotlinx.coroutines.Job
import mozilla.components.support.test.mock
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.verifyNoMoreInteractions

class CancelOnDetachTest {

    @Test
    fun `onViewAttached does nothing`() {
        val job: Job = mock()
        val view: View = mock()

        CancelOnDetach(job).onViewAttachedToWindow(view)

        verifyNoMoreInteractions(job)
        verifyNoMoreInteractions(view)
    }

    @Test
    fun `onViewDetachedFromWindow cancels the job`() {
        val job = Job()

        CancelOnDetach(job).onViewDetachedFromWindow(mock())

        assertTrue(job.isCancelled)
    }
}
