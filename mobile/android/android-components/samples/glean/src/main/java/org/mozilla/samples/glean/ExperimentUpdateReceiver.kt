/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/**
 * Creates a [BroadcastReceiver] interface through which the MainActivity can receive notifications
 * when the experiments are updated.
 */
class ExperimentUpdateReceiver : BroadcastReceiver {

    private var listener: ExperimentUpdateListener

    constructor(listener: ExperimentUpdateListener) : super() {
        this.listener = listener
    }

    override fun onReceive(p0: Context?, p1: Intent?) {
        listener.onExperimentsUpdated()
    }

    interface ExperimentUpdateListener {
        fun onExperimentsUpdated()
    }
}