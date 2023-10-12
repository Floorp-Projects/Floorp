/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.widget.Button
import android.widget.TextView
import androidx.annotation.UiContext
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.action.ExtensionsProcessAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.webextensions.ExtensionsProcessDisabledPromptObserver
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.ext.components
import kotlin.system.exitProcess

/**
 * Controller for handling extensions process spawning disabled events. When the app is in
 * foreground this will call for a dialog to decide on correct action to take (retry enabling
 * process spawning or disable extensions). When in background, we kill the app to prevent
 * extensions from being disabled and network requests continuing.
 *
 * @param context to show the AlertDialog
 * @param browserStore The [BrowserStore] which holds the state for showing the dialog
 * @param appStore The [AppStore] containing the application state
 * @param builder to use for creating the dialog which can be styled as needed
 * @param appName to be added to the message. Optional and mainly relevant for testing
 * @param onKillApp called when the app is backgrounded and extensions process is disabled
 */
class ExtensionsProcessDisabledController(
    @UiContext context: Context,
    browserStore: BrowserStore = context.components.core.store,
    appStore: AppStore = context.components.appStore,
    builder: AlertDialog.Builder = AlertDialog.Builder(context),
    appName: String = context.appName,
    onKillApp: () -> Unit = { killApp() },
) : ExtensionsProcessDisabledPromptObserver(
    browserStore,
    {
        if (appStore.state.isForeground) {
            presentDialog(context, browserStore, builder, appName)
        } else {
            onKillApp.invoke()
        }
    },
) {
    override fun onDestroy(owner: LifecycleOwner) {
        super.onDestroy(owner)
        // In case the activity gets destroyed, we want to re-create the dialog.
        shouldCreateDialog = true
    }

    companion object {
        private var shouldCreateDialog: Boolean = true

        /**
         * When a dialog can't be shown because the app is in the background, instead the app will
         * be killed to prevent leaking network data without extensions enabled.
         */
        private fun killApp() {
            Handler(Looper.getMainLooper()).post {
                exitProcess(0)
            }
        }

        /**
         * Present a dialog to the user notifying of extensions process spawning disabled and also asking
         * whether they would like to continue trying or disable extensions. If the user chooses to retry,
         * enable the extensions process spawning. Otherwise, disable it.
         *
         * @param context to show the AlertDialog
         * @param store The [BrowserStore] which holds the state for showing the dialog
         * @param builder to use for creating the dialog which can be styled as needed
         * @param appName to be added to the message. Necessary to be added as a param for testing
         */
        private fun presentDialog(
            @UiContext context: Context,
            store: BrowserStore,
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
                    store.dispatch(ExtensionsProcessAction.ShowPromptAction(false))
                    store.dispatch(ExtensionsProcessAction.EnabledAction)
                    onDismissDialog?.invoke()
                }
                findViewById<Button>(R.id.negative)?.setOnClickListener {
                    store.dispatch(ExtensionsProcessAction.ShowPromptAction(false))
                    store.dispatch(ExtensionsProcessAction.DisabledAction)
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
