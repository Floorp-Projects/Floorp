/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.components.TestComponents

/**
 * An override of our application for use in Robolectric-based unit tests.
 */
class FenixRobolectricTestApplication : FenixApplication() {

    override val components = TestComponents(this)

    override fun setupInAllProcesses() = Unit

    override fun setupInMainProcessOnly() = Unit
}
