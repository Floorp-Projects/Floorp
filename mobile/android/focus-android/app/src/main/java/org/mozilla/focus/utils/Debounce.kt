/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.ReceiveChannel
import kotlinx.coroutines.channels.consumeEach
import kotlinx.coroutines.channels.produce
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Creates a debounced ReceiveChannel.
 *
 * @param time the amount of time in units to debounce by
 * @param channel the channel to debounce
 * @return a throttled channel
 */
fun <T> CoroutineScope.debounce(time: Long, channel: ReceiveChannel<T>): ReceiveChannel<T> =
    produce(coroutineContext, Channel.CONFLATED) {
        var job: Job? = null
        channel.consumeEach {
            job?.cancel()
            job = launch {
                delay(time)
                send(it)
            }
        }
        job?.join()
    }
