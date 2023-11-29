/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.view.Window
import android.view.WindowManager
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.withContext
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings

/**
 * Binding to react to Private Browsing Mode changes in AppState.
 *
 * @param appStore AppStore to observe state changes from.
 * @param themeManager Theme will be updated based on state changes.
 * @param retrieveWindow Get window to update privacy flags for.
 * @param settings Determine user settings for privacy features.
 * @param ioDispatcher Dispatcher to launch disk reads. Exposed for test.
 */
class BrowsingModeBinding(
    appStore: AppStore,
    private val themeManager: ThemeManager,
    private val retrieveWindow: () -> Window,
    private val settings: Settings,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : AbstractBinding<AppState>(appStore) {
    override suspend fun onState(flow: Flow<AppState>) {
        flow.distinctUntilChangedBy { it.mode }.collect {
            themeManager.currentTheme = it.mode
            setWindowPrivacy(it.mode)
        }
    }

    private suspend fun setWindowPrivacy(mode: BrowsingMode) {
        if (mode == BrowsingMode.Private) {
            val allowScreenshots = withContext(ioDispatcher) {
                settings.allowScreenshotsInPrivateMode
            }
            if (!allowScreenshots) {
                retrieveWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE)
            }
        } else {
            retrieveWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
        }
    }
}
