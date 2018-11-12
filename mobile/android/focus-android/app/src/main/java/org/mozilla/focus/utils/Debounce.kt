/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.ReceiveChannel
import kotlinx.coroutines.channels.produce
import kotlinx.coroutines.selects.whileSelect

/**
 * Creates a debounced ReceiveChannel.
 *
 * @param time the amount of time in units to debounce by
 * @param unit The unit to measure the amount of time to debounce by. Default value = TimeUnit.MILLISECONDS
 * @return a throttled channel
 */
@ExperimentalCoroutinesApi
fun <T> CoroutineScope.debounce(time: Long, channel: ReceiveChannel<T>): ReceiveChannel<T> =
    produce(coroutineContext, Channel.CONFLATED) {
        var value = channel.receive()

        whileSelect {
            onTimeout(time) {
                this@produce.offer(value)
                value = channel.receive()
                true
            }

            channel.onReceive {
                value = it
                true
            }
        }
    }
