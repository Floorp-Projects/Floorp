/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.serviceworker

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.serviceworker.ServiceWorkerDelegate
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession

/**
 * Default implementation for supporting Gecko service workers.
 *
 * @param delegate [ServiceWorkerDelegate] handling service workers requests.
 * @param runtime [GeckoRuntime] current engine's runtime.
 * @param engineSettings [Settings] default settings used when new [EngineSession]s are to be created.
 */
class GeckoServiceWorkerDelegate(
    internal val delegate: ServiceWorkerDelegate,
    internal val runtime: GeckoRuntime,
    internal val engineSettings: Settings?,
) : GeckoRuntime.ServiceWorkerDelegate {
    override fun onOpenWindow(url: String): GeckoResult<GeckoSession> {
        val newEngineSession = GeckoEngineSession(runtime, false, engineSettings, openGeckoSession = false)

        return when (delegate.addNewTab(newEngineSession)) {
            true -> GeckoResult.fromValue(newEngineSession.geckoSession)
            false -> GeckoResult.fromValue(null)
        }
    }
}
