/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus

import android.content.Context
import org.mozilla.experiments.nimbus.AbstractNimbusBuilder
import org.mozilla.experiments.nimbus.EnrolledExperiment
import org.mozilla.experiments.nimbus.NimbusInterface

/**
 * Class to build instances of Nimbus.
 *
 * This _does not_ invoke any networking calls on the subsequent [Nimbus] object, so may safely
 * used before the engine is warmed up.
 */
class NimbusBuilder(context: Context) : AbstractNimbusBuilder<NimbusApi>(context) {
    var onApplyCallback: (NimbusApi) -> Unit = {}
    var onFetchedCallback: (NimbusApi) -> Unit = {}

    override fun newNimbus(
        appInfo: NimbusAppInfo,
        serverSettings: NimbusServerSettings?,
    ) = Nimbus(context, appInfo, serverSettings, errorReporter).apply {
        register(ExperimentsAppliedObserver(this, onApplyCallback))
        register(ExperimentsFetchedObserver(this, onFetchedCallback))
    }

    override fun newNimbusDisabled() = NimbusDisabled(context)
}

private class ExperimentsAppliedObserver(val nimbus: NimbusApi, val callback: (NimbusApi) -> Unit) :
    NimbusInterface.Observer {
    override fun onUpdatesApplied(updated: List<EnrolledExperiment>) {
        callback(nimbus)
    }
}

private class ExperimentsFetchedObserver(val nimbus: NimbusApi, val callback: (NimbusApi) -> Unit) :
    NimbusInterface.Observer {
    override fun onExperimentsFetched() {
        callback(nimbus)
    }
}
