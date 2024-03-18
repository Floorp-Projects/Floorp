/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import android.view.View
import android.widget.TextView
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R

class ExceptionsHeaderViewHolderTest {

    private lateinit var view: View
    private lateinit var description: TextView

    @Before
    fun setup() {
        description = mockk(relaxUnitFun = true)
        view = mockk {
            every { findViewById<TextView>(R.id.exceptions_description) } returns description
            every {
                context.getString(eq(R.string.preferences_passwords_exceptions_description_2), any())
            } returns "Firefox fenix won’t save passwords for these sites."
            every {
                context.getString(R.string.app_name)
            } returns "Firefox fenix"
        }
    }

    @Test
    fun `sets description text`() {
        ExceptionsHeaderViewHolder(view, R.string.preferences_passwords_exceptions_description_2)
        verify { description.text = "Firefox fenix won’t save passwords for these sites." }
    }
}
