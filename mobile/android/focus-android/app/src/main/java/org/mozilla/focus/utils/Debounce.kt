package org.mozilla.focus.utils

import kotlinx.coroutines.experimental.channels.Channel
import kotlinx.coroutines.experimental.channels.ReceiveChannel
import kotlinx.coroutines.experimental.launch
import kotlinx.coroutines.experimental.selects.whileSelect
import java.util.concurrent.TimeUnit

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
