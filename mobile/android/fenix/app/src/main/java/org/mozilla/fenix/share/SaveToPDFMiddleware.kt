/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.StandardSnackbarError
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.components
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_NO_ACTIVITY_CONTEXT
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_NO_ACTIVITY_CONTEXT_DELEGATE
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_NO_PRINT_DELEGATE
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_PRINT_SETTINGS_SERVICE_NOT_AVAILABLE
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_UNABLE_TO_CREATE_PRINT_SETTINGS
import org.mozilla.geckoview.GeckoSession.GeckoPrintException.ERROR_UNABLE_TO_RETRIEVE_CANONICAL_BROWSING_CONTEXT
import java.io.IOException

/**
 * [BrowserAction] middleware reacting in response to Save to PDF related [Action]s.
 *
 * @property context An Application context.
 * @property mainScope Coroutine scope to launch coroutines.
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
                postTelemetryTapped(ctx.state.findTab(action.tabId), isPrint = false)
                // Continue to generate the PDF, passing through here to add telemetry
                next(action)
            }

            is EngineAction.SaveToPdfCompleteAction -> {
                postTelemetryCompleted(ctx.state.findTab(action.tabId), isPrint = false)
            }

            is EngineAction.SaveToPdfExceptionAction -> {
                context.components.appStore.dispatch(
                    AppAction.UpdateStandardSnackbarErrorAction(
                        StandardSnackbarError(
                            context.getString(R.string.unable_to_save_to_pdf_error),
                        ),
                    ),
                )
                postTelemetryFailed(ctx.state.findTab(action.tabId), action.throwable, isPrint = false)
            }

            is EngineAction.PrintContentAction -> {
                postTelemetryTapped(ctx.state.findTab(action.tabId), isPrint = true)
                // Continue to print, passing through here to add telemetry
                next(action)
            }
            is EngineAction.PrintContentCompletedAction -> {
                postTelemetryCompleted(ctx.state.findTab(action.tabId), isPrint = true)
            }
            is EngineAction.PrintContentExceptionAction -> {
                context.components.appStore.dispatch(
                    AppAction.UpdateStandardSnackbarErrorAction(
                        StandardSnackbarError(
                            context.getString(R.string.unable_to_print_page_error),
                        ),
                    ),
                )
                postTelemetryFailed(ctx.state.findTab(action.tabId), action.throwable, isPrint = true)
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
    @VisibleForTesting // package
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
     * @param isPdfViewer If the page has a PDF viewer or not.
     * @return processed page source type to send in telemetry.
     */
    @VisibleForTesting // package
    fun telemetrySource(isPdfViewer: Boolean?): String {
        val source = when (isPdfViewer) {
            null -> "unknown"
            true -> "pdf"
            false -> "non-pdf"
        }
        return source
    }

    /**
     * Indicates the Print or Save As PDF action was requested and posts telemetry via Glean.
     *
     * @param tab tab state to use for page source category
     * @param isPrint if the telemetry is for printing
     */
    private fun postTelemetryTapped(tab: TabSessionState?, isPrint: Boolean) {
        mainScope.launch {
            tab?.engineState?.engineSession?.checkForPdfViewer(
                onResult = { isPdf ->
                    if (isPrint) {
                        Events.printTapped.record(
                            Events.PrintTappedExtra(
                                source = telemetrySource(isPdf),
                            ),
                        )
                    } else {
                        Events.saveToPdfTapped.record(
                            Events.SaveToPdfTappedExtra(
                                source = telemetrySource(isPdf),
                            ),
                        )
                    }
                },
                onException = {
                    if (isPrint) {
                        Events.printTapped.record(
                            Events.PrintTappedExtra(
                                source = telemetrySource(null),
                            ),
                        )
                    } else {
                        Events.saveToPdfTapped.record(
                            Events.SaveToPdfTappedExtra(
                                source = telemetrySource(null),
                            ),
                        )
                    }
                },
            )
        }
    }

    /**
     * Indicates the Print or Save As PDF action completed and generated a PDF and posts telemetry via Glean.
     *
     * @param tab tab state to use for page source category
     * @param isPrint if the telemetry is for printing
     */
    private fun postTelemetryCompleted(tab: TabSessionState?, isPrint: Boolean) {
        mainScope.launch {
            tab?.engineState?.engineSession?.checkForPdfViewer(
                onResult = { isPdf ->
                    if (isPrint) {
                        Events.printCompleted.record(
                            Events.PrintCompletedExtra(
                                source = telemetrySource(isPdf),
                            ),
                        )
                    } else {
                        Events.saveToPdfCompleted.record(
                            Events.SaveToPdfCompletedExtra(
                                source = telemetrySource(isPdf),
                            ),
                        )
                    }
                },
                onException = {
                    if (isPrint) {
                        Events.printCompleted.record(
                            Events.PrintCompletedExtra(
                                source = telemetrySource(null),
                            ),
                        )
                    } else {
                        Events.saveToPdfCompleted.record(
                            Events.SaveToPdfCompletedExtra(
                                source = telemetrySource(null),
                            ),
                        )
                    }
                },
            )
        }
    }

    /**
     * Indicates the Print or Save As PDF action failed and the reason for failure and posts telemetry via Glean.
     *
     * @param tab tab state to use for page source category
     * @param throwable failure state to use for failure reason category
     * @param isPrint if the telemetry is for printing
     */
    private fun postTelemetryFailed(tab: TabSessionState?, throwable: Throwable, isPrint: Boolean) {
        val telFailureReason = telemetryErrorReason(throwable as Exception)
        mainScope.launch {
            tab?.engineState?.engineSession?.checkForPdfViewer(
                onResult = { isPdf ->
                    if (isPrint) {
                        Events.printFailure.record(
                            Events.PrintFailureExtra(
                                source = telemetrySource(isPdf),
                                reason = telFailureReason,
                            ),
                        )
                    } else {
                        Events.saveToPdfFailure.record(
                            Events.SaveToPdfFailureExtra(
                                source = telemetrySource(isPdf),
                                reason = telFailureReason,
                            ),
                        )
                    }
                },
                onException = {
                    if (isPrint) {
                        Events.printFailure.record(
                            Events.PrintFailureExtra(
                                source = telemetrySource(null),
                                reason = telFailureReason,
                            ),
                        )
                    } else {
                        Events.saveToPdfFailure.record(
                            Events.SaveToPdfFailureExtra(
                                source = telemetrySource(null),
                                reason = telFailureReason,
                            ),
                        )
                    }
                },
            )
        }
    }
}
