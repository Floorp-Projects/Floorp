/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.app.AlertDialog
import android.content.Context
import androidx.annotation.UiContext
import mozilla.components.browser.state.action.ExtensionProcessDisabledPopupAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.webextensions.ExtensionProcessDisabledPopupFeature
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.geckoview.WebExtensionController

/**
 * Present a dialog to the user notifying of extension process spawning disabled and also asking
 * whether they would like to continue trying or disable extensions. If the user chooses to retry,
 * enable the extension process spawning with [WebExtensionController.enableExtensionProcessSpawning].
 *
 * @param context to show the AlertDialog
 * @param store The [BrowserStore] which holds the state for showing the dialog
 * @param webExtensionController to call when a user enables the process spawning
 * @param builder to use for creating the dialog which can be styled as needed
 * @param appName to be added to the message. Necessary to be added as a param for testing
 */
private fun presentDialog(
    @UiContext context: Context,
    store: BrowserStore,
    engine: Engine,
    builder: AlertDialog.Builder,
    appName: String,
) {
    val message = context.getString(R.string.addon_process_crash_dialog_message, appName)

    builder.apply {
        setCancelable(false)
        setTitle(R.string.addon_process_crash_dialog_title)
        setMessage(message)
        setPositiveButton(R.string.addon_process_crash_dialog_retry_button_text) { dialog, _ ->
            engine.enableExtensionProcessSpawning()
            Addons.extensionsProcessUiRetry.add()
            store.dispatch(ExtensionProcessDisabledPopupAction(false))
            dialog.dismiss()
        }
        setNegativeButton(R.string.addon_process_crash_dialog_disable_addons_button_text) { dialog, _ ->
            Addons.extensionsProcessUiDisable.add()
            store.dispatch(ExtensionProcessDisabledPopupAction(false))
            dialog.dismiss()
        }
    }

    builder.show()
}

/**
 * Controller for showing the user a dialog when the the extension process spawning has been disabled.
 *
 * @param context to show the AlertDialog
 * @param store The [BrowserStore] which holds the state for showing the dialog
 * @param webExtensionController to call when a user enables the process spawning
 * @param builder to use for creating the dialog which can be styled as needed
 * @param appName to be added to the message. Optional and mainly relevant for testing
 */
class ExtensionProcessDisabledController(
    @UiContext context: Context,
    store: BrowserStore,
    engine: Engine = context.components.core.engine,
    builder: AlertDialog.Builder = AlertDialog.Builder(context, R.style.DialogStyleNormal),
    appName: String = context.appName,
) : ExtensionProcessDisabledPopupFeature(
    store,
    { presentDialog(context, store, engine, builder, appName) },
)
