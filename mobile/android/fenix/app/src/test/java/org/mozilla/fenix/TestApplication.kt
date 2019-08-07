/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.TestComponents

@ObsoleteCoroutinesApi
class TestApplication : FenixApplication() {

    override val components: Components
        get() = TestComponents(this)

    override fun setupApplication() {
    }
}
