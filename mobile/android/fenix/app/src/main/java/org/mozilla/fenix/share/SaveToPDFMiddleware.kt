/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import android.widget.Toast
import android.widget.Toast.LENGTH_LONG
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.R
import org.mozilla.gecko.util.ThreadUtils
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_NO_ACTIVITY_CONTEXT
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_NO_ACTIVITY_CONTEXT_DELEGATE
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_NO_PRINT_DELEGATE
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_PRINT_SETTINGS_SERVICE_NOT_AVAILABLE
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_UNABLE_TO_CREATE_PRINT_SETTINGS
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_UNABLE_TO_RETRIEVE_CANONICAL_BROWSING_CONTEXT
import java.io.IOException
import java.lang.Exception

/**
 * [BrowserAction] middleware reacting in response to Save to PDF related [Action]s.
 * @property context An Application context.
 */
class SaveToPDFMiddleware(
    private val context: Context,
    private val mainScope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : Middleware<BrowserState, BrowserAction> {

    override fun invoke(
        ctx: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is EngineAction.SaveToPdfAction -> {
                postTelemetryTapped(ctx.state.findTab(action.tabId))
                // Continue to generate the PDF, passing through here to add telemetry
                next(action)
            }

            is EngineAction.SaveToPdfCompleteAction -> {
                postTelemetryCompleted(ctx.state.findTab(action.tabId))
            }

            is EngineAction.SaveToPdfExceptionAction -> {
                // See https://github.com/mozilla-mobile/fenix/issues/27649 for more details,
                // why a Toast is used here.
                ThreadUtils.runOnUiThread {
                    Toast.makeText(context, R.string.unable_to_save_to_pdf_error, LENGTH_LONG).show()
                }

                postTelemetryFailed(ctx.state.findTab(action.tabId), action.throwable)
            }
            else -> {
                next(action)
            }
        }
    }

    /**
     * Use to generate failure extra reasons for Save To PDF failure telemetry.
     *
     * @param exception - A given exception that will be properly labeled for telemetry posting.
     * @return processed failure reason to send in telemetry.
     */
    /* package */ @VisibleForTesting
    fun telemetryErrorReason(exception: Exception): String {
        var failureMsg = "unknown"
        // Requiring information from GeckoView isn't a good practice,
        // follow-up to improve this is bug 1838719
        if (exception is GeckoSession.GeckoPrintException) {
            when (exception.code) {
                ERROR_PRINT_SETTINGS_SERVICE_NOT_AVAILABLE -> failureMsg = "no_settings_service"
                ERROR_UNABLE_TO_CREATE_PRINT_SETTINGS -> failureMsg = "no_settings"
                ERROR_UNABLE_TO_RETRIEVE_CANONICAL_BROWSING_CONTEXT -> failureMsg = "no_canonical_context"
                ERROR_NO_ACTIVITY_CONTEXT_DELEGATE -> failureMsg = "no_activity_context_delegate"
                ERROR_NO_ACTIVITY_CONTEXT -> failureMsg = "no_activity_context"
                ERROR_NO_PRINT_DELEGATE -> failureMsg = "no_print_delegate"
            }
        }
        if (exception is IOException) {
            failureMsg = "io_error"
        }
        return failureMsg
    }

    /**
     * Use to generate extra sources for Save To PDF telemetry.
     *
     * @param isPdfViewer - If the page has a PDF viewer or not.
     * @return processed page source type to send in telemetry.
     */
    /* package */ @VisibleForTesting
    fun telemetrySource(isPdfViewer: Boolean?): String {
        val source = when (isPdfViewer) {
            null -> "unknown"
            true -> "pdf"
            false -> "non-pdf"
        }
        return source
    }

    /**
     * Indicates the Save As PDF action was requested and posts telemetry via Glean.
     *
     * @param tab - tab state to use for page source category
     */
    private fun postTelemetryTapped(tab: TabSessionState?) {
        mainScope.launch {
            tab?.engineState?.engineSession?.checkForPdfViewer(
                onResult = { isPdf ->
                    Events.saveToPdfTapped.record(
                        Events.SaveToPdfTappedExtra(
                            source = telemetrySource(isPdf),
                        ),
                    )
                },
                onException = {
                    Events.saveToPdfTapped.record(
                        Events.SaveToPdfTappedExtra(
                            source = telemetrySource(null),
                        ),
                    )
                },
            )
        }
    }

    /**
     * Indicates the Save As PDF action completed and generated a PDF and posts telemetry via Glean.
     *
     * @param tab - tab state to use for page source category
     */
    private fun postTelemetryCompleted(tab: TabSessionState?) {
        mainScope.launch {
            tab?.engineState?.engineSession?.checkForPdfViewer(
                onResult = { isPdf ->
                    Events.saveToPdfCompleted.record(
                        Events.SaveToPdfCompletedExtra(
                            source = telemetrySource(isPdf),
                        ),
                    )
                },
                onException = {
                    Events.saveToPdfCompleted.record(
                        Events.SaveToPdfCompletedExtra(
                            source = telemetrySource(null),
                        ),
                    )
                },
            )
        }
    }

    /**
     * Indicates the Save As PDF action failed and the reason for failure and posts telemetry via Glean.
     *
     * @param tab - tab state to use for page source category
     * @param throwable - failure state to use for failure reason category
     */
    private fun postTelemetryFailed(tab: TabSessionState?, throwable: Throwable) {
        val telFailureReason = telemetryErrorReason(throwable as Exception)
        mainScope.launch {
            tab?.engineState?.engineSession?.checkForPdfViewer(
                onResult = { isPdf ->
                    Events.saveToPdfFailure.record(
                        Events.SaveToPdfFailureExtra(
                            source = telemetrySource(isPdf),
                            reason = telFailureReason,
                        ),
                    )
                },
                onException = {
                    Events.saveToPdfFailure.record(
                        Events.SaveToPdfFailureExtra(
                            source = telemetrySource(null),
                            reason = telFailureReason,
                        ),
                    )
                },
            )
        }
    }
}
