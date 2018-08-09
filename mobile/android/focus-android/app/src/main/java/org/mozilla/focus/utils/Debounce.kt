/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import kotlinx.coroutines.experimental.channels.Channel
import kotlinx.coroutines.experimental.channels.ReceiveChannel
import kotlinx.coroutines.experimental.launch
import kotlinx.coroutines.experimental.selects.whileSelect
import java.util.concurrent.TimeUnit

/**
 * Creates a debounced a ReceiveChannel.
 *
 * @param time the amount of time in units to debounce by
 * @param unit The unit to measure the amount of time to debounce by. Default value = TimeUnit.MILLISECONDS
 * @return a throttled channel
 */
fun <T> ReceiveChannel<T>.debounce(time: Long, unit: TimeUnit = TimeUnit.MILLISECONDS): ReceiveChannel<T> =
    Channel<T>(capacity = Channel.CONFLATED).also { channel ->
        launch {
            var value = receive()

            whileSelect {
                onTimeout(time, unit) {
                    channel.offer(value)
                    value = receive()
                    true
                }

                onReceive {
                    value = it
                    true
                }
            }
        }
    }
