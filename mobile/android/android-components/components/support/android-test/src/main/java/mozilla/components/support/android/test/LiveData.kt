/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test

import androidx.lifecycle.LiveData
import androidx.lifecycle.Observer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * Subscribes to the [LiveData] object and blocks until a value was observed. Returns the value or throws
 * [InterruptedException] if no value was observed in the given [timeout] (using [unit]).
 *
 * This is helpful for tests using [LiveData] objects that won't contain any data unless observed (e.g. LiveData objects
 * returned by Room).
 */
@Throws(InterruptedException::class)
fun <T> LiveData<T>.awaitValue(
    timeout: Long = 1,
    unit: TimeUnit = TimeUnit.SECONDS
): T? {
    val latch = CountDownLatch(1)
    var value: T? = null

    val observer = object : Observer<T> {
        override fun onChanged(t: T?) {
            value = t
            removeObserver(this)
            latch.countDown()
        }
    }

    observeForever(observer)

    latch.await(timeout, unit)

    return value
}
