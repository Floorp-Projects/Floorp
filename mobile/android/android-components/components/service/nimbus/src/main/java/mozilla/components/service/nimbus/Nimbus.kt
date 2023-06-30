/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus

import android.content.Context
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.experiments.nimbus.EnrolledExperiment
import org.mozilla.experiments.nimbus.NimbusAppInfo
import org.mozilla.experiments.nimbus.NimbusDelegate
import org.mozilla.experiments.nimbus.NimbusDeviceInfo
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.experiments.nimbus.NimbusServerSettings
import org.mozilla.experiments.nimbus.Nimbus as ApplicationServicesNimbus

/**
 * Union of NimbusInterface which comes from another repo, and Observable.
 *
 * This only exists to allow the [Nimbus] class to be interchangeable [NimbusDisabled] class below.
 */
interface NimbusApi : NimbusInterface, Observable<NimbusInterface.Observer>

// Re-export these classes which were in this package previously.
// Clients which used these classes do not need to change.
typealias NimbusAppInfo = NimbusAppInfo
typealias NimbusServerSettings = NimbusServerSettings

/**
 * This is the main entry point to the Nimbus experiment subsystem.
 *
 * It can only be run after Glean has been set up, the megazord has finished loading, and viaduct
 * has been initialized.
 */
class Nimbus(
    context: Context,
    appInfo: NimbusAppInfo,
    coenrollingFeatureIds: List<String> = listOf(),
    server: NimbusServerSettings?,
    deviceInfo: NimbusDeviceInfo = NimbusDeviceInfo.default(),
    delegate: NimbusDelegate = NimbusDelegate.default(),
    private val observable: Observable<NimbusInterface.Observer> = ObserverRegistry(),
) : ApplicationServicesNimbus(
    context = context,
    appInfo = appInfo,
    coenrollingFeatureIds = coenrollingFeatureIds,
    server = server,
    deviceInfo = deviceInfo,
    delegate = delegate,
    observer = Observer(observable),
),
    NimbusApi,
    Observable<NimbusInterface.Observer> by observable {
    private class Observer(val observable: Observable<NimbusInterface.Observer>) : NimbusInterface.Observer {
        override fun onExperimentsFetched() {
            observable.notifyObservers { onExperimentsFetched() }
        }

        override fun onUpdatesApplied(updated: List<EnrolledExperiment>) {
            observable.notifyObservers { onUpdatesApplied(updated) }
        }
    }
}

/**
 * An empty implementation of the `NimbusInterface` to allow clients who have not enabled Nimbus (either
 * by feature flags, or by not using a server endpoint.
 *
 * Any implementations using this class will report that the user has not been enrolled into any
 * experiments, and will not report anything to Glean. Importantly, any calls to
 * `getExperimentBranch(slug)` will return `null`, i.e. as if the user is not enrolled into the
 * experiment.
 */
class NimbusDisabled(
    override val context: Context,
    private val observable: Observable<NimbusInterface.Observer> = ObserverRegistry(),
) : NimbusApi, Observable<NimbusInterface.Observer> by observable
