/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

/**
 * This class is used to start a foreground service safely. For api levels >= 31. It will only
 * start the service if the app is in the foreground to prevent throwing the
 * ForegroundServiceStartNotAllowedException.
 *
 * @param processInfoProvider The provider to check if the app is in the foreground.
 * @param buildVersionProvider The provider to get the sdk version.
 */
class StartForegroundService(
    private val processInfoProvider: ProcessInfoProvider = DefaultProcessInfoProvider(),
    private val buildVersionProvider: BuildVersionProvider = DefaultBuildVersionProvider(),
    private val powerManagerInfoProvider: PowerManagerInfoProvider,
) {

    /**
     * @see StartForegroundService
     *
     * @param func The function to run if the app is in the foreground to follow the foreground
     * service restrictions for sdk version >= 31. For lower versions, the function will always run.
     */
    operator fun invoke(func: () -> Unit): Boolean =
        if (buildVersionProvider.sdkInt() >= BuildVersionProvider.FOREGROUND_SERVICE_RESTRICTIONS_STARTING_VERSION) {
            if (powerManagerInfoProvider.isIgnoringBatteryOptimizations() ||
                processInfoProvider.isForegroundImportance()
            ) {
                func()
                true
            } else {
                false
            }
        } else {
            func()
            true
        }
}
