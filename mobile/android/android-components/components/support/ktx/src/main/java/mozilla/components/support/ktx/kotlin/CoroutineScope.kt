/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.ReceiveChannel
import kotlinx.coroutines.channels.consumeEach
import kotlinx.coroutines.channels.produce
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
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
