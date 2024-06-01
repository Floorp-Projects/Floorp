/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

import mozilla.components.support.base.observer.Observable

/**
 * A queue that holds pending device commands until they're ready to be sent.
 */
interface DeviceCommandQueue<in T : DeviceCommandQueue.Type> : Observable<DeviceCommandQueue.Observer> {
    /**
     * Adds a pending command to the queue.
     *
     * @param deviceId The target device ID.
     * @param command The command to add.
     */
    suspend fun add(deviceId: String, command: T)

    /**
     * Removes a pending command from the queue.
     *
     * @param deviceId The target device ID.
     * @param command The command to remove.
     */
    suspend fun remove(deviceId: String, command: T)

    /** Sends all pending commands. */
    suspend fun flush(): FlushResult

    /**
     * The type of the commands held in the queue. This is a marker
     * superinterface implemented by [DeviceCommandOutgoing] commands that
     * support queueing.
     */
    interface Type {
        /** A marker interface for a queueable remote tabs command. */
        sealed interface RemoteTabs : Type
    }

    /** Receives notifications when the command queue changes. */
    interface Observer {
        /** A command was added to the queue. */
        fun onAdded() = Unit

        /** A command was removed from the queue. */
        fun onRemoved() = Unit
    }

    /** The result of a call to [DeviceCommandQueue.flush]. */
    sealed interface FlushResult {
        /** The queue was flushed successfully. */
        data object Ok : FlushResult

        /** The queue should be flushed again. */
        data object Retry : FlushResult

        /**
         * Combines this [FlushResult] with another [FlushResult].
         *
         * @return [Ok] if both results are [Ok], or [Retry] otherwise.
         */
        infix fun and(other: FlushResult): FlushResult =
            if (this is Ok && other is Ok) {
                Ok
            } else {
                Retry
            }

        companion object {
            /** Returns [Ok] as a [FlushResult]. */
            fun ok(): FlushResult = Ok

            /** Returns [Retry] as a [FlushResult]. */
            fun retry(): FlushResult = Retry
        }
    }
}
