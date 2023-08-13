/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Context
import android.view.LayoutInflater
import android.widget.Button
import android.widget.TextView
import androidx.annotation.UiContext
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.action.ExtensionProcessDisabledPopupAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.webextensions.ExtensionProcessDisabledPopupObserver
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

/**
 * Controller for showing the user a dialog when the the extension process spawning has been disabled.
 *
 * @param context to show the AlertDialog
 * @param store The [BrowserStore] which holds the state for showing the dialog
 * @param engine An [Engine] instance used for handling extension process spawning.
 * @param builder to use for creating the dialog which can be styled as needed
 * @param appName to be added to the message. Optional and mainly relevant for testing
 */
class ExtensionProcessDisabledController(
    @UiContext context: Context,
    store: BrowserStore,
    engine: Engine = context.components.core.engine,
    builder: AlertDialog.Builder = AlertDialog.Builder(context),
    appName: String = context.appName,
) : ExtensionProcessDisabledPopupObserver(
    store,
    { presentDialog(context, store, engine, builder, appName) },
) {
    override fun onDestroy(owner: LifecycleOwner) {
        super.onDestroy(owner)
        // In case the activity gets destroyed, we want to re-create the dialog.
        shouldCreateDialog = true
    }

    companion object {
        private var shouldCreateDialog: Boolean = true

        /**
         * Present a dialog to the user notifying of extension process spawning disabled and also asking
         * whether they would like to continue trying or disable extensions. If the user chooses to retry,
         * enable the extension process spawning with [Engine.enableExtensionProcessSpawning].
         * Otherwise, call [Engine.disableExtensionProcessSpawning].
         *
         * @param context to show the AlertDialog
         * @param store The [BrowserStore] which holds the state for showing the dialog
         * @param engine An [Engine] instance used for handling extension process spawning
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
            if (!shouldCreateDialog) {
                return
            }

            val message = context.getString(R.string.addon_process_crash_dialog_message, appName)
            var onDismissDialog: (() -> Unit)? = null
            val layout = LayoutInflater.from(context)
                .inflate(R.layout.crash_extension_dialog, null, false)
            layout?.apply {
                findViewById<TextView>(R.id.message)?.text = message
                findViewById<Button>(R.id.positive)?.setOnClickListener {
                    engine.enableExtensionProcessSpawning()
                    Addons.extensionsProcessUiRetry.add()
                    store.dispatch(ExtensionProcessDisabledPopupAction(false))
                    onDismissDialog?.invoke()
                }
                findViewById<Button>(R.id.negative)?.setOnClickListener {
                    engine.disableExtensionProcessSpawning()
                    Addons.extensionsProcessUiDisable.add()
                    store.dispatch(ExtensionProcessDisabledPopupAction(false))
                    onDismissDialog?.invoke()
                }
            }
            builder.apply {
                setCancelable(false)
                setView(layout)
                setTitle(R.string.addon_process_crash_dialog_title)
            }

            val dialog = builder.show()
            shouldCreateDialog = false
            onDismissDialog = {
                dialog?.dismiss()
                shouldCreateDialog = true
            }
        }
    }
}
