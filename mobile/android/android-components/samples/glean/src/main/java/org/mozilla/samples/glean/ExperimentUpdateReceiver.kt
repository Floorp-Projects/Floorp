/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/**
 * Creates a [BroadcastReceiver] interface through which the MainActivity can receive notifications
 * when the experiments are updated.  This is not relevant to the Glean SDK and is only important
 * to the experiments library.
 */
class ExperimentUpdateReceiver : BroadcastReceiver {

    private var listener: ExperimentUpdateListener

    /**
     * Secondary constructor which allows the listener to be passed in
     *
     * @param listener: The listener to notify when the experiment is updated.
     */
    constructor(listener: ExperimentUpdateListener) : super() {
        this.listener = listener
    }

    /**
     * When the broadcast notification is received, calls the `onExperimentUpdated` on the listener
     */
    override fun onReceive(context: Context?, intent: Intent?) {
        listener.onExperimentsUpdated()
    }

    /**
     * Public interface for the listener to implement
     */
    interface ExperimentUpdateListener {
        /**
         * Callback function that will be invoked when the experiments are updated.
         */
        fun onExperimentsUpdated()
    }
}
