/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.fakes

/**
 * A fake clock exposing a [time] property that can be changed at test runtime.
 *
 * Ideally a class needing time hides the calls to System.currentTimeMillis() behind a lambda
 * that can it can invoke. In a test this lambda can be replaced by FakeClock()::time.
 */
class FakeClock(
    var time: Long = 0,
) {
    fun advanceBy(time: Long) {
        this.time += time
    }
}
