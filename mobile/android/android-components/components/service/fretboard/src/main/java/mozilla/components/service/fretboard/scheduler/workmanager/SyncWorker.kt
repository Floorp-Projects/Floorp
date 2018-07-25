/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.scheduler.workmanager

import androidx.work.Worker
import mozilla.components.service.fretboard.Fretboard

abstract class SyncWorker : Worker() {
    override fun doWork(): Result {
        getFretboard().updateExperiments()
        return Result.SUCCESS
    }

    /**
     * Used to provide the instance of Fretboard
     * the app is using
     *
     * @return current Fretboard instance
     */
    abstract fun getFretboard(): Fretboard
}
