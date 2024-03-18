/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.utils

import java.util.concurrent.Executors
import java.util.concurrent.ThreadFactory
import java.util.concurrent.atomic.AtomicInteger

/**
 * A [ThreadFactory] that names its threads, "prefix-thread-<#>", deferring further thread
 * creation details to [Executors.defaultThreadFactory].
 */
class NamedThreadFactory(
    private val prefix: String,
) : ThreadFactory {

    private val backingFactory = Executors.defaultThreadFactory()
    private val threadNumber = AtomicInteger(1)

    override fun newThread(r: Runnable?): Thread = backingFactory.newThread(r).apply {
        val threadNumber = threadNumber.getAndIncrement()
        name = "$prefix-thread-$threadNumber"
    }
}
