/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.lock

import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.support.base.android.Clock

// Time after the last unlock that will require a new unlock
private const val AUTOLOCK_TIME = 5 * 60 * 1000

/**
 * Helper for keeping track of the lock/unlock state for autofill. The actual unlocking or
 * decrypting of the underlying storage is done by the [LoginsStorage] implementation.
 */
class AutofillLock {
    private var lastUnlockTimestmap: Long = 0

    /**
     * Checks whether the autofill lock is still unlocked and whether autofill options will be shown
     * without authenticating again.
     */
    @Synchronized
    fun isUnlocked() = lastUnlockTimestmap + AUTOLOCK_TIME >= Clock.elapsedRealtime()

    /**
     * If the autofill lock is unlocked then this will keep it unlocked by extending the time until
     * it will automatically get locked again.
     */
    @Synchronized
    fun keepUnlocked(): Boolean {
        return if (isUnlocked()) {
            unlock()
            true
        } else {
            false
        }
    }

    /**
     * Unlocks the autofill lock.
     */
    @Synchronized
    fun unlock() {
        lastUnlockTimestmap = Clock.elapsedRealtime()
    }
}
