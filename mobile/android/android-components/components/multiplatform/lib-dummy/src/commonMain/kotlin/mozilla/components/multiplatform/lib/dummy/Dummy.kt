/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.multiplatform.lib.dummy

/**
 * Platform-specific delegate that will get invoked by [Dummy].
 */
expect class DummyDelegate {
    /**
     * Returns a platform-specific dummy value.
     */
    fun getValue(): String
}

/**
 * A dummy class for testing purposes.
 */
class Dummy(
    private val delegate: DummyDelegate
) {
    /**
     * Returns a platform-specific dummy value.
     */
    fun getValue(): String {
        return "Dummy ${delegate.getValue()}"
    }
}
